# Training And Evidence Tracker

Updated 2026-10-21. This file owns **current status and evidence pointers only**. Schedule and hours: [roadmap](five_month_development_roadmap.md). Kernel milestone contents and checks: [kernel milestone guide](scratch_riscv_os_plan.md). Dated explanations and corrections: [learning log](learning_log.md).

## Current Position

- Phase 1, week 6. `my-linux` v1 is on branches `main`/`v1` and tag `v1-reference` (commit `6610119`), backed up as `C:\workspace\career_ws\backups\my-linux-v1.bundle`. v2 is on branch `v2` (empty start commit `1d392b9`).
- Current lesson: **[K1b-1 talking to the firmware](../notes/kernel/K01b1_talk_to_firmware_sbi_ecall.md)**. K0 and [K1a firmware](../notes/kernel/K01a_firmware_and_boot_contract.md) are done and committed on `my-linux` `v2` (`3fddd0d`).
- Quizzes and mocks: paused until the learner asks.

## Status Words

| Status | Meaning |
| --- | --- |
| Planned | No evidence yet |
| In progress | Lesson or implementation started |
| Blocked | A named gate prevents the experiment |
| Deferred | Postponed with destination and reason |
| Completed-reported | Learner says complete; missing evidence listed |
| Verified | The module's acceptance checks have observed results linked here; states who ran them (learner or tutor) and where (QEMU or hardware) |

A dash means unknown. Never invent hours, dates or results. Source review does not imply runtime verification.

## Module Register

### History (preserved)

