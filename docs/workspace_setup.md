# MCU / RTOS Development Environment

Updated 2026-11-07. The [roadmap](../plans/five_month_development_roadmap.md)
now excludes active Linux-kernel and Yocto work. Native Windows is the primary
build/debug host; Ubuntu/Podman/SSH are optional tools, not another course.

Learner selected available hardware on 2026-10-26: F446RE first, then F767ZI.
No QEMU/emulator setup or execution is required. Existing tools and historical
instructions are not uninstall/removal requests.

### Pico USB Detection And Windows Setup (2026-11-07)

Learner reports that the original Pico appears as `RPI-RP2` in Windows after
the BOOTSEL connection procedure. This establishes learner-reported USB
bootloader detection, not a new firmware build, flash or dual-core run.

Tutor read-only queries (`code --version` and
`code --list-extensions --show-versions`) reported VS Code 1.144.0 x64.
The initial queried CLI profile listed CMake Tools but no Raspberry Pi Pico
extension. After the learner reported installation, a second read-only query
confirmed `raspberry-pi.raspberry-pi-pico@0.23.0`. Its package metadata documents
single-folder workspaces and the command **Raspberry Pi Pico: New Example
Pico Project**. Pico SDK/toolchain readiness remains unverified.

Learner subsequently reports project creation after the `blink` example
instructions for board `pico` (original RP2040). The exact project path,
generated configuration and selected SDK/toolchain versions have not been
inspected.

Learner now reports that the **UF2 file is available**. Record this as
artifact-available-reported: its exact path, source/configuration, build console
and SDK/toolchain versions have not been inspected. No flash, physical blink
or multicore run has been reported.

Next Pico checkpoint, when continuing this optional setup: record the project
and UF2 paths, selected board/SDK/toolchain and build result. Confirm permission
to replace existing Pico firmware and a bare USB-only setup before a
learner-operated UF2 upload. Do not restart the build solely to repeat the
previous planned step. No tutor installation, project creation, build or
hardware operation was performed.

**SWD debug connections:** learner identifies `SWCLK`, `GND` and `SWDIO` on the
Pico. These provide clock, common ground and bidirectional debug data for its
Serial Wire Debug interface. The Pico supports hardware breakpoints and
register inspection through this interface, but has no onboard debug probe.
Its micro-USB port provides power, BOOTSEL loading and firmware-dependent USB
communication; it is not itself an SWD probe.

Before any debug wiring, identify a compatible probe and supported host tools,
verify the exact pinout, 3.3 V target signaling, voltage-reference requirements
and target-power arrangement. Do not infer that an available Nucleo ST-LINK
configuration supports this target without checking it. No wiring, soldering
or purchase is authorized by this record.

**Approved scope:** retain F446RE/FreeRTOS P1 and F767ZI/Zephyr P2. Pico is a
bounded setup check now and an optional later multicore experiment in the
existing roadmap budget, not a third full project.

### Register-Level Lesson And Local Manuals (2026-10-28)

Learner approved the [L15 register-level exercise](../notes/lessons/W01_L15_b1_exti_interrupt_handoff_and_debounce.md):
retain 84 MHz but teach power, Flash timing, PLL/dividers, SysTick and
GPIO/EXTI/NVIC directly, without HAL calls in the exercise. CMSIS register names,
intrinsics and device startup/linker support remain. Use a separate proposed
RegistersDebug configuration; preserve the current working HAL project files.
No new source/configuration was created or built by the tutor.

Manuals now available in [docs/stm32](stm32/):
[RM0390 Rev 6](stm32/um-f446re/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf),
[UM1724 Rev 14](stm32/um-f446re/UM1724.pdf), and
[DUI0553B Cortex-M4 guide](stm32/um-cortexm4/DUI0553.pdf) were read through the
local browser PDF viewer. Exact silicon/current ES0298 errata, compilation and
hardware behavior still require verification.

## Primary Windows Build And Debug Host

Use the current HP Windows laptop for P1 and initially P2. VS Code remains useful
for notes/source editing; the installed STM32CubeIDE is the initial STM32
project/build/debug route. Connect the F446RE probe directly to this host after
board readiness checks. No separate WSL, container, emulator or Linux machine
is required. Choose Zephyr's compatible native Windows tools when P2 starts.

### Read-Only Tool Inventory (2026-10-26)

