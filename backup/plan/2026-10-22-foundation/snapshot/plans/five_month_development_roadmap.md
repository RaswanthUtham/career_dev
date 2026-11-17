# Career Roadmap: Kernel, RTOS and Linux Drivers

Rebaselined 2026-10-21. **This is the only schedule.** The [kernel milestone guide](scratch_riscv_os_plan.md) defines what each kernel milestone builds and how it is accepted; the [tracker](training_tracker.md) records status; the [learning log](learning_log.md) records dated history. The [eight-week file](weeks_01_08_development.md) is only a redirect. Workspace setup: [workspace readiness](../docs/workspace_setup.md).

## 1. Goal

Move from automotive Complex Device Driver validation (baseline about 36 LPA) to **Senior / Staff embedded systems, firmware and Linux kernel/driver engineering** at semiconductor and systems companies (for example Qualcomm, NVIDIA, AMD, TI, Intel, Samsung) in Bengaluru (primary) or Hyderabad. Target **INR 70-90 LPA CTC**. That figure is an aspiration, not a promised outcome. Compare base pay, bonus, annual equity vesting and one-off payments separately when evaluating offers.

### What reaching that band requires

Posted roles at this level ask for evidence across five areas, not only course completion:

| Area | What interviews probe | Evidence this roadmap produces |
| --- | --- | --- |
| C and low-level coding | Pointers, memory layout, bit manipulation, alignment, undefined behaviour, lists/ring buffers/allocators written in C under time pressure | Weekly C interview drills; allocator, ring-buffer and scheduler code in own projects |
| Architecture | Privilege and exception levels, traps, MMU/TLB, caches and coherency, barriers, interrupt controllers, boot chain (RISC-V and Arm64) | `my-linux` K0-K8, Arm64 transfer note K10, DMA capstone |
| OS and kernel internals | Virtual memory, buddy/slab, scheduling, locking, top/bottom halves, context switch, syscalls | `my-linux` K3-K8 built in upstream order with upstream names |
| RTOS and MCU firmware | Cortex-M exception model, context switch, priority inversion, ISR rules, Zephyr device model, DMA pipelines, latency | Mini-RTOS kernel R2, Zephyr capstone R4-R5 on STM32 hardware |
| Linux drivers and debugging | Device model, DT bindings, probe/remove, IRQ/deferred work, regmap/IIO, DMA API, PM, ftrace/lockdep, upstream workflow | Capstones 2 and 3, one submitted upstream patch |

Staff-level interviews additionally probe design judgement and influence: an embedded system design round, deep dives on your own work, and leadership stories. The roadmap reserves time for design write-ups and a story bank.

### Starting evidence

The learner reports 10+ years of C (intermediate), basic/intermediate C++, intermediate Python and basic Rust/Linux shell. Validation strengths (parameterized tests, fault injection, decoding) support debugging stories; they are not claims of production driver ownership. Employer code, protocols and test vectors never enter these projects. Hardware: STM32 NUCLEO-F446RE with ST-LINK, BeagleBone Black, logic analyser. Notice period: two months (affects joining, not learning order).

## 2. Teaching Contract

- Every lesson follows the [Feynman coach](../.github/skills/learning-feynman-coach/SKILL.md) and the matching domain skill: what, why with a failure example, conceptual solution, how with a worked trace, recap, interview answer.
- **Lessons are written as markdown notes in `career_dev/notes/`.** The learner reads, asks questions and types all project code. The tutor verifies reference code in a private scratch copy before publishing and states the verification status at the top of each note.
- Quizzes and mock interviews happen only when the learner asks ("resume quizzes", "assess me", "mock interview").
- Status words mean exactly what the [tracker](training_tracker.md) defines; nothing is marked verified without observed evidence.

## 3. Budget

22 hours per week, unchanged:

| Block | Hours | Use |
| --- | ---: | --- |
| A Concepts and upstream source reading | 4 | Lesson note, upstream trace |
| B Implementation and debugging | 10 | Milestone code, tests, evidence |
| C Interview coding | 2 | One C problem per session from the rotation in section 6 |
| Career | 2 | Role research, story bank, resume, applications, requested mocks |
| Recap | 1 | Optional private Feynman reconstruction |
| Buffer | 3 | Setup, overrun, breaks |

Suggested distribution: Mon A2, Tue B2, Wed A2, Thu B2, Fri C2; Sat B4 + Career1 + Buffer1; Sun B2 + Career1 + Recap1 + Buffer2. When a week slips, use buffer first, then drop stretch work, then shift later weeks. Never compress prerequisites or add hours silently.

## 4. Completed History (Weeks 1-5)