| ID | Training | Status | Completed | Evidence |
| --- | --- | --- | --- | --- |
| W01-A | Workspace and probes | Verified | 2026-09-22 | [Toolchain and xv6 smoke results](../docs/workspace_setup.md#verification-record); not mastery |
| W01-B..D | Ownership, decoder, failure tests, recap | Completed-reported | 2026-09-23 | [Allocation review](learning_log.md#2026-09-22-allocation-ownership-practice); unchecked-malloc finding retained; runtime tests deferred into C drills |
| W02-A..G | Threads, ordering, mutex, condvar, deadlines, teardown, RAII | Completed-reported | 2026-09-27 | [Lessons](../Lessons/) source-reviewed; runtime not observed |
| W03-W05 | xv6 study and `my-linux` v1 (boot, FDT, traps, page allocator, Sv39, high-half, SLUB, timer attempt) | Superseded | 2026-10-21 | v1 tag `v1-reference`; QEMU evidence in the log up to 2026-10-20; final state had a linker small-data defect |

### Phase 1: `my-linux` v2

| ID | Milestone | Status | Actual h | Completed | Evidence / next action |
| --- | --- | --- | --- | --- | --- |
| K0 | Build, toolchain, linker contract | In progress | - | - | Learner-authored tree; tutor-run at the learner's request after a CRLF fix (2026-10-22): `make check` 5/5 PASS, `K0 layout: PASS` under OpenSBI ([log](learning_log.md#2026-10-22-k0-passes-on-learner-tree-k1-published)). Open: GDB stop, `.sbss` negative test, learner commit |
| K1 | Firmware + boot contract, dual boot | In progress | - | - | K1a: learner-typed, committed `3fddd0d`; tutor-run at the learner's request: K0 kernel boots under OpenSBI and my-sbi, banner read-backs match. K1b split into K1b-1/2/3 after learner feedback; first attempt backed up to `backups/my-linux-k1b-attempt1` and removed. [K1b-1](../notes/kernel/K01b1_talk_to_firmware_sbi_ecall.md) published |
| K2 | Traps and diagnostics | Planned | - | - | - |
| K3 | Early MMU and virtual entry | Planned | - | - | - |
| K4 | `setup_arch`: FDT, memblock, final page tables | Planned | - | - | - |
| K5a | Buddy allocator | Planned | - | - | - |
| K5b | SLUB and `kmalloc` | Planned | - | - | - |
| K6 | Time, timer IRQ, softirq | Planned | - | - | - |
| K7 | IRQ core, PLIC, UART | Planned | - | - | - |
| K8a | Tasks, context switch, scheduler, spinlocks | Planned | - | - | - |
| K8b | Wait queues, mutex, completion, workqueue | Planned | - | - | - |
| K10 | Arm64 transfer and kernel story bank | Planned | - | - | - |
| K9 | User space and shell (optional) | Deferred | - | - | After Phase 3 |

### Phase 2: RTOS and MCU

| ID | Milestone | Status | Actual h | Completed | Evidence / next action |
| --- | --- | --- | --- | --- | --- |
| R1 | Cortex-M4 bare-metal and exception model | Planned | - | - | Gate BOARD for hardware part |
| R2 | Mini preemptive RTOS kernel | Planned | - | - | - |
| R3 | Zephyr fundamentals and device model | Planned | - | - | Gate ZEPHYR |
| R4 | Capstone 1A: SPI + circular DMA pipeline (simulation) | Planned | - | - | - |
| R5 | Capstone 1B: hardware run and measurements | Planned | - | - | Gate BOARD, DEVICE |

### Phase 3: Embedded Linux and drivers

| ID | Milestone | Status | Actual h | Completed | Evidence / next action |
| --- | --- | --- | --- | --- | --- |
| L1 | Boot chain, kernel build, rootfs | Planned | - | - | Gate LINUX |
| L2 | Modules, device model, platform driver | Planned | - | - | Gate MODULE |
| L3 | IRQs, deferred work, locking, teardown | Planned | - | - | - |
| L4 | Capstone 2A: IIO regmap client | Planned | - | - | Gate DEVICE or emulated device |
| L5 | Capstone 2B: threaded IRQ, triggered buffer, PM | Planned | - | - | - |
| L6 | Capstone 3A: DMA API and cache maintenance | Planned | - | - | Gate ADVANCED |
| L7 | Capstone 3B: tracing and latency triage | Planned | - | - | - |
| L8 | Upstream patch submission | Planned | - | - | - |
| L9 | Integration and portfolio | Planned | - | - | - |

### Phase 4: Interview consolidation

| ID | Milestone | Status | Actual h | Completed | Evidence / next action |
| --- | --- | --- | --- | --- | --- |
| I1 | System design write-ups, deep-dive decks, resume | Planned | - | - | - |
| I2 | Mock loops (learner-requested) | Planned | - | - | - |

### Application Gates

| Gate | Condition | Status |
| --- | --- | --- |
| F | K0-K8 + R1-R5 verified, C drills fluent | Not open |
| D | Gate F + L1-L5 verified | Not open |
| S | Gate D + L6-L8 + three design write-ups | Not open |

## Environment Gates

| Gate | Current evidence | Closure / fallback |
| --- | --- | --- |
| PHASE1 | QEMU 8.2.2, GCC 13.3, ld 2.42, gdb-multiarch 15.1 in `xv6-lab` observed 2026-10-21; upstream Linux 7.3-rc4 `f0100363d` | Closed for K0 |
| BOARD | Learner reports NUCLEO-F446RE with ST-LINK and BBB | Confirm board revision, power, pins, probe before commands |
| ZEPHYR | Source clone present; west/SDK/modules not verified | Pin release, modules, SDK at R3 |
| DEVICE | External SPI/I2C sensor not selected | Datasheet, wiring and voltage, or labelled emulated device |
| LINUX | Source clone present; arm64 guest/BBB image pending | Disposable QEMU arm64 guest; never the Podman host kernel |
| MODULE | Matching kernel build environment not demonstrated | Matching headers/config/toolchain in guest |
| ADVANCED | DMA/PM/tracing capability depends on target | QEMU guest first, BBB measurements on weekends |

## Session Evidence Template

Append a dated log entry and link it from the module row:

- ID, date, source and version/commit.
- Planned scope; actual minutes as reported.
- Artifact paths; who wrote the code.
- Environment and exact command.
- Expected versus observed result and who ran it.
- Failure, root-cause evidence, correction, remaining gaps, next action.

## Weekly Support Log

| Week | Career h | Recap h | Buffer h | Outcome / blocker / next action |
| --- | --- | --- | --- | --- |
| 6 | - | - | - | Rebaseline; v1 tagged; K0 and K1a typed by learner, committed `3fddd0d`; K1b split into three lessons, K1b-1 published |

## Role Evidence Map

Role samples live in the [roadmap](five_month_development_roadmap.md#9-role-evidence).

| Role sample | Covering IDs | Transferable evidence today | Missing evidence |
| --- | --- | --- | --- |
| AMD 91919 firmware/RTOS | K1-K8, R1-R5 | Validation, fault injection | Original RTOS and DMA pipeline artifacts |
| Qualcomm 3095332 embedded driver | K3-K8, L2-L7 | Callback/error accounting, hardware test exposure | Driver lifecycle, MMU/cache/IRQ/DMA ownership |
| Staff-level samples | L5-L8, I1 | - | Capstones 2-3, upstream patch, design write-ups |
| NVIDIA JR2026342 adjacent | L7, I1 | Automation | Power/telemetry depth |
