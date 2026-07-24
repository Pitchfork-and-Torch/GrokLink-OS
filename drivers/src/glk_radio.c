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
static glk_radio_result_t s_sync_res;
static volatile int s_sync_done;

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
    if (glk_queue_create(&s_q, sizeof(radio_msg_t), 8) != GLK_OK) return GLK_ERR_NOMEM;
    if (glk_mutex_create(&s_sync_mu, "radio_sync") != GLK_OK) return GLK_ERR_NOMEM;
    return GLK_OK;
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
    if (n_freqs > GLK_SPECTRUM_MAX_BANDS) n_freqs = GLK_SPECTRUM_MAX_BANDS;
    size_t done = 0;
    uint32_t settle = settle_ms ? settle_ms : GLK_SPECTRUM_SETTLE_MS;
    for (size_t i = 0; i < n_freqs; i++) {
        glk_radio_result_t r;
        glk_err_t e = glk_radio_rx_sync(actor, freqs_hz[i], dwell_ms, &r);
        out_results[done++] = r;
        if (e != GLK_OK && e != GLK_ERR_BUSY) {
            if (s_pol) glk_policy_note_radio_fault(s_pol);
            break;
        }
        if (i + 1 < n_freqs) {
            uint32_t s = settle > 200 ? 200 : settle;
            glk_task_sleep_ms(s);
        }
    }
    *out_n = done;
    return GLK_OK;
}

glk_radio_state_t glk_radio_state(void) {
    return s_state;
}
