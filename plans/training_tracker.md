# MCU / RTOS Training And Evidence Tracker

Updated 2026-11-17; MCU/FreeRTOS-first pivot approved on 2026-10-26.
[Roadmap](five_month_development_roadmap.md) owns schedule;
[project contracts](project_portfolio.md) own acceptance;
[log](learning_log.md) preserves history.

## Current Position

- Week 1 In progress; 22 h/week through the 22 March 2027 preparation/interview
  target. No extra time or mastery inferred from changing the plan.
- Active focus: MCU driver ownership, C01 runtime evidence and board/toolchain
  readiness before FreeRTOS integration.
- Requested peripheral & DMA reference
  [W04-L25](../notes/lessons/W04_L25_i2c_spi_debug_interrupts_and_dma.md)
  **Published & revised (rev 3)**, 2026-11-17: Part I (`Scenes 1–4`) covers
  simple foundations for `I2C`, `SPI`, `ADC`, `DMA`, and 4 separate interrupt
  paths; Part II (`Scenes 5–9`) traces continuous `6`-byte scenarios
  (`{0xAA..0xFF}` on `SPI1` and `I2C1`, and `3 x 16-bit = 6 bytes` on `ADC1`)
  from `84 MHz` clock + `GPIO` + peripheral init through CPU Polling, CPU
  Interrupt, DMA Polling, and DMA Interrupt in direct CMSIS register code
  (zero HAL), plus outside-in debugging and DMA buffer quiescence.
  Document/source-reviewed synthetic examples only; target selection, wiring,
  implementation and physical tests remain pending. Publication does not mark
  the P1 driver milestones complete.
