#include "glk/glk_kernel.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

static volatile int g_done;

static void worker(void* arg) {
    int* p = (int*)arg;
    *p = 42;
    glk_task_sleep_ms(20);
    g_done = 1;
}

int main(void) {
    glk_kernel_init();
    int val = 0;
    glk_task_t* t = NULL;
    assert(glk_task_create(&t, "w", worker, &val, 5, 1024) == GLK_OK);

    glk_mutex_t* m = NULL;
    assert(glk_mutex_create(&m, "m") == GLK_OK);
    assert(glk_mutex_lock(m, 100) == GLK_OK);
    assert(glk_mutex_unlock(m) == GLK_OK);

    glk_queue_t* q = NULL;
    assert(glk_queue_create(&q, sizeof(int), 4) == GLK_OK);
    int x = 7;
    assert(glk_queue_send(q, &x, 100) == GLK_OK);
    int y = 0;
    assert(glk_queue_recv(q, &y, 100) == GLK_OK);
    assert(y == 7);

    void* p = glk_malloc(128);
    assert(p != NULL);
    glk_free(p);

    for (int i = 0; i < 50 && !g_done; i++) glk_task_sleep_ms(10);
    assert(g_done == 1);
    assert(val == 42);

    printf("test_kernel: OK\n");
    return 0;
}
