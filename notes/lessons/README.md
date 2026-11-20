# Active Lesson Library

Read the lesson Markdown first, then ask questions in chat using its title or
section number. Lessons are published progressively, one manageable topic at a
time. Publication does not mean that you have completed or mastered the topic.

The [roadmap](../../plans/five_month_development_roadmap.md) owns dates and hours.
The [tracker](../../plans/training_tracker.md) owns learning status;
the [log](../../plans/learning_log.md) records actual evidence.
This index is a reading guide, not another schedule.

## Current Interview-Focused FreeRTOS & MCU Track (Updated 2026-11-20)

On **2026-11-20**, the learner approved the **10-Day (Extensible) Interview-Focused
MCU, Embedded C & FreeRTOS Plan** ([schedule](../../plans/five_month_development_roadmap.md#5-weekly-schedule);
prior hardware-first snapshot preserved in [backup/plan/2026-11-20-mcu-hardware-track](../../backup/plan/2026-11-20-mcu-hardware-track/README.md)).
FreeRTOS is the sole active RTOS tool, taught in plain English from official
sources with a complete theoretical `P1` reference architecture connected honestly
to day-to-day AURIX work (`Zephyr` and Linux kernel/IIO/Yocto are excluded; board
runs are optional/not required).

**Published Foundation & Day 1 (`W01-L15`..`W04-L26`):**

Start Day 1 with [three bytes arrive while the driver task is asleep](W04_L26_day_1_tutoring.md); the detailed reference is under Topic 21 below.
- **Topic 10 (`W01-L15`):** [Thumb, Arm instruction sets, and important Cortex-M4 instructions](W01_L15_thumb_and_arm_instruction_sets.md).
- **Topic 11 (`W01-L16`):** [Arm Cortex-M4 important core registers, reset state, and system register blocks](W01_L16_cortexm4_reset_registers.md).
- **Topic 12 (`W01-L17`):** [Complete bare-metal 84 MHz startup, vector table, B1 interrupt handoff, and LD2 debounce (linker script only)](W01_L17_b1_exti_interrupt_handoff_and_debounce.md).
- **Topic 13 (`W01-L18`):** [Bare-metal Cortex-M4 privilege (`CONTROL`), `PSP` vs. `MSP`, `MPU` memory protection, `SVC`, and `MemManage` faults (linker script only)](W01_L18_cortexm4_mpu_privilege_psp_svc_and_memmanage.md).
- **Topic 14 (`W01-L19`):** [Bare-metal `PendSV` context switching, official CMSIS headers, and FreeRTOS tasks (`P1` bridge)](W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md).
- **Topic 15 (`W01-L20`):** [BSP — Board Support Package or Bootstrap Processor; work and prerequisites](W01_L20_bsp_board_support_and_bootstrap_processor.md).
- **Topic 16 (`W02-L21` — `C02`):** [Three-resource initialization and reverse-order unwind (`C02`) — Linux kernel (`goto`), FreeRTOS, and AUTOSAR / MISRA-C](W02_L21_c02_three_resource_init_and_reverse_unwind.md).
- **Topic 17 (`W03-L22`):** [FreeRTOS from the ground up — task states, TCB & lists, priorities, `vTaskDelayUntil`, and ISR task notifications (`P1`)](W03_L22_freertos_tasks_states_delays_and_notifications.md).
- **Topic 18 (`W03-L23` — `C03`):** [Event producer/consumer ownership model (`C03`) — payload lifetime, torn reads, copy queues, and zero-copy slot pools (`P1`)](W03_L23_c03_producer_consumer_payload_ownership.md).
- **Topic 19 (`W04-L24`, rev 5):** [FreeRTOS queues (`pcWriteTo`/`pcReadFrom`, `SendToFront`, `1`-slot `xQueueOverwrite` mailbox), binary/counting semaphores, critical sections vs. `vTaskSuspendAll`, mutex priority inheritance, deadlock & gatekeeper tasks (`P1`)](W04_L24_freertos_synchronization_and_priority_inversion.md).
- **Topic 20 (`W04-L25`, rev 5):** [I2C, SPI, ADC and DMA — simple foundations, generic cross-MCU `DMA` (`Stream` vs. `Channel`), 4 interrupt paths, and continuous 4-mode use-case scenarios](W04_L25_i2c_spi_debug_interrupts_and_dma.md).
- **Topic 21 (`W04-L26` — Day 1 of 10-Day Interview Plan):** [FreeRTOS complete remaining interview pillars — `heap_1..heap_5` vs. static allocation, software timers (`"Tmr Svc"` daemon), event groups (`24` bits, `xEventGroupSync`, `SetBitsFromISR` trap), stream/message buffers (`SPSC` `DMA -> Task`), stack overflow (`Method 1/2` vs. `MPU`), runtime stats & tickless idle](W04_L26_freertos_heaps_timers_event_groups_stream_buffers_and_diagnostics.md).

**Review/publication update, 2026-11-17:** L20-L23 were reviewed and repaired
with approval; the four earlier L17-L19 findings were rechecked. L19's runtime
frame-padding qualification was restored; the other three fixes were already
present. C02/C03 now contain explicit failure-reporting checks, but the learner
selected source review only and will compile them. L24 is a concept/source-trace
lesson. No new firmware build, board run or learner completion is recorded.

**Requested peripheral & DMA reference, 2026-11-18 (revision 5):**
[W04-L25: I2C, SPI, ADC and DMA — simple foundations and continuous 4-mode use-case scenarios](W04_L25_i2c_spi_debug_interrupts_and_dma.md).
Source/document-reviewed, in two parts: **Part I (`Scenes 1–5`)** covers `I2C`,
`SPI`, `ADC`, generic cross-MCU `DMA` (`STM32` / `AURIX` / `RISC-V`) with
`Stream` vs. `Channel`, and the 4 step-by-step interrupt paths (`Pin/SYSCFG/EXTI`,
`SPI/I2C`, `ADC`, `DMA` $\rightarrow$ `NVIC` $\rightarrow$ `CPU`); **Part II
(`Scenes 6–9`)** pairs generic pseudocode with concise production-style
`STM32F446RE` CMSIS register code (`{0xAA..0xFF}` on `SPI1` & `I2C1`, and
`{0x0800, 0x0400, 0x0C00}` on `ADC1`) across CPU Polling, CPU Interrupt, DMA
Polling, and DMA Interrupt. No target wiring, implementation, build or learner
completion is recorded; this reference supports the upcoming P1 driver work.

Updated 2026-11-01 under the learner-approved sequential progression:
`W01-L15` and `W01-L16` cover the Arm Cortex-M4 instruction sets, important
assembly instructions, core registers (`R0..R15`, `xPSR`, `CONTROL`, `PRIMASK`,
`BASEPRI`, `FAULTMASK`), and memory-mapped system blocks (`SCB`, `RCC`, `FLASH`)
first; `W01-L17` and `W01-L18` apply them in complete bare-metal C using **only
`STM32F446RETX_FLASH.ld` and one C file** (zero HAL and zero CMSIS headers); and
`W01-L19` transitions 1-to-1 to official CMSIS headers (`stm32f446xx.h` /
`core_cm4.h`, zero HAL) with a 2-task `SVC` + `PendSV` + `SysTick` context
switcher on `PSP` before Week 3 FreeRTOS.
A separate `RegistersDebug` configuration preserves the learner's working HAL
implementation. Local STM32/Arm manuals reviewed; references not compiled or
flashed. Actual silicon errata, build inputs and hardware observations remain
gated.

Topic 1, [driver contracts and buffer ownership](W01_L06_mcu_driver_contracts_and_buffer_ownership.md),
and Topic 2, [build, flash and debug path](W01_L07_f446re_build_flash_and_debug_path.md),
and Topic 3, [Windows project baseline](W01_L08_windows_cubeide_project_baseline.md),
and Topic 4, [first Debug build](W01_L09_first_debug_build_and_memory_map.md),
and Topic 5, [first ST-LINK session](W01_L10_first_stlink_debug_stop_at_main.md),
and Topic 6, [initialization trace](W01_L11_trace_initialization_clock_gpio_uart.md),
and Topic 7, [drive LD2 and understand blocking delay](W01_L12_ld2_gpio_output_and_blocking_delay.md),
and Topic 8, [non-blocking elapsed-time scheduling and `HAL_GPIO_TogglePin()`](W01_L13_nonblocking_elapsed_time_and_gpio_toggle.md),
and Topic 9, [read `B1` (`PC13`) with non-blocking debounce](W01_L14_b1_button_polling_and_nonblocking_debounce.md),
remain available; proceeding does not establish completion or mastery.
L06-L14 preserve their lesson IDs; on 2026-11-01 the Cortex-M4 instruction and
register prerequisites were ordered first as `W01-L15` and `W01-L16`, followed
by `W01-L17` through `W01-L20`.

**Project update, 2026-10-27:** the
[generated baseline](../../../p1_f446re_baseline) exists at the agreed location.
Its configuration selects STM32CubeF4 V1.28.3; L08's V1.28.1 was an earlier
installed candidate, not a requirement to downgrade. L08's pending-generation
status is superseded here; its original lesson body remains unchanged.

**Build update, 2026-10-27:** Debug ELF/map/list outputs now exist. Tutor
inspected the artifact and generated rules, not a new build or board run.
This supersedes L09's pending-build status without rewriting that lesson.
The learner confirmed a bare USB-only board with replaceable firmware and a
matching data cable; follow L10's prechecks before the learner-operated session.

**Debug update, 2026-10-27:** learner reports the debug exercise done. The saved
launch selects the matching ELF and stop-at-main; its existence is not a
runtime trace. L10's pending-exercise status is superseded by completed-reported,
not by invented target readings. L11 follows the existing initialization code.

**Initialization update, 2026-10-27:** learner reports the observations done,
including an 84 MHz software clock value after initialization and `uwTick`
advancing every 1 ms. Record the report/configured cadence, not an externally
measured frequency or timing bound. L11's pending observation status is
superseded here; L12 now introduces the first visible output behavior.

**Windows-first update:** the current HP Windows laptop is the primary host;
the other HP now has Ubuntu Server installed per learner report and is optional.
L07's unresolved host choice/planned-server wording is superseded here without
rewriting its explanation. Installed tools and current evidence are in
[setup](../../docs/workspace_setup.md).

**Hardware-first update, 2026-10-26:** use the available boards, no QEMU required.
Earlier emulator-first next-action text, including in L06, is superseded by
this approved workflow. Existing lesson bodies are preserved. Optional host-C
logic tests are not emulation and do not establish hardware behavior.

The existing [C01 note](W01_L04_c01_checked_packet_size.md) and repaired
[learner implementation](C01_packet_size.c) remain the Week 1 implementation
thread. Runtime evidence is pending; no implementation is restarted because of
the pivot. Readiness/startup follows under the new roadmap.

P1/P2 lessons will be added progressively; no full curriculum is pre-generated.
Preserve W01-L01 through L05 bodies and their historical claims rather than
silently rewriting them. Their old next-topic directions do not override the
current roadmap/tracker.

## Week 1 Reading Path

The table below preserves the **former Linux-oriented reading sequence**.
It is not the next required sequence for the MCU track; C01 remains applicable.

The new project-track Week 1 started early on 2026-10-23. Its planned calendar
and 22-hour allocation remain in the roadmap; historical completed weeks are
not being restarted.

| Order | Topic | Note status |
| --- | --- | --- |
| 1 | [Linux process, kernel and device boundaries](W01_L01_process_kernel_device_boundaries.md) | Published, source-reviewed; revised style approved by learner; understanding not assessed |
| 2 | [System calls and waiting for buffered data](W01_L02_system_calls_and_waiting_for_data.md) | Published; learner-edited explanation preserved; understanding not assessed |
| 3 | [Privilege and address spaces: RISC-V to Arm](W01_L03_privilege_and_address_spaces.md) | Published, source-reviewed; updated in concise Feynman style |
| 4 | [C01: checked header-plus-payload size calculation](W01_L04_c01_checked_packet_size.md) | Published; learner submitted and repaired [C01_packet_size.c](C01_packet_size.c) (source-reviewed; runtime check pending) |
| 5 | [From `start_kernel()` to driver init and QEMU preflight](W01_L05_linux_boot_to_driver_init_and_qemu_preflight.md) | Published, source-reviewed; bridges Week 1 boot/ownership trace to Week 2 QEMU module lifecycle |

Readiness work uses the [setup guide](../../docs/workspace_setup.md) alongside
these topics. No installation, build or board experiment is implied by reading.
Questions and clarifications can be incorporated into the relevant lesson;
new major topics get their own note instead of making the first note endless.

## Reading A Lesson

1. Start with **Know This First**.
2. Follow the problem, design and worked example.
3. Use source links for the small section being explained, not the entire tree.
4. Treat expected/illustrative results differently from measured results.
5. Use the recap and interview explanation for optional private reconstruction.

The learner approved L01's revised approach: explain terms through the project,
use a starting-values table and small worked cases, then minimal source excerpts
with the concrete behavior explained inline. Continue this approach in new
lessons; do not replace it with long code dumps or abstract prerequisite lists.
Preserve the learner's L01/L02 edits. Explain correctness concerns separately
and obtain approval for minimal repairs instead of automatically rewriting notes.

**First implementation:** L04/C01 is a small C function plus boundary tests,
still within Week 1. It does not require Yocto or a full driver. The learner
types the code after explanation; creation/editing of implementation files and
execution require the relevant permission.

Quizzes remain paused. You do not need to submit answers to continue learning.
No future lesson is marked complete merely because its title appears here.

## Authoring Convention

Use `Wxx_Lyy_topic.md` in this folder. `Wxx` refers to the active project-track
week; `Lyy` is a stable lesson ID within that week. The MCU pivot continues at
L06 rather than overwriting L01-L05; the current reading order is stated above.
Every lesson names its scope,
prerequisites, source/evidence status and corrections. Do not pre-generate the
whole curriculum or overwrite historical notes to populate this library.

The [older note guide](../README.md) identifies historical references and
superseded recipes. Learner implementation code remains in its own repositories
or exercises; this folder contains teaching material.
