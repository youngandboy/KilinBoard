/*
 * Minimal newlib system call stubs for the GCC build.
 * Keil/ARMCC does not use this file.
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "usart.h"

extern char end;       /* first free RAM address, from the linker script */
extern char _estack;   /* top of RAM, from the linker script */

void *_sbrk(ptrdiff_t incr)
{
    static char *heap_end = 0;
    char *prev;
    uint32_t limit = ((uint32_t)&_estack) - 0x800;  /* reserve 2 KB for stack */

    if (heap_end == 0) {
        heap_end = &end;
    }

    prev = heap_end;

    if ((uint32_t)(heap_end + incr) > limit) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end += incr;
    return prev;
}

int _write(int file, char *ptr, int len)
{
    (void)file;

    if (ptr != 0 && len > 0) {
        Usart_SendString(USART1, (unsigned char *)ptr, (unsigned short)len);
    }

    return len;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void)
{
    return 1;
}

void _exit(int status)
{
    (void)status;
    while (1) {
    }
}
