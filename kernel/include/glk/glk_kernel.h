/**
 * GrokLink RTOS — public kernel API.
 * Preemptive multi-priority tasks, mutexes, queues, events, timers, pools.
 */
#pragma once

#include "glk_types.h"
#include "glk_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*glk_task_fn)(void* arg);
typedef void (*glk_timer_fn)(void* arg);

typedef struct glk_task glk_task_t;
typedef struct glk_mutex glk_mutex_t;
typedef struct glk_queue glk_queue_t;
typedef struct glk_event glk_event_t;
typedef struct glk_timer glk_timer_t;
typedef struct glk_pool glk_pool_t;

/* ---- Lifecycle ---- */
void glk_kernel_init(void);
void glk_kernel_start(void); /* does not return on target; host runs until stop */
void glk_kernel_stop(void);  /* host sim */
bool glk_kernel_running(void);
glk_tick_t glk_tick_get(void);
void glk_tick_isr(void); /* call from SysTick / host timer */

/* ---- Tasks ---- */
glk_err_t glk_task_create(
    glk_task_t** out,
    const char* name,
    glk_task_fn fn,
    void* arg,
    glk_prio_t prio,
    uint32_t stack_words);

void glk_task_yield(void);
void glk_task_sleep_ms(uint32_t ms);
glk_task_t* glk_task_self(void);
const char* glk_task_name(const glk_task_t* t);

/* ---- Mutex ---- */
glk_err_t glk_mutex_create(glk_mutex_t** out, const char* name);
glk_err_t glk_mutex_lock(glk_mutex_t* m, uint32_t timeout_ms);
glk_err_t glk_mutex_unlock(glk_mutex_t* m);

/* ---- Queue ---- */
glk_err_t glk_queue_create(glk_queue_t** out, size_t item_size, size_t capacity);
glk_err_t glk_queue_send(glk_queue_t* q, const void* item, uint32_t timeout_ms);
glk_err_t glk_queue_recv(glk_queue_t* q, void* item, uint32_t timeout_ms);
size_t glk_queue_count(const glk_queue_t* q);

/* ---- Event flags ---- */
glk_err_t glk_event_create(glk_event_t** out);
glk_err_t glk_event_set(glk_event_t* e, uint32_t flags);
glk_err_t glk_event_clear(glk_event_t* e, uint32_t flags);
glk_err_t glk_event_wait(
    glk_event_t* e,
    uint32_t wait_flags,
    bool wait_all,
    uint32_t* out_flags,
    uint32_t timeout_ms);

/* ---- Software timers ---- */
glk_err_t glk_timer_create(glk_timer_t** out, glk_timer_fn fn, void* arg);
glk_err_t glk_timer_start(glk_timer_t* t, uint32_t period_ms, bool periodic);
glk_err_t glk_timer_stop(glk_timer_t* t);

/* ---- Memory pool ---- */
glk_err_t glk_pool_create(glk_pool_t** out, size_t block_size, size_t block_count);
void* glk_pool_alloc(glk_pool_t* p);
void glk_pool_free(glk_pool_t* p, void* block);

/* ---- Monitored heap ---- */
void glk_heap_init(void* mem, size_t size);
void* glk_malloc(size_t size);
void glk_free(void* ptr);
size_t glk_heap_free_bytes(void);
size_t glk_heap_used_bytes(void);
size_t glk_heap_high_watermark(void);

/* ---- Critical section ---- */
void glk_enter_critical(void);
void glk_exit_critical(void);

/* ---- Stats ---- */
typedef struct {
    uint32_t task_switches;
    uint32_t ticks;
    uint16_t tasks_alive;
    uint16_t peak_tasks;
} glk_kernel_stats_t;

void glk_kernel_stats(glk_kernel_stats_t* out);

#ifdef __cplusplus
}
#endif
