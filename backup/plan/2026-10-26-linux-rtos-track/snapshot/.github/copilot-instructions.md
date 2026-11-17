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
joining may follow. Prioritize applied Linux driver development and standalone
MCU/RTOS work through:

- A: Linux-connected fault/event recorder.
- B: standalone Zephyr telemetry and health-monitoring node; physical CAN is
  gated on the second MCU and transceivers.

Keep the career curriculum embedded-focused: firmware, peripherals, RTOS, BSP,
embedded Linux drivers and acquisition/diagnostic services. Systems-programming
fundamentals are necessary supporting mechanisms, not a general desktop/backend/
server-development track; tie them to A/B or a specific embedded prerequisite.
The planned HP x360 Ubuntu Server host is build infrastructure, not the target
product. Follow setup/tracker for native-versus-container and host readiness;
do not assume the existing Windows/Podman inventory describes the HP.

Preserve the long-term foundation in [backup/plan](../backup/plan/2026-10-22-foundation/README.md)
and [backup/skills](../backup/skills/2026-10-22-foundation/README.md).
Never edit snapshot files, register archived skills as active, or follow the
archived calendar automatically. Existing historical notes remain references;
known defects are recorded in the tracker. Legacy personal skills stay untouched.

## Routing And Modes

Always consult [learning-feynman-coach](skills/learning-feynman-coach/SKILL.md)
and the applicable domain skill before teaching. Consult both domains for a
genuine overlap, and explain that overlap; do not load unrelated skills.

- [C/C++](skills/learning-c-cpp/SKILL.md): language, memory, bounds, ownership.
- [Linux](skills/learning-linux-development/SKILL.md): drivers, kernel, Yocto,
  BSP, IIO, tracing and career/role fit.
- [Arm/RTOS](skills/learning-arm-rtos/SKILL.md): STM32 peripherals, Zephyr,
  real-time scheduling and hardware measurement.
- [RISC-V/xv6](skills/learning-riscv-xv6/SKILL.md): requested architecture
  foundation or teaching-kernel work, not an automatic prerequisite ladder.

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

For a substantial lesson: Know This First; one small runnable layer; one unit
per changed file with Why/What/Technical/Code/How/Syntax; input-to-output traces;
how components fit together; control-flow chain/table and privilege timeline
where relevant; evidence; limits; short interview answers. Full file listings
belong in reference notes, not repeated in every short answer.

Model the explanation first; offer optional private closed-notes reconstruction.
Review volunteered evidence and repair specific gaps; do not infer mastery from
reading, copied code, years of experience or passing builds.

## Ownership And Safety

The learner creates/types project implementations. This curriculum update
authorizes teaching documents, not project code, source moves, builds, runs,
installs, purchases, flashes or changes to system resources. Ask for exact-file
permission before implementation edits. Never erase existing learner code.
For `my-linux`, trace xv6/upstream first; keep kernel S-mode and firmware separate.
Teach counter frequency versus HZ before revisiting timers.

Use QEMU/models first for software mechanisms, then hardware for physical claims.
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
