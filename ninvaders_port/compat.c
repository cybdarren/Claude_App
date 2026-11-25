/*
 * compat.c - libc functions missing from this xc32/musl build, used by
 * nInvaders.
 *
 *   random()/srandom() - aliens.c and ufo.c use random(); this build
 *                        lacks it.  A plain POSIX-style LCG (the classic
 *                        glibc TYPE_0 generator) is more than enough for
 *                        a game.
 *   usleep()           - globals.c's doSleep() calls usleep(); backed by
 *                        the port busy-wait.
 *
 * These are defined in an ordinary object file, so if the C library does
 * happen to provide the same symbols its archive member is simply not
 * pulled in (no multiple-definition conflict).
 */
#include <stdint.h>
#include "port.h"

/* ------------------------------------------------------------------ */
/* random() / srandom()  - POSIX.1-2001 minimal (type 0) LCG           */
/* ------------------------------------------------------------------ */
static uint32_t g_rand_state = 1u;

void srandom(unsigned int seed)
{
    g_rand_state = (uint32_t)seed;
    if (g_rand_state == 0u)
    {
        g_rand_state = 1u;   /* avoid the fixed point at 0 */
    }
}

long random(void)
{
    /* glibc TYPE_0: state = state * 1103515245 + 12345; return bits 30..0 */
    g_rand_state = g_rand_state * 1103515245u + 12345u;
    return (long)((g_rand_state >> 1) & 0x7FFFFFFFu);
}

/* initstate()/setstate() are not used by nInvaders, so omitted. */

/* ------------------------------------------------------------------ */
/* usleep()                                                           */
/* ------------------------------------------------------------------ */
int usleep(unsigned int usec)
{
    port_delay_us((uint32_t)usec);
    return 0;
}
