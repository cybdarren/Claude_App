/*
 * ncurses_shim.c - a just-enough ncurses implementation for nInvaders,
 * rendering to an ANSI/VT100 terminal over the SAM E54 USART.
 *
 * Model
 * -----
 *  - A WINDOW is a rectangular grid of (char, colour-pair) cells backed
 *    by a static bump arena (no malloc/heap).  newpad()/newwin() carve
 *    windows out of the arena.
 *  - nInvaders draws sprites into off-screen pads, then copywin()s them
 *    onto the one on-screen window (wBattleField, created by newwin).
 *  - wrefresh(screen) diffs that window against a shadow of what the
 *    terminal currently shows and emits only the changed cells, so each
 *    50 Hz frame is cheap over the serial link.
 *
 * Transient pads
 * --------------
 *  nInvaders allocates per-frame pads in the title screen and explosion
 *  animation and never frees them (it relied on process exit).  To stay
 *  bounded, the arena locks a baseline at the first on-screen refresh and
 *  rewinds to that baseline at the end of every subsequent refresh, so
 *  those per-frame pads are reclaimed.  (At most one generation, from the
 *  very first frame, is retained permanently.)
 *
 * Timing / input
 * --------------
 *  nInvaders' main loop is `do { readInput(); } while (1)`, and readInput
 *  calls getch() each pass.  getch() is therefore the pump: it services
 *  the watchdog, runs the captured SIGALRM handler (handleTimer) at the
 *  requested rate (~50 Hz), and returns a decoded key or ERR (it never
 *  blocks).  setitimer/sigaction/signal only capture the handler/interval.
 */
#include <stddef.h>
#include <stdint.h>
#include "ncurses.h"
#include "signal.h"
#include "sys/time.h"
#include "port.h"

/* ---- UART (from the framework's uart.c), declared locally to avoid a
 *      cross-directory include dependency. ---------------------------- */
#define STDOUT_FILENO_ 1
extern int write(int fd, const void *buf, size_t count);
extern int uart_rx_read(void);   /* -1 if no byte waiting */

/* ================================================================== */
/* Cell / window model                                                */
/* ================================================================== */
typedef struct {
    unsigned char ch;
    unsigned char attr;   /* colour-pair index (0 = default) */
} cell_t;

struct _win_st {
    int     h, w;
    int     cy, cx;       /* cursor for waddch/waddstr */
    int     attr;         /* current colour-pair for subsequent writes */
    int     is_screen;    /* 1 for the on-screen window (newwin) */
    cell_t *cells;
};

#define SCREEN_H 24
#define SCREEN_W 80

/* ---- static arena ------------------------------------------------- */
#define ARENA_BYTES (32u * 1024u)
static uint8_t  g_arena[ARENA_BYTES] __attribute__((aligned(8)));
static size_t   g_top       = 0u;
static size_t   g_perm_top  = 0u;   /* 0 => baseline not yet locked */

static void *arena_alloc(size_t n)
{
    size_t aligned = (n + 7u) & ~((size_t)7u);
    if ((g_top + aligned) > ARENA_BYTES)
    {
        return NULL;          /* out of arena: caller tolerates NULL cells */
    }
    void *p = &g_arena[g_top];
    g_top += aligned;
    return p;
}

static WINDOW *g_screen = NULL;   /* the single on-screen window */
WINDOW        *stdscr   = NULL;

/* ---- colour pairs ------------------------------------------------- */
static unsigned char g_pair_fg[16] = { COLOR_WHITE };

/* ================================================================== */
/* Terminal output (buffered, flushed to the UART)                    */
/* ================================================================== */
static char g_obuf[512];
static int  g_olen = 0;

static void oflush(void)
{
    if (g_olen > 0)
    {
        (void)write(STDOUT_FILENO_, g_obuf, (size_t)g_olen);
        g_olen = 0;
    }
}
static void oputc(char c)
{
    if (g_olen >= (int)sizeof(g_obuf)) { oflush(); }
    g_obuf[g_olen++] = c;
}
static void oputs(const char *s)
{
    while (*s != '\0') { oputc(*s++); }
}
static void oput_uint(unsigned v)
{
    char t[10];
    int  n = 0;
    if (v == 0u) { oputc('0'); return; }
    while (v != 0u) { t[n++] = (char)('0' + (v % 10u)); v /= 10u; }
    while (n != 0)  { oputc(t[--n]); }
}

/* ================================================================== */
/* Window creation / editing                                          */
/* ================================================================== */
static WINDOW *win_create(int h, int w, int is_screen)
{
    WINDOW *win = (WINDOW *)arena_alloc(sizeof(WINDOW));
    if (win == NULL) { return NULL; }
    win->h = h; win->w = w;
    win->cy = 0; win->cx = 0;
    win->attr = 0;
    win->is_screen = is_screen;
    win->cells = (cell_t *)arena_alloc((size_t)h * (size_t)w * sizeof(cell_t));
    if (win->cells != NULL)
    {
        int i, n = h * w;
        for (i = 0; i < n; i++) { win->cells[i].ch = ' '; win->cells[i].attr = 0; }
    }
    return win;
}

