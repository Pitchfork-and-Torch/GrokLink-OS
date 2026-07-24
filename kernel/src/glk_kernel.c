/**
 * GrokLink RTOS — host-friendly cooperative/preemptive hybrid.
 *
 * On HOST (GLK_PLATFORM_HOST): each task runs as a native thread; scheduling
 * uses OS primitives for realistic concurrency tests.
 * On TARGET: the same API is backed by PendSV/SysTick (see platform/stm32wb55).
 *
 * This file implements the HOST path fully; target assembly hooks call the
 * same data structures via glk_port_*.
 */
#include "glk/glk_kernel.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(GLK_PLATFORM_HOST) || !defined(GLK_PLATFORM_STM32)
#define GLK_HOST 1
#else
#define GLK_HOST 0
#endif

#if GLK_HOST
#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#endif
#endif

/* -------------------------------------------------------------------------- */
/* Internal structures                                                        */
/* -------------------------------------------------------------------------- */

struct glk_task {
    char name[GLK_TASK_NAME_MAX];
    glk_task_fn fn;
    void* arg;
    glk_prio_t prio;
    uint8_t alive;
    uint8_t index;
#if GLK_HOST
#ifdef _WIN32
    HANDLE thread;
    HANDLE wake;
#else
    pthread_t thread;
    pthread_cond_t wake;
    pthread_mutex_t wake_mu;
#endif
#endif
};

struct glk_mutex {
    char name[GLK_TASK_NAME_MAX];
#if GLK_HOST
#ifdef _WIN32
    CRITICAL_SECTION cs;
#else
    pthread_mutex_t mu;
#endif
#endif
    uint8_t in_use;
};

struct glk_queue {
    uint8_t* storage;
    size_t item_size;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
#if GLK_HOST
#ifdef _WIN32
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE not_empty;
    CONDITION_VARIABLE not_full;
#else
    pthread_mutex_t mu;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
#endif
#endif
    uint8_t in_use;
};

struct glk_event {
    uint32_t flags;
#if GLK_HOST
#ifdef _WIN32
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv;
#else
    pthread_mutex_t mu;
    pthread_cond_t cv;
#endif
#endif
    uint8_t in_use;
};

struct glk_timer {
    glk_timer_fn fn;
    void* arg;
    uint32_t period_ms;
    uint32_t due_tick;
    bool periodic;
    bool active;
    uint8_t in_use;
};

struct glk_pool {
    uint8_t* mem;
    size_t block_size;
    size_t block_count;
    uint8_t* free_list; /* singly linked via first sizeof(void*) bytes */
    size_t free_count;
    uint8_t in_use;
};

/* -------------------------------------------------------------------------- */
/* Globals                                                                    */
/* -------------------------------------------------------------------------- */

static glk_task_t s_tasks[GLK_MAX_TASKS];
static glk_mutex_t s_mutexes[GLK_MAX_MUTEXES];
static glk_queue_t s_queues[GLK_MAX_QUEUES];
static glk_event_t s_events[GLK_MAX_EVENTS];
static glk_timer_t s_timers[GLK_MAX_TIMERS];
static glk_pool_t s_pools[GLK_MAX_POOLS];

static volatile glk_tick_t s_tick;
static volatile int s_running;
static glk_kernel_stats_t s_stats;
static glk_task_t* s_current;

/* heap */
static uint8_t* s_heap;
static size_t s_heap_size;
static size_t s_heap_used;
static size_t s_heap_hwm;
static uint8_t s_heap_builtin[GLK_HEAP_SIZE];

#if GLK_HOST
#ifdef _WIN32
static CRITICAL_SECTION s_kernel_cs;
static HANDLE s_tick_thread;
static DWORD s_tls_task;
#else
static pthread_mutex_t s_kernel_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_t s_tick_thread;
static pthread_key_t s_tls_task;
#endif
#endif

/* -------------------------------------------------------------------------- */
/* Critical                                                                   */
/* -------------------------------------------------------------------------- */