| Component | Observed | Limit |
| --- | --- | --- |
| STM32CubeIDE | Registry version 1.16.1; executable under `C:\ST\STM32CubeIDE_1.16.1\STM32CubeIDE` | Launch/generation/build not tested |
| STM32CubeProgrammer | Registry version 2.18.0; executable present | Probe/programming not tested |
| Bundled compiler | GNU Tools for STM32 12.3.rel1-labelled plugin; `arm-none-eabi-gcc.exe` present | Runtime version output/build not tested |
| CubeMX integration | 6.12.1-labelled plugin present | Configuration editor not launched |
| STM32CubeF4 | V1.28.1 folder in default package repository | Completeness/project compatibility not established |
| PATH | Searched STM32 tools/compiler names not found | IDE-bundled tools can work without global PATH entries |

Do not reinstall, upgrade or edit PATH solely because a command name is absent.
Record actual selected tool/package versions before the first build. Follow
[W01-L08](../notes/lessons/W01_L08_windows_cubeide_project_baseline.md) for
workspace/project separation. The table above preserves the 2026-10-26
installation inspection; the project update below supersedes its pending
generation/package-candidate status.

### Generated F446RE Baseline (2026-10-27)

Learner reports generation at the agreed location,
[p1_f446re_baseline](../../p1_f446re_baseline). Tutor read-only inspection found
the project/configuration and generated source; no IDE/build/board operations
were performed. The configuration records CubeMX 6.12.1, NUCLEO-F446RE/
STM32F446RETx and **STM32Cube FW_F4 V1.28.3**, with that package folder present.
The previously observed V1.28.1 was a candidate, not a requirement to downgrade.

Both Debug and Release select `STM32F446RETX_FLASH.ld`. The first generation
inspection found no build artifacts; the learner subsequently reported a
successful build. The current [Debug output](../../p1_f446re_baseline/Debug)
contains ELF/map/list files, inspected by the tutor without rebuilding.

The ELF is an Arm executable with hard-float metadata, main at 0x080004f4.
Generated C rules specify -O0/-g3 and GNU Tools for STM32 12.3.rel1; the full
original compiler console/warning history was not supplied. ELF size-tool
summary: text 8408, data 12, bss 1644 bytes; this is not runtime memory usage.
The bss summary includes linker heap/stack reservations and alignment.

Learner confirmed the bare F446RE's firmware may be replaced, no external
wiring/power, and a matching USB data cable. Learner subsequently reports the
[W01-L10 debug exercise](../notes/lessons/W01_L10_first_stlink_debug_stop_at_main.md)
done. Treat this as completed-reported, not tutor-observed target readings.
The saved launch selects the Debug ELF, stop-at-main and resume; it records
`connect_under_reset` as its strategy. These are configuration facts, not proof
of a particular reset/programming result, and were not changed by the tutor.

The empty-loop ELF fingerprint remains unchanged at the next inspection.
Learner reports completing
[W01-L11](../notes/lessons/W01_L11_trace_initialization_clock_gpio_uart.md),
including the software clock becoming 84 MHz and uwTick progressing every 1 ms.
This is learner-reported observation/configured cadence, not an external
frequency measurement. Other unreported values, probe identity, standalone boot
and physical timing remain unverified.

Continue with [W01-L12](../notes/lessons/W01_L12_ld2_gpio_output_and_blocking_delay.md)
for the learner's first onboard LED behavior. The proposed four-call source
edit has not been applied or built by the tutor; learner explicitly chose
source review only and will build it themselves. A changed image needs a new
successful build and matching debug/programming input. No tutor probe
connection, flash or target run was performed.

## Optional Ubuntu Server Host

Learner reports Ubuntu Server is now installed on another HP laptop.
The exact installed release, SSH, toolchain and free space remain unverified;
the earlier plan named 26.04.1 with reported 16 GB RAM/220 GB SSD.
Do not confuse that machine with the current Windows HP or treat total disk
capacity as free space. Use it only for a concrete Linux-specific dependency,
headless automation or an agreed fallback; do not maintain two environments
as a prerequisite.

If selected later, use current laptop VS Code -> Remote-SSH -> Ubuntu tools,
with the board accessible to the host running the debug server.
Retain the [SSH-first guide](hp_linux_ssh_setup.md) as optional reference only;
its older Yocto/kernel instructions are historical, not current setup tasks.
Keep projects private; GitHub publication is not required. Any sync must use
approved original content and preserve dirty worktrees. No clone, install,
commit, push or credential change was performed by this plan.