WINDOW *newpad(int nlines, int ncols)
{
    return win_create(nlines, ncols, 0);
}

WINDOW *newwin(int nlines, int ncols, int begin_y, int begin_x)
{
    WINDOW *win = win_create(nlines, ncols, 1);
    (void)begin_y; (void)begin_x;   /* only 0,0 full-screen is used */
    if (win != NULL) { g_screen = win; }
    return win;
}

int wclear(WINDOW *win)
{
    if (win != NULL && win->cells != NULL)
    {
        int i, n = win->h * win->w;
        for (i = 0; i < n; i++) { win->cells[i].ch = ' '; win->cells[i].attr = (unsigned char)win->attr; }
    }
    if (win != NULL) { win->cy = 0; win->cx = 0; }
    return OK;
}

int wattrset(WINDOW *win, int attrs)
{
    if (win != NULL) { win->attr = attrs; }
    return OK;
}

int waddch(WINDOW *win, int ch)
{
    if (win == NULL || win->cells == NULL) { return ERR; }
    if (win->cy < win->h && win->cx < win->w)
    {
        cell_t *cp = &win->cells[win->cy * win->w + win->cx];
        cp->ch   = (unsigned char)ch;
        cp->attr = (unsigned char)win->attr;
    }
    /* advance cursor with wrap, clamped to the window */
    win->cx++;
    if (win->cx >= win->w) { win->cx = 0; if (win->cy < win->h) { win->cy++; } }
    return OK;
}

int waddstr(WINDOW *win, const char *str)
{
    if (str == NULL) { return ERR; }
    while (*str != '\0') { (void)waddch(win, (unsigned char)*str++); }
    return OK;
}

int copywin(WINDOW *src, WINDOW *dst,
            int sminrow, int smincol,
            int dminrow, int dmincol, int dmaxrow, int dmaxcol,
            int overlay)
{
    int rows, cols, i, j;
    (void)overlay;                     /* only overwrite mode is used */
    if (src == NULL || dst == NULL || src->cells == NULL || dst->cells == NULL)
    {
        return ERR;
    }
    rows = dmaxrow - dminrow + 1;
    cols = dmaxcol - dmincol + 1;
    for (i = 0; i < rows; i++)
    {
        int sr = sminrow + i;
        int dr = dminrow + i;
        if (sr < 0 || sr >= src->h || dr < 0 || dr >= dst->h) { continue; }
        for (j = 0; j < cols; j++)
        {
            int sc = smincol + j;
            int dc = dmincol + j;
            if (sc < 0 || sc >= src->w || dc < 0 || dc >= dst->w) { continue; }
            dst->cells[dr * dst->w + dc] = src->cells[sr * src->w + sc];
        }
    }
    return OK;
}

int mvwin(WINDOW *win, int y, int x)   { (void)win; (void)y; (void)x; return OK; }
int redrawwin(WINDOW *win)             { (void)win; return OK; }  /* diff handles it */

/* ================================================================== */
/* Rendering                                                          */
/* ================================================================== */
static cell_t g_shadow[SCREEN_H * SCREEN_W];

static void emit_move(int r, int c)
{
    oputs("\x1b[");
    oput_uint((unsigned)(r + 1));
    oputc(';');
    oput_uint((unsigned)(c + 1));
    oputc('H');
}
static void emit_sgr(int attr)
{
    unsigned fg = (attr >= 0 && attr < 16) ? g_pair_fg[attr] : COLOR_WHITE;
    oputs("\x1b[");
    oput_uint(30u + fg);
    oputc('m');
}

static void render_screen(WINDOW *win)
{
    int r, c;
    int cur_r = -1, cur_c = -1;   /* unknown terminal cursor position */
    int cur_attr = -1;            /* unknown terminal SGR state */

    for (r = 0; r < SCREEN_H && r < win->h; r++)
    {
        for (c = 0; c < SCREEN_W && c < win->w; c++)
        {
            cell_t nw = win->cells[r * win->w + c];
            cell_t sh = g_shadow[r * SCREEN_W + c];

            /* A space is visually identical regardless of its colour. */
            int same = (nw.ch == sh.ch) &&
                       (nw.ch == ' ' || nw.attr == sh.attr);
            if (same) { continue; }

            if (cur_r != r || cur_c != c) { emit_move(r, c); cur_r = r; cur_c = c; }

            if (nw.ch != ' ' && (int)nw.attr != cur_attr)
            {
                emit_sgr((int)nw.attr);
                cur_attr = (int)nw.attr;
            }
            oputc((char)nw.ch);

            g_shadow[r * SCREEN_W + c] = nw;
            cur_c++;
            if (cur_c >= SCREEN_W) { cur_r = -1; cur_c = -1; }  /* wrap: resync */
        }
    }
    oflush();
}

