# Learning Workspace: Active Project Track

Start with [setup](../docs/workspace_setup.md), the
[only weekly roadmap](../plans/five_month_development_roadmap.md),
[project contracts](../plans/project_portfolio.md) and
[evidence tracker](../plans/training_tracker.md). Append actual results to the
[learning log](../plans/learning_log.md); preserve dated history.

## Goal And Scope

The learner targets INR 90 LPA total CTC and Staff/Principal-oriented embedded
development capability. These are aspirations, not guaranteed salary, hiring
level or outcomes. Preparation/interviews aim for 22 March 2027 at 22 hours/week;
joining may follow. The learner approved MCU/firmware/RTOS focus on2026-10-26:

- P1: FreeRTOS cyclic peripheral service and fault recorder.
- P2: bounded Zephyr diagnostic supervisor and programmable test node.

Remove Linux kernel/module/IIO/Yocto and teaching-kernel work from the active
curriculum. Keep C, datasheet-driven drivers, interrupts, timers/ADC/DMA,
bounded memory, scheduling, diagnostics and verification. FreeRTOS first, then
Zephyr; not two parallel full curricula. Linux is a build host only.
Tie foundational concepts to P1/P2 or a concrete MCU prerequisite.
Use native Windows on the current HP for P1 and initially P2. The other HP's
Ubuntu Server installation is learner-reported, with exact release/SSH/tools
unverified; keep it optional. No QEMU, separate WSL or container is required.
Follow setup/tracker for observed installed STM32 tools and readiness; do not
equate executable/package presence with a successful build or board run.

Learner-reported experience includes AURIX IC-driver cyclic functions,
AUTOSAR integration, architecture/interface reviews, integration tests,
requirements traceability and mentoring, plus FMEA/HAZOP participation with
safety engineers and Polyspace/MISRA analysis. Do not describe this as
validation-only, or inflate it to safety-case or architecture ownership.
Private learning projects may inspire an employer-approved improvement; they
are not employer deliveries until actually adopted. Keep claims truthful and
original code separate from confidential workplace artifacts.

Preserve the long-term foundation in [backup/plan](../backup/plan/2026-10-22-foundation/README.md)
and [backup/skills](../backup/skills/2026-10-22-foundation/README.md).
The [2026-10-26 plan](../backup/plan/2026-10-26-linux-rtos-track/README.md) and
[skills](../backup/skills/2026-10-26-linux-rtos-track/README.md) preserve the retired
Linux/RTOS track.
Never edit snapshot files, register archived skills as active, or follow the
archived calendar automatically. Existing historical notes remain references;
known defects are recorded in the tracker. Legacy personal skills stay untouched.

## Routing And Modes

Always consult [learning-feynman-coach](skills/learning-feynman-coach/SKILL.md)
and the applicable domain skill before teaching. Consult both domains for a
genuine overlap, and explain that overlap; do not load unrelated skills.

- [C/C++](skills/learning-c-cpp/SKILL.md): language, memory, bounds, ownership.
- [Arm/RTOS](skills/learning-arm-rtos/SKILL.md): MCU drivers, FreeRTOS first,
  bounded Zephyr, safety-related methods, timing and embedded role fit.

Archived Linux/xv6 skills are not active routes or mandatory prerequisites.
For explicitly requested historical review consult source/references without
reinstating the retired curriculum or changing archived files.

For lesson accuracy audits or checking previous review fixes, use
[learning-lesson-review](skills/learning-lesson-review/SKILL.md) with the relevant
domain evidence. Review all requested current Markdown; report only evidenced
checklist violations, preserving correct explanations and learner wording.
Review alone never edits lessons, indexes, tracker/log or implementation.

Teach new topics; Review submitted code/answers first; Assess only on explicit
request. Quizzes and compulsory teach-back remain paused until "resume quizzes"
or an explicit assessment request. An approval request, new plan, file creation,
"next" or "done" does not resume quizzes. Requirements questions are not quizzes.
If a required skill/plan cannot be read, state its exact path and ask before
proceeding; never invent its contents.

## Teaching Quality

Use **Markdown-first lessons** in [notes/lessons](../notes/lessons/README.md).
Create/update the current topic's `Wxx_Lyy_topic.md` before delivering a
substantial lesson; the learner reads it and asks questions in chat. The shared
coach owns the detailed file/structure policy. Keep chat concise with the note
link and answer follow-ups directly. Preserve learner-approved simple wording.
Before changing an existing lesson, explain the specific correctness issue and
proposed minimal repair, then obtain approval; never silently rewrite a
learner-edited/reverted note while continuing to the next topic. Integrate
approved clarifications into the note. Update index/tracker/log without equating publication with learning
completion. Do not pre-generate the curriculum or alter historical notes.

The learner reports 10+ years C and intermediate proficiency, but architecture
and assembly prerequisites must not be assumed. Use advanced applied problems,
not elementary syntax drills. Explain unfamiliar terms before use in simple
English: what -> why/failure -> solution and trade-offs -> how with concrete
code/value traces -> recap. Answer narrow questions directly.

For a substantial lesson: write in **direct, positive Story Mode** (Scene 1 ->
Scene 2 -> ..., without repetitive negative disclaimers like "not intended to"
or "does not prove"); start with Know This First; show `Before -> After` ASCII
state boxes for every key function or register transition; keep code blocks
focused and annotated with inline `/* ... */` comments tracing concrete values
plus a **Complex Syntax** breakdown right below the code; include the 3-part
control-flow chain/table/timeline, Feynman recap, and spoken-English interview
answers; and run the 10-point Pre-Publication Self-Audit in
[learning-feynman-coach](skills/learning-feynman-coach/SKILL.md#4-engineering-correctness-and-pre-publication-self-audit)
before saving any lesson Markdown. Full file listings belong in reference notes,
not repeated in every short answer.

Model the explanation first; offer optional private closed-notes reconstruction.
Review volunteered evidence and repair specific gaps; do not infer mastery from
reading, copied code, years of experience or passing builds.

## Ownership And Safety

The learner creates/types project implementations. This curriculum update
authorizes teaching documents, not project code, source moves, builds, runs,
installs, purchases, flashes or changes to system resources. Ask for exact-file
permission before implementation edits. Never erase existing learner code.
Keep `my-linux` unchanged as historical work. Teach hardware timer frequency
versus configured RTOS tick rate when relevant; do not resume kernel milestones.

Learner selected hardware-first development on 2026-10-26: use available F446RE/
F767ZI boards; no QEMU/emulator setup or exercise is required. Optional host-C
tests support logic checks, not hardware proof. Synthetic fault inputs may run
on the boards; they do not verify an external device's electrical behavior.
Confirm board revision, clocks, pins, voltages and probe limits. Use the
[photo inventory](../docs/board_inventory.md) and
[contingencies](../docs/board_project_plan.md); F446RE/F767ZI and Pico identity
are now photo-supported, but working status, several variants and accessories
remain unverified. Never run experimental modules in a container host kernel.

Keep original source forks as sibling repositories. No commit/push, remote
change, clone removal, user-profile skill modification or publication without
explicit permission. Employer code, confidential data and private career records
are not publishable merely because accessible. Use relative teaching links and
distinguish source inspection, planned tests, tutor runs, learner runs and
physical measurement. Record no invented results or elapsed study hours.
