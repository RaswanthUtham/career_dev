# Five-Month MCU, Firmware And RTOS Development Roadmap

Replaced with learner approval on 2026-10-26. **Only active weekly schedule.**
[Projects](project_portfolio.md), [tracker](training_tracker.md),
[setup](../docs/workspace_setup.md), [role evidence](career_role_evidence.md).

## 1. Goal And Actual Starting Experience

Target interesting Senior/Lead/Staff embedded firmware, MCU peripheral-driver,
RTOS/platform and MCU applications roles in Bengaluru. Principal is a stretch
where actual scope supports it. INR 90 LPA total CTC is an aspiration, not an
outcome promised by this course. Preparation/interviews and aim for an offer:
22 March 2027; joining may follow.

The learner clarified professional work on 32-bit Infineon AURIX systems:
reviewing architecture/interface software-architecture documents, implementing
cyclic functions for motor-control/BMS IC drivers, integrating AUTOSAR modules
with the developed CDD, integration tests and mentoring. This is driver
development **and** verification, not a validation-only baseline.
Safety-related experience reported: participation in FMEA/HAZOP meetings and
assessments with safety engineers, Polyspace/MISRA analysis and requirements
traceability. Independent safety-case ownership/certification is not inferred.

Use those strengths without claiming AURIX/TriCore and Cortex-M are identical.
AUTOSAR is useful employment context, not a required new specialization.
Explain only the boundaries needed to describe actual work accurately.
Employer identity, proprietary protocols and confidential code are not inferred
or publishable from the use of Infineon devices.

## 2. Focus And Exclusions

**FreeRTOS first**, then a bounded **Zephyr** companion project.
Learn drivers and real-time software, not just RTOS API vocabulary.

Keep C, datasheets, startup/linker basics, Cortex-M exception/context state,
SPI/I2C/UART, timers/ADC/DMA, bounded memory, synchronization, fault diagnosis,
testing and measured timing. CAN is hardware-gated. Add MPU/cache concepts when
the target and driver require them.

Remove from the active course: Linux kernel/module/IIO drivers, Yocto, BBB Linux
bring-up, Linux scheduler/MMU implementation and `my-linux` rebuilding.
Native Windows is the primary build/debug host for P1 and initially P2.
The other HP's Ubuntu Server installation is learner-reported; retain it as an
optional alternative, not a setup prerequisite. Podman/SSH/Git are development
tools only. Basic command-line or host-C tests do not create a Linux track.

Preserve all prior C and architecture work. Linux lessons are historical
references, not prerequisites. The
[2026-10-26 snapshot](../backup/plan/2026-10-26-linux-rtos-track/README.md)
preserves the former active plan; the
[2026-10-22 foundation](../backup/plan/2026-10-22-foundation/README.md) is untouched.

## 3. Two Private, Production-Style Learning Projects

- **P1: FreeRTOS cyclic peripheral service and fault recorder.** F446RE,
  asynchronous driver state machine, acquisition, bounded cyclic service,
  diagnostics, recovery and requirement-linked tests.
- **P2: Zephyr diagnostic supervisor and programmable test node.** F767ZI,
  standalone task health/telemetry and repeatable fault scenarios; optional
  peer tests with P1, not a second complete rewrite of its driver.

### Board Decision Confirmed 2026-11-07

- Keep **F446RE + FreeRTOS for P1** and **F767ZI + Zephyr for P2**.
  Existing STM32 lessons, peripheral work and onboard debugging remain the
  main path.
- Pico/RP2040 is a capable alternative for P1's general architecture and
  FreeRTOS learning, but requires different startup, peripheral and port
  configuration. Its Cortex-M0+ cores do not provide the M4's `BASEPRI`.
  Do not restart P1 solely to obtain two cores.
