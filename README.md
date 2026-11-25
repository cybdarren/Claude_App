
# same54_blinky — nInvaders over USART

This project runs the **nInvaders** space-invaders clone on the SAM E54, using the
serial console (USART, 115200 8N1, EDBG virtual COM port) as an ANSI/VT100 display.
Connect a terminal emulator (≥ 80×24, VT100/ANSI) and play over serial.

- **Controls:** `h` / `l` or ← / → to move, `space` / `k` to fire, `p` to pause,
  `q` to quit (the board then reboots back to the title screen).
- The framework `main()` (`src/main.c`) initialises the chip, UART and port layer,
  then calls the game's entry point.

### Layout

| Folder | Contents |
| --- | --- |
| `src/` | Framework: entry point (`main.c`) and the SERCOM2 USART driver (`uart.c/.h`). |
| `ninvaders/` | Upstream [nInvaders](https://github.com/cybdarren/ninvaders) sources, unmodified apart from one buffer-size bug fix (`aliens.h`, see the comment there). Its `main()` is renamed to `ninvaders_main` at compile time via `user.cmake`. |
| `ninvaders_port/` | Bare-metal glue: a minimal **ncurses shim** (`ncurses_shim.c` + `include/`) that renders windows/pads to the terminal and reads keys from the UART; a **port layer** (`port_system.c`) providing a SysTick 1 ms timebase, the watchdog pet, and RNG seeding; and **compat** functions (`compat.c`: `random`/`srandom`/`usleep`) this musl build lacks. |
| `cmake/.../user.cmake` | Adds the game + port sources to the build (the supported, non-generated CMake hook). |

The game's 50 Hz tick and keyboard input are both pumped from the shim's `getch()`,
which nInvaders' main loop calls continuously.

### Watchdog note (important on this board)

The programmer in use **erases the config fuses on every program**, so the device boots
with the watchdog enabled in **window mode** (erased User Page `0xFFFFFFFF`: closed window
≈ 16 s, reset ≈ 32 s — see the fuse discussion below). We cannot fix the fuses, so the
firmware **clears the watchdog on a ~20 s cadence** (`WDT_PET_INTERVAL_MS` in
`ninvaders_port/include/port.h`), which lands inside the 16–32 s open window. The pet is
deliberately **not** continuous: clearing during the closed window would itself trip a
reset. If you ever program the fuses correctly and power-cycle (below), the watchdog is off
and the petting simply becomes a harmless no-op.

## Getting Started

- Press **Ctrl + Shift + B** to build the project.
- Open the Command Palette (**Ctrl + Shift + P**) and search for "MPLAB" to discover other available commands.
- Need more help? Visit the [MPLAB support site](https://www.microchip.com/en-us/support/all) or the [MPLAB website](https://www.microchip.com).
- Found a bug or have feedback? Open the Command Palette (**Ctrl + Shift + P**) and run
  **MPLAB: Report Issue** to let us know.

## Watchdog / fuse settings

The WDT is controlled by the NVM User Page fuses, set in
`config.mcc/src/config/default/initialization.c`:

```
#pragma config WDT_ENABLE   = CLEAR   // WDT disabled  (User Word 1, bit 16)
#pragma config WDT_ALWAYSON = CLEAR   // not forced on (User Word 1, bit 17)
```

The build encodes these correctly: `out/same54_blinky/default.hex` contains
User Word 1 (`0x00804004`) = `0x2AA80080` (WDT off).

### If the board resets about every 32 s

That is the watchdog firing. An erased/default User Page reads `0xFFFFFFFF`,
which is WDT enabled + ALWAYSON + window mode (PER = WINDOW = CYC16384 ≈ 32 s).
Two things both have to be true to clear it:

1. **The User Page must actually be programmed.** A plain flash download over
   the on-board EDBG does **not** write it (a SAM E5x chip-erase leaves the
   User Page untouched). Program it once with **MPLAB IPE**: connect the EDBG,
   load `out/same54_blinky/default.hex`, and **Program** — IPE writes the
   Configuration/User Page region. (OpenOCD's `atsame5 userpage` can also write
   it directly.)

2. **Then fully power-cycle the board.** The WDT/BOD fuses are latched into the
   peripheral only at **power-on reset**. A reset button, a reprogram, an IDE
   "device clear", and even the WDT reset itself are all *warm* resets and do
   **not** reload the User Page — so a freshly programmed fuse does nothing
   until power is physically removed and reapplied. Unplug USB for a few
   seconds, replug, and the watchdog stays off across subsequent warm resets.

A software disable in firmware cannot help once the running peripheral has
loaded ALWAYSON from an erased page: ALWAYSON is read-only-locked until a POR
reloads the (now disabled) fuse. So the fix is fuse + power-cycle, not code.

### Make sure you are actually programming the hardware

- **Tool must be `EDBG`, not `Simulator`.** `.vscode/same54_blinky.mplab.json`
  sets `"tool": "EDBG"`. If it is set to `Simulator`, builds/downloads never
  touch the real device and the watchdog on the board is unaffected.
- **Full program, every time.** `default-tool-base` sets
  `programoptions.smart.program = "Never"` (no incremental/"smart" programming)
  and `programoptions.eraseb4program = "true"` (erase-all before program) so the
  whole image — including the User Page region at `0x00804000` — is written on
  each download rather than diffed and skipped.
- If an EDBG download *still* does not clear the 32 s reset, the User Page is not
  being written by this flow. Program it once with **MPLAB IPE** (load
  `out/same54_blinky/default.hex`, Program) or OpenOCD's `atsame5 userpage`, then
  do the power-cycle below. The User-Page bytes are already correct in the hex:
  `0x00804004` = `0x2AA80080` (WDT_ENABLE=0, ALWAYSON=0).

## Project Structure

- 📦 `_build` — The [CMake build tree](https://cmake.org/cmake/help/latest/manual/cmake.1.html#introduction-to-cmake-buildsystems); safe to delete, regenerated on next build.
- 📦 `cmake` — Generated [CMake](https://cmake.org/) files; can be deleted if `user.cmake` has not been added.
- 🚀 `out` — Final build artifacts.
- ⚙️ `.vscode` — VS Code workspace configuration; see [VS Code docs](https://code.visualstudio.com/docs/getstarted/settings).
- ⚙️ `.vscode\settings.json` — Workspace-specific settings.
- 📝 `.vscode\same54_blinky.mplab.json` — The MPLAB project file. ❗ Should not be deleted.