| Week | Completed / dates | Content | Status |
| --- | --- | --- | --- |
| 1 | Completed 2026-09-23 | Workspace, safe C contracts, allocation ownership, decoder | Completed, learner-reported |
| 2 | Completed 2026-09-27 | Threads, publication/ordering, mutex, condition variables, timeouts, cancellation/teardown, RAII | Completed, learner-reported |
| 3-5 | 2026-09-28 to 2026-10-18 | xv6 boot/traps study; `my-linux` v1: S-mode boot, FDT walk, traps, page allocator, Sv39, high-half, SLUB attempt, timer attempt | v1 preserved at tag `v1-reference`; concepts covered, code superseded by v2 |

Week 1-2 detailed module tables remain in git history of this file (commit before 2026-10-21). Their deferred runtime checks are carried into the C drill rotation.

## 5. Schedule

Week start dates are targets. Re-estimate after each phase gate using actual hours; do not treat dates as deadlines.

### Phase 1: Linux-aligned kernel (`my-linux` v2)

Detailed contents and acceptance checks: [kernel milestone guide](scratch_riscv_os_plan.md).

| Week | Starts | ID | Work (A + B blocks) | Acceptance |
| --- | --- | --- | --- | --- |
| 6 | 2026-10-19 | K0 | Build system, flags, linker script with VMA/LMA and all sections | K0 checks in guide |
| 7 | 2026-10-26 | K1 | Learner firmware (mini-SBI) + S-mode entry + SBI console; dual boot | Same kernel boots under OpenSBI and own firmware |
| 8 | 2026-11-02 | K2 | Trap entry/exit, dispatch table, `die()` diagnostics | Faults produce correct register dumps |
| 9 | 2026-11-09 | K3 | `setup_vm` static early tables, `relocate_enable_mmu`, virtual `start_kernel` | Kernel runs at `KERNEL_LINK_ADDR` |
| 10 | 2026-11-16 | K4 | `setup_arch`: FDT, memblock, `setup_vm_final` linear map | RAM from DTB; `__va`/`__pa` round-trip |
| 11 | 2026-11-23 | K5a | `mem_map`, buddy allocator (orders 0-10) | Split/merge tests |
| 12 | 2026-11-30 | K5b | SLUB caches, `kmalloc`/`kfree` | Slot reuse and cache lookup tests |
| 13 | 2026-12-07 | K6 | Sstc timer, jiffies, `timer_list`, softirq bottom half | Timer callback in softirq at expected jiffy |
| 14 | 2026-12-14 | K7 | IRQ core, INTC/PLIC, interrupt-driven UART, tasklet | Typed input via IRQ path |
| 15 | 2026-12-21 | K8a | `task_struct`, `__switch_to`, kthreads, preemptive scheduler, spinlocks | Two kthreads interleave under preemption |
| 16 | 2026-12-28 | K8b | Wait queues, completion, mutex, workqueue thread; phase buffer (holidays) | Sleep/wake through UART bottom half |
| 17 | 2027-01-04 | K10 | Arm64 transfer note; kernel portfolio README; kernel interview story bank | Comparison note and 3 kernel stories |

### Phase 2: RTOS and MCU firmware

| Week | Starts | ID | Work | Acceptance |
| --- | --- | --- | --- | --- |
| 18 | 2027-01-11 | R1 | Cortex-M4: reset, vector table, linker/startup, clocks, NVIC, exception entry stacking, MSP/PSP, SysTick, PendSV; bare-metal on QEMU Cortex-M target then F446RE | Bare-metal UART + SysTick interrupt on board; exception-entry trace |
| 19 | 2027-01-18 | R2 | Mini preemptive RTOS kernel: PendSV context switch, tick, ready lists by priority, sleep, semaphore, mutex with priority inheritance, ISR-safe queue | Priority-inversion demo fixed by inheritance; context-switch trace |
| 20 | 2027-01-25 | R3 | Zephyr: west, Kconfig, devicetree overlays/bindings, threads, sync, message queues, workqueues, device model and init levels; compare with R2 | Version-pinned Zephyr app on F446RE; design comparison note |
| 21 | 2027-02-01 | R4 | **Capstone 1A:** SPI sensor acquisition with STM32 circular DMA, double buffering, ISR-to-thread handoff, ring buffer | Pipeline runs in simulation; buffer-ownership contract |
| 22 | 2027-02-08 | R5 | **Capstone 1B:** hardware run, logic-analyser capture, overrun and recovery tests, latency measurement, write-up | Measured trace, failure matrix, RTOS interview story |

### Phase 3: Embedded Linux and drivers

