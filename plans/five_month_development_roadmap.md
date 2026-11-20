# 10-Day (Extensible) Interview-Focused MCU, Embedded C & FreeRTOS Preparation Plan

Replaced with learner approval on 2026-11-20. **Only active preparation schedule.**
Preserved snapshots: [2026-11-20 MCU hardware track](../backup/plan/2026-11-20-mcu-hardware-track/README.md),
[2026-10-26 Linux/RTOS track](../backup/plan/2026-10-26-linux-rtos-track/README.md), and
[2026-10-22 foundation](../backup/plan/2026-10-22-foundation/README.md).
Active companion docs: [Theoretical P1 Architecture & Workplace Bridge](project_portfolio.md),
[Tracker](training_tracker.md), [Setup](../docs/workspace_setup.md), [Role Evidence](career_role_evidence.md).

## 1. Goal, Study Mode And Starting Strengths

- **Target Roles & Compensation Aspiration:** Senior / Lead / Staff Embedded Firmware, MCU Peripheral-Driver, RTOS/Platform, and Automotive/SoC MCU Subsystem roles in Bengaluru (aspirational target up to INR 90 LPA total CTC; Principal selective where scope matches; no title, salary, or offer date is guaranteed).
- **Timeline & Flexibility:** Focused **10-Day Interview-Readiness Sprint (`2026-11-21` to `2026-11-30`)** at **`1.5–2 hours/day` (`~18–20 hours` baseline)** so resume finalization and interview attendance can begin right after Day 10 (`2026-12-01`). **Zero required/important concepts are skipped**: if any module needs deeper reading, the learner may freely extend daily study hours or add days rather than cutting technical depth.
- **Reading-First, Interview-Focused Workflow (Approved 2026-11-20):**
  - As a working embedded professional (`10+ years C`), **hands-on board flashing, soldering, and lab runs are NOT required right now**.
  - Every concept is taught in **plain, crystal-clear English from first principles**, grounded in **official specifications, upstream `FreeRTOS-Kernel` C source, and *Mastering the FreeRTOS Real Time Kernel***, with zero gaps in pointer/slot state, timing windows, failure modes, and concrete code walkthroughs.
  - **FreeRTOS is the single RTOS tool** used to master all real-time OS concepts (`Zephyr`, `Linux kernel/IIO/Yocto`, and `my-linux` are excluded from active study).
- **Connecting Theory To Real Day-To-Day Work (100% Truthful Positioning):**
  - **Real Workplace Evidence:** 10+ years C and 32-bit Infineon **AURIX** professional work—reviewing software-architecture/interface specs, implementing cyclic functions for motor-control/BMS IC drivers, integrating AUTOSAR modules with Complex Device Drivers (`CDD`), writing integration tests, mentoring engineers, participating in FMEA/HAZOP assessments with safety engineers, Polyspace/MISRA static analysis, and requirements traceability.
  - **Theoretical Capstone Reference (`P1`):** A complete, production-grade **FreeRTOS Asynchronous Cyclic IC Driver Service, `DMA` Acquisition & Fault Recorder** architecture (`STM32F446RE` / Cortex-M4 reference) studied at full code-and-state-machine depth so you can whiteboard clean RTOS/driver designs in interviews and connect every concept (`SPI`/`I2C`/`ADC`/`DMA`, bounded queues, mutex priority inheritance, watchdog progress supervision, fault rings) directly to your real AURIX engineering experience—without ever misrepresenting private theoretical designs as deployed employer products or unrun hardware measurements.

---

## 2. Already Covered Foundation (`W01-L15` .. `W04-L25` — Ready For Fast Review)

You have already built a strong bottom-up library covering **~70% of core Cortex-M + FreeRTOS internals**:

