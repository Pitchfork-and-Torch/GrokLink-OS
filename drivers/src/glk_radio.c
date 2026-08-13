/**
 * Async radio worker (host: queue+thread; device: inline jobs).
 */
#include "glk_drv/glk_radio.h"
#include "glk_hal/glk_hal_subghz.h"
#include "glk/glk_kernel.h"
#include "glk/glk_config.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(GLK_PLATFORM_STM32) && !defined(GLK_PLATFORM_HOST)
#define GLK_RADIO_INLINE 1
#else
#define GLK_RADIO_INLINE 0
#endif

typedef struct {
    glk_radio_job_t job;
    glk_radio_cb cb;
    void* user;
    uint8_t valid;
} radio_msg_t;

static glk_policy_state_t* s_pol;
static glk_queue_t* s_q;
static glk_task_t* s_worker;
static volatile glk_radio_state_t s_state;
static volatile int s_stop;
static glk_mutex_t* s_sync_mu;
static glk_mutex_t* s_arb_mu;
static volatile int s_arb_held;
static glk_radio_result_t s_sync_res;
static volatile int s_sync_done;

/* Spectrum planner duty accounting (cannot be bypassed). */
#ifndef GLK_SPECTRUM_DUTY_WINDOW_MS
#define GLK_SPECTRUM_DUTY_WINDOW_MS 30000u
#endif
#ifndef GLK_SPECTRUM_DUTY_MAX_MS
#define GLK_SPECTRUM_DUTY_MAX_MS 12000u /* max on-air within window */
#endif
static uint32_t s_duty_window_start_ms;
static uint32_t s_duty_used_ms;
static uint32_t s_last_settle_ms;
static uint32_t s_last_bands;
static uint32_t s_breaker_trips;

static void duty_reset_if_needed(void) {
    uint32_t now = glk_tick_get();
    if (s_duty_window_start_ms == 0 ||
        (now - s_duty_window_start_ms) > GLK_SPECTRUM_DUTY_WINDOW_MS) {
        s_duty_window_start_ms = now;
        s_duty_used_ms = 0;
    }
}

static bool duty_allow(uint32_t add_ms) {
    duty_reset_if_needed();
    if (s_duty_used_ms + add_ms > GLK_SPECTRUM_DUTY_MAX_MS) return false;
    return true;
}

static void duty_account(uint32_t ms) {
    duty_reset_if_needed();
    s_duty_used_ms += ms;
}

static bool breaker_tripped(void) {
    if (!s_pol) return false;
    return s_pol->radio_faults >= GLK_RADIO_FAULT_BREAKER;
}

static void sync_cb(const glk_radio_result_t* res, void* user) {
    (void)user;
    s_sync_res = *res;
    s_sync_done = 1;
}

static void process_job(const radio_msg_t* msg) {
    glk_radio_result_t res;
    memset(&res, 0, sizeof(res));
    res.freq_hz = msg->job.freq_hz;
    res.duration_ms = msg->job.duration_ms;
#if defined(GLK_RADIO_SIM) || !defined(GLK_PLATFORM_STM32) || defined(GLK_PLATFORM_HOST)
    res.simulated = true;
#else
    res.simulated = false;
#endif

    glk_policy_request_t req;
    memset(&req, 0, sizeof(req));
    req.actor = msg->job.actor;
    req.freq_hz = msg->job.freq_hz;
    req.gpio_pin = -1;
    req.confirm_id = msg->job.confirm_id[0] ? msg->job.confirm_id : NULL;

    if (msg->job.is_tx) {
        req.action = "subghz_tx";
        req.risk = GLK_RISK_ACTIVE_TX;
        glk_policy_decision_t d = glk_policy_check(s_pol, &req);
        if (d.result != GLK_POLICY_ALLOW) {
            res.err = GLK_ERR_DENIED;
            s_state = GLK_RADIO_IDLE;
            if (msg->cb) msg->cb(&res, msg->user);
            return;
        }
        s_state = GLK_RADIO_TX;
        if (msg->job.tx_path[0]) {
            FILE* f = fopen(msg->job.tx_path, "rb");
            if (!f) {
                res.err = GLK_ERR_NOTFOUND;
                glk_policy_note_radio_fault(s_pol);
                s_state = GLK_RADIO_IDLE;
                if (msg->cb) msg->cb(&res, msg->user);
                return;
            }
            fclose(f);
        }
        uint32_t tms = msg->job.duration_ms ? msg->job.duration_ms : 100;
        glk_err_t te = glk_hal_subghz_tx_carrier_ms(msg->job.freq_hz, tms);
        glk_policy_note_tx(s_pol, tms);
        res.err = te;
        res.pulses = 0;
        /* keep res.simulated from build flag (false on real SPI builds) */
    } else {
        req.action = "subghz_rx";
        req.risk = GLK_RISK_PASSIVE_RX;
        glk_policy_decision_t d = glk_policy_check(s_pol, &req);
        if (d.result != GLK_POLICY_ALLOW) {
            res.err = (d.result == GLK_POLICY_RATE_LIMITED) ? GLK_ERR_BUSY : GLK_ERR_DENIED;
            s_state = GLK_RADIO_IDLE;
            if (msg->cb) msg->cb(&res, msg->user);
            return;
        }
        s_state = GLK_RADIO_RX;
        uint32_t ms = msg->job.duration_ms;
        if (ms == 0) ms = GLK_RX_DURATION_DEFAULT_MS;
        if (ms > GLK_RX_DURATION_MAX_MS) ms = GLK_RX_DURATION_MAX_MS;
        int32_t pulses = 0;
        int16_t rssi = 0;
        int16_t rssi_min = 0;
        int16_t rssi_max = 0;
        glk_err_t re = glk_hal_subghz_rx_async_stats(
            msg->job.freq_hz, ms, &pulses, &rssi, &rssi_min, &rssi_max);
        res.duration_ms = ms;
        res.pulses = pulses;
        res.rssi_est = rssi;
        res.rssi_min = rssi_min;
        res.rssi_max = rssi_max;
        /* edges/sec over actual dwell (integer; host may refine) */
        if (ms > 0) {
            res.pulse_rate_hz = (int32_t)((pulses * 1000L) / (int32_t)ms);
        } else {
            res.pulse_rate_hz = 0;
        }
        if (re == GLK_OK) {
            glk_policy_note_rx(s_pol);
            glk_policy_clear_radio_faults(s_pol);
        } else {
            glk_policy_note_radio_fault(s_pol);
        }
        res.err = re;
    }
    s_state = GLK_RADIO_IDLE;
    if (msg->cb) msg->cb(&res, msg->user);
}

