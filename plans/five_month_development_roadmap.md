# Five-Month Embedded Development Curriculum

Rebaseline initiated 2026-10-07. **This is the sole authoritative weekly curriculum.** Start with [workspace readiness](../docs/workspace_setup.md). The [Linux-aligned teaching-kernel milestones](scratch_riscv_os_plan.md) specify project dependencies, not a second calendar. Record current status in the [training tracker](training_tracker.md) and dated evidence in the [learning log](learning_log.md). The former [eight-week plan](weeks_01_08_development.md) remains a navigation redirect.

## Goal And Starting Evidence

Transition from automotive Complex Device Driver (CDD) validation (current baseline ~36 LPA) to firmware and embedded software development, then deepen Linux kernel/driver implementation targeting **Senior 2 / Staff Systems Engineer** roles at semiconductor leaders (Qualcomm, NVIDIA, AMD, TI) in **Bengaluru** (primary) and **Hyderabad** (secondary). Target: **INR 70-90 LPA CTC**, not guaranteed salary, fixed pay, seniority or employment. Moving from 36 LPA to the 70-90 LPA band requires moving past basic peripheral templates to demonstrate mastery of high-throughput buses, DMA engines, concurrency, and cache coherency. The curriculum anchors this leap through 3 Flagship Capstone Artifacts (SPI with circular DMA in Zephyr, IIO SPI/I2C client driver in Linux, and streaming DMA with cache management). Compare base, annual bonus, annual equity vesting, employer contributions and one-off payments separately; a multi-year equity grant is not annual pay. The two-month notice period affects joining, not the learning sequence.

The learner reports 10+ years of C, intermediate C, basic/intermediate C++, intermediate Python and basic Rust/Linux shell familiarity. Prior source sampling showed parameterized tests, callback/error accounting, byte decoding and state-targeted fault injection. These are useful validation strengths, not proof of sole authorship or production driver ownership. Use them to design failure investigations while building original development artifacts. Do not copy employer protocols, code, test vectors or device details into these labs.

Weeks 1 and 2 are **completed by learner report**; source review, deferred runtime evidence and earlier defects remain in the log. The original scratch kernel's Days 1-8 were previously verified in QEMU, but this does not verify the new build. Do not repeat completed utilities or infer delayed retention from a completed checkbox.

Hardware reported: STM32F446RE with onboard ST-LINK, BeagleBone Black and logic analyser. Confirm NUCLEO-F446RE silkscreen/revision, probe access and safe connections before board-specific commands. ESP32 and Raspberry Pi Pico/Zero models are unknown and not prerequisites. Existing xv6 kernel/utility migration smoke checks passed; Zephyr west/modules/SDK and Linux guest/module readiness remain separate gates. Source clones already exist; no cloning restart is needed.

## Teaching And Evidence

For every question consult the [shared Feynman coach](../.github/skills/learning-feynman-coach/SKILL.md) and applicable domain skills. Teach **what -> why with a simple failure example -> conceptual solution -> how to implement/use**, then trace and recap. Explain headers, prototypes, errors and lifetime before the complete chat example. For the new `my-linux` kernel the learner creates every folder/file and writes all active code after comparing xv6 mechanisms with actual upstream Linux source. Do not relocate the old kernel, overwrite learner work or run it merely because they say "done". Review volunteered work; execute only when requested.

Quizzes remain paused. Private reconstruction for an imagined 12-year-old is optional: explain, identify gaps, revisit, simplify and transfer. Offer later recall without automatic reminders. Distinguish learner-reported completion, inspected source, observed builds/runs and measured hardware. A tutor example, clean sanitizer run or course certificate is not proof of independent mastery. Pending verification does not block conceptual teaching, but unsafe/unavailable hardware blocks that experiment.

## Budget And Calendar

From week 3, confirmed budget: **22 hours/week**, including breaks, weekdays 2 hours each plus 6 hours each weekend day. Weeks 1-2 retain their historical 16-hour allocations; these are not claimed actual study hours.

| Weekly category | Hours |
| --- | ---: |
| A: Concepts and focused official source reading | 4 |
| B: Implementation, debugging and project experiments | 10 |
| C: Applied C/C++ and coding reasoning | 2 |
| Career research, communication and applications | 2 |
| Optional private Feynman recap | 1 |
| Setup, recovery and breaks | 3 |
| Total | 22 |

Every week-3 onward table allocates A/B/C = 16 hours. The **same 6 support hours** above complete that week; external training time is inside these allocations, never additional. Suggested distribution: Mon A2, Tue B2, Wed A2, Thu B2, Fri C2; Sat B4 + career1 + buffer1; Sun B2 + career1 + recap1 + buffer2. Breaks may interrupt any block and consume buffer time. One main artifact per week; optional topics replace stretch work, not the recovery allowance.

Week 3 began provisionally 2026-09-28; its original dates and all later week dates below are **historical targets, not current deadlines**. The new Linux-aligned kernel foundation extends the xv6/RISC-V block until its essential boot, DTB, memory, trap and interrupt checkpoints are understood and evidenced. Shift subsequent Zephyr and Linux-driver weeks rather than compressing prerequisites or adding hours. The original weeks 3-20 allocation was **18 x 22 = 396 planned hours**, not a completed-hours claim or a binding end date; re-estimate remaining dates only after milestone evidence and actual time are known. Do not treat the original 2027-01-31 endpoint or February buffer as guaranteed.

The original firmware interview checkpoint (2026-11-16 through 2026-11-22) is now subject to rebaseline; begin suitable conversations/applications as evidence supports them, without promising Linux-driver readiness by a date. Expand driver-focused interviews only as later artifacts support them. If two consecutive deliverables slip, use recovery time and drop stretch work before moving dependent labs. Do not silently compress prerequisites or increase hours.