| Pillar | Completed Lesson Notes | Key Interview Takeaways Already Covered |
| --- | --- | --- |
| **Cortex-M4 CPU, Registers & Bare-Metal IRQ** | [W01-L15](../notes/lessons/W01_L15_thumb_and_arm_instruction_sets.md), [W01-L16](../notes/lessons/W01_L16_cortexm4_reset_registers.md), [W01-L17](../notes/lessons/W01_L17_b1_exti_interrupt_handoff_and_debounce.md) | `Thumb-2`, `R0..R15`/`xPSR`, `PRIMASK` vs. `BASEPRI` vs. `FAULTMASK`, `VTOR`, vector table bit `0 = 1`, `GPIO -> SYSCFG -> EXTI -> NVIC -> CPU`, `W1C` (`=`, never `\|=`) |
| **Privilege, `MPU`, `MSP`/`PSP` & Context Switch** | [W01-L18](../notes/lessons/W01_L18_cortexm4_mpu_privilege_psp_svc_and_memmanage.md), [W01-L19](../notes/lessons/W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md), [W01-L20](../notes/lessons/W01_L20_bsp_board_support_and_bootstrap_processor.md) | `CONTROL` (`nPRIV`/`SPSEL`), `SVC` (`#11`), `MemManage` (`#4`), `PendSV` (`#14`, prio `15`) `16`-word frame (`R4..R11` SW + `R0..xPSR` HW), `EXC_RETURN = 0xFFFFFFFD`, BSP boundaries |
| **Applied C Lifetimes & Unwind (`C01–C03`)** | [W01-L04](../notes/lessons/W01_L04_c01_checked_packet_size.md), [W02-L21](../notes/lessons/W02_L21_c02_three_resource_init_and_reverse_unwind.md), [W03-L23](../notes/lessons/W03_L23_c03_producer_consumer_payload_ownership.md) | Overflow-checked packet math (`C01`), reverse-order init unwind (`goto` vs. FreeRTOS vs. AUTOSAR/MISRA `C02`), torn reads, copy-by-value vs. zero-copy `FREE->FILLING->READY->IN_USE` pool (`C03`) |
| **FreeRTOS Tasks, Queues, Semaphores & Mutexes** | [W03-L22](../notes/lessons/W03_L22_freertos_tasks_states_delays_and_notifications.md), [W04-L24](../notes/lessons/W04_L24_freertos_synchronization_and_priority_inversion.md) | `TCB_t`, Ready/Delayed lists, `vTaskDelayUntil`, Task Notifications, `Queue_t` (`pcWriteTo`/`pcReadFrom`, `SendToFront`, `1`-slot `xQueueOverwrite` Mailbox), Binary 3-IRQ burst + `while`-drain, Counting Semaphores, `taskENTER_CRITICAL` vs. `vTaskSuspendAll`, Mutex Priority Inheritance & Simplified Inheritance caveats, Deadlock, Recursive Mutex, Gatekeeper Task |
| **`I2C`, `SPI`, `ADC` & Cross-MCU `DMA`** | [W04-L25](../notes/lessons/W04_L25_i2c_spi_debug_interrupts_and_dma.md) | Open-drain `I2C` pull-ups & repeated-START read, full-duplex `SPI` `CPOL`/`CPHA`, `SAR ADC`, generic `DMA` (`Stream` vs. `Channel`), 4 interrupt paths & 4 transfer modes |

---

## 5. Weekly Schedule

*(Section number `#5-weekly-schedule` retained for cross-document links; now organized as the **10-Day Interview-Focused Mastery Schedule**, extensible in days/hours as needed without skipping any required topic.)*

