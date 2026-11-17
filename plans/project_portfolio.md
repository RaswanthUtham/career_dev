# Private MCU Project Contracts

Approved MCU/FreeRTOS-first scope, 2026-10-26. Projects are planned, not built.
[Roadmap](five_month_development_roadmap.md) owns hours/dates;
[tracker](training_tracker.md) owns evidence; [boards](../docs/board_inventory.md)
owns photo provenance.

## Purpose And Truthful Use

Deepen skills through problems seen in professional driver engineering, not
through a broad demonstration of every framework. Initial work is private
learning work using public specifications or an explicitly original device model.
No employer code, register protocol, test vectors or confidential architecture
are copied. No public GitHub showcase, commit or push is required.

The learner may propose a resulting idea at work through the employer's IP,
review and safety processes. Only after actual approval/adoption may it be
described as workplace work, with accurate authorship, implementation scope,
review responsibility and results. This plan is not evidence of employment at
Infineon or of an already-delivered company project.

## P1. FreeRTOS Cyclic Peripheral Service And Fault Recorder

### Real Problem

A peripheral driver must keep cyclic service bounded while transfers complete
asynchronously, an IC is busy or reset, and diagnostics need reliable evidence.
Unbounded retries, stale buffers or callbacks after shutdown can break the
whole device service. This matches the kind of interface/driver/testing work
the learner already performs without recreating a proprietary product.

**Primary board:** F446RE. **Initial OS:** pinned FreeRTOS Cortex-M port.

```text
timer / request -> bounded cyclic service -> device protocol state machine
                       |                              |
                       |                       SPI/I2C transport
                       |                              |
                       |                     ISR/DMA completion
                       |                              |
                       +<----- notification / validated completion
                       |
               snapshot + fault ring -> low-priority UART export
```

Use the available boards; no QEMU/emulator is required. First exercise a
synthetic register device in board firmware, with optional host-C unit tests.
Its interface must
state reset values, ready/fault flags, transfer sizes, sequence/checksum policy
and side effects. Label it synthetic. Later select ONE safe public-datasheet
peripheral or validated peer MCU; approval and hardware checks precede wiring.
Not a real motor controller, BMS protection unit or high-energy test installation.

### Core Requirements

- Separation of device logic, transport and RTOS scheduling with a small explicit
  interface, not a universal HAL framework. Do not copy AUTOSAR implementations.
- States such as UNINITIALIZED/READY/BUSY/RECOVERING/FAILED have defined events,
  entry/exit conditions and legal public calls.
- Each cyclic invocation has a declared work budget; slow transactions complete
  later. State the period/deadline only after deriving it from requirements.
- Initialization and configuration read-back follow the chosen IC/model's
  register semantics. W1C/read-to-clear/reserved fields are not generic RW bits.
- Bounded queue/pool, one owner per transfer buffer, explicit callback lifetime,
  and generation/session handling for late completion after reset.
- Use correct FreeRTOS ISR-safe APIs and priority limits; a normal mutex is not
  an ISR lock. Notification is not the data's lifetime/ownership proof.
- Timeouts, cancellation, failed retries and degraded behavior are explicit,
  observable and bounded. Zero progress is not success.
- Timer/ADC/DMA acquisition adds a known-input path only after protocol tests.
  Overrun accounting and finite capture storage, not guaranteed no-loss claims.
- Fault records contain source/version, event ordering, error state and validity.
  Export must not block high-priority service indefinitely.

### Initial Design Records

1. Polling versus async transfer; cyclic budget versus completion deadline.
2. Task notification versus queue, and where payload ownership resides.
3. Fixed blocks versus allocation; storage/copy cost versus latency.
4. Timeout/retry/degraded policy; evidence needed to justify recovery.
5. MCU sampling timestamp versus export time and reset epoch.

### Acceptance

| Area | Required evidence, not yet achieved |
| --- | --- |
| Functional | Known commands/data, initialization, valid read-back and declared protocol outputs |
| Lifetime | Completion/cancel/reset orderings; no reuse while owned; all buffers accounted |
| Time | Declared workload, cycle/deadline budget and measured response distribution |
| Faults | Busy device, corrupt/truncated frame, timeout, stale data, late callback and full queue |
| Memory | Static/pool/stack budget, overflow policy and failure behavior |
| Release | Pinned sources/config/toolchain, reproducible command, useful failing exit status and known limits |

Use bench LEDs/status records as the modeled safe/degraded output. Do not
control a vehicle, real battery pack, mains-powered actuator or motor power
stage. Model sensor faults first; hardware claims require safe measured inputs.

## P2. Zephyr Diagnostic Supervisor And Programmable Test Node

### Real Problem

Driver teams need repeatable faults and timing scenarios instead of waiting for
an intermittent failure to occur on a bench. Build a standalone MCU that runs
bounded sensing/status tasks, accepts a small diagnostic command set and
reproduces documented data/fault scenarios.

**Primary board:** F767ZI. **OS:** one pinned Zephyr release/SDK.
Run independently before any link to P1.

```text
scripted scenario / local input -> event/state model -> bounded response queue
                  |                         |
          periodic acquisition        diagnostic transport
                  |                         |
             local status <------ useful-progress supervision
```

### Core Requirements

- Start with UART commands, local button/LEDs and synthetic data. Network,
  display and CAN are not prerequisites.
- Define task periods, deadlines, priority ordering and maximum blocking.
  Demonstrate controlled priority inversion and compare inheritance with
  single-owner/message-passing design.
- Simulate stale/invalid measurements, slow consumers and a stalled worker
  using explicit reproducible scenarios. No random unsafe register pokes.
- Supervise useful progress, not merely scheduled execution. Watchdog recovery
  needs preserved fault context and an approved reset/recovery procedure.