#if !GLK_RADIO_INLINE
static void worker_main(void* arg) {
    (void)arg;
    radio_msg_t msg;
    while (!s_stop) {
        if (glk_queue_recv(s_q, &msg, 100) != GLK_OK) continue;
        if (!msg.valid) continue;
        process_job(&msg);
    }
}
#endif

glk_err_t glk_radio_init(glk_policy_state_t* policy) {
    s_pol = policy;
    s_state = GLK_RADIO_IDLE;
    s_stop = 0;
    s_arb_held = 0;
    s_duty_window_start_ms = 0;
    s_duty_used_ms = 0;
    s_last_settle_ms = 0;
    s_last_bands = 0;
    s_breaker_trips = 0;
    if (glk_queue_create(&s_q, sizeof(radio_msg_t), 8) != GLK_OK) return GLK_ERR_NOMEM;
    if (glk_mutex_create(&s_sync_mu, "radio_sync") != GLK_OK) return GLK_ERR_NOMEM;
    if (glk_mutex_create(&s_arb_mu, "radio_arb") != GLK_OK) return GLK_ERR_NOMEM;
    return GLK_OK;
}

bool glk_radio_arbiter_acquire(uint32_t timeout_ms) {
    if (!s_arb_mu) return false;
    glk_err_t e = glk_mutex_lock(s_arb_mu, timeout_ms ? timeout_ms : 1000);
    if (e == GLK_OK) {
        s_arb_held = 1;
        return true;
    }
    return false;
}

void glk_radio_arbiter_release(void) {
    if (!s_arb_mu || !s_arb_held) return;
    s_arb_held = 0;
    (void)glk_mutex_unlock(s_arb_mu);
}

bool glk_radio_arbiter_held(void) {
    return s_arb_held != 0;
}

glk_err_t glk_radio_start_worker(void) {
#if GLK_RADIO_INLINE
    (void)s_worker;
    return GLK_OK;
#else
    return glk_task_create(&s_worker, "radio", worker_main, NULL, GLK_PRIO_RADIO, 1024);
#endif
}

void glk_radio_stop(void) {
    s_stop = 1;
}

glk_err_t glk_radio_submit(const glk_radio_job_t* job, glk_radio_cb cb, void* user) {
    if (!job) return GLK_ERR_INVAL;
    if (breaker_tripped()) {
        s_breaker_trips++;
        if (cb) {
            glk_radio_result_t res;
            memset(&res, 0, sizeof(res));
            res.err = GLK_ERR_BREAKER;
            res.freq_hz = job->freq_hz;
            cb(&res, user);
        }
        return GLK_ERR_BREAKER;
    }
    radio_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.job = *job;
    msg.cb = cb;
    msg.user = user;
    msg.valid = 1;
#if GLK_RADIO_INLINE
    process_job(&msg);
    return GLK_OK;
#else
    if (!s_q) return GLK_ERR_INVAL;
    return glk_queue_send(s_q, &msg, 500);
#endif
}

glk_err_t glk_radio_rx_sync(
    glk_actor_t actor,
    uint32_t freq_hz,
    uint32_t duration_ms,
    glk_radio_result_t* out) {
    glk_radio_job_t job;
    memset(&job, 0, sizeof(job));
    job.actor = actor;
    job.freq_hz = freq_hz;
    job.duration_ms = duration_ms;
    job.is_tx = false;
    s_sync_done = 0;
    glk_err_t e = glk_radio_submit(&job, sync_cb, NULL);
    if (e != GLK_OK) return e;
#if GLK_RADIO_INLINE
    /* already done in submit */
#else
    for (int i = 0; i < 200 && !s_sync_done; i++) glk_task_sleep_ms(10);
    if (!s_sync_done) return GLK_ERR_TIMEOUT;
#endif
    if (out) *out = s_sync_res;
    return s_sync_res.err;
}