## Sources

Official provider/project pages consulted on **2026-09-23**. Study the listed sections, not entire courses unless explicitly scheduled. These are self-study selections/adaptations, not enrollment, certification or claims that provider lab hardware is interchangeable. Free materials are core; paid training is optional.

| Source | Selected modules and use |
| --- | --- |
| [MIT 6.1810, 2025 schedule](https://pdos.csail.mit.edu/6.1810/2025/schedule.html) and [official xv6 book](https://mit-pdos.github.io/xv6-riscv-book/) | Weeks 3-5: organization, page tables, traps, drivers, locking, switching, coordination, filesystems/logging. The 2026 schedule was under construction at research time. Match book/source editions; current fork is not automatically a MIT lab branch with graders. |
| [RISC-V specifications](https://docs.riscv.org/) | Week 3 privilege, registers and calling-convention context; week 4 address translation/traps. Use implementation-matched sections, not the entire specification. |
| [Zephyr getting started](https://docs.zephyrproject.org/latest/develop/getting_started/index.html), [kernel services](https://docs.zephyrproject.org/latest/kernel/services/index.html), [device model](https://docs.zephyrproject.org/latest/kernel/drivers/index.html) | Weeks 6-9: west, SDK, Kconfig, devicetree, threads, synchronization, queues, timing, interrupts and driver initialization. Pin release/module/SDK versions and switch documentation to that version before commands. |
| [Zephyr Nucleo F446RE](https://docs.zephyrproject.org/latest/boards/st/nucleo_f446re/doc/index.html), [ST board information](https://www.st.com/en/evaluation-tools/nucleo-f446re.html), [Arm documentation](https://developer.arm.com/documentation/) | Week 6 Cortex-M4 reset/exception and board debugging; week 9 peripherals. Confirm board identity and use its reference manual/errata. |
| [Nordic nRF Connect SDK Fundamentals](https://academy.nordicsemi.com/courses/nrf-connect-sdk-fundamentals/) | Optional weeks 6-8 supplement: devicetree, Kconfig, GPIO/UART/I2C and RTOS execution/synchronization. Nordic hardware/SDK labs are not required on STM32; no Nordic purchase or certificate required. |
| [Bootlin Embedded Linux](https://bootlin.com/training/embedded-linux/) | Weeks 10-11: toolchain, boot/U-Boot, kernel configuration/build, rootfs, filesystems, build system and userspace debugging. Free slides and BeagleBone Black labs are linked by the provider. Linux shell prerequisites repaired in context. |
| [Bootlin kernel and driver development](https://bootlin.com/training/kernel/) | Weeks 12-17: kernel architecture, module/device model, hardware/user interfaces, memory, synchronization, IRQs and sleeping. Requires strong C and embedded Linux fundamentals; therefore follows weeks 10-11. Select/adapt labs to available hardware. |
| [Linux driver API](https://docs.kernel.org/driver-api/index.html), [kernel build](https://docs.kernel.org/kbuild/), [development tools](https://docs.kernel.org/dev-tools/index.html) | Weeks 11-19: implementation-matched APIs, I2C/SPI/IIO, user access, DMA, power and testing. Documentation supplements training; it is not an ordered course itself. |
| [Bootlin debugging, tracing and performance](https://bootlin.com/training/debugging/) | Weeks 10 and 18: GDB/ELF, strace, crash investigation, ftrace/perf, memory and locking diagnostics. Current provider labs target STM32MP1/BeaglePlay; adapt tools to guest/BBB, not claim those original labs ran on F446. |
| [Linux IIO & Regmap SPI/I2C drivers](../../linux/drivers/iio/imu/inv_mpu6050/inv_mpu_spi.c) | Weeks 14 and 16: official in-tree reference for unified regmap transport, threaded IRQ, hardware FIFO, and IIO triggered buffers. |
| [Linux DMA cache management](../../linux/drivers/iio/adc/nxp-sar-adc.c) | Weeks 17 and 18: official in-tree reference for streaming DMA cache sync (`dma_sync_single_for_device/cpu`) and L1 cache line alignment (`IIO_DMA_MINALIGN`). |
| [Zephyr STM32 SPI with DMA](../../zephyr-workspace/zephyr/drivers/spi/spi_stm32.c) | Weeks 8 and 9: official in-tree reference for STM32 circular DMA (`dma_stm32`), devicetree bindings, and asynchronous SPI transfers. |
| [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines), [SEI CERT C](https://wiki.sei.cmu.edu/confluence/display/c) | Weekly C/C++ slice: ownership, safe integer/pointer contracts, error handling and concurrency. Linux kernel code remains C; C++ belongs in host clients or suitable firmware. |

## Flagship Capstone Artifacts (70-90 LPA Semiconductor Target)

To command Senior 2 / Staff Systems Engineer compensation (**INR 70-90 LPA CTC**) at Qualcomm, NVIDIA, AMD, and TI, candidate evaluation hinges on demonstrated mastery over high-throughput peripheral buses, DMA engines, concurrency, and cache coherency rather than basic driver templates. The curriculum centers on the industry's #1 most in-demand peripheral and protocol combination: **High-Speed SPI and I2C with Direct Memory Access (DMA), Regmap, Industrial I/O (IIO), and Cache Coherency Management**.

| Flagship Artifact | Domain & Week | Technical Architecture & Subsystems | Production Problem Solved & Interview Talking Point | Reference Source |
| --- | --- | --- | --- | --- |
| **Artifact 1: High-Speed SPI Sensor Telemetry Pipeline with Circular DMA** | Zephyr RTOS & MCU (Weeks 6-9) | Double-buffered SPI acquisition, STM32 circular DMA (`STM32_DMA_MODE_CYCLIC`, `dma_stm32`), Devicetree bindings, and zero-copy lockless ring buffer (`sys_ring_buf`). | Eliminates dropped sensor frames under high-frequency interrupts and prevents priority inversion across telemetry threads. | [drivers/spi/spi_stm32.c](../../zephyr-workspace/zephyr/drivers/spi/spi_stm32.c) |
| **Artifact 2: Industrial I/O (IIO) High-Speed SPI/I2C Client Driver** | Linux Kernel Drivers (Weeks 12-16) | Unified `regmap` transport abstraction (`devm_regmap_init_spi`), hardware FIFO polling/watermark, threaded interrupt handling (`request_threaded_irq`), Devicetree YAML bindings, and Runtime PM. | Separates hard-IRQ latency budgets (top-half ~10µs) from slow bus FIFO reading (bottom-half ~100µs), enabling transport agility across SPI and I2C. | [drivers/iio/imu/inv_mpu6050/inv_mpu_spi.c](../../linux/drivers/iio/imu/inv_mpu6050/inv_mpu_spi.c), [inv_mpu_core.c](../../linux/drivers/iio/imu/inv_mpu6050/inv_mpu_core.c) |
| **Artifact 3: Streaming DMA Driver with Cache Management & Latency Profiling** | Linux Kernel DMA & Tracing (Weeks 17-18) | DMAEngine API (`dmaengine_prep_slave_sg`), explicit cache synchronization (`dma_sync_single_for_device/cpu`), 64-byte L1 cache alignment (`IIO_DMA_MINALIGN`), and latency triage via `ftrace` and `lockdep`. | Prevents silent data corruption from stale CPU cache lines across Point of Coherency (PoC) vs Point of Unification (PoU) under heavy concurrent memory traffic. | [drivers/iio/adc/nxp-sar-adc.c](../../linux/drivers/iio/adc/nxp-sar-adc.c) |

### Simulation-First Execution Policy

To guarantee steady learning momentum without blocking on physical setup:
1. **Weeknight Lab Execution (Simulation First)**: All drivers and firmware pipelines are developed and verified first in simulation:
   - RISC-V and xv6 in QEMU via Podman.
   - Zephyr applications on `native_sim` or `qemu_cortex_m3` / `qemu_cortex_m4`.
   - Linux kernel drivers in a dedicated QEMU ARM/ARM64 virtual guest (protecting the WSL2 host).
2. **Dedicated Weekend Hardware Lab Blocks**: Once simulation logic and data contracts pass, code is flashed to the physical **STM32 NUCLEO-F446RE** and **BeagleBone Black** to capture physical traces (Logic Analyzer, Saleae, serial oscilloscope traces) for interview portfolio documentation. Hardware issues never block conceptual progress.

## Week 1

Workspace and safe C contracts. **Status: Completed, learner-reported on 2026-09-23.** Preserve historical scope; do not restart it. Original allocation was 16 hours, including setup. Source review and deferred checks remain separate in the tracker.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W01-A | 6 | None | Sessions 1-3: tools/repos, preserve work, public/private boundaries, forks/workspace, C17/C++17 probes and xv6 readiness | Setup record; previously observed toolchain and xv6 smoke results |
| W01-B | 4 | W01-A | Sessions 4-5: realloc success/failure including same-address lifetime, saved index, range/size guards, promotions and two-byte decoding | Ownership and input/output contract; worked traces |
| W01-C | 3 | W01-B | Weekend A: decoder integration, boundary tests and simulated allocation failure | Small decoder/helpers and real test results when supplied |
| W01-D | 3 | W01-B | Weekend B: private recap 1, role/evidence mapping 1, carry-over 1 | Optional recap, role notes and unresolved evidence recorded |

Contract retained: sized input, validate before access, unchanged output on rejection, explicit null/zero/overlap policies, overflow-safe subtraction/division guards, preserve allocation on nonzero-size realloc failure. Core is one decoder; fuzzing and additional formats are stretch. The learner deferred standalone verification; reuse those checks inside later real paths. A reported week completion does not erase the recorded unchecked-malloc finding or establish runtime correctness.

## Week 2

Completion, timeouts and ownership. **Status: Completed, learner-reported on 2026-09-27.** Retain the original 16-hour coverage allocation and completed work; actual elapsed hours are unknown and source review does not establish runtime verification. Sources: C/POSIX interfaces for the selected host and C++ Core Guidelines concurrency/ownership sections. Do not substitute host atomics for kernel or MCU rules.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W02-A | 2 | W01-B | Thread lifecycle, data races, one-time release/acquire publication | Source-reviewed memory-ordering exercise; expected vs observed results separate |
| W02-B | 2 | W02-A | Mutex and condition predicate loops; protection of later updates | Mutex source-reviewed; condition wait demonstration and implementation pending |
| W02-C | 2 | W02-B | READY/TIMEOUT/ERROR; monotonic deadline and explicit request ownership | State diagram and bounded wait outcome tests |
| W02-D | 2 | W02-C | Missing, late and duplicate callbacks; cancel or drain before shutdown | No expired operation writes freed storage; teardown tests |
| W02-E | 2 | W01-B | C++ RAII/unique ownership 1; worked code-review/interview case 1 | Early-return resource cleanup comparison |
| W02-F | 3 | W02-D, W02-E | Integration 1, private recap 1, official role research 1 | Test evidence and role requirements mapped to gaps |
| W02-G | 3 | None | Recovery and setup buffer | Carry over unfinished core before optional tools |

Contract retained: exactly one terminal outcome per request, predicate rechecked after wake-up, explicit timeout, initialized shared state, no later unsynchronized payload writes, and no cleanup while callbacks can access freed storage. No huge real allocations to simulate failure. Lock-free queues and ThreadSanitizer are optional, tool-dependent extensions. Assessment remains paused.

## Week 3

**Rebaseline note:** W03's xv6 source tracing and later W04-W05 OS concepts remain foundations, but the learner now extends these blocks through the [Linux-aligned teaching-kernel milestones](scratch_riscv_os_plan.md) before moving to Zephyr. The former 35-day/day-9 timer schedule is retired. Start by comparing xv6 M-mode boot to the OpenSBI S-mode entry used by the new project, then proceed through DTB, memory, traps and interrupts at an evidence-based pace. The learner builds `my-linux` from new active files; old Day 1-8 QEMU evidence remains historical. Do not premark future modules completed or infer calendar dates from the old weekly headings.

**2026-09-28 to 2026-10-04. xv6 organization and RISC-V execution.** Build on C ownership and thread concepts, not beginner utilities. Source: MIT OS design/organization and xv6 boot chapters; relevant RISC-V register/privilege definitions. Gate: working xv6/QEMU path from setup; inspect current revision and available targets first.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W03-A | 4 | W02-A | Process/address space, user/kernel, registers, RV64 loads/stores/call-return, ABI and privilege; refresh prior boot locally | Annotated reset-to-first-user map with privilege/stack state |
| W03-B | 10 | W03-A | GDB breakpoints, ELF/disassembly; trace entry/start/main and one syscall boundary in actual source | Reproducible debug session and one explained control-flow note |
| W03-C | 2 | W01-B | Bit masks, endian representation, pointers in disassembly | One small source-to-instruction explanation |

Plus 6 support hours from the weekly template. Core: one accurate trace. Stretch: additional assembly paths. Fallback: use the verified Podman/QEMU workflow; do not rebuild all old utilities as new assignments. Gate to deeper work: distinguish a C function, assembly wrapper and architectural instruction.

## Week 4

**2026-10-05 to 2026-10-11. Virtual memory, traps and syscalls.** Source: MIT page tables, syscall entry/exit, page faults and selected COW discussion. Memory mappings precede the detailed trampoline trace. Gate: W03 control flow; use current checkout definitions, not stale notes.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W04-A | 4 | W03-B | Sv39 VPN/PTE/permissions, page allocation, walk/mappages; trap entry/return and syscall dispatch | One worked translation and user-to-kernel-to-user diagram |
| W04-B | 10 | W04-A | Follow allocator/mapping/error paths; implement one small page-table diagnostic OR syscall trace and run relevant checks | Patch, revision, build command, expected/observed trace |
| W04-C | 2 | W03-C | Pointer validity versus numeric address; non-owning views and invalidation | Ownership note linked to a page-lifetime example |

Plus 6 support hours. Core: one diagnostic, not two complete course labs. Explain page-fault/lazy/COW concepts; full COW and mmap implementations are stretch. Never assume the public fork supports MIT graders. If a patch blocks, retain the correct trace and repair within buffer.

## Week 5

**2026-10-12 to 2026-10-18. Scheduling, interrupts and coordination.** Source: MIT device drivers, locking, thread switching, coordination, filesystem and crash-recovery lectures. Gate: W04 trap/address-space map. This is a focused OS foundation, not completion of MIT's semester.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W05-A | 4 | W04-B, W02-B | UART/interrupts, spinlocks, scheduler/swtch, trap frame versus context, sleep/wakeup; descriptor/inode/cache/log overview | Blocking-read-to-wakeup state diagram and persistence overview |
| W05-B | 10 | W05-A | Trace a pipe or UART wait through scheduler; one controlled diagnostic and regression | Actual source paths, state transitions and explained failure/repair |
| W05-C | 2 | W04-C | Queue/list invariants and complexity; critical-section lifetime | Bounded queue reasoning connected to the source path |

Plus 6 support hours. Verify Sstc versus older timer paths from the checkout. Core: one wait/wakeup path; filesystem rewrite, network driver lab and scheduler redesign are deferred. QEMU suffices for this software trace, not real device timing.

## Week 6

**2026-10-19 to 2026-10-25. Cortex-M and Zephyr entry.** Source: Arm Cortex-M4 documentation, ST/Zephyr Nucleo F446RE guide, Zephyr getting started/application configuration. Gate: board identity before flashing; pin Zephyr release, west modules and SDK. Nordic fundamentals configuration lessons are optional concepts only.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W06-A | 4 | W05-A | RV versus Cortex-M execution, reset/vector/NVIC, Thread/Handler modes, MSP/PSP, startup/linker and MMIO | Reset-to-main/exception diagram with architecture distinctions |
| W06-B | 10 | W06-A | Configure west/Kconfig/devicetree; build/debug first app; GPIO/UART polling to one interrupt path where supported | Version-pinned build and debugger or simulator trace |
| W06-C | 2 | W05-C | Register bit operations, access side effects, volatile versus synchronization | One documented register-access contract, no guessed addresses |

Plus 6 support hours. After one 2-hour hardware setup block without progress, switch to supported QEMU/native_sim and track hardware pending. native_sim needs a Linux environment and is not F446 peripheral emulation. No AArch64 exception-level terminology for Cortex-M. Core: boot/configuration and execution context, not a new BSP port.

## Week 7

**2026-10-26 to 2026-11-01. Zephyr threads and bounded data passing.** Source: Zephyr kernel services: threads, scheduling, mutexes, semaphores, message queues and memory slabs. Gate: a buildable Zephyr target from W06; compare semantics with xv6/POSIX without copying incompatible APIs.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W07-A | 4 | W06-B, W02-B | Priorities/preemption, synchronization choices, priority inversion, stacks and buffer ownership | Execution-context and ownership table |
| W07-B | 10 | W07-A | Queue-based acquisition; define full/empty/drop policy; inject backlog and record recovery | Working bounded queue demo and overrun evidence |
| W07-C | 2 | W02-E, W06-C | RAII, move-only ownership and resource lifetime comparison | Host-side ownership example, not C++ kernel code |

Plus 6 support hours. Core: one queue and one policy. Fallback: synthetic samples on a supported simulator. Stretch: priority-inheritance experiment after basic scheduling is understood; do not build a lock-free queue.

## Week 8

**2026-11-02 to 2026-11-08. Interrupt-to-worker pipeline and DMA buffer contracts.** Source: Zephyr interrupts, workqueue threads, timing, device model, and [Zephyr STM32 SPI with DMA](../../zephyr-workspace/zephyr/drivers/spi/spi_stm32.c). Gate: W07 bounded queue and W02 cancellation contract. Select one public device/datasheet (e.g. SPI IMU / ADC) for later bus work.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W08-A | 4 | W07-B, W02-D | ISR restrictions, deferred work, timers, driver config/data/API, DMA descriptor setup and initialization readiness | Event-to-consumer source map and DMA buffer lifecycle |
| W08-B | 10 | W08-A | Integrate GPIO/UART/SPI event, queue and worker; circular DMA double-buffer setup; timeout/stop under load; inspect stack/config faults | Acquisition demo with shutdown and failure matrix |
| W08-C | 2 | W07-C | Callback context and borrowed-buffer lifetime | Late-callback counterexample and repaired contract |

Plus 6 support hours. Core: one event path. Simulated events in `native_sim` / QEMU establish software behavior first; actual IRQ/electrical timing tested on physical board during weekend lab block.

## Week 9

**2026-11-09 to 2026-11-15. Flagship Capstone 1: High-Speed SPI Sensor Telemetry Pipeline with Circular DMA.** Source: Zephyr device model, [Zephyr STM32 SPI driver](../../zephyr-workspace/zephyr/drivers/spi/spi_stm32.c), STM32F446 reference manual, and SPI device datasheet. Gate: supported board/device/voltage/pins or explicitly labelled simulation model.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W09-A | 4 | W08-B | SPI vs I2C bus transactions, clock polarity/phase (CPOL/CPHA), STM32 DMA streams/channels, ring buffers, and peripheral role mapping | One bus contract and peripheral-role map |
| W09-B | 10 | W09-A | Implement Flagship Capstone 1: High-speed SPI sensor telemetry pipeline with STM32 circular DMA (`STM32_DMA_MODE_CYCLIC`, `dma_stm32`), Devicetree overlay, double-buffering, and zero-copy lockless ring buffer (`sys_ring_buf`); timeout/recovery; ztest/Twister test | Working telemetry pipeline demo, reproducible build, and overrun/recovery test results |
| W09-C | 2 | W08-C | Alignment, representation, integer promotions, CRC/parity and byte decoder | Boundary tests linked to the actual sensor packet path |

Plus 6 support hours. Core: Flagship Capstone 1. Simulation First: develop and verify logic on `native_sim` / QEMU; flash to physical NUCLEO-F446RE with logic analyzer during weekend lab block. Core interview story: eliminating dropped sensor frames under interrupt load.

## Week 10

**2026-11-16 to 2026-11-22. Embedded Linux foundations and firmware checkpoint.** Source: Bootlin Embedded Linux architecture/toolchain sections and Debugging GDB/ELF sections. Gate: xv6 foundations; BBB serial/recovery setup or disposable QEMU Linux guest. CDD debugging strengths support this transition, not a claim of existing Linux ownership.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W10-A | 4 | W05-B, W09-B | Linux processes/files/syscalls, shell tools, cross-compilation; Cortex-A8 versus M4, MMU/cache and AArch32/AArch64 distinctions | User/kernel and architecture comparison grounded in a boot |
| W10-B | 10 | W10-A | Boot BBB or guest; cross-build/run/debug one user program; inspect ELF and syscall trace | Boot record, compiler/architecture and GDB/strace evidence |
| W10-C | 2 | W09-C | File-descriptor RAII and error paths | Host/client resource cleanup example |

Plus 6 support hours, including firmware readiness review/CV/referrals within career time. Suitable November interviews need a demonstrable firmware artifact and honest limits, not all later kernel skills. Missing hardware routes to QEMU; BBB is Cortex-A8, not AArch64. Defer specialization if basic build/debug is unresolved.

## Week 11

**2026-11-23 to 2026-11-29. Bootable Linux system.** Source: Bootlin Embedded Linux bootloader, kernel configuration, rootfs, filesystem and build-system sections; Linux kbuild. Gate: W10 toolchain/boot understanding and sufficient guest/build resources.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W11-A | 4 | W10-B | BootROM/SPL/U-Boot/kernel/DTB/rootfs/init, modules versus built-ins, kernel configuration | Boot-chain map distinguishing actual from conceptual stages |
| W11-B | 10 | W11-A | One minimal Buildroot or equivalent pinned image; record kernel/config/rootfs and matching module build environment | Reproducible image boot and userspace program run |
| W11-C | 2 | W10-C | Build/link stages, libraries and symbol inspection | Explain an actual link/configuration failure |

Plus 6 support hours. Core: one reproducible guest or board image. QEMU direct-kernel boot does not demonstrate U-Boot execution; label that stage conceptual unless actually run. Full Yocto and multiple distributions are deferred. Never load experimental modules into the Podman host kernel.

## Week 12

**2026-11-30 to 2026-12-06. Kernel lifecycle and interfaces.** Source: Bootlin kernel architecture/modules/user interfaces/memory; matching Linux driver basics. Gate: disposable guest or spare BBB with matching running kernel, headers/config, toolchain and recovery.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W12-A | 4 | W11-B | Module versus driver, process/interrupt context, init/exit, allocation and user-copy rules | Resource acquisition/unwind diagram |
| W12-B | 10 | W12-A | Minimal module lifecycle plus narrow misc/character interface drill; bounded read/write and failed-init cleanup | Build/load/unload and error-path logs inside guest/board |
| W12-C | 2 | W11-C, W01-B | User/kernel buffer contracts and partial-copy errors | Boundary and ownership cases tied to the drill |

Plus 6 support hours. The character interface is a teaching drill, not a commitment to a custom capstone ABI beside IIO/hwmon. Core: lifecycle and one bounded interface. If guest readiness fails, keep source study progressing but runtime gate pending.

## Week 13

**2026-12-07 to 2026-12-13. Device model and hardware resources.** Source: Bootlin device tree/model/hardware/interrupt sections; Linux driver model, device I/O and IRQ documentation. Gate: W12 module lifecycle and memory-context reasoning.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W13-A | 4 | W12-B | Platform/bus matching, bindings, probe/remove, managed resources, MMIO, clocks/regulators/pinctrl, threaded IRQs | DT-to-driver-to-resource source trace |
| W13-B | 10 | W13-A | Modify one small supported platform path; inject a resource acquisition failure and verify unwind | One resource/IRQ lifecycle experiment with actual context |
| W13-C | 2 | W12-C | Ownership tables and error unwind | Review a multi-resource failure path |

Plus 6 support hours. Use an existing supported guest device or labelled synthetic platform device if hardware blocks. Do not invent MMIO addresses, IRQs or peripheral emulation. Core: one trace and failure, not complete board support.

## Week 14

**2026-12-14 to 2026-12-20. Flagship Capstone 2: Linux Industrial I/O (IIO) High-Speed SPI/I2C Client Driver.** Source: Bootlin bus/subsystem sections, Linux IIO subsystem documentation, and [Linux IIO & Regmap SPI/I2C drivers](../../linux/drivers/iio/imu/inv_mpu6050/inv_mpu_spi.c). Gate: documented available peripheral, QEMU ARM Linux guest, or supported stub.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W14-A | 4 | W13-B, W09-B | Controller versus client, `regmap` configuration (`regmap_config`), IIO core (`iio_dev`, `iio_info`), hardware FIFO watermark, and threaded IRQ (`request_threaded_irq`) split | Subsystem design note and top-half/bottom-half latency budget |
| W14-B | 10 | W14-A | Implement Flagship Capstone 2: Probe/remove lifecycle, devm managed allocations (`devm_regmap_init_spi`), reading channel raw data, and handling transport errors; verify in QEMU ARM guest first, BeagleBone Black during weekend lab | Driver source, reproducible build, and raw reading/error evidence |
| W14-C | 2 | W13-C | C++ containers/non-owning view invalidation in host client | Client lifetime example and userspace sysfs consumer |

Plus 6 support hours. Core: Flagship Capstone 2. Simulation First: develop and test inside disposable QEMU ARM Linux guest; flash/wire to BeagleBone Black or external SPI/I2C IMU during weekend lab. Core interview story: regmap transport abstraction and hard-IRQ latency elimination.

## Week 15

**2026-12-21 to 2026-12-27. Concurrent access and teardown.** Source: Bootlin locking/sleeping; Linux locking, workqueues, wait queues/poll and lifetime documentation. Gate: one working or explicitly simulated driver interface.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W15-A | 4 | W14-B, W02-D | Mutex/spinlock/context, waits/poll, workqueues, references, cancellation and kernel ordering distinctions | Lock/context/lifetime table |
| W15-B | 10 | W15-A | Missing/late/duplicate completion, concurrent reads and removal; stop producers before release; prevent use-after-free under sysfs access | Teardown stress/fault results and no-after-free rationale |
| W15-C | 2 | W14-C | Shutdown and callback ownership in host client | One safe cancellation/error sequence |

Plus 6 support hours. Managed allocation alone does not stop callbacks or protect open-file lifetime. Teach poll/user-copy on the prior drill if the chosen subsystem already owns those interfaces. Core: safe teardown, not a new subsystem or lock-free redesign.

## Week 16

**2026-12-28 to 2027-01-03. Flagship Capstone 2 Extension: Triggered Buffers and Runtime PM.** Source: Linux IIO triggered buffer guides and runtime/system power-management documentation. Gate: W15 lifecycle contract. January role review can now include driver evidence, not just coursework.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W16-A | 4 | W15-B | IIO triggered buffer architecture (`iio_triggered_buffer_setup`), trigger handlers, and Runtime PM lifecycle (`pm_runtime_enable`, `pm_runtime_get_sync`, `pm_runtime_put_autosuspend`) | State diagram covering active/suspended/removing and power domains |
| W16-B | 10 | W16-A | Extend Capstone 2 with triggered continuous sampling and autosuspend power management; test resume/failure recovery under QEMU and BeagleBone Black | Regression results, continuous acquisition logs, and standard sysfs demo |
| W16-C | 2 | W15-C | Host interfaces and templates only where useful; RAII client | Small maintainable client without kernel C++ |

Plus 6 support hours. Core: Capstone 2 continuous buffering and PM extension. Simulation First: QEMU guest testing on weeknights; hardware power verification during weekend lab.

## Week 17

**2027-01-04 to 2027-01-10. Flagship Capstone 3: Streaming DMA Driver with Cache Management.** Source: Linux DMA mapping/DMAengine guides, [Linux DMA cache management](../../linux/drivers/iio/adc/nxp-sar-adc.c), and ARM/RISC-V architecture manuals. Gate: IRQ, buffer lifetime, teardown and address-space concepts already covered.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W17-A | 4 | W16-B, W04-A | CPU/device/bus addresses, coherent (`dma_alloc_coherent`) vs streaming (`dma_map_single`) DMA, CPU cache maintenance (PoC vs PoU), and 64-byte L1 cache-line alignment (`IIO_DMA_MINALIGN`) | CPU-to-device ownership timeline and cache maintenance flow |
| W17-B | 10 | W17-A | Implement Flagship Capstone 3: Streaming DMA buffer setup with `dma_sync_single_for_device` before transfer and `dma_sync_single_for_cpu` on completion interrupt; scatter-gather transfers and safe shutdown | Working streaming DMA driver, reproducible build, and cache sync logs |
| W17-C | 2 | W16-C | Alignment, length and lifetime contracts for DMA buffers, memory barriers (`smp_mb`, `dma_rmb`, `dma_wmb`) | Safe descriptor/buffer contract with boundary cases |

Plus 6 support hours. Core: Flagship Capstone 3. Simulation First: QEMU ARM/ARM64 guest with DMA engine emulation; BeagleBone Black physical DMA verification during weekend lab. Core interview story: preventing silent data corruption by mastering cache lines and DMA synchronization.

## Week 18

**2027-01-11 to 2027-01-17. Flagship Capstone 3 Profiling, Concurrency Tracing & Latency Triage.** Source: Bootlin Debugging ELF/GDB, syscall tracing, system tracing, memory/locking diagnostics; Linux dev-tools and selected RCU introduction. Gate: reproducible working artifact and known failure/measurement question.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W18-A | 4 | W17-B | Crash analysis, latency versus throughput, ftrace (`function_graph`, `irqsoff`), perf, lockdep verification, and RCU lifetime introduction | One falsifiable performance/debug hypothesis |
| W18-B | 10 | W18-A | Run synthetic stress benchmarks on Capstone 3 DMA transfers; capture latency profiles with `ftrace`, verify lock freedom with `lockdep`, and document before/after optimization | Raw non-sensitive trace, lockdep report, and before/after comparison |
| W18-C | 2 | W17-C | Optimized-code debugging, effective type, aliasing and restrict in a concrete case | Explain one compiler-sensitive defect without relying on UB output |

Plus 6 support hours. Guest timing is not target worst-case timing. RCU is conceptual/source-level, not a new implementation task. Core interview story: diagnosing interrupt latency spikes and proving deadlock-freedom with lockdep.

## Week 19

**2027-01-18 to 2027-01-24. Acquisition-Pipeline Hardening & End-to-End Integration.** Source: the selected Zephyr/Linux subsystems and testing guides. Gate: earlier adapters and failure contracts. Return deferred safe-C checks to the real input path rather than reopening beginner exercises.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W19-A | 4 | W18-B | Threat/input boundaries, memory protection/secure-boot concepts, recovery and reproducibility | Requirements-to-failure matrix with stated security limits |
| W19-B | 10 | W19-A | Integrate end-to-end data pipeline contract connecting Zephyr SPI DMA telemetry frontend (Capstone 1) with Linux IIO/DMA driver receiver (Capstones 2 & 3); replay fault injection (under-voltage, packet loss, CRC errors, buffer overrun) | Reproducible end-to-end demo with error, timeout, and teardown evidence |
| W19-C | 2 | W18-C, W01-C | Testability, parser guards and deterministic failure injection | Contract tests linked to actual project paths |

Plus 6 support hours. One unified project theme connecting MCU acquisition with Linux gateway processing. Simulation in QEMU / `native_sim` on weeknights; physical hardware linkup during weekend lab.

## Week 20

**2027-01-25 to 2027-01-31. Portfolio, Staff Interview Architecture & Compensation Consolidation.** Source: selected official job descriptions below and project documentation. Gate: describe original versus reused work and measured versus simulated behavior accurately.

| ID | Hours | Prerequisites | Training and work | Acceptance artifact |
| --- | ---: | --- | --- | --- |
| W20-A | 4 | W19-B | Design tradeoffs, limitations; role map for PCIe/USB/network/storage and next specialization | One chosen next-depth area tied to an actual role |
| W20-B | 10 | W20-A | Package Capstones 1, 2, and 3 into Staff Systems Engineer architecture walkthroughs; prepare 3 deep interview technical stories (DMA cache sync, threaded IRQ latency, lockless ring buffers); CV & GitHub documentation | Professional demo guide, test report, and 3 Staff-level architectural deep-dive decks |
| W20-C | 2 | W19-C | Worked C/C++ coding/design interview cases | Reasoning and complexity explanation; mocks only if requested |

Plus 6 support hours. Two optional mocks fit existing career/practice time only when assessment resumes. Optional small upstream-quality patch, not acceptance as a graduation gate. Continue applications and maintenance after January; target ₹70-90 LPA Senior 2 / Staff Engineer compensation bands.

## Role Fit And Career Work

Official descriptions retrieved 2026-09-23; vacancies and requirements can change. These are samples, not a market-wide survey. Preserve detailed application/offer records privately. Build a record of requirement -> evidence -> gap, not a list of brand names.

| Official role | Observed requirements | Curriculum fit and caution |
| --- | --- | --- |
| [AMD 91919, Firmware Developer RTOS/Bare-metal, Bengaluru](https://careers.amd.com/jobs/91919?lang=en-us) | C/C++, RTOS/bare-metal, RTOS/Linux drivers, ARM/x86, I2C/SPI/UART/PCIe, memory/performance/power | Direct firmware direction; W06-W19 builds evidence. PCIe implementation remains later specialization. |
| [Qualcomm 3095332, Senior Embedded Software and Driver Development, Hyderabad](https://careers.qualcomm.com/careers/job/446720516557) | Role text asks 5+ years embedded development, low-level drivers, C/C++, MMU/cache, clocks, IRQ/DMA, buses and debug tools; Linux/RTOS preferred | W03-W18 maps well; validation experience is transferable but not identical to development years. Generic minimum-qualification block differs from role-specific expectations. |
| [AMD 84900, Power Management Firmware, Bengaluru](https://careers.amd.com/jobs/84900?lang=en-us) | 9-15 years hands-on firmware, BIOS/ACPI/UEFI, PCIe, power and delivery leadership | Stretch specialization, not a promised immediate match from years of C validation. |
| [Qualcomm 3090149, ARM Server Systems, Bengaluru](https://careers.qualcomm.com/careers/job/446719135898) | Senior staff/principal architecture, 12+ years specific system/firmware/SoC breadth and leadership | Long-term example of depth; not the foundation program's entry target. |
| [NVIDIA JR2026342, Senior System Software, Pune/Bengaluru](https://nvidia.wd5.myworkdayjobs.com/en-US/NVIDIAExternalCareerSite/job/India-Pune/Senior-System-Software-Engineer_JR2026342) | Power/performance, Windows-on-ARM telemetry, SoC knowledge, scripting and automation | Adjacent systems/performance work, not evidence of a Zephyr-driver requirement. Do not redirect the learner into validation-only work. |

Career progression inside weekly hours: W03-W05 map strengths and draft debug/ownership stories; W06-W09 select fresh Bengaluru/Hyderabad roles and seek referrals; W10 review firmware readiness and start matched interviews; W11-W15 use feedback without abandoning prerequisites; W16-W20 add Linux-driver evidence and applications. Salary bands were not published in the retrieved descriptions. Job titles alone do not establish level or pay.

## Environment And Fallback Gates

| Gate | Needed for | If unavailable (Simulation-First Policy) |
| --- | --- | --- |
| Actual xv6 revision, compiler and QEMU targets | W03-W05 executable source study | Reuse verified container; no invented course graders or replacement of learner edits |
| Confirmed STM32 board identity, pin/voltage/probe and matched Zephyr release/modules/SDK | W06-W09 Flagship Capstone 1 (SPI DMA) | Weeknights: build & run on `native_sim` / `qemu_cortex_m3`. Weekends: flash NUCLEO-F446RE with logic analyzer probe |
| Documented public peripheral and supported bus | W09/W14 Flagship Capstones 1 & 2 (SPI/I2C) | QEMU simulated bus / software stub driver; wire physical IMU/ADC during weekend lab |
| Disposable Linux guest or spare BBB with matching kernel/config/headers and recovery | W11-W18 Flagship Capstones 2 & 3 (IIO & DMA) | Build & test in isolated QEMU ARM/ARM64 virtual guest (protects host WSL2); deploy to BeagleBone Black during weekend lab |
| Supported DMA, tracing and PM path | W16-W18 Capstone 3 cache sync & ftrace | QEMU DMA model & ftrace trace; physical BBB DMA measurement during weekend lab |

Use [setup records](../docs/workspace_setup.md) for cloned identities and prior builds. Check guest resources before heavy builds: the previously inspected Podman VM had about 2 GiB RAM despite larger host RAM. No automatic installations, firmware flashes, privileged commands, purchases, commits or pushes are authorized by this curriculum. Keep reference versions with each artifact and avoid unrelated board/platform setup.

## Scope Reconciliation

The earlier eight-week document scheduled Zephyr in W04, Linux in W05-W07 and interview launch in W08, while the roadmap held extra W09-W20 topics without detailed modules. The sequence above replaces that competition. Historical weeks 1-2 and progress remain; old future schedules are superseded, not marked completed.

| Earlier topic | New home |
| --- | --- |
| xv6 syscall/trap source study | W03-W05 with explicit VM and scheduling prerequisites |
| Zephyr configuration, Cortex-M exceptions and hardening | W06, W08, W09 (Flagship Capstone 1: SPI DMA pipeline) |
| Scheduling, queues, semaphores, mutexes, priority inversion | W07, building on W02 and W05 |
| ISR/deferred work and DMA progression | W08 then W17 (Flagship Capstone 3: Streaming DMA) |
| Timer/PWM/capture, ADC, watchdog and CAN | W09 conceptual coverage; one supported practical extension, extra hardware deferred |
| Cortex-A/M, AArch32/64, addresses/cache and boot | W06 comparisons, W10-W11 Linux transfer, W17 DMA cache coherency |
| Device model, probe/remove and resources | W12-W13 |
| SPI/I2C, regmap, IIO/hwmon and PM | W14 then W16 (Flagship Capstone 2: IIO SPI/I2C client driver) |
| Blocking I/O, user copies, poll and removal lifetime | W12 drill then W15 |
| SMP ordering, barriers and RCU | W15 context and W18 introduction; lock-free depth deferred |
| Tracing, latency, perf/ftrace, lockdep and sanitizers | W10 introductory tools and W18 focused investigation |
| Portfolio/regression/design review/interviews | Weekly career slice, W10 checkpoint and W19-W20 (3 Staff-level Capstone stories) |

Explicitly deferred: complete MIT lab sequence/COW/filesystem implementations, full BSP port, full Yocto course, USB/PCIe/controller or networking stack implementation, UEFI/ACPI specialization and advanced Rust. Learn them after the foundation when a chosen role/project requires them. Full OS-internal mastery is not claimed by this selected curriculum.