| Week | Starts | ID | Work | Acceptance |
| --- | --- | --- | --- | --- |
| 23 | 2027-02-15 | L1 | Boot chain on QEMU arm64 and BeagleBone Black: firmware, U-Boot, kernel Kconfig/kbuild, DT, Buildroot rootfs; kernel debugging with GDB | Reproducible image boot; documented chain |
| 24 | 2027-02-22 | L2 | Modules, device model, platform driver, DT binding, `devm_*`, misc/char interface, user copies | Probe/remove/error-unwind evidence |
| 25 | 2027-03-01 | L3 | IRQs and deferred work in Linux (threaded IRQ, workqueue), locking, teardown lifetime, RCU introduction, lockdep | Teardown stress results without use-after-free |
| 26 | 2027-03-08 | L4 | **Capstone 2A:** IIO SPI/I2C client driver with `regmap`, raw channel reads, YAML binding | Driver reads real or emulated device |
| 27 | 2027-03-15 | L5 | **Capstone 2B:** FIFO with threaded IRQ, triggered buffer, runtime PM | Continuous capture and suspend/resume logs |
| 28 | 2027-03-22 | L6 | **Capstone 3A:** DMA API (coherent versus streaming, scatter-gather), dmaengine, cache maintenance, barriers | Ownership timeline and working transfer |
| 29 | 2027-03-29 | L7 | **Capstone 3B:** ftrace, perf, lockdep, KASAN; latency triage before/after | Trace evidence and analysis |
| 30 | 2027-04-05 | L8 | Upstream workflow: `checkpatch`, `get_maintainer`, `git send-email`; submit one small real patch; oops decoding | Patch on a public mailing list (acceptance not required) |
| 31 | 2027-04-12 | L9 | Integration: MCU pipeline feeding Linux driver (subset); portfolio packaging | End-to-end demo or documented limits |

### Phase 4: Interview consolidation

| Week | Starts | ID | Work | Acceptance |
| --- | --- | --- | --- | --- |
| 32 | 2027-04-19 | I1 | Embedded system design practice (sensor gateway, OTA updater, bootloader, low-power logger); deep-dive decks for own projects; resume | Three design write-ups; one-page resume |
| 33 | 2027-04-26 | I2 | Learner-requested mock loops: C coding, OS/kernel, drivers, RTOS, design, behavioural; gap repair | Mock feedback log and repair actions |
| 34-35 | 2027-05-03 | Buffer | Contingency only, unallocated | - |

Optional after the core: `my-linux` K9 user space and shell, SMP, PCIe/USB/networking specialization, Yocto.

## 6. Continuous Tracks

**C interview drill rotation (block C, every week).** Cycle through: bit manipulation and masks; pointer arithmetic and memory layout; endianness and packing; linked lists (intrusive); ring buffers; bitmap and buddy-style allocation; string parsing with bounds; integer overflow and promotions; concurrency with locks/atomics; one medium data-structure problem written in C. Each drill ends with complexity and an interview-style explanation.

**Career track (career block).**

| Weeks | Activity |
| --- | --- |
| 6-17 | Requirement-to-evidence matrix from current postings; resume baseline; story bank (debugging, ownership, kernel projects) |
| 18-22 | Learner-requested weekly mock round in the current domain; referrals; firmware/RTOS applications open at Gate F |
| 23-31 | Driver/kernel applications open at Gate D; interviews scheduled around milestones |
| 32-33 | Full mock loops, offer comparison worksheet |

**Application gates.**

| Gate | Opens after | Role focus |
| --- | --- | --- |
| F | K0-K8 accepted, R1-R5 accepted, C drills fluent | Firmware, RTOS, bare-metal, BSP-adjacent |
| D | Gate F + L1-L5 accepted (Capstone 2) | Linux driver, BSP, kernel |
| S | Gate D + Capstone 3 + submitted upstream patch + three design write-ups | Staff/Senior-2 framing |

## 7. Flagship Artifacts

| Artifact | Phase | What it proves |
| --- | --- | --- |
| `my-linux` v2 | 1 | Boot-to-scheduler kernel built in upstream Linux order; can explain every link |
| Mini-RTOS kernel | 2 | Context switching, scheduling and priority inheritance from first principles on Cortex-M |
| Capstone 1: SPI + circular DMA telemetry (Zephyr, STM32) | 2 | Interrupt-to-thread pipelines, DMA buffer ownership, measured latency |
| Capstone 2: IIO SPI/I2C client driver (Linux) | 3 | Device model, regmap, threaded IRQ, triggered buffers, PM |
| Capstone 3: streaming DMA with cache maintenance and tracing | 3 | Coherency, barriers, latency triage with ftrace/lockdep |
| Upstream patch | 3 | Real kernel workflow and review etiquette |