void glk_enter_critical(void) {
#if GLK_HOST
#ifdef _WIN32
    EnterCriticalSection(&s_kernel_cs);
#else
    pthread_mutex_lock(&s_kernel_mu);
#endif
#endif
}

void glk_exit_critical(void) {
#if GLK_HOST
#ifdef _WIN32
    LeaveCriticalSection(&s_kernel_cs);
#else
    pthread_mutex_unlock(&s_kernel_mu);
#endif
#endif
}

/* -------------------------------------------------------------------------- */
/* Tick                                                                       */
/* -------------------------------------------------------------------------- */

glk_tick_t glk_tick_get(void) {
    return s_tick;
}

void glk_tick_isr(void) {
    s_tick++;
    s_stats.ticks = s_tick;
    for (int i = 0; i < GLK_MAX_TIMERS; i++) {
        glk_timer_t* t = &s_timers[i];
        if (!t->in_use || !t->active) continue;
        if ((int32_t)(s_tick - t->due_tick) >= 0) {
            if (t->fn) t->fn(t->arg);
            if (t->periodic) {
                t->due_tick = s_tick + t->period_ms;
            } else {
                t->active = false;
            }
        }
    }
}

#if GLK_HOST
#ifdef _WIN32
static unsigned __stdcall tick_thread_main(void* arg) {
    (void)arg;
    while (s_running) {
        Sleep(1);
        glk_tick_isr();
    }
    return 0;
}
#else
static void* tick_thread_main(void* arg) {
    (void)arg;
    while (s_running) {
        struct timespec ts = {0, 1000000L};
        nanosleep(&ts, NULL);
        glk_tick_isr();
    }
    return NULL;
}
#endif
#endif

/* -------------------------------------------------------------------------- */
/* Init / start                                                               */
/* -------------------------------------------------------------------------- */

void glk_kernel_init(void) {
    memset(s_tasks, 0, sizeof(s_tasks));
    memset(s_mutexes, 0, sizeof(s_mutexes));
    memset(s_queues, 0, sizeof(s_queues));
    memset(s_events, 0, sizeof(s_events));
    memset(s_timers, 0, sizeof(s_timers));
    memset(s_pools, 0, sizeof(s_pools));
    memset(&s_stats, 0, sizeof(s_stats));
    s_tick = 0;
    s_running = 0;
    s_current = NULL;
    glk_heap_init(s_heap_builtin, sizeof(s_heap_builtin));
#if GLK_HOST
#ifdef _WIN32
    InitializeCriticalSection(&s_kernel_cs);
    s_tls_task = TlsAlloc();
#else
    pthread_key_create(&s_tls_task, NULL);
#endif
#endif
}

bool glk_kernel_running(void) {
    return s_running != 0;
}

void glk_kernel_start(void) {
    s_running = 1;
#if GLK_HOST
#ifdef _WIN32
    s_tick_thread = (HANDLE)_beginthreadex(NULL, 0, tick_thread_main, NULL, 0, NULL);
#else
    pthread_create(&s_tick_thread, NULL, tick_thread_main, NULL);
#endif
    /* Host: tasks already running as threads from create; park caller. */
    while (s_running) {
#if GLK_HOST && defined(_WIN32)
        Sleep(50);
#else
        struct timespec ts = {0, 50 * 1000000L};
        nanosleep(&ts, NULL);
#endif
    }
#endif
}

void glk_kernel_stop(void) {
    s_running = 0;
#if GLK_HOST
#ifdef _WIN32
    if (s_tick_thread) {
        WaitForSingleObject(s_tick_thread, 2000);
        CloseHandle(s_tick_thread);
        s_tick_thread = NULL;
    }
#else
    pthread_join(s_tick_thread, NULL);
#endif
#endif
}

void glk_kernel_stats(glk_kernel_stats_t* out) {
    if (!out) return;
    *out = s_stats;
}

/* -------------------------------------------------------------------------- */
/* Tasks                                                                      */
/* -------------------------------------------------------------------------- */

