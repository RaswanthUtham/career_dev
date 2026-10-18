# Training And Evidence Tracker

Updated 2026-10-07. Weekly budget and scheduling live only in the [authoritative curriculum](five_month_development_roadmap.md); [kernel milestones](scratch_riscv_os_plan.md) define project dependencies. This file owns current status and evidence pointers; the [learning log](learning_log.md) preserves dated explanations and corrections. Do not maintain another calendar here.

## Current Position

- Week 1: **Completed, learner-reported** on 2026-09-23. This does not retroactively verify all runtime tests; retain the older allocation review and deferred integration evidence.
- Week 2: **Completed, learner-reported** on 2026-09-27. Memory ordering, mutex, condition-variable, cancellation/drain, and reverse goto unwinding (W02-A through W02-F) source-reviewed.
- Week 3 foundation: **In progress, rebaselined** to the [Linux-aligned teaching-kernel milestones](scratch_riscv_os_plan.md). New project `my-linux` is learner-created with `reference/` populated; the old `riscv-kernel` is preserved as historical reference. Plan clarifies hardware reset (`0x1000`), dual-boot firmware contract (custom M-mode "Mini-SBI" vs production OpenSBI S-mode handoff), and explicit inclusion of user programs and interactive shell in Milestone 7. Prior iteration Days 1-8 were QEMU-verified; Day 9 files were empty and the old schedule is retired. This evidence does **not** verify new code. xv6 and upstream Linux remain separate reference repositories.
- Next lesson: **Milestone 1: xv6 M-mode boot versus OpenSBI S-mode kernel entry**; learner creates/organizes the new project after the boot contract is understood. DTB, timer calculations, IRQ top/bottom halves and later subsystems follow the milestone dependencies, not the old day count.
- Future budget is 22 hours/week; actual hours are unknown until reported. Source completion is not course certification, independent mastery or delayed retention.

## Status And Evidence Rules

| Status | Meaning |
| --- | --- |
| Planned | No completion evidence supplied |
| In progress | Explanation or implementation started; acceptance not yet recorded |
| Blocked | A named prerequisite or environment gate prevents this experiment |
| Deferred | Deliberately postponed; record destination and reason |
| Completed-reported | Learner says complete; record any evidence still missing |
| Verified | Specified acceptance checks have observed results and evidence links; not a blanket mastery label |

The module table's status and evidence columns are independent: source review does not imply runtime verification. A dash means unknown/not recorded, never zero. No invented start dates, hours, grades or test results. For W01 module rows, Completed-reported inherits the learner's week-level report, not a claim that every individual test ran. Preserve an earlier review defect even when the learner reports the week complete.

To finish a training: use its ID, report actual work/time if known, record source/version and artifact, compare expected/observed outcomes, record remaining limitations, then update status. `Verified` requires the module-specific artifact/checks; distinguish tutor-run versus learner-run output and physical hardware versus simulation. Recall remains unassessed unless evidence is voluntarily supplied or assessment resumes. An environment blocker does not prohibit the conceptual lesson.

## Module Register

Each ID links to its canonical weekly table, including prerequisites, provider sections and planned hours. A/B/C for future weeks occupy 16 technical hours; the remaining 6 support hours are logged separately below. Weekly completion requires applicable technical artifacts and an honest account of outstanding gates, not merely marking the reading done.

