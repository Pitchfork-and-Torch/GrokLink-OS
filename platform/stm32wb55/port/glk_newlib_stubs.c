/* Minimal newlib stubs for bare-metal GrokLink OS images */
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>

extern char end;
/* Stack grows down from RAM top; keep a hard ceiling so printf never collides. */
#define GLK_NEWLIB_HEAP_MAX (24 * 1024)
static char* heap_end;
static size_t heap_used;

void* _sbrk(int incr) {
    if (!heap_end) {
        heap_end = &end;
        heap_used = 0;
    }
    if (incr < 0) {
        errno = EINVAL;
        return (void*)-1;
    }
    if (heap_used + (size_t)incr > (size_t)GLK_NEWLIB_HEAP_MAX) {
        errno = ENOMEM;
        return (void*)-1;
    }
    char* prev = heap_end;
    heap_end += incr;
    heap_used += (size_t)incr;
    return prev;
}

int _close(int fd) {
    (void)fd;
    return -1;
}
int _fstat(int fd, struct stat* st) {
    (void)fd;
    if (st) st->st_mode = S_IFCHR;
    return 0;
}
int _isatty(int fd) {
    (void)fd;
    return 1;
}
int _lseek(int fd, int ptr, int dir) {
    (void)fd;
    (void)ptr;
    (void)dir;
    return 0;
}
int _read(int fd, char* ptr, int len) {
    (void)fd;
    (void)ptr;
    (void)len;
    return 0;
}
int _write(int fd, char* ptr, int len) {
    (void)fd;
    (void)ptr;
    return len;
}
void _exit(int code) {
    (void)code;
    for (;;) {
    }
}
int _kill(int pid, int sig) {
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}
int _getpid(void) {
    return 1;
}
