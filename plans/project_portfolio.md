# Theoretical Capstone Project (`P1`) & Workplace Connection Contract

Updated with learner approval on 2026-11-20 ([previous hardware-track contract](../backup/plan/2026-11-20-mcu-hardware-track/snapshot/plans/project_portfolio.md)).
[10-Day Plan](five_month_development_roadmap.md) owns schedule;
[Tracker](training_tracker.md) owns status.

## Purpose And Truthful Interview Framing

As an experienced working engineer (`10+ years C`, 32-bit Infineon **AURIX** motor-control/BMS IC cyclic driver functions, AUTOSAR CDD integration, architecture/interface reviews, integration tests, FMEA/HAZOP participation, Polyspace/MISRA), you do **not** need hobbyist LED/board demos right now. Instead, `P1` serves as a **complete, Staff-grade theoretical & code-level FreeRTOS reference architecture** that:
1. Lets you see **every RTOS, peripheral, `DMA`, memory-safety, and fault-recovery mechanism** working together in one realistic MCU subsystem (`Day 8 / W04-L33`).
2. Maps **1-to-1 to your real day-to-day AURIX driver & integration engineering**, so in interviews you can explain both **how you solved cyclic IC communication, timeout/retry, diagnostics, and safety analysis at work** AND **how you design the exact same subsystem on Arm Cortex-M + FreeRTOS**.
3. Keeps **100% professional honesty**: no confidential employer register maps or proprietary code are copied here, and `P1` is presented honestly as a reference architecture / design study rather than a fabricated workplace product or unrun hardware measurement. (`P2` Zephyr is deferred outside the active FreeRTOS-focused interview track.)

## P1. FreeRTOS Asynchronous Cyclic IC Driver Service, `DMA` Acquisition & Fault Recorder

### 1-To-1 Bridge Between Your Day-To-Day AURIX Work And `P1` (FreeRTOS / Cortex-M)

| Day-To-Day AURIX / AUTOSAR Engineering Concept | FreeRTOS + Cortex-M (`P1`) Equivalent You Explain In Interviews |
| --- | --- |
| Periodic `1 ms` / `10 ms` AUTOSAR Runnable & Cyclic IC MainFunction | FreeRTOS periodic task using `vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(10U))` (`W03-L22`) |
| Non-blocking IC SPI/I2C register sequence (`ISSUE -> WAIT_COMPLETE -> VALIDATE -> RECOVER`) | Bounded driver state machine (`UNINIT -> INIT_VERIFY -> IDLE -> BUSY_ASYNC -> RECOVERING -> SAFE_DEGRADED`) woken via `xTaskNotifyWait` / `DMA` completion ISR (`W04-L25`, `Day 8`) |
| `SchM_Enter` / `SuspendAllInterrupts` short critical section | `taskENTER_CRITICAL()` (`BASEPRI = 0x50`, `< 1 us` `Task <-> ISR`) vs. `vTaskSuspendAll()` (locks scheduler without delaying `ISR`s) (`W04-L24`) |
| Shared bus / configuration protection across priority levels | FreeRTOS Mutex (`xMutexHolder` + Priority Inheritance) or zero-lock **Gatekeeper Task** (`W04-L24`) |
| High-rate ADC/sensor burst buffering | `xQueueCreateStatic` uses caller-supplied storage; a fixed `C03` pool can hold the payloads. Alternatively `heap_4` is a dynamic allocator, even if used only at startup. Coalescing adjacent free blocks reduces fragmentation; it cannot merge holes separated by a live object. |
| AUTOSAR `WdgM` Alive + Program-Flow Deadline Supervision | FreeRTOS Supervisor Task verifying per-task alive bitmasks + checkpoint ordering before kicking hardware `IWDG`/`WWDG` (`Day 4 / L29`) |
| DEM / Fault Event Memory & Safe Degraded State (FMEA-driven) | Bounded `FaultRecord` queue + persistent `.noinit` / flash ring with sequence epoch, CRC, overflow drop counter, and deterministic safe output (`Day 4 & Day 8`) |
| Polyspace / MISRA-C static safety & memory partitioning | Zero runtime `malloc` after `vTaskStartScheduler()`, reverse-order init unwind (`C02`), checked frame bounds (`C01`/`C05`), stack watermark + Cortex-M `MPU` guard (`W01-L18`, `W04-L26`) |

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

**Deferred hardware validation only:** If bench work resumes, first exercise a
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

### Interview walkthrough versus deferred hardware acceptance

For Day 8, draw the workplace boundary and P1 separately, and trace the
deadline/cancellation cases in the roadmap. Label predicted results **expected**.
The table below applies only to a future implementation; builds, board runs
and measured timing are not sprint completion requirements.

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

## P2. Zephyr Diagnostic Supervisor And Bench Extensions (Deferred — Outside Active 10-Day Interview Track)

> **2026-11-20 Update:** Per learner decision, **FreeRTOS is the sole active RTOS tool** for the interview-readiness track, and physical board flashing (`F446RE`, `F767ZI`, `Pico`) is **optional / deferred** so `100%` of study time goes into mastering every required C, Cortex-M, Peripheral/`DMA`, FreeRTOS, Fault-Debugging, and Architecture concept in plain English and connecting `P1` to day-to-day AURIX work. Task-progress watchdog supervision and Cortex-M7 `D-Cache`/`DMA` coherency from `P2` are folded directly into **Days 4 & 6 (`W04-L29` & `W04-L31`)** of the [10-Day Plan](five_month_development_roadmap.md#5-weekly-schedule). The historical `P2` and `Pico` contracts remain preserved in [backup/plan/2026-11-20-mcu-hardware-track](../backup/plan/2026-11-20-mcu-hardware-track/README.md).
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