- Optional Pico setup, 2026-11-07: learner reports `RPI-RP2` USB bootloader
  detection and extension installation. Tutor's second read-only VS Code CLI
  inventory confirms official Pico extension 0.23.0. Learner subsequently
  reports project creation and UF2 availability after the `blink`/`pico`
  instructions; path, configuration, build console and SDK/toolchain versions
  remain uninspected. SWCLK/GND/SWDIO presence is learner-reported; compatible
  debug probe and wiring are not established. See
  [setup](../docs/workspace_setup.md#pico-usb-detection-and-windows-setup-2026-11-07)
  and the dated [log](learning_log.md). No upload, physical blink, hardware
  debug or multicore run is inferred.
- Board decision approved 2026-11-07: retain **F446RE/FreeRTOS P1** and
  **F767ZI/Zephyr P2**. Pico remains a bounded setup check and a later optional
  producer/consumer multicore experiment, replacing another Week 17 extension
  only if the P1/P2 cores are sound. No third project or extra weekly hours.
- Current topics 10–19 **Published / learner evidence pending where unstated** (re-ordered 2026-11-01 so
  Arm Cortex-M4 instructions and registers come first):
  - Topic 10 ([W01-L15](../notes/lessons/W01_L15_thumb_and_arm_instruction_sets.md)):
    Thumb (`T32` / `Thumb-2`), `A32`, `A64`, instruction width vs. register
    width, and important Cortex-M4 assembly instructions (`movs`, `ldr`/`str`,
    `mrs`/`msr`, `cpsid`/`cpsie`, `dsb`/`isb`, `push`/`pop`, `stmdb`/`ldmia`,
    `svc`, `tst`/`ite`, `bx lr`).
  - Topic 11 ([W01-L16](../notes/lessons/W01_L16_cortexm4_reset_registers.md)):
    Cortex-M4 core registers (`R0..R15`, `xPSR`), special registers (`CONTROL`,
    `IPSR`, `PRIMASK`, `BASEPRI`, `FAULTMASK`), and memory-mapped system blocks
    (`SCB->VTOR`, `RCC->CR`/`CFGR`, `FLASH->ACR`).
  - Topic 12 ([W01-L17](../notes/lessons/W01_L17_b1_exti_interrupt_handoff_and_debounce.md)):
    Complete bare-metal 84 MHz startup, vector table, and `B1`/`LD2` handoff
    (linker script only).
  - Topic 13 ([W01-L18](../notes/lessons/W01_L18_cortexm4_mpu_privilege_psp_svc_and_memmanage.md)):
    Complete bare-metal Cortex-M4 `MPU` (`0xE000ED90`), `CONTROL` privilege
    (`nPRIV`/`SPSEL`), `PSP` vs. `MSP`, `SVC` (`#11`) privilege escalation, and
    `MemManage_Handler` (`#4`) fault capture (linker script only).
  - Topic 14 ([W01-L19](../notes/lessons/W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md)):
    1-to-1 bare-metal-to-CMSIS header mapping (`stm32f446xx.h` / `core_cm4.h`)
    and 2-task `SVC` + `PendSV` (`#14`, priority `15`) + `SysTick` context
    switching on `PSP` bridging into Week 3 FreeRTOS (`P1`).
  - Topic 15 ([W01-L20](../notes/lessons/W01_L20_bsp_board_support_and_bootstrap_processor.md)):
    Board Support Package versus Bootstrap Processor, MCU board-support work,
    HAL/CMSIS/RTOS boundaries and technical prerequisites.
  - Topic 16 ([W02-L21](../notes/lessons/W02_L21_c02_three_resource_init_and_reverse_unwind.md)):
    Three-resource initialization and reverse-order unwind (`C02`) across Linux
    kernel (`goto`), FreeRTOS (`P1`), and AUTOSAR / MISRA-C (`goto`-free).
  - Topic 17 ([W03-L22](../notes/lessons/W03_L22_freertos_tasks_states_delays_and_notifications.md)):
    FreeRTOS from the ground up — `tskTCB` & lists, task states (`Running`,
    `Ready`, `Blocked`, `Suspended`), `configMAX_SYSCALL_INTERRUPT_PRIORITY` /
    `BASEPRI`, `vTaskDelayUntil`, and `ulTaskNotifyTake` / `vTaskNotifyGiveFromISR`.
  - Topic 18 ([W03-L23](../notes/lessons/W03_L23_c03_producer_consumer_payload_ownership.md)):
    Event producer/consumer ownership model (`C03`) — ISO C payload lifetime,
    why `volatile` does not prevent torn multi-word reads, bounded copy-by-value
    queues, and zero-copy `FREE -> FILLING -> READY -> IN_USE -> FREE` slot
    pools with generation-guarded release (`P1`).
  - Topic 19 ([W04-L24](../notes/lessons/W04_L24_freertos_synchronization_and_priority_inversion.md)):
    Queue versus notification/binary semaphore versus mutex, task/ISR rules,
    short critical sections, and an illustrative H/M/L priority-inversion and
    inheritance trace. Source/API-reviewed publication; runtime trace planned.
  - Approved review repairs, 2026-11-17: L19 frame alignment, L20 GPIO/clock
    facts, L21 acquire/quiescence/MISRA/test contracts, L22 scheduling and
    integration semantics, L23 payload/pool/error/test contracts. Three other
    earlier L17-L19 findings were already fixed. Learner selected static/source
    review only; C02/C03 harness compilation/execution remains pending.
  Proposed `RegistersDebug` configuration preserves the existing HAL source.
  References are document/source-reviewed only, not compiled/flashed. Topic 9
  ([W01-L14](../notes/lessons/W01_L14_b1_button_polling_and_nonblocking_debounce.md))
  was implemented and repaired in `Core/Src/main.c` (`Completed-reported /
  source-inspected`).
- L11 observations completed-reported: software clock value 84 MHz after
  initialization; learner describes uwTick incrementing every 1 ms. This agrees
  with the intended configuration, not externally measured oscillator/tick
  accuracy. No additional unreported values or mastery inferred.
- Learner reports generation of
  [p1_f446re_baseline](../../p1_f446re_baseline) at the agreed location.
  Files confirm NUCLEO-F446RE/STM32F446RETx and selected STM32CubeF4 V1.28.3.
  Debug/Release select the FLASH linker script. Debug ELF/map/list artifacts
  now exist. Tutor inspected them without rebuilding or executing firmware.
- Use the current HP Windows host and available hardware, no QEMU required.
  The other HP's Ubuntu Server is learner-reported installed and optional.
  Learner confirmed existing F446RE firmware may be replaced; bare board,
  no external wiring/power and a matching USB data cable. This authorizes
  learner-operated lesson steps, not tutor hardware access. Debug-session
  completion is learner-reported; no probe-identity/physical-measurement details
  are inferred. Recheck safety if the setup changes.
  Optional host-C tests remain logic evidence, not hardware validation.
- Topic 1 published; learner completion **unassessed**:
  [MCU driver contracts and buffer ownership (W01-L06)](../notes/lessons/W01_L06_mcu_driver_contracts_and_buffer_ownership.md).
  Conceptual lesson published 2026-10-26; worked traces are illustrative,
  not runtime evidence or proof of learner completion.
- [C01 note](../notes/lessons/W01_L04_c01_checked_packet_size.md) remains useful.
  Learner submitted and repaired [C01 code](../notes/lessons/C01_packet_size.c);
  the latest dated log records source review as Correct, runtime still pending.
  Do not overwrite that implementation or assign it from scratch.
- L01-L03/L05 Linux/architecture notes remain historical references, not the
  next required module. No lesson body changed by this pivot.
- Quizzes stay paused; actual hours and independent/delayed mastery unknown.
- Real experience clarified: AURIX IC-driver cyclic functions, AUTOSAR
  integration, architecture/interface reviews, integration tests and mentoring.
  Safety: FMEA/HAZOP participation with safety engineers, Polyspace/MISRA and
  requirements traceability. These are learner reports, not an audited CV.
- P1/P2 are private learning implementations initially. Employer adoption,
  project ownership and certification cannot be recorded before they occur.

## Status Rules

Planned = no completion evidence; In progress = work begun; Blocked = named gate;
Deferred = explicitly outside current core; Completed-reported = learner report;
Verified = named observed checks with runner/environment/revision; Historical =
earlier work, not current target acceptance.

Explain/trace/implement/debug/design dimensions remain separately unassessed,
with help, independent, or delayed fresh evidence. Source review is not runtime
proof. Tutor publication is not learner completion.

## Preserved Progress

| Area | Evidence retained | Current use/limit |
| --- | --- | --- |
| Earlier C/concurrency weeks | Completed-reported, source/answer reviews in log | Reuse in driver lifetime/RTOS examples; no automatic mastery |
| C01 | Learner repaired source; recorded source review Correct | Targeted runtime check pending, not restarted |
| RISC-V boot/traps/FDT/page allocation/Sv39 | Earlier implementation and QEMU evidence in dated log | Historical architecture knowledge, not active Linux curriculum |
| `my-linux` v1 | Latest `6610119` superseded with linker/timer defects | Frozen reference; do not rebuild or repair automatically |
| `my-linux` v2 K0/K1a | `3fddd0d`, recorded tutor checks | Historical boot/firmware evidence; no new execution |
| Prior active Linux/RTOS plan | [43-file snapshot](../backup/plan/2026-10-26-linux-rtos-track/README.md) | Schedule/skill policies archived, not active |

## Readiness And Project Gates

| Gate | Status | Evidence / next action |
| --- | --- | --- |
| Pre-pivot archive | Verified | 43 working files SHA-256 copied before edits; original 37-file foundation also preserved |
| Board identification | Verified | [Photo inventory](../docs/board_inventory.md): F446RE/F767ZI/Pico identities, not powered functionality |
| F446RE | In progress | Debug/initialization completed-reported, 84 MHz software clock/tick behavior reported; first LED behavior and physical timing evidence pending |
| F767ZI | Blocked | Verify debug/core behavior; cache/DMA memory placement and PA7 Ethernet conflict where relevant |
| Pico setup / UF2 | In progress | BOOTSEL detection, project creation and UF2 availability learner-reported; extension 0.23.0 inspected; artifact/configuration and physical run unverified |
| Pico SWD debug | Planned | SWCLK/SWDIO/GND identified by learner; compatible probe, pinout, signaling/power and host tools require verification |
| Optional Pico multicore | Deferred | P1/P2 and single-core FreeRTOS gates first; one bounded experiment may replace another Week 17 option |
| FreeRTOS | Planned | Local CubeF4 V1.28.3 V10.3.1 task/CM4F reference inspected; P1 build/heap/config integration and runtime not established |
| Zephyr | Planned | Existing reference checkout; release/modules/SDK/runtime not validated for P2 |
| Saleae | Blocked | Model/probe limits/sample rate unresolved |
| Windows primary host | In progress | Build/debug completed-reported; ELF/map and launch metadata inspected, no tutor target access or detailed runtime record |
| F446RE baseline generation | Completed-reported / source-inspected | [Project](../../p1_f446re_baseline) exists; selected F4 V1.28.3, CubeMX 6.12.1; no build/runtime proof |
| First Debug build | Completed-reported / artifact-inspected | Arm ELF/map present; main at 0x080004f4; generated rules -O0/-g3; full console/warning history not supplied |
| First ST-LINK session | Completed-reported | Learner says done/debugged; saved launch selects Debug ELF and main breakpoint; no console/state capture supplied |
| Initialization checkpoints | Completed-reported | Learner says observed values: software clock 84 MHz after setup and 1 ms tick progression; measurement method/raw capture not supplied |
| First LD2 output | Planned | L12: learner types four calls, builds and observes onboard LED; no tutor compile/run requested |
| Register-level EXTI layer | Planned | Approved current L17 reference (formerly L15); local RM0390 Rev6, UM1724 Rev14 and DUI0553B checked; separate config/build/silicon-errata/board checks pending |
| Ubuntu HP alternative | Installed-reported | Ubuntu Server installed per learner; exact release/SSH/tools/free space unverified; not a prerequisite |
| Existing Podman | Optional host-C reference | Prior host/compiler evidence retained; no requirement to use it for Windows board builds |
| P1 core | Planned | Requirements, model, cyclic driver and fault-recording evidence |
| P2 core | Planned | Independent Zephyr diagnostic supervisor/test scenarios |
| Peer/CAN | Deferred | Confirm transceivers, termination, voltage, target APIs and recovery; UART/model fallback |
| Sensors/BOM | Deferred | One public-datasheet safe peripheral if needed, after approval; no purchase authorized |
| Career | In progress | [Official jobs and salary limits](career_role_evidence.md); actual CV/impact examples not yet audited |

## Weekly Execution Register

The [roadmap](five_month_development_roadmap.md#5-weekly-schedule) owns dates and
the [C01-C21 table](five_month_development_roadmap.md#weekly-c-practice-c01-c21).
`-` means unknown. These rows record execution, not a second schedule.

| Week | Main slice | C ID | Actual h | Status | Evidence / blocker |
| --- | --- | --- | --- | --- | --- |
| 1 | C01/requirements and readiness | C01 | - | In progress | [Topic 7 published](../notes/lessons/W01_L12_ld2_gpio_output_and_blocking_delay.md); initialization observed-reported; LED implementation and C01 runtime pending |
| 2 | Startup/polling/IRQ | C02 | - | Planned | - |
| 3 | FreeRTOS tasks/events | C03 | - | Planned | - |
| 4 | Sync/ISR/inversion | C04 | - | Planned | - |
| 5 | Device protocol | C05 | - | Planned | - |
| 6 | Cyclic driver lifecycle | C06 | - | Planned | - |
| 7 | Acquisition/DMA | C07 | - | Planned | - |
| 8 | Transport/recorder | C08 | - | Planned | - |
| 9 | Timing/memory/faults | C09 | - | Planned | - |
| 10 | P1 core release | C10 | - | Planned | - |
| 11 | Zephyr foundation | C11 | - | Planned | - |
| 12 | P2 supervision | C12 | - | Planned | - |
| 13 | P2 fault scenarios | C13 | - | Planned | - |
| 14 | P2 integration | C14 | - | Planned | - |
| 15 | Fault campaign | C15 | - | Planned | - |
| 16 | Cache/MPU/DMA | C16 | - | Planned | - |
| 17 | One optional interface / Pico multicore experiment, or core repair | C17 | - | Planned | Select one only after P1/P2 core gates; no added hours |
| 18 | Traceability/release | C18 | - | Planned | - |
| 19 | Design/work deep dives | C19 | - | Planned | - |
| 20 | Interview gap repair | C20 | - | Planned | - |
| 21 | Contingency | C21 | - | Planned | - |

Review checkpoints at Weeks6/10/14/19 remain planned until actual evidence.
They do not block earlier applications.

## Evidence Template

Date, requirement, source/config/toolchain, author/runner, board or model,
command, expected/observed result, raw trace, failure/repair, remaining limits,
actual time only if reported, next action. Keep employer material separate.

Known old kernel/lesson defects remain in their original log/snapshot records.
Do not reuse a stored high-linked pointer before MMU setup, unconditional PASS
summary, or unbounded timer-wait pattern from historical material. The active
lesson reviewer remains available without rewriting old lessons automatically.