| ID | Training | Status | Actual h | Completed date | Evidence / next action |
| --- | --- | --- | --- | --- | --- |
| [W01-A](five_month_development_roadmap.md#week-1) | Workspace and probes | Verified | - | 2026-09-22 | [Recorded toolchain and xv6 smoke results](../docs/workspace_setup.md#verification-record); not learner mastery |
| [W01-B](five_month_development_roadmap.md#week-1) | Ownership, bounds and decoder concepts | Completed-reported | - | 2026-09-23 | [Allocation review](learning_log.md#2026-09-22-allocation-ownership-practice); earlier unchecked malloc remains recorded |
| [W01-C](five_month_development_roadmap.md#week-1) | Integration and failure tests | Completed-reported | - | 2026-09-23 | Week-level report; standalone runtime evidence deferred; revisit tests in W09-C/W19-C |
| [W01-D](five_month_development_roadmap.md#week-1) | Recap and role mapping | Completed-reported | - | 2026-09-23 | Private practice reported at week level; no independent retention claim |
| [W02-A](five_month_development_roadmap.md#week-2) | Thread lifecycle and publication | Completed-reported | - | 2026-09-23 | [Memory ordering source](../Lessons/03_memory_ordering.c), source-reviewed correct; runtime not observed |
| [W02-B](five_month_development_roadmap.md#week-2) | Mutex and condition completion | Completed-reported | - | 2026-09-24 | [Mutex](../Lessons/04_mutex.c) & [condition variable](../Lessons/05_condition_variable.c) source-reviewed; concurrency contract correct, minor syntax fixes noted |
| [W02-C](five_month_development_roadmap.md#week-2) | Monotonic deadline and outcomes | Completed-reported | - | 2026-09-27 | Monotonic deadline & carry implemented in [cancellation teardown source](../Lessons/07_cancellation_teardown.c) |
| [W02-D](five_month_development_roadmap.md#week-2) | Late callbacks and shutdown | Completed-reported | - | 2026-09-27 | Cancel/drain protocol & single-join teardown in [cancellation teardown source](../Lessons/07_cancellation_teardown.c) source-reviewed |
| [W02-E](five_month_development_roadmap.md#week-2) | RAII and review | Completed-reported | - | 2026-09-27 | Kernel reverse goto error-unwind contract & RAII comparison source-reviewed |
| [W02-F](five_month_development_roadmap.md#week-2) | Integration, recap and roles | Completed-reported | - | 2026-09-27 | Week 2 concurrency contracts mapped to semiconductor driver requirements |
| [W02-G](five_month_development_roadmap.md#week-2) | Recovery buffer | Completed-reported | - | 2026-09-27 | Empty 06_timed_wait.c superseded by 07_cancellation_teardown.c |
| [W03-A](five_month_development_roadmap.md#week-3) | xv6 organization and RV64 execution | Completed-reported | - | 2026-09-28 | Privilege modes (M/S/U), CSRs (mstatus, mepc, satp, medeleg, pmp), entry.S stack math, start.c mret trick to main() |
| [W03-B](five_month_development_roadmap.md#week-3) | Boot/syscall boundary debug trace | In progress | - | - | Boot and device tree discovery verified in QEMU via OpenSBI S-mode (my-linux Milestones 2 & 3); [learning log](learning_log.md#2026-10-08-milestone-3-verified-in-qemu-fdt-parsing--hardware-tree-traversal); advancing to Milestone 4 (Memory and Trap Boundary) |
| [W03-C](five_month_development_roadmap.md#week-3) | Bits, endian and disassembly | Planned | - | - | - |
| [W04-A](five_month_development_roadmap.md#week-4) | Sv39, page tables and traps | In progress | - | - | Trap vector (entry.S/traps.c) and Linux-style physical page frame allocator (mem_map/list_head) verified in QEMU; [learning log](learning_log.md#2026-10-14-physical-page-frame-allocator-verification-milestone-4-part-2); advancing to Sv39 page tables and MMU activation |
| [W04-B](five_month_development_roadmap.md#week-4) | One mapping/syscall diagnostic | Planned | - | - | - |
| [W04-C](five_month_development_roadmap.md#week-4) | Pointer and view lifetime | Planned | - | - | - |
| [W05-A](five_month_development_roadmap.md#week-5) | IRQ, scheduler, locks and filesystem overview | Planned | - | - | - |
| [W05-B](five_month_development_roadmap.md#week-5) | Blocking/wakeup source trace | Planned | - | - | - |
| [W05-C](five_month_development_roadmap.md#week-5) | Queue/list invariants | Planned | - | - | - |
| [W06-A](five_month_development_roadmap.md#week-6) | Cortex-M reset and exception model | Planned | - | - | - |
| [W06-B](five_month_development_roadmap.md#week-6) | Zephyr configuration and debug | Planned | - | - | Gate ZEPHYR |
| [W06-C](five_month_development_roadmap.md#week-6) | Register semantics | Planned | - | - | - |
| [W07-A](five_month_development_roadmap.md#week-7) | Zephyr scheduling and ownership | Planned | - | - | - |
| [W07-B](five_month_development_roadmap.md#week-7) | Bounded acquisition queue | Planned | - | - | - |
| [W07-C](five_month_development_roadmap.md#week-7) | Move-only resource ownership | Planned | - | - | - |
| [W08-A](five_month_development_roadmap.md#week-8) | ISR, workqueue, timers and DMA buffer lifecycle | Planned | - | - | - |
| [W08-B](five_month_development_roadmap.md#week-8) | Interrupt-to-worker pipeline and circular DMA setup | Planned | - | - | Gate BOARD for physical results; native_sim for software flow |
| [W08-C](five_month_development_roadmap.md#week-8) | Callback lifetime | Planned | - | - | - |
| [W09-A](five_month_development_roadmap.md#week-9) | SPI vs I2C bus transactions and DMA streams | Planned | - | - | - |
| [W09-B](five_month_development_roadmap.md#week-9) | Flagship Capstone 1: High-Speed SPI DMA Telemetry Pipeline | Planned | - | - | Gate DEVICE or simulation double; native_sim first, NUCLEO-F446RE weekend |
| [W09-C](five_month_development_roadmap.md#week-9) | Representation and decoder tests | Planned | - | - | - |
| [W10-A](five_month_development_roadmap.md#week-10) | Embedded Linux architecture/toolchain | Planned | - | - | - |
| [W10-B](five_month_development_roadmap.md#week-10) | Boot, user program and debugger | Planned | - | - | Gate LINUX |
| [W10-C](five_month_development_roadmap.md#week-10) | File-descriptor RAII | Planned | - | - | - |
| [W11-A](five_month_development_roadmap.md#week-11) | Boot flow, configuration and rootfs | Planned | - | - | - |
| [W11-B](five_month_development_roadmap.md#week-11) | Reproducible Linux image | Planned | - | - | Gate LINUX |
| [W11-C](five_month_development_roadmap.md#week-11) | Linking and symbols | Planned | - | - | - |
| [W12-A](five_month_development_roadmap.md#week-12) | Kernel lifecycle and execution context | Planned | - | - | - |
| [W12-B](five_month_development_roadmap.md#week-12) | Module and bounded user interface | Planned | - | - | Gate MODULE |
| [W12-C](five_month_development_roadmap.md#week-12) | User-copy contracts | Planned | - | - | - |
| [W13-A](five_month_development_roadmap.md#week-13) | Device model, resources and IRQ | Planned | - | - | - |
| [W13-B](five_month_development_roadmap.md#week-13) | Probe/remove failure experiment | Planned | - | - | - |
| [W13-C](five_month_development_roadmap.md#week-13) | Error unwind ownership | Planned | - | - | - |
| [W14-A](five_month_development_roadmap.md#week-14) | Bus client, regmap and IIO subsystem design | Planned | - | - | - |
| [W14-B](five_month_development_roadmap.md#week-14) | Flagship Capstone 2: Linux IIO SPI/I2C Client Driver | Planned | - | - | Gate DEVICE or QEMU stub; QEMU ARM guest first, BBB weekend |
| [W14-C](five_month_development_roadmap.md#week-14) | Client containers/views | Planned | - | - | - |
| [W15-A](five_month_development_roadmap.md#week-15) | Kernel synchronization/lifetime | Planned | - | - | - |
| [W15-B](five_month_development_roadmap.md#week-15) | Teardown and concurrent-access tests | Planned | - | - | - |
| [W15-C](five_month_development_roadmap.md#week-15) | Client cancellation | Planned | - | - | - |
| [W16-A](five_month_development_roadmap.md#week-16) | Buffering/timestamps and power | Planned | - | - | - |
| [W16-B](five_month_development_roadmap.md#week-16) | Capstone 2 Extension: Triggered Buffers & Runtime PM | Planned | - | - | Gate ADVANCED for physical PM evidence; QEMU guest first |
| [W16-C](five_month_development_roadmap.md#week-16) | Host client interfaces | Planned | - | - | - |
| [W17-A](five_month_development_roadmap.md#week-17) | DMA mapping/cache ownership (PoC vs PoU) | Planned | - | - | - |
| [W17-B](five_month_development_roadmap.md#week-17) | Flagship Capstone 3: Streaming DMA Driver with Cache Sync | Planned | - | - | Gate ADVANCED; QEMU ARM guest with DMA model first, BBB weekend |
| [W17-C](five_month_development_roadmap.md#week-17) | DMA buffer contracts | Planned | - | - | - |
| [W18-A](five_month_development_roadmap.md#week-18) | Tracing, SMP and RCU concepts | Planned | - | - | - |
| [W18-B](five_month_development_roadmap.md#week-18) | Capstone 3 Profiling, Concurrency Tracing & Latency Triage | Planned | - | - | Gate ADVANCED; ftrace/lockdep in QEMU guest |
| [W18-C](five_month_development_roadmap.md#week-18) | Optimized C debugging | Planned | - | - | - |
| [W19-A](five_month_development_roadmap.md#week-19) | Recovery and input/security boundaries | Planned | - | - | - |
| [W19-B](five_month_development_roadmap.md#week-19) | Acquisition-Pipeline Hardening & End-to-End Integration | Planned | - | - | Zephyr frontend + Linux driver backend pipeline demo |
| [W19-C](five_month_development_roadmap.md#week-19) | Contract testability | Planned | - | - | - |
| [W20-A](five_month_development_roadmap.md#week-20) | Role-specific design tradeoffs | Planned | - | - | - |
| [W20-B](five_month_development_roadmap.md#week-20) | Portfolio, Staff Interview Architecture & Compensation Consolidation | Planned | - | - | 3 Staff-level architectural deep-dive decks covering Capstones 1-3 |
| [W20-C](five_month_development_roadmap.md#week-20) | Worked coding/design interviews | Planned | - | - | No mocks unless requested |

## Environment Gates

| Gate | Current evidence | Closure / fallback |
| --- | --- | --- |
| XV6 | Migration build and QEMU utility smoke passed 2026-09-22; revision recorded in setup | Record revision and command for each new diagnostic; no automatic course graders |
| BOARD | Learner reports STM32F446RE with onboard ST-LINK and BBB | Confirm board silkscreen/revision, power/pins/probe access before commands; Simulation-First on native_sim/QEMU, weekend physical lab |
| ZEPHYR | Source clone present; no west/SDK/module runtime verification | Pin release/modules/SDK, run supported board or simulator probe when requested |
| DEVICE | Public external peripheral not selected | Datasheet, identity, wiring and API support, or labelled synthetic/stub evidence in QEMU; physical sensor wiring on weekend |
| LINUX | Source clone present; guest/BBB build/run target pending | Boot disposable QEMU ARM/ARM64 Linux guest (protects host WSL2) or spare BBB with recovery; record architecture, image, kernel/config |
| MODULE | Matching target kernel build environment not yet demonstrated | Matching headers/config/toolchain plus successful guest/board load/unload; never Podman host modules |
| ADVANCED | DMA/PM/tracing capabilities unknown until target selection | QEMU DMA model, ftrace, and lockdep in virtual guest; physical BBB DMA/PM measurements during dedicated weekend labs |

Do not mark all future weeks blocked merely because a gate is not yet needed. Concepts and setup preparation may proceed while physical verification is pending. Exact ESP32/Pico/Zero identification is deferred, not a blocker for this curriculum.

## Session Evidence Template

Append a dated entry to the learning log and link it from the appropriate module row. Use these fields; leave unknowns explicit:

- Training ID and session date; source URL/module and source version or commit.
- Planned scope (from curriculum); actual minutes reported, not inferred from chat length.
- Artifact path, original versus adapted code, and source-review finding.
- Environment: compiler/SDK/kernel/architecture/board, native/guest/simulator/physical, exact command.
- Expected result, actual result and who ran/observed it; attach non-sensitive output when available.
- Failure case, correction and remaining gaps; completion date/status and next action.
- Optional private recap report; independently assessed explain/trace/implement/debug/transfer remain N/A until evidence exists. Optional revisit date, no automatic reminder.

## Weekly Support Log

Support allocations are inside the canonical 22-hour week, not extra assignments. Add rows when a week starts; do not invent hours for future weeks. Week-1/2 support is already included in their historical module tables.

| Week | Actual career h | Actual recap h | Actual buffer/break h | Weekly outcome / blocker / next action |
| --- | --- | --- | --- | --- |
| 3 | - | - | - | Planned; dates and 6-hour support allocation owned by curriculum |

## Role Evidence Map

Official descriptions and research dates are owned by the [curriculum role table](five_month_development_roadmap.md#role-fit-and-career-work). Recheck vacancies before applications; salary remains an unverified target, not a promised band. Keep applications and offers private.

| Role reference | Matching training IDs | Existing transferable evidence | Missing development evidence |
| --- | --- | --- | --- |
| AMD 91919, Bengaluru firmware/RTOS | W06-B, W07-B, W08-B, W09-B (Capstone 1: SPI DMA), W14-B, W18-B | Parameterized validation and fault-injection background | Original RTOS/peripheral implementation, DMA ring buffer, and performance artifacts |
| Qualcomm 3095332, Hyderabad embedded driver | W04-B, W10-B, W13-B, W14-B (Capstone 2: IIO SPI/I2C), W15-B, W17-B (Capstone 3: Streaming DMA) | Callback/error bookkeeping and hardware-test exposure | Driver lifecycle, MMU/cache/IRQ/DMA and bring-up ownership |
| AMD 84900 / Qualcomm 3090149 (Staff Systems) | W16-B (Runtime PM), W17-B (DMA cache sync), W18-B (ftrace/lockdep), W20-B | Do not equate validation tenure with platform architecture experience | 3 Staff-level Flagship Capstone architecture walkthroughs; stretch/senior staff roles |
| NVIDIA JR2026342, adjacent power/performance | W18-B, W20-A | Validation/automation could transfer | Role-specific power/telemetry experience; not the primary firmware-driver target |

## Reading To Resume

Use the next in-progress module, not a stale historical "next" sentence in the log. At this rebaseline: W02-B -> W02-C -> W02-D, with W02-E/F completed before closing week 2. Then follow W03-A and its canonical prerequisites. A completed row does not silently authorize an assessment; keep the established Teach/Review workflow.