#if GLK_HOST
#ifdef _WIN32
static unsigned __stdcall task_trampoline(void* arg) {
    glk_task_t* t = (glk_task_t*)arg;
    TlsSetValue(s_tls_task, t);
    s_current = t;
    if (t->fn) t->fn(t->arg);
    t->alive = 0;
    return 0;
}
#else
static void* task_trampoline(void* arg) {
    glk_task_t* t = (glk_task_t*)arg;
    pthread_setspecific(s_tls_task, t);
    s_current = t;
    if (t->fn) t->fn(t->arg);
    t->alive = 0;
    return NULL;
}
#endif
#endif

glk_err_t glk_task_create(
    glk_task_t** out,
    const char* name,
    glk_task_fn fn,
    void* arg,
    glk_prio_t prio,
    uint32_t stack_words) {
    (void)stack_words;
    if (!out || !fn) return GLK_ERR_INVAL;
    glk_enter_critical();
    glk_task_t* slot = NULL;
    for (int i = 0; i < GLK_MAX_TASKS; i++) {
        if (!s_tasks[i].alive && s_tasks[i].fn == NULL) {
            slot = &s_tasks[i];
            slot->index = (uint8_t)i;
            break;
        }
        if (!s_tasks[i].alive && s_tasks[i].fn != NULL) {
            /* reusable dead slot */
            slot = &s_tasks[i];
            slot->index = (uint8_t)i;
            break;
        }
    }
    if (!slot) {
        /* find free */
        for (int i = 0; i < GLK_MAX_TASKS; i++) {
            if (!s_tasks[i].alive) {
                slot = &s_tasks[i];
                slot->index = (uint8_t)i;
                break;
            }
        }
    }
    if (!slot) {
        glk_exit_critical();
        return GLK_ERR_FULL;
    }
    memset(slot, 0, sizeof(*slot));
    slot->index = slot->index; /* keep */
    for (int i = 0; i < GLK_MAX_TASKS; i++) {
        if (&s_tasks[i] == slot) {
            slot->index = (uint8_t)i;
            break;
        }
    }
    if (name) {
        strncpy(slot->name, name, GLK_TASK_NAME_MAX - 1);
    } else {
        snprintf(slot->name, sizeof(slot->name), "t%u", slot->index);
    }
    slot->fn = fn;
    slot->arg = arg;
    slot->prio = prio;
    slot->alive = 1;
    s_stats.tasks_alive++;
    if (s_stats.tasks_alive > s_stats.peak_tasks) s_stats.peak_tasks = s_stats.tasks_alive;
    glk_exit_critical();

#if GLK_HOST
#ifdef _WIN32
    slot->wake = CreateEventA(NULL, FALSE, FALSE, NULL);
    slot->thread = (HANDLE)_beginthreadex(NULL, 0, task_trampoline, slot, 0, NULL);
    if (!slot->thread) {
        slot->alive = 0;
        return GLK_ERR_GENERIC;
    }
    /* Priority hint */
    int wp = THREAD_PRIORITY_NORMAL;
    if (prio >= 16) wp = THREAD_PRIORITY_ABOVE_NORMAL;
    if (prio >= 20) wp = THREAD_PRIORITY_HIGHEST;
    if (prio <= 1) wp = THREAD_PRIORITY_BELOW_NORMAL;
    SetThreadPriority(slot->thread, wp);
#else
    pthread_mutex_init(&slot->wake_mu, NULL);
    pthread_cond_init(&slot->wake, NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&slot->thread, &attr, task_trampoline, slot);
    pthread_attr_destroy(&attr);
#endif
#endif
    *out = slot;
    return GLK_OK;
}

void glk_task_yield(void) {
#if GLK_HOST
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
#endif
    s_stats.task_switches++;
}

void glk_task_sleep_ms(uint32_t ms) {
#if GLK_HOST
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000);
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
#else
    /* Device: approximate 1 ms steps + advance software tick (no SysTick yet). */
    for (uint32_t i = 0; i < ms; i++) {
        for (volatile uint32_t j = 0; j < 1600u; j++) {
        }
        glk_tick_isr();
    }
