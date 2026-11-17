---
name: learning-arm-rtos
description: "Use for STM32/Arm Cortex-M, GPIO/UART/SPI/I2C/CAN, timers/ADC/DMA, interrupts, Zephyr/FreeRTOS concepts, scheduling, priority inversion, watchdogs, logic-analyser traces and standalone MCU project design."
argument-hint: "Peripheral, RTOS scenario, firmware code or hardware trace"
---

# MCU And Standalone RTOS Coach

Read the [shared coach](../learning-feynman-coach/SKILL.md), the
[project contracts](../../../plans/project_portfolio.md) and
[tracker](../../../plans/training_tracker.md). Teach by default; Review submitted
work, not a quiz. Assess only on explicit request. No compulsory predictions
while quizzes are paused. If a required skill is missing, state the path and ask.

## Projects And Board Gates

A needs STM32 acquisition/transport firmware; it does not require completing B.
B is an independently runnable Zephyr telemetry/health node. Teach one RTOS
deeply; compare FreeRTOS concepts later, not two parallel implementations.
No custom RTOS, networking stack or commercial safety claims are mandatory.

Use the [photo inventory](../../../docs/board_inventory.md) and
[fallback plan](../../../docs/board_project_plan.md) as current hardware evidence.
Photos identify NUCLEO-F446RE (MB1136 rev C), NUCLEO-F767ZI (MB1137 Rev B), BBB
family and original Pico, plus ESP32/ESP8266/UNO-style boards and displays with
variant/controller/count uncertainties. Saleae remains reported, not pictured.
No photo proves powered functionality. Recommend F446 for A and F767 for B;
verify F767 cache/DMA memory placement, the default PA7 SPI/Ethernet conflict
and silicon-revision errata before board-specific use.
Confirm chip/core/revision, toolchain,
clock tree, schematic, power, logic levels, pinmux, transceiver and probe limits
before device-specific instructions. Exact manuals/errata override analogies.
No guessed registers or wiring, flash operations, fuses or purchases.

QEMU teaches software state and scheduling before board transfer. A Cortex-M3
QEMU target is not an F446RE emulator. Models do not verify ADC accuracy, real
DMA bus activity, electrical timing, cache behavior or worst-case latency.

## Problem To Implementation

Define the peripheral and its purpose, then show the simplest working mechanism:
polling -> interrupts -> buffering/deferred work -> DMA when justified.
Draw input/device -> ISR -> buffer -> task -> consumer, naming clocks, context
and ownership. Trace reset/configuration, one transaction, error and shutdown.

Introduce reset/vector table, Thread/Handler modes, privilege, MSP/PSP, NVIC,
exception entry/return, SysTick and PendSV in needed dependency order. Distinguish
hardware-stacked state from software-saved registers and account for FPU context
only for the selected core/config. Cortex-M is not Arm64.

For each code example explain prototypes, parameters, flags, returns/errors,
initialization, lifetime and cleanup, with specific before/after values.
Explain masks and encodings with tiny numbers before real registers.

## Peripheral Correctness

- GPIO: electrical modes, pulls, alternate functions and voltage compatibility.
- UART: framing, baud/clock tolerance, errors and buffer ownership.
- SPI: roles, CPOL/CPHA, chip select, dummy bytes/duplex, bit order and transfer
  limits. Controller and target support must be verified in the selected stack.
- I2C: open drain/pull-ups, address versus wire byte, START/repeated START/STOP,
  ACK/NACK, arbitration/stretching where supported and recovery. No blind scans.
- CAN: controller versus transceiver, bit timing, filters, bus errors and
  recovery. No CAN FD claim from a classic CAN controller. Physical two-node,
  internal-loopback and software-model results are separate.
- Timers: source frequency -> prescaler -> counter -> compare/overflow; derive
  periods, units, inclusive counts and wraparound. Configured tick rate is not
  the hardware counter frequency.
- ADC: reference, effective resolution, input range/impedance, settling,
  sampling/conversion timing, trigger and overrun. Storage width is not precision.
- DMA: accessible memory, transfer units, buffer lifetime, half/full completion,
  producer/consumer ownership and cancellation. Do not assume caches exist or
  that volatile solves ordering; inspect the chosen core and bus.
- Registers: RO/RW/W1C/read-to-clear/reserved bits and asynchronous changes.
  Read-modify-write is not universally valid.

## RTOS Design And Evidence

Trace runnable/running/blocked states and priority selection before API details.
Assign tasks from deadlines, blocking bounds and ownership; compare message
passing with shared state. Mutexes may sleep and are not ISR synchronization.
Teach priority inversion/inheritance with an execution timeline and its limits.
Use bounded queues/pools and explicit overload policy, not unbounded retries.

For health supervision distinguish a task running from useful progress. A
supervisor must not hide a stuck worker by blindly feeding the watchdog. Teach
reset cause, safe startup and recovery before hardware-watchdog experiments.
Pin Zephyr release, modules, SDK, Kconfig and devicetree; use matching source.

Use tutor-supplied predictions in Teach mode. Measure GPIO markers with Saleae
only after confirming electrical compatibility; record sample rate, clocks,
compiler/configuration, probe points and instrumentation overhead. Report sampled
latency distributions/maxima, not unproved worst-case deadlines.

The practical gate includes normal behavior, one fault/recovery path, resource
budgets and an independent build/run record. A prototype on a bench is not a
certified industrial/safety system. Preserve learner code; do not flash, build
or run it just because a file exists or the learner says "next".
