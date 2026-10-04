/**
 * Async SubGHz radio worker + multi-band planner.
 */
#pragma once

#include "glk/glk_types.h"
#include "glk_svc/glk_policy.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    GLK_RADIO_IDLE = 0,
    GLK_RADIO_RX = 1,
    GLK_RADIO_TX = 2,
    GLK_RADIO_FAULT = 3,
} glk_radio_state_t;

typedef struct {
    uint32_t freq_hz;
    uint32_t duration_ms;
    char confirm_id[24];
    glk_actor_t actor;
    bool is_tx;
    char tx_path[192]; /* optional file for TX */
} glk_radio_job_t;

typedef struct {
    glk_err_t err;
    uint32_t freq_hz;
    uint32_t duration_ms;
    int32_t pulses;
    int32_t rssi_est;
    bool simulated;
    /* v3.5 Signal Cognition - light stats (stack only; no heap) */
    int32_t pulse_rate_hz; /* edges/sec over dwell; 0 if unknown */
    int16_t rssi_min;      /* dBm; equal to rssi_est if single sample */
    int16_t rssi_max;      /* dBm */
} glk_radio_result_t;

typedef void (*glk_radio_cb)(const glk_radio_result_t* res, void* user);

glk_err_t glk_radio_init(glk_policy_state_t* policy);
glk_err_t glk_radio_start_worker(void);
void glk_radio_stop(void);

/** Non-blocking submit; result via callback or glk_radio_wait. */
glk_err_t glk_radio_submit(const glk_radio_job_t* job, glk_radio_cb cb, void* user);

/** Blocking helper with timeout (uses queue). */
glk_err_t glk_radio_rx_sync(
    glk_actor_t actor,
    uint32_t freq_hz,
    uint32_t duration_ms,
    glk_radio_result_t* out);

glk_err_t glk_radio_tx_sync(
    glk_actor_t actor,
    uint32_t freq_hz,
    const char* path,
    const char* confirm_id,
    glk_radio_result_t* out);

/**
 * Multi-band planner: sequential RX with settle; never tight-loops on RPC task.
 * max_bands limited by GLK_SPECTRUM_MAX_BANDS.
 * Enforces minimum settle (GLK_SPECTRUM_SETTLE_MS), per-job duty budget, and
 * global circuit breaker - cannot be bypassed by RPC or agent.
 */
glk_err_t glk_radio_spectrum(
    glk_actor_t actor,
    const uint32_t* freqs_hz,
    size_t n_freqs,
    uint32_t dwell_ms,
    uint32_t settle_ms,
    glk_radio_result_t* out_results,
    size_t* out_n);

/**
 * Resource arbiter: exclusive SubGHz domain.
 * Compatible concurrent domains (GPIO IRQ + IR) not modeled as exclusive.
 */
bool glk_radio_arbiter_acquire(uint32_t timeout_ms);
void glk_radio_arbiter_release(void);
bool glk_radio_arbiter_held(void);

/** Planner / duty telemetry for RPC (duty_ms window, faults, last settle). */
typedef struct {
    uint32_t duty_window_ms;
    uint32_t duty_used_ms;
    uint32_t last_settle_ms;
    uint32_t last_bands;
    uint32_t breaker_trips;
    bool busy;
} glk_radio_planner_status_t;

void glk_radio_planner_status(glk_radio_planner_status_t* out);
size_t glk_radio_planner_status_json(char* buf, size_t buflen);

glk_radio_state_t glk_radio_state(void);

#ifdef __cplusplus
}
#endif
