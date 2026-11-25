/*
 * ncurses.h - minimal ncurses shim for nInvaders on bare-metal SAM E54.
 *
 * Implements only the subset of the ncurses API that nInvaders uses: a
 * window/pad model backed by a static arena, composited to an ANSI/VT100
 * terminal over the USART.  See ninvaders_port/ncurses_shim.c.
 *
 * This is NOT ncurses.  It is just enough to run the game over a serial
 * console.  Behaviour differs from real ncurses where nInvaders does not
 * depend on the difference (e.g. copywin only supports overwrite mode,
 * pad wrefresh is a no-op, redrawwin is a no-op).
 */
#ifndef NCURSES_SHIM_H
#define NCURSES_SHIM_H

#include <stddef.h>
#include <stdio.h>   /* real ncurses.h pulls this in; nInvaders relies on it
                        for sprintf/snprintf without an explicit include */

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Return / boolean values                                            */
/* ------------------------------------------------------------------ */
#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define ERR   (-1)
#define OK    (0)

/* ------------------------------------------------------------------ */
/* Colours.  Values match ANSI SGR foreground offsets (30 + COLOR_x). */
/* ------------------------------------------------------------------ */
#define COLOR_BLACK    0
#define COLOR_RED      1
#define COLOR_GREEN    2
#define COLOR_YELLOW   3
#define COLOR_BLUE     4
#define COLOR_MAGENTA  5
#define COLOR_CYAN     6
#define COLOR_WHITE    7

/* A character attribute is just a colour-pair index in this shim.
 * COLOR_PAIR(n) packs the pair number; wattrset() stores it per window. */
#define COLOR_PAIR(n)  ((int)(n))

/* ------------------------------------------------------------------ */
/* Keys returned by getch() when keypad() is enabled.                 */
/* Use the standard ncurses values ( > 0xff so they never collide     */
/* with ordinary bytes ).                                             */
/* ------------------------------------------------------------------ */
#define KEY_DOWN   0402
#define KEY_UP     0403
#define KEY_LEFT   0404
#define KEY_RIGHT  0405

/* ------------------------------------------------------------------ */
/* Window / pad object.                                               */
/* ------------------------------------------------------------------ */
typedef struct _win_st WINDOW;

/* stdscr exists so keypad(stdscr, TRUE) compiles; it is not rendered. */
extern WINDOW *stdscr;

/* Lifecycle */
WINDOW *initscr(void);
int     endwin(void);
int     start_color(void);
int     init_pair(short pair, short f, short b);
int     keypad(WINDOW *win, int bf);
int     nonl(void);
int     cbreak(void);
int     noecho(void);

/* Window / pad creation */
WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x);
WINDOW *newpad(int nlines, int ncols);

/* Drawing into a window/pad */
int     wclear(WINDOW *win);
int     wattrset(WINDOW *win, int attrs);
int     waddch(WINDOW *win, int ch);
int     waddstr(WINDOW *win, const char *str);

/* Compositing: copy a rectangle of src into dst.
 * Only overwrite mode (overlay == 0) is implemented, which is all
 * nInvaders ever requests. */
int     copywin(WINDOW *srcwin, WINDOW *dstwin,
                int sminrow, int smincol,
                int dminrow, int dmincol, int dmaxrow, int dmaxcol,
                int overlay);

/* Output / refresh */
int     wrefresh(WINDOW *win);
int     redrawwin(WINDOW *win);
int     mvwin(WINDOW *win, int y, int x);

/* Input (non-blocking in this shim; returns ERR when no key waiting) */
int     getch(void);

#ifdef __cplusplus
}
#endif

#endif /* NCURSES_SHIM_H */