| Day | Target Date | Lesson ID | Core Interview Topic (Plain English, FreeRTOS + MCU + Workplace Link) | Why High-Paying Interviews Test This |
| --- | --- | --- | --- | --- |
| **Day 1** | 2026-11-21 *(Published 2026-11-20)* | `W04-L26` | [Start with the Day 1 tutoring guide](../notes/lessons/W04_L26_day_1_tutoring.md): static allocation vs. `heap_1..5`, software timers, event groups, stream/message buffers, stack checks, runtime statistics and tickless idle. | Selected FreeRTOS interview topics: explain where data lives, who executes a callback, and how to detect lost work. |
| **Day 2** | 2026-11-22 | `W04-L27` | **Embedded C Live-Coding & Concurrency Traps (`C04` + `C07` + `C11`):** `volatile` vs. `const` vs. atomics vs. CPU barriers (`DMB`/`DSB`/`ISB`), struct padding/alignment/aliasing, endianness & safe byte packing (`C05`), bit-twiddling idioms, wrap-safe tick math (`(uint32_t)(now - start) >= dt`), Ring Buffer (`C07`), and Safe Producer/Consumer Stop, Quiesce & Drain (`C04`) | Directly clears 30–45 minute C coding screens and eliminates subtle `volatile`/pointer/teardown traps |
| **Day 3** | 2026-11-23 | `W04-L28` | **Boot-to-`main()`, Linker Scripts, Memory Map & Secure Bootloader (`A/B` OTA):** Reset vector -> `.data` Flash(`LMA`)-to-SRAM(`VMA`) copy -> `.bss` zeroing -> stack/heap layout, executing time-critical code from RAM, and clean Bootloader-to-Application jump (`disable IRQs/DMA/SysTick -> VTOR -> MSP -> app Reset_Handler` + CRC/rollback) | Mandatory in Senior/Staff firmware & platform loops (`TI`, `Qualcomm`, `Infineon`) |
| **Day 4** | 2026-11-24 | `W04-L29` | **Hardware Timers, `PWM`, Watchdogs (`IWDG`/`WWDG`) & Logical Task-Progress Supervision (`C12`):** Timer `PSC`/`ARR`/capture/compare math, why kicking a watchdog in a timer `ISR` is broken, multi-task **alive + sequence checkpoint supervision** (FreeRTOS equivalent of AUTOSAR `WdgM`), reset-cause decoding & `.noinit` crash records | Bridges your real AURIX/AUTOSAR safety & cyclic supervision background directly to FreeRTOS design |
| **Day 5** | 2026-11-25 | `W04-L30` | **`UART`, `CAN` / `CAN-FD` Architecture, Transceivers & Bus Fault Recovery:** `UART` framing/overrun + `IDLE`-line `DMA` RX; `CAN` vs. `CAN-FD` differential bus, bitwise arbitration, bit timing, filters, `Error Active -> Error Passive -> Bus-Off` (`TEC > 255`) recovery, plus `I2C` 9-clock stuck-bus recovery | Essential for automotive/industrial/SoC driver roles alongside your `SPI`/`I2C`/`ADC` knowledge |
| **Day 6** | 2026-11-26 | `W04-L31` | **Cortex-M Crash Debugging (`HardFault`, `CFSR`, `BFAR`) & `D-Cache` vs. `DMA` Coherency:** Decoding `EXC_RETURN` (`MSP` vs. `PSP`) to find stacked `PC`/`LR`, precise vs. imprecise faults, plus **CPU Cache (`Clean` vs. `Invalidate`, cache-line alignment, non-cacheable `MPU` regions)** when `DMA` shares SRAM | Classic Staff/Principal separator question on Cortex-M7, AURIX TriCore, and heterogeneous SoCs |
| **Day 7** | 2026-11-27 | `W04-L32` | **Real-Time Scheduling, Latency Budgets, Overload Policies & Multi-Core IPC:** Rate-Monotonic / Deadline-Monotonic priority assignment, WCET + blocking budget, interrupt storm throttling, `Drop-Newest` vs. `Drop-Oldest` (`C15`), low-power design, and multi-core `IPC` (`shared RAM + HW spinlock/doorbell + memory barriers`) | Tests whether you think like a Staff Firmware Architect rather than an API caller |
| **Day 8** | 2026-11-28 | `W04-L33` | **Theoretical Capstone Walkthrough (`P1`) Connected To Day-To-Day AURIX Work:** End-to-end architecture, state machine (`UNINIT -> INIT_VERIFY -> IDLE -> BUSY_ASYNC -> RECOVERING -> SAFE_DEGRADED`), `C06` timeout + generation-counter protection against late `DMA` callbacks, fault ring, and 1-to-1 mapping to your real AURIX motor-control/BMS IC cyclic driver & AUTOSAR `CDD` work | Gives you a crystal-clear whiteboard system design + authentic workplace connection for deep-dive rounds |
| **Day 9** | 2026-11-29 | `W04-L34` | **Senior/Lead/Staff Resume Blueprint & Workplace STAR Deep-Dive Stories:** Structuring a sharp 2-page CV and spoken-English answers for your real AURIX cyclic IC drivers, AUTOSAR integration, architecture/interface reviews, FMEA/HAZOP + Polyspace/MISRA, integration tests, mentoring, and theoretical FreeRTOS architecture mastery | Positions your 10+ years of real engineering impact cleanly for 50–90 LPA shortlists without exaggeration |
| **Day 10** | 2026-11-30 | `W04-L35` | **Master Mock-Interview Playbook (Top 45 Questions, Follow-Up Traps & Live-C Checklist):** Rapid-fire spoken-English answers across C, Cortex-M, `SPI`/`I2C`/`UART`/`CAN`/`ADC`/`DMA`, FreeRTOS, Fault Debugging, and Architecture + final interview readiness checklist | Locks in crisp, confident 60-second spoken answers right before attending interviews |

---

### Embedded C Interview Patterns (`#weekly-c-practice-c01-c21`)

**Worked cases for the scheduled days** (part of the existing sessions, not extra prerequisites):