Reference sources in the workspace: [Zephyr STM32 SPI driver](../../zephyr-workspace/zephyr/drivers/spi/spi_stm32.c), [Linux MPU6050 IIO SPI](../../linux/drivers/iio/imu/inv_mpu6050/inv_mpu_spi.c), [IIO core](../../linux/drivers/iio/imu/inv_mpu6050/inv_mpu_core.c), [Linux DMA cache example](../../linux/drivers/iio/adc/nxp-sar-adc.c).

**Simulation first:** weeknights use QEMU (RISC-V, Cortex-M, arm64) or Zephyr `native_sim`; weekends move proven code to the F446RE or BeagleBone Black with logic-analyser evidence. Hardware problems never block conceptual progress.

## 8. Sources

Study only the listed sections.

| Source | Use |
| --- | --- |
| [RISC-V specifications](https://docs.riscv.org/) and SBI specification | K1-K8 privilege, traps, Sv39, Sstc, SBI calls |
| Linux source in workspace and [kernel documentation](https://docs.kernel.org/) | All K milestones and Phase 3; driver API, kbuild, dev-tools, process |
| [MIT 6.1810](https://pdos.csail.mit.edu/6.1810/) and [xv6 book](https://mit-pdos.github.io/xv6-riscv-book/) | OS mechanism comparison only |
| [Arm documentation](https://developer.arm.com/documentation/) | Cortex-M4 (R1-R2) and Arm64 transfer (K10, L1) |
| [Zephyr documentation](https://docs.zephyrproject.org/) | R3-R5; pin the release before commands |
| [Bootlin embedded Linux, kernel and debugging training](https://bootlin.com/training/) | L1-L7 selected slides and labs |
| [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines), [SEI CERT C](https://wiki.sei.cmu.edu/confluence/display/c) | C drill rotation |

## 9. Role Evidence

Official descriptions retrieved 2026-09-23; postings change and salary bands were not published. Keep application and offer details private.

| Role sample | Requirements seen | Where covered |
| --- | --- | --- |
| [AMD 91919 Firmware Developer RTOS/Bare-metal, Bengaluru](https://careers.amd.com/jobs/91919?lang=en-us) | C/C++, RTOS/bare-metal, drivers, Arm/x86, I2C/SPI/UART/PCIe, performance/power | Phases 1-2; PCIe deferred |
| [Qualcomm 3095332 Senior Embedded Software and Driver Development, Hyderabad](https://careers.qualcomm.com/careers/job/446720516557) | Low-level drivers, MMU/cache, clocks, IRQ/DMA, buses, debug tools, Linux/RTOS | Phases 1-3 |
| [AMD 84900 Power Management Firmware, Bengaluru](https://careers.amd.com/jobs/84900?lang=en-us) | BIOS/ACPI/UEFI, PCIe, power, leadership | Later specialization |
| [Qualcomm 3090149 Arm Server Systems, Bengaluru](https://careers.qualcomm.com/careers/job/446719135898) | Principal-level SoC/firmware breadth | Long-term direction |
| [NVIDIA JR2026342 Senior System Software](https://nvidia.wd5.myworkdayjobs.com/en-US/NVIDIAExternalCareerSite/job/India-Pune/Senior-System-Software-Engineer_JR2026342) | Power/performance, SoC, scripting | Adjacent; tracing in Phase 3 |

Refresh this table with current postings during weeks 6-17 of the career track.

## 10. Environment Gates

| Gate | Needed for | Fallback |
| --- | --- | --- |
| Podman `xv6-lab` with QEMU 8.2.2 and GCC 13.3 | Phase 1 | Re-verify container; no host installs without approval |
| Cortex-M QEMU target and STM32 toolchain/OpenOCD | R1-R2 | QEMU first; board on weekends |
| Pinned Zephyr release, SDK and modules; confirmed F446RE board identity, pins, voltage | R3-R5 | `native_sim` / QEMU; board pending |
| Disposable QEMU arm64 Linux guest; BBB with serial recovery | L1-L9 | Guest only; never load modules into the Podman host kernel |

No installations, firmware flashes, privileged commands, purchases, commits or pushes are implied by this roadmap; each needs explicit learner approval.

## 11. Scope Decisions

- Old weekly plan (weeks 6-20 dated 2026-10-19 to 2027-01-31) is superseded by section 5; its topics moved as follows: Zephyr and Capstone 1 -> R3-R5; Linux foundations, device model, IIO, DMA and tracing -> L1-L7; portfolio and staff stories -> K10, L9, I1-I2.
- The schedule now ends 2027-04-30 plus two buffer weeks instead of 2027-01-31, because the kernel is rebuilt in upstream order and a mini-RTOS was added. Applications start earlier, at Gates F and D.
- Deferred: `my-linux` user space and shell (K9), SMP, full Yocto, PCIe/USB/network stacks, UEFI/ACPI, advanced Rust.