For direct hardware debugging, connect the board to the computer running the
programming/debug server: HP tools need the probe accessible on the HP; local
tools need it accessible locally. Remote-SSH does not automatically forward a
USB probe. Confirm the chosen host before selecting installation/debug commands.

## Existing Podman For Optional Host-C Checks

Retain an already working host-C test environment where useful; it is not the
main STM32 build/debug route. Its accepted existing WSL2 backend does not
require a separate WSL workflow or migration.

Historical 2026-10-23 inspection: Windows31.69GiB physical RAM, C:102.13GiB free,
Podman guest15.47GiBRAM despite2GiB metadata. Learner could free approximately
200GB; cleanup was not observed. These values are not the HP's inventory.
Prior reference C01 tests used GCC13.3.0 in `localhost/xv6-lab:latest`;
that does not prove all MCU/RTOS tools exist in the image.
Container build support does not establish USB/ST-LINK access. Prefer a verified
native debug path on the probe-connected host rather than assuming passthrough.

## Toolchain And Board Gates

| Surface | Required before using |
| --- | --- |
| Host-C tests | Known compiler/language flags, failing test status, bounded commands |
| Bare-metal STM32 | Validate the installed Windows CubeIDE/compiler and selected package, linker/startup, board ID and debug probe |
| FreeRTOS P1 | Official release/tag, exact Cortex-M port, vendor/CMSIS dependencies, build system, heap/config and interrupt priorities |
| Zephyr P2 | Native Windows first: compatible release/SDK, west modules, Python venv, board target/Kconfig/devicetree; no global sudo-pip |
| Hardware debug | ST-LINK/OpenOCD or chosen vendor tool, permissions, safe power and recoverable flash procedure |
| Optional host logic tests | Known compiler/test commands; no emulator needed and no hardware behavior inferred |
| Measurement | Saleae model/input limits, sample rate, clocks, probe points and instrumentation overhead |

Do not install every toolchain or build all platforms at once. Source availability
is not successful build/runtime evidence. Native host tests establish logic,
not MCU timing or DMA behavior. Never execute experimental modules on a host
kernel; there are no kernel-module exercises in this active track.

## Repository Boundaries

Existing `career_dev`, Linux, xv6, Zephyr and `my-linux` repositories remain
where they are. Linux/xv6/my-linux are historical references; do not delete,
move or rebuild them automatically. The existing workspace roots can stay.
The learner created the P1 baseline as the sibling project linked above.
Do not move it or initialize/publish a repository automatically. Choose later
FreeRTOS/vendor source and P2 directories with the learner before creating or
cloning. Keep dependencies identifiable and pin the selected versions.

Use Linux filesystem storage for remote builds; no need for a Yocto-sized image
build simply to learn RTOS drivers. LF line endings are required for shell/
Make/startup files; git attributes alone do not fix newly typed CRLF files.

## Hardware Inventory

Use [photo descriptions](board_inventory.md) and
[active MCU fallbacks](board_project_plan.md).

- F446RE (MB1136revC): P1 acquisition/driver-service candidate.
- F767ZI (MB1137RevB): P2 supervisor/test-node candidate.
- Pico/ESP32: conditional fixtures/spares after tool/voltage support checks.
- UNO/ESP8266/displays: optional, no core dependency.
- BBB: owned historical Linux hardware, not an active course prerequisite.
- Saleae: learner-reported, model/probes still unknown.

Photos identify models, not working boards. Keep silicon revision separate
from PCB revision. Check F767 DMA-accessible memory/cache rules and PA7 default
SPI/Ethernet conflict if those features are selected. Confirm all transceivers,
input ranges, grounding and probe limits before connections.

## Safe Execution Contract

Ask before builds/runs of learner code, installs, flashes, filesystem/system
changes or purchases. No motors, real battery packs, mains or safety-critical
equipment are controlled by these learning projects. Preserve existing firmware
and a recovery path. Bound timeouts/retries and report failure explicitly.

The full previous [Linux/Yocto environment](../backup/plan/2026-10-26-linux-rtos-track/snapshot/docs/workspace_setup.md)
and earlier [foundation setup](../backup/plan/2026-10-22-foundation/snapshot/docs/workspace_setup.md)
remain reference history; their tasks are not current requirements.