| Day | What to walk through | Reference |
| --- | --- | --- |
| 2 — unfamiliar C problem | Merge two sorted event streams into caller-owned storage. State capacity, empty-input and overlap rules; trace one exhausted input; explain time and space cost. Timed coding is optional; expand algorithms only for a confirmed general screen. | [Interview Cake: merge sorted arrays](https://www.interviewcake.com/question/c/merge-sorted-arrays) |
| 3 — update and trust | Distinguish CRC for accidental corruption from signature verification against a trusted key. Trace a trial boot, power cut before confirmation, permitted recovery and rejection of an image below the security-version floor. | [MCUboot design](https://docs.mcuboot.com/design.html) |
| 6 — intermittent fault without a crash | For stale SPI data, compare an old value on the wire with a fresh value copied from the wrong buffer. Choose a bus trace and correlated transfer IDs before halting the CPU; explain what a CPU watchpoint can miss. End with a regression test. | [GDB watchpoints](https://www.sourceware.org/gdb/current/onlinedocs/gdb.html/Set-Watchpoints.html) |
| 8 — workplace boundary | First draw an anonymized real AURIX transaction: which runnable initiated it, actual CDD/MCAL boundary, callback context, diagnostic recipient and your contribution. Then draw P1 and identify one mechanism that does **not** transfer unchanged. | [AUTOSAR Classic](https://www.autosar.org/standards/classic-platform) · [Infineon tutorials](https://github.com/Infineon/AURIX_code_examples) |
| 8 — verification | Use a fake clock and transport to walk completion just before the deadline, at the deadline (timeout wins), and after cancellation. For each, record expected state, owner and diagnostic counter. Separate host-testable policy from DMA stop confirmation and timing on target. | [Unity: C unit testing](https://www.throwtheswitch.org/unity) |
| 9 — ownership | Prepare one real integration defect and one interface or release decision. Name the affected team, alternative rejected, who decided, what you personally delivered, and how the outcome was established. Use qualitative evidence when numbers are unavailable. | [TI RTOS Lead: component ownership and release delivery](https://careers.ti.com/en/sites/CX/job/25009764/) |

Use Day 10 for worked follow-ups on these cases; a live mock remains opt-in.
At recruiter screening, confirm the Bengaluru level, actual scope and interview format.
Compare fixed pay, annual bonus and equity **vesting that year**; separate one-off awards. The target is not a verified employer band.

*(Anchor `#weekly-c-practice-c01-c21` preserved; consolidated into the high-yield C patterns interviewers ask you to write or review on a whiteboard/editor.)*

| Pattern ID | Embedded C Interview Pattern | Covered In | Key Bug Interviewers Watch For |
| --- | --- | --- | --- |
| `C01` | Overflow-checked frame/allocation size math | `W01-L04` (Done) | Checking `a > MAX - b` *before* `a + b` |
| `C02` | Multi-resource init & reverse-order unwind (`goto` vs. MISRA) | `W02-L21` (Done) | Releasing in exact reverse order; enabling `IRQ` producer last |
| `C03` | Producer/consumer buffer pool (`FREE -> FILLING -> READY -> IN_USE`) | `W03-L23` (Done) | Generation tag preventing double-free / stale pointer reuse |
| `C04` | Safe producer/consumer stop, `IRQ`/`DMA` quiesce & queue drain | `Day 2 (L27)` | Stopping hardware producer *before* draining or deleting queues |
| `C05` | Endian-safe unaligned register/packet parser & bitfields | `Day 2 (L27)` | Never casting unaligned `uint8_t *` to `uint32_t *` (strict aliasing + fault!) |
| `C06` | Async driver timeout, retry & late-`DMA`-callback state machine | `Day 8 (L33)` | Never reusing a `DMA` buffer on timeout until `DMA` hardware is disabled! |
| `C07` | Lock-free / ISR-safe SPSC Ring Buffer (`head`, `tail`, power-of-2 mask) | `Day 2 (L27)` | Distinguishing Full vs. Empty without data races; `Drop-Newest` vs. `Drop-Oldest` |
| `C11` | Wrap-safe unsigned tick & timestamp comparison | `Day 2 (L27)` | `(uint32_t)(now - start) >= timeout` works cleanly across `0xFFFFFFFF -> 0x00000000` |
| `C12` | Logical watchdog / task-progress alive + sequence monitor | `Day 4 (L29)` | Detecting a deadlocked worker task even when higher-priority tasks still run |

---

## 6. Career Entry And Truthful Interview Positioning

- **Target Launch:** Resume ready on **Day 9 (`2026-11-29`)**; ready to start attending interviews from **Day 11 (`2026-12-01`)** (or after any optional extra revision days you choose).
- **How To Connect Theory With Your Real Day-To-Day Work Honestly:**
  1. **Lead with your real production engineering work:** 10+ years C, implementing cyclic functions for motor-control/BMS IC drivers on 32-bit Infineon AURIX, AUTOSAR CDD/module integration, architecture/interface reviews, integration testing, mentoring, FMEA/HAZOP participation with safety engineers, Polyspace/MISRA compliance, and requirements traceability.
  2. **Translate across architectures fluently:** When an interviewer asks how your cyclic AUTOSAR/AURIX experience maps to an event-driven FreeRTOS/Arm Cortex-M SoC, walk straight through `P1`'s asynchronous state machine (`Task` vs. `ISR`, `BASEPRI`, `SPI`/`ADC` + `DMA`, `Queue_t` / `StreamBuffer` / `Mutex` Priority Inheritance / `Gatekeeper`, `D-Cache` coherency, and `WdgM`-style progress supervision).
  3. **Keep strict integrity:** Never claim confidential employer code, safety-case ownership you did not hold, or physical STM32 bench measurements you have not run; presenting `P1` as a clean reference architecture paired with genuine AURIX production experience is respected by Staff/Principal interviewers.