#endif
}

glk_task_t* glk_task_self(void) {
#if GLK_HOST
#ifdef _WIN32
    return (glk_task_t*)TlsGetValue(s_tls_task);
#else
    return (glk_task_t*)pthread_getspecific(s_tls_task);
#endif
#else
    return s_current;
#endif
}

const char* glk_task_name(const glk_task_t* t) {
    return t ? t->name : "?";
}

/* -------------------------------------------------------------------------- */
/* Mutex                                                                      */
/* -------------------------------------------------------------------------- */

glk_err_t glk_mutex_create(glk_mutex_t** out, const char* name) {
    if (!out) return GLK_ERR_INVAL;
    for (int i = 0; i < GLK_MAX_MUTEXES; i++) {
        if (!s_mutexes[i].in_use) {
            glk_mutex_t* m = &s_mutexes[i];
            memset(m, 0, sizeof(*m));
            m->in_use = 1;
            if (name) strncpy(m->name, name, sizeof(m->name) - 1);
#if GLK_HOST
#ifdef _WIN32
            InitializeCriticalSection(&m->cs);
#else
            pthread_mutex_init(&m->mu, NULL);
#endif
#endif
            *out = m;
            return GLK_OK;
        }
    }
    return GLK_ERR_FULL;
}

glk_err_t glk_mutex_lock(glk_mutex_t* m, uint32_t timeout_ms) {
    if (!m) return GLK_ERR_INVAL;
#if GLK_HOST
#ifdef _WIN32
    (void)timeout_ms;
    EnterCriticalSection(&m->cs);
    return GLK_OK;
#else
    if (timeout_ms == UINT32_MAX || timeout_ms == 0xFFFFFFFFu) {
        pthread_mutex_lock(&m->mu);
        return GLK_OK;
    }
    /* simple: ignore timeout precision */
    pthread_mutex_lock(&m->mu);
    return GLK_OK;
#endif
#else
    (void)timeout_ms;
    return GLK_OK;
#endif
}

glk_err_t glk_mutex_unlock(glk_mutex_t* m) {
    if (!m) return GLK_ERR_INVAL;
#if GLK_HOST
#ifdef _WIN32
    LeaveCriticalSection(&m->cs);
#else
    pthread_mutex_unlock(&m->mu);
#endif
#endif
    return GLK_OK;
}

/* -------------------------------------------------------------------------- */
/* Queue                                                                      */
/* -------------------------------------------------------------------------- */

glk_err_t glk_queue_create(glk_queue_t** out, size_t item_size, size_t capacity) {
    if (!out || item_size == 0 || capacity == 0) return GLK_ERR_INVAL;
    for (int i = 0; i < GLK_MAX_QUEUES; i++) {
        if (!s_queues[i].in_use) {
            glk_queue_t* q = &s_queues[i];
            memset(q, 0, sizeof(*q));
            q->storage = (uint8_t*)calloc(capacity, item_size);
            if (!q->storage) return GLK_ERR_NOMEM;
            q->item_size = item_size;
            q->capacity = capacity;
            q->in_use = 1;
#if GLK_HOST
#ifdef _WIN32
            InitializeCriticalSection(&q->cs);
            InitializeConditionVariable(&q->not_empty);
            InitializeConditionVariable(&q->not_full);
#else
            pthread_mutex_init(&q->mu, NULL);
            pthread_cond_init(&q->not_empty, NULL);
            pthread_cond_init(&q->not_full, NULL);
#endif
#endif
            *out = q;
            return GLK_OK;
        }
    }
    return GLK_ERR_FULL;
}