glk_err_t glk_radio_tx_sync(
    glk_actor_t actor,
    uint32_t freq_hz,
    const char* path,
    const char* confirm_id,
    glk_radio_result_t* out) {
    glk_radio_job_t job;
    memset(&job, 0, sizeof(job));
    job.actor = actor;
    job.freq_hz = freq_hz;
    job.duration_ms = 100;
    job.is_tx = true;
    if (path) strncpy(job.tx_path, path, sizeof(job.tx_path) - 1);
    if (confirm_id) strncpy(job.confirm_id, confirm_id, sizeof(job.confirm_id) - 1);
    s_sync_done = 0;
    glk_err_t e = glk_radio_submit(&job, sync_cb, NULL);
    if (e != GLK_OK) return e;
#if !GLK_RADIO_INLINE
    for (int i = 0; i < 200 && !s_sync_done; i++) glk_task_sleep_ms(10);
    if (!s_sync_done) return GLK_ERR_TIMEOUT;
#endif
    if (out) *out = s_sync_res;
    return s_sync_res.err;
}

glk_err_t glk_radio_spectrum(
    glk_actor_t actor,
    const uint32_t* freqs_hz,
    size_t n_freqs,
    uint32_t dwell_ms,
    uint32_t settle_ms,
    glk_radio_result_t* out_results,
    size_t* out_n) {
    if (!freqs_hz || !out_results || !out_n) return GLK_ERR_INVAL;
    *out_n = 0;
    if (breaker_tripped()) {
        s_breaker_trips++;
        return GLK_ERR_BREAKER;
    }
    if (n_freqs > GLK_SPECTRUM_MAX_BANDS) n_freqs = GLK_SPECTRUM_MAX_BANDS;
    /* Enforce MINIMUM settle - never shrink below policy (v3.7 bug capped to 200). */
    uint32_t settle = settle_ms ? settle_ms : GLK_SPECTRUM_SETTLE_MS;
    if (settle < GLK_SPECTRUM_SETTLE_MS) settle = GLK_SPECTRUM_SETTLE_MS;
    s_last_settle_ms = settle;

    uint32_t dwell = dwell_ms ? dwell_ms : GLK_RX_DURATION_DEFAULT_MS;
    if (dwell > GLK_RX_DURATION_MAX_MS) dwell = GLK_RX_DURATION_MAX_MS;

    if (!glk_radio_arbiter_acquire(2000)) return GLK_ERR_BUSY;

    size_t done = 0;
    glk_err_t final_err = GLK_OK;
    for (size_t i = 0; i < n_freqs; i++) {
        if (breaker_tripped()) {
            s_breaker_trips++;
            final_err = GLK_ERR_BREAKER;
            break;
        }
        if (!duty_allow(dwell)) {
            final_err = GLK_ERR_BUSY; /* duty budget exhausted */
            break;
        }
        glk_radio_result_t r;
        glk_err_t e = glk_radio_rx_sync(actor, freqs_hz[i], dwell, &r);
        out_results[done++] = r;
        duty_account(dwell);
        if (e != GLK_OK && e != GLK_ERR_BUSY) {
            if (s_pol) glk_policy_note_radio_fault(s_pol);
            final_err = e;
            break;
        }
        if (i + 1 < n_freqs) {
            glk_task_sleep_ms(settle); /* full settle gap - not capped */
        }
    }
    s_last_bands = (uint32_t)done;
    *out_n = done;
    glk_radio_arbiter_release();
    return final_err == GLK_OK || done > 0 ? GLK_OK : final_err;
}

void glk_radio_planner_status(glk_radio_planner_status_t* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    duty_reset_if_needed();
    out->duty_window_ms = GLK_SPECTRUM_DUTY_WINDOW_MS;
    out->duty_used_ms = s_duty_used_ms;
    out->last_settle_ms = s_last_settle_ms;
    out->last_bands = s_last_bands;
    out->breaker_trips = s_breaker_trips;
    out->busy = (s_state != GLK_RADIO_IDLE) || s_arb_held;
}

size_t glk_radio_planner_status_json(char* buf, size_t buflen) {
    glk_radio_planner_status_t st;
    glk_radio_planner_status(&st);
    if (!buf || buflen < 8) return 0;
    return (size_t)snprintf(
        buf,
        buflen,
        "{\"duty_window_ms\":%u,\"duty_used_ms\":%u,\"last_settle_ms\":%u,"
        "\"last_bands\":%u,\"breaker_trips\":%u,\"busy\":%s,\"min_settle_ms\":%u}",
        (unsigned)st.duty_window_ms,
        (unsigned)st.duty_used_ms,
        (unsigned)st.last_settle_ms,
        (unsigned)st.last_bands,
        (unsigned)st.breaker_trips,
        st.busy ? "true" : "false",
        (unsigned)GLK_SPECTRUM_SETTLE_MS);
}

glk_radio_state_t glk_radio_state(void) {
    return s_state;
}
