# Five-Month Embedded Development Roadmap

Fresh plan, 2026-10-22; review refinements applied 2026-10-23.
**This is the only active weekly schedule.**
[Project contracts](project_portfolio.md) own scope and acceptance;
[tracker](training_tracker.md) owns status; [log](learning_log.md) owns history;
[setup](../docs/workspace_setup.md) owns environment gates.

## 1. Outcome And Limits

Prepare for embedded Linux/device-driver and firmware development roles with
Staff/Principal-style design reasoning: explain concepts in simple English,
walk through real C, make justified decisions and debug from evidence.
Target INR 90 LPA total CTC, Bengaluru primarily and Hyderabad secondarily.
This is aspirational, not a guaranteed offer, title or compensation result.

**Embedded scope:** firmware/RTOS, peripherals, BSP/boot, embedded Linux
drivers, acquisition services and hardware debugging. This is not a general
desktop/backend/server-development or system-administration curriculum.
Embedded Linux development still uses systems-programming mechanisms:
syscalls, files, memory, threads and synchronization. Teach each through a
specific embedded use (device access, bounded buffering, timeout or teardown),
not as an unrelated specialization. Ubuntu Server is our build host, not the
product or career focus. Keep the teaching-kernel extension long-term/optional.

Preparation and active interviews run through **22 March 2027**. Aim for an
offer; joining may follow the notice period. Consider Senior/Lead and selected
Staff roles where actual experience fits. Principal production ownership cannot
be inferred from a five-month project. Keep workplace impact stories truthful
and separate from learning-project experience.

Two bounded products:

- **A: Low-voltage embedded fault/event recorder** using STM32 acquisition
  firmware, a BBB Linux IIO client driver and a reproducible Yocto image.
- **B: Standalone Zephyr telemetry and health-monitoring node** demonstrating
  real-time scheduling, overload handling, supervision and fault recovery.

Project A was selected by the learner. Project B is the tutor's researched
recommendation following the learner's request for a second, MCU-only RTOS
project. The 2026-10-25 [photo inventory](../docs/board_inventory.md) identifies
F446RE, F767ZI, BBB and Pico plus auxiliary boards/displays. Recommend A on
BBB/F446RE and B on F767ZI; the
[hardware fallback plan](../docs/board_project_plan.md) keeps progress independent
of one failed board. No extra calendar blocks or required display/networking
project are added. Neither flagship has been implemented or hardware-verified.

## 2. Preserve Progress, Not An Endless Restart

Historical weeks 1 and 2 remain completed by learner report; the 21 weeks below
number the new project track, not a reset of that progress. Prior boot, FDT, traps,
page-allocation and Sv39 work remains historical evidence; see the tracker.
The latest v1 kernel is superseded and partly broken, not proof of v2 behavior.
Reuse concepts and inspect relevant source; do not assign the entire curriculum
again. The complete former plan/skills are preserved in
[backup/plan](../backup/plan/2026-10-22-foundation/README.md) and
[backup/skills](../backup/skills/2026-10-22-foundation/README.md).

The teaching kernel is long-term depth. Completing buddy/SLUB, a scheduler,
user space, custom SBI extensions or a home-built RTOS is not a prerequisite
for these projects or job applications.

## 3. Budget And Teaching

| Weekly block | Hours | Use |
| --- | ---: | --- |
| Concepts, upstream traces and design | 4 | What/why/how, alternatives and execution context |
| Implementation and debugging | 10 | Current project slice and evidence |
| Applied C practice | 2 | Bounds, ownership, concurrency, parsing, data structures |
| Career and design communication | 2 | Job fit, real-work stories, design summaries, requested mocks |
| Recap | 1 | Optional private reconstruction |
| Buffer | 3 | Setup, overrun, breaks |
| **Total** | **22** | **21 weeks = 462 hours, not all coding time** |