glk_err_t glk_queue_send(glk_queue_t* q, const void* item, uint32_t timeout_ms) {
    if (!q || !item) return GLK_ERR_INVAL;
#if GLK_HOST
#ifdef _WIN32
    EnterCriticalSection(&q->cs);
    glk_tick_t start = glk_tick_get();
    while (q->count >= q->capacity) {
        if (timeout_ms == 0) {
            LeaveCriticalSection(&q->cs);
            return GLK_ERR_FULL;
        }
        DWORD wait = (timeout_ms == 0xFFFFFFFFu) ? INFINITE : timeout_ms;
        if (!SleepConditionVariableCS(&q->not_full, &q->cs, wait)) {
            LeaveCriticalSection(&q->cs);
            return GLK_ERR_TIMEOUT;
        }
        if (timeout_ms != 0xFFFFFFFFu && (glk_tick_get() - start) >= timeout_ms) {
            LeaveCriticalSection(&q->cs);
            return GLK_ERR_TIMEOUT;
        }
    }
    memcpy(q->storage + q->tail * q->item_size, item, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    WakeConditionVariable(&q->not_empty);
    LeaveCriticalSection(&q->cs);
    return GLK_OK;
#else
    (void)timeout_ms;
    pthread_mutex_lock(&q->mu);
    while (q->count >= q->capacity) {
        pthread_cond_wait(&q->not_full, &q->mu);
    }
    memcpy(q->storage + q->tail * q->item_size, item, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mu);
    return GLK_OK;
#endif
#else
    (void)timeout_ms;
    if (q->count >= q->capacity) return GLK_ERR_FULL;
    memcpy(q->storage + q->tail * q->item_size, item, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    return GLK_OK;
#endif
}

glk_err_t glk_queue_recv(glk_queue_t* q, void* item, uint32_t timeout_ms) {
    if (!q || !item) return GLK_ERR_INVAL;
#if GLK_HOST
#ifdef _WIN32
    EnterCriticalSection(&q->cs);
    while (q->count == 0) {
        if (timeout_ms == 0) {
            LeaveCriticalSection(&q->cs);
            return GLK_ERR_EMPTY;
        }
        DWORD wait = (timeout_ms == 0xFFFFFFFFu) ? INFINITE : timeout_ms;
        if (!SleepConditionVariableCS(&q->not_empty, &q->cs, wait)) {
            LeaveCriticalSection(&q->cs);
            return GLK_ERR_TIMEOUT;
        }
    }
    memcpy(item, q->storage + q->head * q->item_size, q->item_size);
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    WakeConditionVariable(&q->not_full);
    LeaveCriticalSection(&q->cs);
    return GLK_OK;
#else
    (void)timeout_ms;
    pthread_mutex_lock(&q->mu);
    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->mu);
    }
    memcpy(item, q->storage + q->head * q->item_size, q->item_size);
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mu);
    return GLK_OK;
#endif
#else
    (void)timeout_ms;
    if (q->count == 0) return GLK_ERR_EMPTY;
    memcpy(item, q->storage + q->head * q->item_size, q->item_size);
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    return GLK_OK;
#endif
}

size_t glk_queue_count(const glk_queue_t* q) {
    return q ? q->count : 0;
}

/* -------------------------------------------------------------------------- */
/* Events                                                                     */
/* -------------------------------------------------------------------------- */

glk_err_t glk_event_create(glk_event_t** out) {
    if (!out) return GLK_ERR_INVAL;
    for (int i = 0; i < GLK_MAX_EVENTS; i++) {
        if (!s_events[i].in_use) {
            glk_event_t* e = &s_events[i];
            memset(e, 0, sizeof(*e));
            e->in_use = 1;
#if GLK_HOST
#ifdef _WIN32
            InitializeCriticalSection(&e->cs);
            InitializeConditionVariable(&e->cv);
#else
            pthread_mutex_init(&e->mu, NULL);
            pthread_cond_init(&e->cv, NULL);
#endif
#endif
            *out = e;
            return GLK_OK;
        }
    }
    return GLK_ERR_FULL;
}

