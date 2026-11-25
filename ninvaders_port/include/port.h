/*
 * port.h - board/port services for the nInvaders port on the SAM E54.
 *
 * Provides a monotonic millisecond timebase (SysTick), the watchdog
 * "pet" service (the programmer erases the config fuses, leaving the WDT
 * enabled in window mode, so firmware must clear it inside the open
 * window), a microsecond busy-wait, and RNG seeding.
 */
#ifndef PORT_H
#define PORT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * How often to pet the watchdog, in milliseconds.
 *
 * With the config fuses erased (User Page = 0xFFFFFFFF) the WDT runs in
 * WINDOW mode off OSCULP32K: the counter must NOT be cleared during the
 * closed window (~16 s) and MUST be cleared before the period expires
 * (~32 s).  ~20 s sits inside that 16-32 s open window with margin on
 * both edges even allowing for OSCULP32K tolerance.  Do NOT pet
 * continuously - clearing during the closed window triggers a reset.
 */
#define WDT_PET_INTERVAL_MS 20000u

/* Initialise SysTick (1 ms tick), seed the RNG, arm the WDT pet
 * baseline.  Call once after SYS_Initialize()/uart_init(). */
void port_init(void);

/* Milliseconds since port_init() (monotonic, wraps after ~49 days). */
uint32_t port_millis(void);

/* Clear the watchdog if WDT_PET_INTERVAL_MS has elapsed since the last
 * pet.  Safe to call very frequently; it rate-limits itself so it only
 * ever clears inside the WDT open window.  Called from the getch() pump. */
void port_wdt_service(void);

/* Busy-wait for the given number of microseconds (backs usleep()). */
void port_delay_us(uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* PORT_H */