int wrefresh(WINDOW *win)
{
    if (win == NULL || !win->is_screen) { return OK; }  /* pad refreshes: no-op */

    render_screen(win);

    if (g_perm_top == 0u)
    {
        g_perm_top = g_top;          /* lock baseline on first screen refresh */
    }
    else
    {
        g_top = g_perm_top;          /* reclaim this frame's transient pads */
    }
    return OK;
}

/* ================================================================== */
/* Lifecycle                                                          */
/* ================================================================== */
WINDOW *initscr(void)
{
    int i;
    stdscr = win_create(1, 1, 0);

    for (i = 0; i < SCREEN_H * SCREEN_W; i++) { g_shadow[i].ch = ' '; g_shadow[i].attr = 0; }

    /* Reset attributes, clear screen, home cursor, hide cursor. */
    oputs("\x1b[0m\x1b[2J\x1b[H\x1b[?25l");
    oflush();
    return stdscr;
}

int endwin(void)
{
    /* Restore: default attrs, show cursor, move below the field. */
    oputs("\x1b[0m\x1b[?25h");
    emit_move(SCREEN_H, 0);
    oputc('\r'); oputc('\n');
    oflush();
    return OK;
}

int start_color(void)            { return OK; }
int keypad(WINDOW *w, int bf)    { (void)w; (void)bf; return OK; }
int nonl(void)                   { return OK; }
int cbreak(void)                 { return OK; }
int noecho(void)                 { return OK; }

int init_pair(short pair, short f, short b)
{
    (void)b;   /* background assumed black / terminal default */
    if (pair >= 0 && pair < 16) { g_pair_fg[pair] = (unsigned char)f; }
    return OK;
}

/* ================================================================== */
/* Timer / signal capture + the getch() pump                          */
/* ================================================================== */
static __sighandler_t g_timer_handler = NULL;   /* SIGALRM => handleTimer */
static __sighandler_t g_sigint_handler = NULL;  /* SIGINT  => finish      */
static uint32_t       g_tick_ms  = 20u;         /* 50 Hz default          */
static uint32_t       g_last_tick = 0u;

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
    if (oldact != NULL) { oldact->sa_handler = g_timer_handler; }
    if (act != NULL && signum == SIGALRM) { g_timer_handler = act->sa_handler; }
    return 0;
}

__sighandler_t signal(int signum, __sighandler_t handler)
{
    __sighandler_t prev = (signum == SIGINT) ? g_sigint_handler : NULL;
    if (signum == SIGINT) { g_sigint_handler = handler; }
    return prev;
}

int setitimer(int which, const struct itimerval *nv, struct itimerval *ov)
{
    (void)which; (void)ov;
    if (nv != NULL)
    {
        uint32_t us = (uint32_t)nv->it_interval.tv_usec
                    + (uint32_t)nv->it_interval.tv_sec * 1000000u;
        g_tick_ms = us / 1000u;
        if (g_tick_ms == 0u) { g_tick_ms = 1u; }
    }
    return 0;
}

/* Poll the UART for one byte, up to timeout_ms, for escape sequences. */
static int rx_wait(uint32_t timeout_ms)
{
    uint32_t start = port_millis();
    for (;;)
    {
        int b = uart_rx_read();
        if (b >= 0) { return b; }
        if ((uint32_t)(port_millis() - start) >= timeout_ms) { return -1; }
    }
}

static int read_key(void)
{
    int b = uart_rx_read();
    if (b < 0) { return ERR; }

    if (b == 0x03)   /* Ctrl-C => SIGINT handler (clean up + exit) */
    {
        if (g_sigint_handler != NULL && g_sigint_handler != SIG_IGN)
        {
            g_sigint_handler(SIGINT);
        }
        return ERR;
    }

    if (b == 0x1b)   /* ESC: try to complete an arrow-key CSI sequence */
    {
        int b2 = rx_wait(3u);
        if (b2 == '[' || b2 == 'O')
        {
            int b3 = rx_wait(3u);
            switch (b3)
            {
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                default:  return ERR;
            }
        }
        return 27;   /* bare ESC */
    }
    return b;
}

int getch(void)
{
    uint32_t now;

    /* 1. Keep the watchdog happy (rate-limited to the open window). */
    port_wdt_service();

    /* 2. Drive the game tick (handleTimer) at the requested rate. */
    now = port_millis();
    if (g_timer_handler != NULL &&
        (uint32_t)(now - g_last_tick) >= g_tick_ms)
    {
        g_last_tick = now;
        g_timer_handler(SIGALRM);   /* runs game logic + render */
    }

    /* 3. Return a key if one is waiting, else ERR (non-blocking). */
    return read_key();
}