Explain prerequisites inside the current problem. Teach simple English with
deep code and value traces, not shallow analogies. Quizzes remain paused;
practice/assessment is voluntary. Missing evidence stays unassessed rather
than blocking explanations or triggering mandatory tests of the learner.

For each mechanism: QEMU/model first -> source trace -> board transfer where
applicable -> fault case -> measured explanation. QEMU does not certify
electrical timing or emulate every board peripheral. Do not wait until every
foundation is complete before beginning useful hardware work.

### Execution Environment (Updated 2026-10-23)

The learner plans **Ubuntu Server 26.04.1 on an HP x360** as a Linux build
machine. Edit on the current laptop, push approved original source to GitHub,
then pull the chosen revision on the HP to build. The HP's installation,
CPU/RAM/storage and access are not verified. Existing Windows measurements must
not be assumed to describe it.

Recommend the HP as the primary builder after readiness checks. For faster
daily iteration, consider VS Code Remote-SSH: local editor UI, remote source,
build and debugger; GitHub for checkpoints rather than every save. This remains
a recommendation, not a configured workflow or permission to publish.

Native versus container builds depend on the pinned release. Official Yocto 6.0
requirements list Ubuntu 26.04; the earlier 5.0.14 candidate lists Ubuntu 24.04,
not 26.04. A compatible Podman container on the HP can retain the older build
environment. Recheck HP resources and BBB BSP/layers before choosing; documented
resource estimates differ between releases. See the
[HP plan and compatibility table](../docs/workspace_setup.md#planned-hp-linux-build-host).

Keep existing Windows/Podman as a fallback until HP readiness is established.
Its accepted WSL2 backend does not require a separate WSL development workflow.
Previously measured RAM/disk and the learner's planned increase to 200 GB remain
recorded in setup; none verifies the new HP host. No installs, migration, Git
operations or host configuration changes are performed by this plan update.
Test experimental modules in disposable guests/approved boards, not the Linux
build host's kernel. QEMU emulation and hardware evidence remain distinct.

## 4. Schedule

Dates are allocation targets, not promises. Each week has one main outcome.
Use buffer, then drop stretch work, then explicitly rebaseline if core work
cannot fit; never silently add hours or mark partial work accepted.

| Week | Starts | Main work | Evidence target |
| --- | --- | --- | --- |
| 1 | 2026-10-26 | Consolidate existing architecture/C gaps; Linux process/kernel/device boundaries; QEMU readiness and early Yocto host/storage preflight | Source-grounded boot/ownership trace; environment inventory, not a Yocto build |
| 2 | 2026-11-02 | Linux QEMU kernel/configuration, modules, logs, GDB and recovery | Boot/debug record; compatible module lifecycle check |
| 3 | 2026-11-09 | Driver model, DT, probe/remove, resource ownership; A protocol model | Normal and partial-initialization tests using a supported guest device or labelled software model |
| 4 | 2026-11-16 | IRQ/deferred work, locking, waiting and teardown | Context/ownership diagram; failure and cancellation tests; model limits explicit |
| 5 | 2026-11-23 | Recheck Yocto resource gate, pin release/layers, build minimal QEMU image | Build/boot manifest, or explicit blocker with working-guest fallback |
| 6 | 2026-11-30 | Custom Yocto layer, kernel fragment, package earlier driver, SDK/devtool iteration | Rebuild and deploy one small change; identify running version |
| 7 | 2026-12-07 | BBB boot/serial recovery and board resource discovery | Boot approved image; verify selected transport pins/config |
| 8 | 2026-12-14 | A: STM32 timer/ADC/DMA fundamentals and acquisition buffers | Model then board capture; buffer ownership and overrun evidence |
| 9 | 2026-12-21 | A: bounded MCU transport and first Linux IIO read path | Known input through MCU -> Linux -> user, or transport gate blocker |
| 10 | 2026-12-28 | B: Zephyr QEMU threads, priorities, queues and deadlines | Multi-rate model with queue saturation and priority-inversion trace |
| 11 | 2027-01-04 | B: STM32 port, timer/peripheral callbacks and supervision | Independent MCU demo; Saleae markers with measurement conditions |
| 12 | 2027-01-11 | B: diagnostics, stale data, task health and recovery; peer feasibility | Inject a software fault; log detection/recovery; CAN gate decision |
| 13 | 2027-01-18 | A: IIO buffers, trigger semantics, timestamps and recorder service | Pre/post-event capture; sequence and session metadata |
| 14 | 2027-01-25 | A: transport stalls, MCU reset, stop/remove under load | Bounded recovery and no silent corruption in declared tests |
| 15 | 2027-02-01 | B: overload policies, priority inheritance and bounded memory | Before/after scheduling traces; test recovery with slow consumer |
| 16 | 2027-02-08 | B: standalone release tests; two-board CAN only if gated ready | Core MCU acceptance; distinct optional physical-bus evidence |
| 17 | 2027-02-15 | A: Yocto packaging, persistent records, device/service restart | Reproducible deployment and damaged/incomplete-record detection |
| 18 | 2027-02-22 | A: ftrace/performance, lifecycle stress and soak tests | Baseline/change comparison with workload and limits |
| 19 | 2027-03-01 | Both projects: design decisions, release evidence and walkthroughs | Two independently runnable demos and honest portfolio dossiers |
| 20 | 2027-03-08 | Interview-driven gap repair and requested mock rounds | Role-specific feedback; no forced assessment |
| 21 | 2027-03-15 | Protected contingency, unresolved core gates, application follow-up | Final evidence/limits review by 2027-03-21 |

### Bounded Architecture And OS Traces

Each scheduled trace uses **at most two hours of that week's existing four-hour
concept block**, not extra study time. The output is a guided source/value trace
and a short explanation, not mastery, a new kernel implementation or guaranteed
interview readiness. Revisit gaps inside the projects.

| Week | Bounded trace | Scope boundary |
| --- | --- | --- |
| 1 | Compare known RISC-V privilege/address-space concepts with Arm64 roles; identify where the analogy breaks | Do not teach all translation registers, GIC and exception routing at once; defer detailed tables/controllers until relevant |
| 4 | Follow one scheduling decision into saved/restored task state using Linux `task_struct`, reschedule/preemption state and [RISC-V switch code](../../linux/arch/riscv/kernel/entry.S) alongside [scheduler core](../../linux/kernel/sched/core.c) | Pin configuration and trace one path; context-switch state is not a full trap frame |
| 10 | Trace Cortex-M exception entry, MSP/PSP, EXC_RETURN and software-saved state in [Zephyr PendSV](../../zephyr-workspace/zephyr/arch/arm/core/cortex_m/swap_helper.S) | Pin target/configuration and FPU behavior; this is not implementation of a new RTOS |

Use the relevant current source and define terms before registers/macros.
Architecture comparisons do not certify board behavior. The
[SPI/DMA experiment](project_portfolio.md#spi-protocol-and-dma-observation)
uses the existing Week 9 design/trace and Week 18 measurement allocations;
it must not introduce an additional weekly block.

### Weekly C Practice (C01-C21)

One bounded exercise per existing two-hour C block: contract, worked trace,
edge cases, complexity and a short explanation. In Teach mode the tutor models
the solution; learner practice is voluntary. Timed grading happens only on
request. Record tutor demonstrations separately from independent learner work.
Reuse prior exercises where relevant; reported completion is not automatically
runtime or delayed-retention evidence.

Models below are explicit: ISO C language rules, POSIX userspace threading,
Linux kernel APIs/GNU extensions, or MCU/Zephyr context. A userspace atomic
example is not a DMA synchronization recipe. Explain synchronization/lifetime
prerequisites in Weeks 3-4 rather than waiting for the later SPSC exercise.

| Week | ID | Bounded exercise | Model | Completion evidence to seek |
| --- | --- | --- | --- | --- |
| 1 | C01 | Checked header-plus-payload size calculation | ISO C | Units/types explicit; zero, exact-fit and overflow cases |
| 2 | C02 | Initialize three resources with one injected failure and unwind ownership | ISO C model of driver initialization | Each acquired resource released once; no release of unacquired resources |
| 3 | C03 | Model callback registration, in-flight ownership and unregister states | ISO C state model + Linux lifetime trace | No new invocation after closing; active invocation retained until drained; concurrency assumptions explicit |
| 4 | C04 | Stop a producer and wake/drain a waiting consumer | POSIX mutex/condition-variable model + Linux context comparison | Predicate loop, cancellation and bounded completion; no claim that mutexes are ISR-safe |
| 5 | C05 | Parse one bounded frame with unaligned little-endian fields | ISO C | Truncation/bounds checked before reads; no cast-based unaligned access |
| 6 | C06 | Validate sequence/checksum fields of a frame using the chosen checksum contract | ISO C | Known valid, corrupted and missing-field cases; no invented integrity guarantees |
| 7 | C07 | Implement a sequential bounded ring with reject-on-full policy | ISO C, single-threaded | Empty/full/wrap invariants and exact capacity tested |
| 8 | C08 | Model a fixed set of acquisition blocks moving through FREE/FILLING/READY/IN_USE | ISO C state model, then MCU ownership trace | Reject double ownership, early reuse and invalid completion; model does not prove DMA ordering |
| 9 | C09 | Extend a bounded ring to single-producer/single-consumer handoff without overwrite | C11 atomics model; target lock-free/ISR suitability checked separately | Payload publication/reuse ordering and counter wrap; saturation rejects new data |
| 10 | C10 | Allocate/release typed slots from a fixed-block pool | ISO C | Exhaustion and reuse; valid slot/alignment contract; no general-purpose allocator |
| 11 | C11 | Check deadline expiry and sample age across counter wrap | ISO C unsigned arithmetic model | Counter width and maximum interval stated; no unconditional signed-difference trick |
| 12 | C12 | Implement a bounded retry and stale-data recovery state machine | ISO C model + Zephyr event mapping | Retry limit, reset epoch and terminal failure tested |
| 13 | C13 | Slice a pre/post-trigger window across a circular buffer boundary | ISO C | Exact first/last sample, wrap and insufficient-history cases |
| 14 | C14 | Account for sequence gaps, duplicates and device reset | ISO C | Explicit modulo ordering window and session identity; reset is not mistaken for a giant gap |
| 15 | C15 | Compare drop-newest and drop-oldest under a slow consumer | Sequential ISO C model + Zephyr queue API trace | Explicit ownership changes; do not reuse simple SPSC safety arguments for overwrite |
| 16 | C16 | Trace/remove one object from an intrusive list and derive its containing object | Linux/GNU C with ISO `offsetof` distinction | Membership/lifetime/iteration rules; kernel idioms not advertised as universally portable C |
| 17 | C17 | Reserve/release an ID in a fixed bitmap | ISO C single-threaded model | Invalid index, full bitmap and double release; concurrency is a separate contract |
| 18 | C18 | Repair one late-completion versus teardown race in a bounded callback model | POSIX model + Linux quiescence comparison | Producer stop, drain and final release; timeout cannot permit premature free |
| 19 | C19 | Fix one real project parser or buffer regression | Declare the actual project context | Reproducer, minimal repair and boundary regression test |
| 20 | C20 | Walk through one mixed ownership/parsing bug; timed attempt only if requested | Declared ISO/POSIX/kernel context | Explain first invalid operation, repair and tests; no compulsory mock |
| 21 | C21 | Close one remaining project edge-case regression | Actual project context | Targeted evidence; no new topic if contingency/gap repair needs the block |

Protect the final weeks. Buddy/slab index math is optional replacement material,
not an added allocator project or compulsory late syllabus. If an exercise
overruns, reduce scope or carry it forward explicitly; do not accumulate a quiz
backlog or silently spend the buffer twice.

## 5. Application And Scope Gates

Start role research and resume positioning immediately. Begin relevant
applications once a defensible working development slice and honest experience
match exist; a full kernel, both complete projects or an accepted upstream
patch are not universal hiring prerequisites.

| Gate | Required evidence | What it does not mean |
| --- | --- | --- |
| Development story | Working slice, own contribution, one failure investigation, explanation | Automatic Staff readiness |
| A core | Recorder contract in project brief; real data path and lifecycle tests | Certified instrument or industrial deployment |
| B core | Independent RTOS node; scheduling, bounded resources and fault evidence | Certified real-time/safety system |
| Staff-oriented portfolio | Alternatives, measured trade-offs, interface decisions and real-work impact | Principal-level organizational scope by itself |

### Portfolio Review Checkpoints

These are review targets, not automatic completion, new hiring gates or reasons
to delay earlier applications. If evidence is missing, record the gap and use
the actual working slice rather than relabelling it complete.

| Target | Review scope | Honest presentation |
| --- | --- | --- |
| End of Week 9, 2026-12-27 | A v0.1 Linux/MCU slice, one failure investigation and initial design decisions | Apply to matching driver/BSP roles if evidence supports it; v2 is boot/build/firmware, while prior MMU work belongs to v1 history |
| End of Week 12, 2027-01-17 | B standalone v0.1 behavior, task responsibilities and initial recovery evidence | Add firmware/RTOS applications where appropriate; full B release tests and optional CAN may still be pending |
| End of Week 18, 2027-02-28 | Expanded design, fault-injection, Yocto and timing evidence | Target relevant Senior/Lead/Staff roles from actual experience; not a guaranteed "Staff-ready" designation |

Build [decision and failure records](project_portfolio.md#incremental-design-and-failure-evidence)
throughout implementation, within the current concept, implementation and career
blocks; Week 19 assembles the existing evidence rather than inventing it late.

Core precedes extensions. Defer custom DMA controllers, PCIe, USB/network-stack
development, PRU, full Yocto distribution engineering, cloud dashboards,
machine learning, custom RTOS, full kernel rebuild and secure OTA/A-B boot
unless core gates are satisfied and the learner explicitly reallocates time.
Optional I2C sensors and CAN must not consume the core schedule merely to add
protocol names.

## 6. Market Evidence And Compensation

Official examples inspected 2026-10-22:

| Role | Relevant demand and limits |
| --- | --- |
| [Qualcomm Staff Linux kernel 3082351](https://careers.qualcomm.com/careers?pid=446715556302) | Role-specific 10+ years Linux/Arm, upstream and architecture; total C experience is not equivalent |
| [AMD Sr. Firmware 88686](https://careers.amd.com/careers-home/jobs/88686?lang=en-us) | C/C++, OS/system design, automation and debug; possible bridge only where real experience matches |
| [NVIDIA Halos JR2025421](https://nvidia.wd5.myworkdayjobs.com/NVIDIAExternalCareerSite/job/India-Bengaluru/Senior-System-Software-Engineer---Halos-Core-and-Robotics-Platform_JR2025421) | RTOS/Linux/BSP/drivers/debugging; Zephyr practice does not establish production QNX expertise |

These are samples, not a demand ranking or guarantees of vacancy availability.
Refresh during the career block. Freelance client requirements in the project
brief inform usefulness, not Staff-level salaries.

INR 90 LPA is 2.5 times the reported INR 36 LPA baseline. Official sampled roles
did not disclose usable India pay bands. Secondary self-reported
[Qualcomm](https://www.levels.fyi/companies/qualcomm/salaries/software-engineer/locations/india)
and [NVIDIA](https://www.levels.fyi/companies/nvidia/salaries/software-engineer/locations/india)
aggregates show that such total-compensation levels exist, but are not
embedded-only or personal forecasts. Compare fixed cash, bonus, annual equity
vesting and one-off payments separately. Keep INR 90 LPA as the aspiration;
evaluate development ownership and role fit, not title alone.
