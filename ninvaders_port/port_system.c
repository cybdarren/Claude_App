/*
 * port_system.c - board services for the nInvaders port on the SAM E54.
 *
 *   - SysTick 1 ms monotonic timebase (survives long, blocking renders,
 *     unlike polling the 24-bit SysTick COUNTFLAG).
 *   - Watchdog pet on a ~20 s cadence.  The programmer in use erases the
 *     config fuses on every program, so the device boots with the WDT
 *     enabled in WINDOW mode (erased User Page 0xFFFFFFFF).  We cannot
 *     fix the fuses, so we clear the WDT inside its open window.  See
 *     WDT_PET_INTERVAL_MS in port.h for why ~20 s is safe and why we must
 *     never clear continuously.
 *   - A microsecond busy-wait used by usleep().
 *   - RNG seeding.
 */
#include <stdint.h>
#include "definitions.h"   /* device registers + CMSIS core (SysTick) */
#include "port.h"

/* Must match the MCC Clock Configuration (GCLK0 / CPU clock). */
#define CPU_HZ   120000000UL

/* srandom() is provided by the port's compat.c. */
extern void srandom(unsigned int seed);

/* ------------------------------------------------------------------ */
/* 1 ms timebase                                                      */
/* ------------------------------------------------------------------ */
static volatile uint32_t g_millis = 0u;
static uint32_t          g_last_wdt_pet = 0u;

/* Strong override of the weak SysTick_Handler alias in interrupts.c. */
void SysTick_Handler(void);
void SysTick_Handler(void)
{
    g_millis++;
}

uint32_t port_millis(void)
{
    return g_millis;
}

/* ------------------------------------------------------------------ */
/* Watchdog                                                           */
/* ------------------------------------------------------------------ */
static void wdt_clear(void)
{
    /* Wait for any previous clear to synchronise, clear, done.
     * (Clear-sync is a few WDT clocks; this only runs ~once per 20 s.) */
    while ((WDT_REGS->WDT_SYNCBUSY & WDT_SYNCBUSY_CLEAR_Msk) != 0u) { }
    WDT_REGS->WDT_CLEAR = (uint8_t)WDT_CLEAR_CLEAR_KEY;
}

void port_wdt_service(void)
{
    /* Rate-limit so we only ever clear inside the WDT open window.
     * First pet fires ~WDT_PET_INTERVAL_MS after boot. */
    if ((uint32_t)(g_millis - g_last_wdt_pet) >= WDT_PET_INTERVAL_MS)
    {
        wdt_clear();
        g_last_wdt_pet = g_millis;
    }
}

/* ------------------------------------------------------------------ */
/* Busy-wait microsecond delay                                        */
/* ------------------------------------------------------------------ */
void port_delay_us(uint32_t us)
{
    /* SysTick is already running (1 ms reload) and counts down at CPU_HZ.
     * Derive a cycle budget and spin, watching for wraps via COUNTFLAG. */
    uint32_t cycles = us * (CPU_HZ / 1000000u);
    uint32_t last   = SysTick->VAL;
    uint32_t reload = SysTick->LOAD + 1u;

    while (cycles != 0u)
    {
        uint32_t now  = SysTick->VAL;
        uint32_t diff = (last >= now) ? (last - now) : (last + reload - now);

        if (diff >= cycles)
        {
            break;
        }
        cycles -= diff;
        last    = now;
    }
}

/* ------------------------------------------------------------------ */
/* Init                                                               */
/* ------------------------------------------------------------------ */
void port_init(void)
{
    /* 1 ms SysTick from the CPU clock, interrupt enabled. */
    SysTick->LOAD = (CPU_HZ / 1000u) - 1u;
    SysTick->VAL  = 0u;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                  | SysTick_CTRL_TICKINT_Msk
                  | SysTick_CTRL_ENABLE_Msk;

    g_millis       = 0u;
    g_last_wdt_pet = 0u;

    /* Seed the PRNG with some entropy from the free-running SysTick. */
    srandom((unsigned int)(SysTick->VAL ^ 0xA5A5u));
}