- Use Zephyr Ztest for scoped unit/integration scenarios; Twister where supported.
  Separate optional host logic tests, on-board synthetic fault scenarios and
  physical device/timing measurements; no emulator is required.
- Pin Kconfig/devicetree and driver dependencies; the board DTS's existence is
  not proof all peripherals or target-mode APIs work.
- M7 cache/MPU and DMA-accessible memory placement are taught only on a selected
  actual DMA path. Do not disable cache globally as a substitute for analysis.

### Optional Extension Gate

After both standalone cores, select ONE extension within the existing budget:
CAN peer diagnostics, a verified SPI/I2C target model, a low-priority display,
Ethernet telemetry, or the bounded Pico experiment below. Check framework
support first. Physical CAN requires
transceivers/termination; F767 SPI/Ethernet PA7 conflict and silicon errata are
real gates. No unsupported peripheral is silently treated as working.

Pico/ESP32 may act as a separate fixture after validation. An ESP-IDF/FreeRTOS
fallback is not evidence of Zephyr compatibility. Do not maintain three full
SDKs solely to demonstrate a protocol.

### Acceptance

| Area | Required evidence, not yet achieved |
| --- | --- |
| Standalone | Starts and produces local status/diagnostics without Linux or P1 |
| Scheduling | Priority/blocking decisions, inversion experiment and overload behavior |
| Fault supervision | Stale source, stuck progress and recovery traces with clear expected outcomes |
| Test reproducibility | Scenario IDs, known inputs, bounded waits and machine-readable outcomes |
| Memory/time | Stack/queue budgets and measured timing under declared load |
| Optional peer | Separate physical transport evidence, not just software loopback |

## Optional Pico Multicore Experiment (Approved 2026-11-07)

**Role:** a later learning extension, not a replacement for F446RE P1 or
F767ZI P2. Use the existing Week 17 optional slot only after the standalone
project cores and single-core FreeRTOS foundations are sound; otherwise defer.

- Keep the present Pico effort to a bounded blink/toolchain check. UF2
  availability is learner-reported; target execution remains unverified.
- Pin a compatible Pico SDK, FreeRTOS kernel/RP2040 port and toolchain before
  RTOS implementation. The
  [RP2040 port](https://github.com/FreeRTOS/FreeRTOS-Kernel/tree/main/portable/ThirdParty/GCC/RP2040)
  documents one-/two-core operation; inspected development-branch support is
  not a tested project configuration.
- Start with one-core producer/consumer behavior, then extend the same bounded
  queue/buffer contract across two cores. No full P1 driver rewrite.
- Specify ownership, queue capacity, timeout and overflow behavior. Disabling
  interrupts on one core does not exclude accesses from the other core; use
  synchronization supported by the selected port.
- Acceptance: record actual core IDs, sequence/count checks and buffer ownership;
  test normal handoff plus a full queue or stalled consumer; report memory
  budget and measured timing under stated conditions.
- Keep it separate from physical peer-interface acceptance. Two cores exchanging
  RAM data do not establish correct SPI/I2C wiring or external-device behavior.
- Hardware debugging requires a compatible SWD probe and verified connections,
  even though the Pico exposes `SWCLK`, `SWDIO` and `GND`. USB UF2 loading and
  SWD debugging are different paths; see [setup](../docs/workspace_setup.md).

## Safety-Related Method, Not Certification

Use the learner's reported FMEA/HAZOP participation, MISRA tooling, integration
tests and requirements traceability as a starting strength. For each project:

`requirement -> hazard/failure assumption -> design decision -> test -> result -> limit`

Record modeled safe/degraded states, interfaces, fault injection, review findings,
test coverage limitations and static-analysis results. A tool pass is not MISRA
compliance by itself; deviations and applicability matter. No safety integrity
level, diagnostic-coverage percentage, independent assessment or certification
is inferred from the projects. Ordinary FreeRTOS or Zephyr use does not make a
complete product safety-certified.

No paid safety tool is required initially. Use available compiler warnings,
tests and appropriate free tools; Polyspace familiarity is reported work
experience, not a license or installed tool for this course.

## Evidence And Private Work-Improvement Proposal

During implementation maintain concise design decisions and fault/timing
evidence within each project. Those files are created when real work starts,
not filled with invented results during planning.

Record author/runner, source/config, test input, expected/observed output,
instrument settings, resource budget and limitations. Aggregate test failures;
bound retries/waits. Software timestamps cannot certify electrical timing.

An optional employer-facing proposal can explain a generic idea, expected
benefit, integration boundary and trial/measurement plan. Keep personal projects
and employer deliverables separate until actual authorization and adoption.
Never present reviewed architecture as architecture independently authored.

## Research References

Checked 2026-10-26; pin versions before implementation:

- [FreeRTOS task states](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/01-Tasks-and-co-routines/02-Task-states)
  and [task notifications](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/03-Direct-to-task-notifications/01-Task-notifications):
  topic/API references; some fetched pages exposed only titles, so detailed
  behavior must be checked in the selected official kernel/source documentation.
- [Zephyr Ztest](https://docs.zephyrproject.org/latest/develop/test/ztest.html):
  unit/integration test framework, not proof of our application's coverage.
- [Task watchdog](https://docs.zephyrproject.org/latest/services/task_wdt/index.html):
  per-task supervision mechanisms; useful-progress policy belongs to the app.
- [Zephyr safety overview](https://docs.zephyrproject.org/latest/safety/safety_overview.html):
  safety scope and process are separate from simply using an RTOS.
- [Bengaluru role evidence](career_role_evidence.md): TI drivers/FreeRTOS,
  Qualcomm management-controller diagnostics and Infineon MCU/AURIX engineering
  inform these problem choices, not a claim of a replicated employer product.
