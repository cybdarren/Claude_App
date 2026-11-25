#include <stdint.h>
#include "definitions.h"   /* device registers + SYS_Initialize() */
#include "uart.h"
#include "port.h"          /* ninvaders_port: SysTick, watchdog, RNG seed */

/* nInvaders' entry point.  Its source is unmodified; the build renames
 * its main() to ninvaders_main (-Dmain=ninvaders_main on nInvaders.c)
 * so this framework main() stays the real entry point. */
extern int ninvaders_main(int argc, char **argv);

int main(void)
{
    /* MCC-generated chip init: wait states, clocks to 120 MHz, pins. */
    SYS_Initialize(NULL);

    uart_init();

    /* Port services: 1 ms SysTick timebase, RNG seed, and the watchdog
     * pet schedule.  The programmer erases the config fuses on program,
     * so the WDT boots enabled in window mode; port_wdt_service() (called
     * from the game's input pump) clears it inside the open window. */
    port_init();

    (void)write(STDOUT_FILENO_, "Program Started\r\n", 17u);

    /* Hand off to the game.  It drives everything from here (never
     * returns under normal play; 'q' exits via the C library and the
     * unpetted watchdog then reboots the board back to the title). */
    {
        static char  arg0[] = "ninvaders";
        static char *argv[]  = { arg0, 0 };
        (void)ninvaders_main(1, argv);
    }

    for (;;)
    {
        /* Should not get here; keep the watchdog fed just in case. */
        port_wdt_service();
    }
}
