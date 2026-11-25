/*
 * signal.h - minimal signal shim for nInvaders on bare-metal SAM E54.
 *
 * nInvaders installs a SIGALRM handler (the 50 Hz game tick, via
 * sigaction/setitimer) and a SIGINT handler (clean-up on Ctrl-C, via
 * signal()).  There is no OS and no real signal delivery here: these
 * shims merely CAPTURE the handlers so the port's getch() pump can call
 * the SIGALRM handler on a timer, and so an incoming Ctrl-C byte (0x03)
 * can invoke the SIGINT handler.  See ninvaders_port/ncurses_shim.c.
 */
#ifndef PORT_SIGNAL_SHIM_H
#define PORT_SIGNAL_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

/* Signal numbers we care about (values are arbitrary but distinct). */
#define SIGINT    2
#define SIGALRM  14

/* Flags accepted by sigaction (ignored by the shim). */
#define SA_RESTART 0x10000000

/* POSIX-ish signal handler type. */
typedef void (*__sighandler_t)(int);
#define SIG_DFL ((__sighandler_t)0)
#define SIG_IGN ((__sighandler_t)1)
#define SIG_ERR ((__sighandler_t)-1)

/* Minimal sigset_t / sigaction so nInvaders' setUpTimer() compiles. */
typedef unsigned long sigset_t;

struct sigaction {
    __sighandler_t sa_handler;
    sigset_t       sa_mask;
    int            sa_flags;
};

/* Capture-only implementations (see ncurses_shim.c). */
__sighandler_t signal(int signum, __sighandler_t handler);
int            sigaction(int signum, const struct sigaction *act,
                         struct sigaction *oldact);

#ifdef __cplusplus
}
#endif

#endif /* PORT_SIGNAL_SHIM_H */