- Keep Pico setup bounded to a blink/toolchain check. Learner reports a UF2
  is available; upload, physical blink and SWD debugging remain unverified.
  See [setup](../docs/workspace_setup.md#pico-usb-detection-and-windows-setup-2026-11-07).
- After single-core FreeRTOS foundations and the P1/P2 core gates, Pico may
  fill the existing Week 17 optional slot with one producer/consumer multicore
  experiment. Establish a single-core baseline first, then test cross-core
  ownership and synchronization with a supported, pinned FreeRTOS RP2040 port.
  This replaces another optional extension, not a third full project.
- No extra hours or parallel MCU curriculum: retain 22 hours/week and defer
  the Pico experiment if core work needs the optional slot.

These resemble real product-engineering problems; they are not claimed to be
deployed MNC products. Their purpose is stronger understanding and potential
ideas for work, not public GitHub display. Keep private unless separately
approved. If an employer later authorizes adoption, record the actual scope,
review, delivery and measured impact before describing it as workplace work.
Architecture ownership is claimed only for architecture actually owned.

## 4. Time And Teaching Contract

| Weekly block | Hours | Work |
| --- | ---: | --- |
| Foundations, source traces, design | 4 | What -> why/failure -> options -> how |
| Implementation and debugging | 10 | One project layer, including tests and measurements |
| Applied C | 2 | One bounded exercise tied to that layer |
| Career and design communication | 2 | Role fit, genuine work stories, decision records, requested mocks |
| Recap | 1 | Optional private reconstruction |
| Buffer | 3 | Setup, overrun and breaks |
| **Total** | **22** | **21 weeks = 462 hours, including buffer** |

Use simple English, concrete inputs, Before/After diagrams and small annotated
source excerpts. Preserve approved lesson wording; report inaccuracies and ask
before repairing existing lessons. Markdown-first, no compulsory quizzes.
The learner types project code. Do not produce the entire implementation for
them or infer mastery from tutor-run examples.

Hardware-first, per learner request on 2026-10-26: use F446RE for P1 and F767ZI
for P2 after readiness checks. No QEMU/emulator installation or exercise is
required. Optional host-C tests check pure logic; synthetic device/fault inputs
can run on a real board. Neither replaces physical peripheral/timing evidence.

## 5. Weekly Schedule

One primary result per week. Use buffer, then drop optional features, then
explicitly rebaseline if needed. Do not silently add hours. Week 1 includes the
early C01 work; no restart because the career direction changed.

| Week | Starts | Main work | Evidence target |
| --- | --- | --- | --- |
| 1 | 2026-10-26 | Confirm C01 repair/runtime, board/toolchain readiness; driver requirements and ownership | Known-source build path; C01 results if run; P1 requirements, not a completed driver |
| 2 | 2026-11-02 | Cortex-M4 instruction sets & instructions (`W01-L15`), core/reset registers (`W01-L16`), bare-metal startup/clock/EXTI interrupt handoff (`W01-L17`), and Cortex-M4 MPU/Privilege (`CONTROL`)/`PSP`-`MSP`/`MemManage` fault basics (`W01-L18`) using complete bare-metal (linker script only; custom C `Reset_Handler`, MMIO structs & inline asm) before switching to CMSIS headers & `PendSV` (`W01-L19`) | Small board path, bare-metal startup/interrupt trace, and MPU/privilege boundary check; no new RTOS kernel |
| 3 | 2026-11-09 | FreeRTOS task states, priorities, periodic work and notifications | Two tasks and an event producer; measured/illustrative timing distinguished |
| 4 | 2026-11-16 | Queue/mutex/ISR API rules, critical sections and priority inversion | Controlled inversion trace and repair; configuration/IRQ limits recorded |
| 5 | 2026-11-23 | P1 driver contracts, register protocol and asynchronous transfer state machine | Initialization/read-back and injected error tests using public/synthetic device |
| 6 | 2026-11-30 | Bounded cyclic service, timeout/cancel, late completion and recovery | Small driver vertical slice; requirement-linked integration test |
| 7 | 2026-12-07 | Timer/ADC acquisition and DMA block ownership | Known input, buffer state transitions and overflow reporting |
| 8 | 2026-12-14 | Transport framing, sequence/reset identity and fault recording | Recording/export path; integrity and stale-data checks |
| 9 | 2026-12-21 | CPU/stack/memory budgets, scheduling and acquisition faults | Measured baseline, resource limits and failed/recovered cases |
| 10 | 2026-12-28 | P1 core integration, repeatable tests and release checkpoint | Standalone P1 demo; candidate work-improvement design note |
| 11 | 2027-01-04 | Zephyr build/device model, Kconfig/devicetree, threads/queues | P2 board starts; compare with learned FreeRTOS mechanisms |
| 12 | 2027-01-11 | P2 periodic sensing, diagnostics and task-progress supervision | Stale data/stuck task detection without a Linux host |
| 13 | 2027-01-18 | P2 deterministic fault scenarios, bounded command interface and recovery | Test scripts/scenarios and observable state transitions |
| 14 | 2027-01-25 | P2 integration; peer protocol capability check | Independent P2 demo; optional physical peer gate resolved/deferred |
| 15 | 2027-02-01 | Fault campaign across memory, transfer, task and reset boundaries | Expected/observed fault matrix with bounded waits |
| 16 | 2027-02-08 | F767 cache/MPU and DMA memory placement where used; timing investigation | Evidence for selected memory path; no automatic certification claims |
| 17 | 2027-02-15 | One optional extension: physical CAN OR display load OR Ethernet diagnostics OR bounded Pico multicore producer/consumer experiment | Only if P1/P2 cores are sound; selected extension's normal/fault evidence, otherwise core gap repair |
| 18 | 2027-02-22 | Traceability, static analysis, targeted coverage and release reproducibility | Test-to-requirement map, limitations, configuration/source manifest |
| 19 | 2027-03-01 | Project/real-work deep dives and design trade-offs | Explain own contributions and measurements; honest role positioning |
| 20 | 2027-03-08 | Interview-driven gap repair and requested mocks | Role-specific feedback, no automatic quiz |
| 21 | 2027-03-15 | Protected contingency and remaining core defects | Final evidence review by 2027-03-21; no new major feature |

### Weekly C Practice (C01-C21)

Each row fits the existing two-hour block. ISO C models do not automatically
prove ISR safety or DMA ordering; compare them with the actual target APIs.
No new implementation files are created by this plan.

| Week | ID | Bounded exercise | Evidence |
| --- | --- | --- | --- |
| 1 | C01 | Existing checked frame-size function | Preserve repaired source; run declared boundary tests only with permission |
| 2 | C02 | Three-resource initialization and unwind | Fail each acquisition; release only owned resources |
| 3 | C03 | Event producer/consumer ownership model | Valid payload lifetime and explicit handoff |
| 4 | C04 | Stop/drain a producer-consumer pair | RTOS rules; no reuse before completion |
| 5 | C05 | Bounded register-frame parser | Endian, truncated, invalid-length and reserved-field cases |
| 6 | C06 | Timeout/cancel state machine | Late callback and bounded retry outcomes |
| 7 | C07 | Sequential fixed-capacity ring | Empty/full/wrap and one overload policy |
| 8 | C08 | Acquisition block pool | FREE/FILLING/READY/IN_USE transitions; no double ownership |
| 9 | C09 | SPSC handoff | C11 model plus target atomic/ISR analysis; no automatic drop-oldest extension |
| 10 | C10 | Fixed typed-object pool | Exhaustion/reuse and memory budget |
| 11 | C11 | Wrap-safe deadline/sample-age math | Width and maximum comparison interval explicit |
| 12 | C12 | Useful-progress supervisor state | Differentiate running from making progress |
| 13 | C13 | Circular pre/post-event window | Boundary, wrap and insufficient-history cases |
| 14 | C14 | Sequence gaps versus reset epoch | Duplicate/stale/rollover cases |
| 15 | C15 | Slow-consumer overload comparison | Drop-newest/drop-oldest ownership differences |
| 16 | C16 | Intrusive list reasoning | Valid containing-object/lifetime contract; no required Linux macros |
| 17 | C17 | Fixed bitmap IDs | Full/invalid/double-release cases |
| 18 | C18 | Deferred callback teardown regression | Drain before free; timeout is not permission to reuse |
| 19 | C19 | Actual project bug repair | Reproducer, minimal fix and regression |
| 20 | C20 | Mixed C/debug walkthrough | Timed only on explicit request |
| 21 | C21 | Remaining project edge case | No new topic if contingency needs the block |

## 6. Career Entry And Evidence Gates

Start Bengaluru role matching and recruiter conversations now, emphasizing
actual AURIX driver development, AUTOSAR integration and verification strengths.
Apply earlier if a role genuinely fits; do not wait for an entire course.

| Review target | Evidence to present | Not an automatic conclusion |
| --- | --- | --- |
| End Week 6 | Working RTOS driver slice and one fault investigation | Staff-ready or proven safety expert |
| End Week 10 | P1 demonstration, design alternatives and measurements | Delivered at an employer |
| End Week 14 | Independent P2 and explainable RTOS differences | Expert in every FreeRTOS/Zephyr feature |
| End Week 19 | Portfolio of actual code, requirements/tests, debug and workplace impact | Guaranteed title, salary or certification |

Do not select job titles solely from broad employee salary averages. Prioritize
responsibility match, interesting development work, level and recruiter-confirmed
compensation. Include MCU applications/platform roles, not only product roles.
Board brand, price and core count do not establish interview readiness or a
salary outcome. Emphasize driver decisions, debugging, ownership, measurements
and genuine professional contributions; Pico work is optional supporting evidence.
Preserve existing AUTOSAR experience on the CV accurately without adding an
AUTOSAR specialization. Optional upstream work requires separate publication
approval and is not a course gate.