glk_err_t glk_event_set(glk_event_t* e, uint32_t flags) {
    if (!e) return GLK_ERR_INVAL;
#if GLK_HOST
#ifdef _WIN32
    EnterCriticalSection(&e->cs);
    e->flags |= flags;
    WakeAllConditionVariable(&e->cv);
    LeaveCriticalSection(&e->cs);
#else
    pthread_mutex_lock(&e->mu);
    e->flags |= flags;
    pthread_cond_broadcast(&e->cv);
    pthread_mutex_unlock(&e->mu);
#endif
#else
    e->flags |= flags;
#endif
    return GLK_OK;
}

glk_err_t glk_event_clear(glk_event_t* e, uint32_t flags) {
    if (!e) return GLK_ERR_INVAL;
#if GLK_HOST
#ifdef _WIN32
    EnterCriticalSection(&e->cs);
    e->flags &= ~flags;
    LeaveCriticalSection(&e->cs);
#else
    pthread_mutex_lock(&e->mu);
    e->flags &= ~flags;
    pthread_mutex_unlock(&e->mu);
#endif
#else
    e->flags &= ~flags;
#endif
    return GLK_OK;
}

glk_err_t glk_event_wait(
    glk_event_t* e,
    uint32_t wait_flags,
    bool wait_all,
    uint32_t* out_flags,
    uint32_t timeout_ms) {
    if (!e) return GLK_ERR_INVAL;
#if GLK_HOST
#ifdef _WIN32
    EnterCriticalSection(&e->cs);
    for (;;) {
        uint32_t f = e->flags & wait_flags;
        bool ok = wait_all ? (f == wait_flags) : (f != 0);
        if (ok) {
            if (out_flags) *out_flags = e->flags;
            LeaveCriticalSection(&e->cs);
            return GLK_OK;
        }
        DWORD wait = (timeout_ms == 0xFFFFFFFFu) ? INFINITE : timeout_ms;
        if (timeout_ms == 0) {
            LeaveCriticalSection(&e->cs);
            return GLK_ERR_TIMEOUT;
        }
        if (!SleepConditionVariableCS(&e->cv, &e->cs, wait)) {
            LeaveCriticalSection(&e->cs);
            return GLK_ERR_TIMEOUT;
        }
    }
#else
    (void)timeout_ms;
    pthread_mutex_lock(&e->mu);
    for (;;) {
        uint32_t f = e->flags & wait_flags;
        bool ok = wait_all ? (f == wait_flags) : (f != 0);
        if (ok) {
            if (out_flags) *out_flags = e->flags;
            pthread_mutex_unlock(&e->mu);
            return GLK_OK;
        }
        pthread_cond_wait(&e->cv, &e->mu);
    }
#endif
#else
    (void)timeout_ms;
    uint32_t f = e->flags & wait_flags;
    bool ok = wait_all ? (f == wait_flags) : (f != 0);
    if (!ok) return GLK_ERR_TIMEOUT;
    if (out_flags) *out_flags = e->flags;
    return GLK_OK;
#endif
}

/* -------------------------------------------------------------------------- */
/* Timers                                                                     */
/* -------------------------------------------------------------------------- */

glk_err_t glk_timer_create(glk_timer_t** out, glk_timer_fn fn, void* arg) {
    if (!out || !fn) return GLK_ERR_INVAL;
    for (int i = 0; i < GLK_MAX_TIMERS; i++) {
        if (!s_timers[i].in_use) {
            glk_timer_t* t = &s_timers[i];
            memset(t, 0, sizeof(*t));
            t->fn = fn;
            t->arg = arg;
            t->in_use = 1;
            *out = t;
            return GLK_OK;
        }
    }
    return GLK_ERR_FULL;
}

glk_err_t glk_timer_start(glk_timer_t* t, uint32_t period_ms, bool periodic) {
    if (!t || period_ms == 0) return GLK_ERR_INVAL;
    t->period_ms = period_ms;
    t->periodic = periodic;
    t->due_tick = glk_tick_get() + period_ms;
    t->active = true;
    return GLK_OK;
}

glk_err_t glk_timer_stop(glk_timer_t* t) {
    if (!t) return GLK_ERR_INVAL;
    t->active = false;
    return GLK_OK;
}

/* -------------------------------------------------------------------------- */
/* Pool                                                                       */
/* -------------------------------------------------------------------------- */

