# Active Training And Evidence Tracker

Updated 2026-10-25. [Roadmap](five_month_development_roadmap.md) owns the calendar;
[project briefs](project_portfolio.md) own acceptance; [log](learning_log.md)
preserves history. Status is not inferred from lesson publication.

## Current Position

- **Week 1: In progress.** Actual teaching start: 2026-10-23, ahead of the
  roadmap's planned 2026-10-26 start. Keep the calendar and 22-hour allocation;
  early work belongs to Week 1, not an extra unbudgeted week.
- **Current topic: Privilege and address spaces - RISC-V to Arm (In progress).**
  Compare application/kernel roles and VA/PA using one read example; distinguish
  Arm64 from BBB's Armv7-A and STM32's Cortex-M. Source review is not execution.
- **Current reading:** [W01-L03 lesson](../notes/lessons/W01_L03_privilege_and_address_spaces.md),
  published 2026-10-23. Learner restored the simpler L01 explanation and edited
  L02; both remain unchanged in this continuation. Existing-note correctness
  repairs now require prior approval. Moving forward does not imply mastery.
  See the [lesson index](../notes/lessons/README.md) for the reading path.
- **Next implementation: L04/C01 size arithmetic**, a bounded learner-authored C
  function and tests in Week 1; still planned, not yet started. Remaining
  environment-readiness work is planned/blocked as below; actual hours unknown.
- Review refinements applied 2026-10-23: driver work in Weeks 3-4, Yocto in
  Weeks 5-6; early resource preflight retained. Bounded source traces and C01-C21
  fit existing weekly blocks. No learner competence or project status upgraded.
- A selected by learner; standalone MCU RTOS project requested; B is the tutor's
  researched Zephyr recommendation, with hardware-specific extensions open.
- Quizzes remain paused. Learner reports 10+ years C/intermediate proficiency.
  Architecture prerequisites are explained when needed, not assumed.
- Confirmed budget: 22 hours/week. Deadline: preparation/interviews and aim for
  offer by 22 March 2027; joining may follow.
- Embedded-only career scope reaffirmed: use OS/C foundations for firmware,
  peripherals and embedded Linux, not general server/desktop software training.
- Latest environment plan, 2026-10-23: HP x360 with Ubuntu Server 26.04.1 for
  builds; current laptop for editing and GitHub source synchronization.
  HP installation/resources/access pending. Remote-SSH is a suggested faster
  loop, not configured. Existing Windows/Podman stays the fallback; its accepted
  WSL2 backend does not imply a separate WSL workflow.
- Board photos reviewed 2026-10-25: F446RE MB1136 rev C, F767ZI MB1137 Rev B,
  BBB family and original Pico identified; ESP/UNO/display variants and
  accessory details have explicit uncertainties in the
  [inventory](../docs/board_inventory.md). No board operation verified.

## Status And Competence Are Separate

| Label | Meaning |
| --- | --- |
| Planned | Scope exists, no completion evidence |
| In progress | Specific work started; remaining checks named |
| Blocked | Named prerequisite missing |
| Deferred | Explicitly postponed; reason recorded |
| Completed-reported | Learner reports completion; missing evidence explicit |
| Verified | Named acceptance checks observed; who/where/revision/command recorded |
| Historical/superseded | Earlier evidence, not acceptance of the current implementation |

Track explain, trace, implement, debug and design/transfer separately:
unassessed / with help / independently / delayed fresh evidence. Tutor-provided
answers, code existence and compilation do not establish independent mastery.
No assessment is required to receive teaching.

## Preserved Evidence

| Area | Existing evidence | Limits / next use |
| --- | --- | --- |
| Weeks 1-2 C/concurrency | Completed-reported; source reviews and answers in log | Several runtime and independent checks remain unobserved; revisit inside projects |
| Original scratch-kernel Days 1-8 | Prior-iteration verified results recorded in log | Not current v2 acceptance; do not erase or reassign wholesale |
| v1 boot/FDT/traps | Earlier commits/logs record QEMU tests | Later state superseded; no fresh run |
| v1 allocator/Sv39 | Oct 14/18 recorded reuse/translation/alias/page-fault checks | Preserve concepts; not proof of final v1 correctness or delayed mastery |
| v1 slab/timer | Source at `5a52669`/`6610119`; final timer attempt unverified | Linker small-data defect; historical test runner can print overall PASS after failures |
| v2 K0 | Source committed in `3fddd0d`; log records tutor-run `make check` 5/5 and QEMU layout PASS | Full learner acceptance still incomplete: GDB/negative tests not all established |
| v2 K1a | Firmware committed in `3fddd0d`; log records requested tutor-run dual boot | Source reviewed now, not re-run; known DBCN nonblocking-contract issue recorded below |
| K1b-1 | Lesson published with historical tutor verification | Kernel client not present in inspected v2; not learner completion |

