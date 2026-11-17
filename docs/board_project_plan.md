# MCU Project Allocation And Fallbacks

Updated 2026-11-07; STM32 primary allocation reaffirmed. The
[inventory](board_inventory.md) owns photo evidence;
[projects](../plans/project_portfolio.md) define P1/P2;
[roadmap](../plans/five_month_development_roadmap.md) owns time.

## Primary Allocation

| Project | Board | Why | Limits |
| --- | --- | --- | --- |
| P1 FreeRTOS cyclic peripheral service/fault recorder | F446RE | Onboard debug, Cortex-M4, timer/ADC/DMA/serial interfaces | Actual power/clock/driver/run status unverified |
| P2 Zephyr diagnostic supervisor/test node | F767ZI | More memory, Cortex-M7/MPU/cache, onboard debug; independent tasks/diagnostics | Cache/memory placement; Ethernet erratum and PA7 conflict if used |
| Optional protocol/stimulus fixture | Pico or ESP32 | Another MCU for repeatable input/response scenarios | Port/driver support, header connections and safe voltage gates |
| Optional later multicore experiment | Original Pico/RP2040 | Two Cortex-M0+ cores for bounded producer/consumer ownership and synchronization | One-core baseline first; pinned FreeRTOS port, core/fault evidence; replaces another Week 17 optional extension |
| Optional low-rate peripherals | UNO/ESP8266/TFT/LCD | Bounded stimulus or status only | No core project depends on them |
| Historical reference only | BBB | Preserved for later Linux interests | No active Linux/IIO/Yocto work |

Use the actual boards after readiness checks; no QEMU/emulator prerequisite.
Begin with a minimal board/debug path, then onboard status/UART and synthetic
protocol/fault inputs before adding external peripherals. Host logic tests are
optional support, not a substitute for hardware evidence.
Public register-device design or synthetic IC only; no employer-specific
motor/BMS protocol is inferred or copied. No real high-energy actuation.

### Pico Decision And Readiness (2026-11-07)

Keep P1 on F446RE and P2 on F767ZI. Pico can support the general P1 design and
FreeRTOS, but changing targets requires a peripheral/startup/port adaptation;
it does not improve interview readiness merely by adding a second core.
Retain it as a bounded blink setup now and a later optional experiment after
the main project gates, not a parallel implementation or additional weekly hours.

Learner reports `RPI-RP2` detection, project creation and UF2 availability.
Official Pico extension 0.23.0 was tutor-inspected; source/build configuration,
UF2 contents, upload and physical behavior remain unverified.

The Pico's `SWCLK`, `SWDIO` and `GND` expose hardware debugging. A compatible
SWD probe, verified pinout/voltage/power arrangement and host-tool support are
still required. USB UF2 loading does not require that probe. Probe selection,
wiring and purchases remain gated; see [setup](workspace_setup.md).

## Failure Fallback Matrix

| Failure | Continue with | What is not proved |
| --- | --- | --- |
| F446 unusable | Diagnose one bounded block; port P1 to F767 if validated; P2 can time-share or use a reduced Pico port | Original MCU timings/memory map, simultaneous independent operation |
| F767 unusable | Run P2 on F446; keep P1/P2 as separate images and test sequentially | M7 cache/Ethernet-specific claims |
| Both STM32s blocked | Agree a supported Pico/ESP32 hardware path; optional host logic tests while diagnosing, no automatic emulator fallback | STM32-specific behavior and any untested physical interfaces |
| SPI/I2C target support blocks | Keep protocol model tests; verify another MCU target implementation or agree UART fallback | Physical original bus path; UART is not an SPI acceptance pass |
| CAN accessories missing | UART or software peer model; standalone cores continue | Arbitration, bus-off and physical CAN measurements |
| Display controller/voltage unknown | UART/LED output | Display bandwidth/contention evidence |
| F767 Ethernet unstable | UART diagnostics; inspect errata/pin conflict before blaming code | Physical Ethernet correctness |
| Saleae unavailable | Software counters and logs | Electrical signals or externally measured latency |
| Windows tool/debug path blocked | Diagnose the specific installed-tool/probe issue; use the Ubuntu HP only after agreeing and verifying its tools/probe access | Automatic host parity or USB forwarding; no run inferred from Ubuntu installation |
| Public sensor IC unavailable | Explicit synthetic register/input model | IC electrical/accuracy/peripheral-specific behavior |

Collect the precise fault and preserve recovery. After one two-hour diagnostic
block, continue an independent slice rather than consuming the whole schedule.
Record reduced scope and tests still pending; a fallback never silently closes
a physical gate. Any replacement purchase/port needs agreement.

## Useful Measurements

For each selected target record build/configuration, clock assumptions, sample/
service rate, buffer/stack budget, test duration, peak/percentile observed
latency and error counts. A sampled maximum is not a mathematical worst-case
bound. Do not suppress a fault by disabling all protection or adding retries
without a limit.

The former BBB/Linux allocation and larger fallback matrix are preserved in the
[pre-pivot snapshot](../backup/plan/2026-10-26-linux-rtos-track/snapshot/docs/board_project_plan.md).