glk_err_t glk_pool_create(glk_pool_t** out, size_t block_size, size_t block_count) {
    if (!out || block_size < sizeof(void*) || block_count == 0) return GLK_ERR_INVAL;
    for (int i = 0; i < GLK_MAX_POOLS; i++) {
        if (!s_pools[i].in_use) {
            glk_pool_t* p = &s_pools[i];
            memset(p, 0, sizeof(*p));
            p->mem = (uint8_t*)calloc(block_count, block_size);
            if (!p->mem) return GLK_ERR_NOMEM;
            p->block_size = block_size;
            p->block_count = block_count;
            p->free_count = block_count;
            p->free_list = NULL;
            for (size_t b = 0; b < block_count; b++) {
                uint8_t* blk = p->mem + b * block_size;
                *(void**)blk = p->free_list;
                p->free_list = blk;
            }
            p->in_use = 1;
            *out = p;
            return GLK_OK;
        }
    }
    return GLK_ERR_FULL;
}

void* glk_pool_alloc(glk_pool_t* p) {
    if (!p || !p->free_list) return NULL;
    glk_enter_critical();
    void* blk = p->free_list;
    p->free_list = *(void**)blk;
    p->free_count--;
    glk_exit_critical();
    return blk;
}

void glk_pool_free(glk_pool_t* p, void* block) {
    if (!p || !block) return;
    glk_enter_critical();
    *(void**)block = p->free_list;
    p->free_list = (uint8_t*)block;
    p->free_count++;
    glk_exit_critical();
}

/* -------------------------------------------------------------------------- */
/* Heap (simple bump + free list for host tests)                              */
/* -------------------------------------------------------------------------- */

typedef struct heap_hdr {
    size_t size;
    uint8_t free;
    struct heap_hdr* next;
} heap_hdr_t;

static heap_hdr_t* s_heap_head;

void glk_heap_init(void* mem, size_t size) {
    s_heap = (uint8_t*)mem;
    s_heap_size = size;
    s_heap_used = 0;
    s_heap_hwm = 0;
    s_heap_head = (heap_hdr_t*)s_heap;
    s_heap_head->size = size - sizeof(heap_hdr_t);
    s_heap_head->free = 1;
    s_heap_head->next = NULL;
}

void* glk_malloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 7u) & ~7u;
    glk_enter_critical();
    heap_hdr_t* h = s_heap_head;
    while (h) {
        if (h->free && h->size >= size) {
            if (h->size >= size + sizeof(heap_hdr_t) + 8) {
                heap_hdr_t* split = (heap_hdr_t*)((uint8_t*)h + sizeof(heap_hdr_t) + size);
                split->size = h->size - size - sizeof(heap_hdr_t);
                split->free = 1;
                split->next = h->next;
                h->next = split;
                h->size = size;
            }
            h->free = 0;
            s_heap_used += h->size;
            if (s_heap_used > s_heap_hwm) s_heap_hwm = s_heap_used;
            glk_exit_critical();
            return (uint8_t*)h + sizeof(heap_hdr_t);
        }
        h = h->next;
    }
    glk_exit_critical();
    return NULL;
}

void glk_free(void* ptr) {
    if (!ptr) return;
    glk_enter_critical();
    heap_hdr_t* h = (heap_hdr_t*)((uint8_t*)ptr - sizeof(heap_hdr_t));
    h->free = 1;
    if (s_heap_used >= h->size) s_heap_used -= h->size;
    /* coalesce forward */
    if (h->next && h->next->free) {
        h->size += sizeof(heap_hdr_t) + h->next->size;
        h->next = h->next->next;
    }
    glk_exit_critical();
}

size_t glk_heap_free_bytes(void) {
    return s_heap_size > s_heap_used ? s_heap_size - s_heap_used : 0;
}

size_t glk_heap_used_bytes(void) {
    return s_heap_used;
}

size_t glk_heap_high_watermark(void) {
    return s_heap_hwm;
}