## Weekly Execution Register

This records execution against the
[roadmap schedule](five_month_development_roadmap.md#4-schedule), not a second
calendar. Week numbers below belong to the new project track; historical
weeks 1-2 remain completed-reported above. Deliverables are short roadmap
pointers, not independent scope. C IDs refer to the
[canonical practice table](five_month_development_roadmap.md#weekly-c-practice-c01-c21).

`-` means not reported/recorded. Update actual hours, C evidence and blockers
after work; never mark a whole week Verified from a tutorial or elapsed date.
Label tutor-run, learner-run, source-only and hardware results in the log.
The current environment inventory does not verify a future weekly deliverable.

| Week | Main deliverable | C topic ID | Actual h | Status | Evidence | Blocker |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | Foundations and early environment preflight | C01 | - | In progress | [W01-L01..L05 notes](../notes/lessons/README.md); learner submitted and repaired [C01_packet_size.c](../notes/lessons/C01_packet_size.c) (source-reviewed; runtime run pending) | Run `C01_packet_size.c` when ready; see environment gates |
| 2 | Linux boot/module/debug path | C02 | - | Planned | - | Linux QEMU readiness |
| 3 | Driver matching, ownership and protocol model | C03 | - | Planned | - | - |
| 4 | IRQ/context/teardown and task-switch trace | C04 | - | Planned | - | - |
| 5 | Minimal Yocto image | C05 | - | Planned | - | Yocto host gate |
| 6 | Driver packaging and SDK iteration | C06 | - | Planned | - | - |
| 7 | BBB bring-up and transport discovery | C07 | - | Planned | - | BBB gate |
| 8 | STM32 acquisition and DMA blocks | C08 | - | Planned | - | STM32 gate |
| 9 | A first read path and DMA eligibility trace | C09 | - | Planned | - | A transport gate |
| 10 | B task model and PendSV trace | C10 | - | Planned | - | Zephyr gate |
| 11 | B board port and timing markers | C11 | - | Planned | - | Saleae/STM32 gates |
| 12 | B diagnostics and recovery | C12 | - | Planned | - | - |
| 13 | A trigger capture and timestamps | C13 | - | Planned | - | - |
| 14 | A reset/stall/lifecycle faults | C14 | - | Planned | - | - |
| 15 | B overload and ownership decisions | C15 | - | Planned | - | - |
| 16 | B core release tests | C16 | - | Planned | - | CAN optional, not a core blocker |
| 17 | A deployment and persistence | C17 | - | Planned | - | - |
| 18 | A tracing/stress and evidence review | C18 | - | Planned | - | - |
| 19 | Existing evidence and project walkthroughs | C19 | - | Planned | - | - |
| 20 | Requested interviews and gap repair | C20 | - | Planned | - | Assessment optional |
| 21 | Protected contingency and final gaps | C21 | - | Planned | - | No new topic required |

### Portfolio Review Records

Dates and criteria live only in the
[roadmap checkpoints](five_month_development_roadmap.md#portfolio-review-checkpoints).
These reviews do not prevent earlier applications.

| Checkpoint | Status | Evidence / gaps | Next action |
| --- | --- | --- | --- |
| Week 9: A slice | Planned | - | Review actual working slice and v1/v2 provenance |
| Week 12: B v0.1 | Planned | - | Separate initial demo from release/CAN acceptance |
| Week 18: expanded evidence | Planned | - | Review design, failures, deployment and timing without title claims |

## Active Gate Register

| Gate | Status | Evidence / next action |
| --- | --- | --- |
| Archive | Verified | 37 original working files SHA-256 matched; [manifest](../backup/plan/2026-10-22-foundation/manifest.csv); no source rewrite in backup |
| Host/Podman inventory | Verified | Read-only 2026-10-23 checks: host RAM 31.69 GiB; C: free 102.13 GiB; running guest RAM 15.47 GiB; metadata's 2 GiB is not the observed guest ceiling. [Details](../docs/workspace_setup.md#resource-observation-2026-10-23) |
| HP Linux builder | Planned | Ubuntu Server 26.04.1 planned; supplied proposal says 16 GB RAM/220 GB SSD, not measured free space. CPU, installation, SSH and builds unverified. [SSH-first guide](../docs/hp_linux_ssh_setup.md) |
| Source synchronization | Planned | Learner proposes local edit -> GitHub -> HP pull/build; no GitHub action performed. Remote-SSH fast loop recommended; repository visibility, paths and access require confirmation |
| Linux QEMU | Planned | Pin image/kernel/config, module environment, debugger and recovery on the chosen HP or fallback host. Existing native Windows QEMU availability does not verify HP or container tools |
| Yocto host | Blocked | Select release/BSP and native/container distribution together; 6.0 lists Ubuntu 26.04, while 5.0.14 lists 24.04. HP resources unknown. Existing Windows C: 102.13 GiB measured, 200 GB planned, not HP evidence |
| Yocto image/SDK | Planned | Minimal pinned QEMU image, layer, SDK and running-version proof |
| Photo inventory | Verified | All 11 photos inspected, labels/layouts recorded; this verifies inspection/identification only, not powered hardware. [Inventory](../docs/board_inventory.md) |
| BBB | Blocked | Family photo-identified; revision/media/serial recovery and boot not verified. Guest fallback preserves software work, not physical bus evidence |
| STM32 | Blocked | F446RE MB1136 rev C and F767ZI MB1137 Rev B photo-identified; silicon revisions, toolchains, wiring, power and debug operation still pending |
| Zephyr | Blocked | Source exists; release/SDK/west modules and simulator build not verified |
| Saleae | Blocked | Learner-reported, not in photo set; model, probe limits and capture settings pending |
| A transport | Planned | Verify selected MCU SPI-target/API and Linux controller support before committing protocol |
| A core | Planned | Acceptance matrix in project brief; no code or measurements yet |
| B core | Planned | QEMU task model -> standalone board evidence |
| B CAN | Deferred | Two STM32 models now identified; functionality, transceivers, termination, pinmux and release support not established |
| Optional sensors/BOM | Deferred | Displays visible but controllers/voltages unknown; sensors/transceivers/cables not inventoried. Confirm UNO quantity and accessories before purchases |
| Career evidence | In progress | Current official role samples researched; resume/work-impact stories not yet reviewed |

Recommended board allocations and per-failure evidence limits live in the
[contingency plan](../docs/board_project_plan.md). Use the existing two-hour
diagnostic-block fallback policy; never label a model test as board verification.

## Known Review Findings To Carry Forward

- Preserve historical notes, but do not use old boot/MMU/linker recipes as
  current v2 instructions. They include incompatible layouts and assumptions.
- K1b-1 prose overstates `asm volatile`, local register variables and `memory`
  clobber guarantees. Active skills teach the corrected distinction; historical
  lesson content remains unchanged. Consult GCC before reusing it.
- Firmware DBCN buffer writes call a blocking UART path despite the SBI
  nonblocking buffer-write contract. Kernel/firmware stays read-only; explain
  and request exact-file permission before any learner-owned repair.
- Do not copy the v1 unconditional overall PASS or unbounded timer-wait pattern.
- Historical log contains 24 file-link targets absent from current v2. Use its
  named old revision/context; missing targets do not prove work never existed.
- Previous market claim that SPI/I2C/IIO was the industry's number-one demand
  combination is unsupported; see the dated correction in the log.

## Evidence Record For Each Result

Date; module/project; source revision/configuration; author; who ran it;
emulator or exact board; command; expected/observed result; relevant raw evidence
location; error/repair; remaining limitations; actual hours only if reported;
next action. Keep personal/workplace material private.

Do not turn suggested checks into recorded results. Update this tracker and
append a dated log entry after actual work; do not rewrite old entries.
