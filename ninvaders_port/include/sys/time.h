/*
 * sys/time.h - minimal time shim for nInvaders on bare-metal SAM E54.
 *
 * nInvaders uses setitimer(ITIMER_REAL, ...) to request a 50 Hz tick.
 * There is no OS timer here; the shim just records the requested
 * interval so the port's getch() pump can drive the SIGALRM handler at
 * the right rate.  See ninvaders_port/ncurses_shim.c.
 */
#ifndef PORT_SYS_TIME_SHIM_H
#define PORT_SYS_TIME_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

#define ITIMER_REAL    0
#define ITIMER_VIRTUAL 1
#define ITIMER_PROF    2

struct timeval {
    long tv_sec;    /* seconds      */
    long tv_usec;   /* microseconds */
};

struct itimerval {
    struct timeval it_interval;  /* reload value   */
    struct timeval it_value;     /* current value  */
};

int setitimer(int which, const struct itimerval *new_value,
              struct itimerval *old_value);

#ifdef __cplusplus
}
#endif

#endif /* PORT_SYS_TIME_SHIM_H */
