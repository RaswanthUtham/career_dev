# MCU, Firmware And RTOS Learning Workspace

Start with the [active roadmap](plans/five_month_development_roadmap.md),
[private project contracts](plans/project_portfolio.md),
[tracker](plans/training_tracker.md) and [build setup](docs/workspace_setup.md).

## Focus

FreeRTOS-first MCU/peripheral-driver development, followed by a bounded Zephyr
project. Linux kernel development, Yocto and teaching-kernel rebuilding are
**not active course requirements**. Native Windows is the primary build/debug
host; the other HP's learner-reported Ubuntu Server installation is optional.
Existing Podman remains available for host-C checks, not mandatory board setup.
Use the available F446RE/F767ZI hardware after readiness checks; no QEMU or
emulator is required. Optional host-C tests remain useful for logic checks.

The learner reports actual AURIX driver development and testing: cyclic
functions for motor/BMS ICs, AUTOSAR integration, architecture/interface
reviews, integration tests, traceability and mentoring; FMEA/HAZOP participation
and Polyspace/MISRA work. Preparation builds on that rather than treating the
learner as validation-only or restarting elementary C.

INR90LPA total CTC and Staff/Principal-oriented reasoning remain aspirations, not
guaranteed salary/title. Target preparation/interviews by22March2027,22h/week.
See [official job examples and compensation limits](plans/career_role_evidence.md).

## Projects

- **P1:** FreeRTOS cyclic peripheral service and fault recorder, F446RE first.
- **P2:** Zephyr diagnostic supervisor and programmable test node, F767ZI first.

Private production-style learning initially; potential ideas for employer review
later. No new project is described as an employer delivery until actually
authorized/adopted, and only genuine contributions are claimed. No proprietary
work material, public showcase, high-energy control or safety certification is
implied.

[Photo inventory](docs/board_inventory.md) and
[MCU fallbacks](docs/board_project_plan.md) separate model identification from
working hardware. No code/board implementation is created by the curriculum.

## Teaching And Skills

Read [Markdown lessons](notes/lessons/README.md), then ask questions. Simple
English, what/why/how, Before/After diagrams, concrete code/value traces and
short recaps. Preserve learner-approved text; ask before existing-note repairs.
Quizzes remain paused and source/runtime/competence evidence stays separate.

| Active skill | Use |
| --- | --- |
| [Shared coach](.github/skills/learning-feynman-coach/SKILL.md) | Markdown-first teaching and evidence rules |
| [C/C++](.github/skills/learning-c-cpp/SKILL.md) | Applied language, ownership and concurrency |
| [MCU/FreeRTOS/Zephyr](.github/skills/learning-arm-rtos/SKILL.md) | Drivers, scheduling, hardware and safety-related methods |
| [Lesson reviewer](.github/skills/learning-lesson-review/SKILL.md) | Comments only for evidenced accuracy violations |

Linux/xv6 skills are archived, not active discovery routes. Reload/check VS Code
customization discovery if needed; static files do not prove fresh-chat routing.

## Preserved History

- [2026-10-22 foundation plan](backup/plan/2026-10-22-foundation/README.md) and
  [skills](backup/skills/2026-10-22-foundation/README.md):37verified original files.
- [2026-10-26 pre-pivot track](backup/plan/2026-10-26-linux-rtos-track/README.md)
  and [skills](backup/skills/2026-10-26-linux-rtos-track/README.md):43verified files.
- [Learning log](plans/learning_log.md): dated history preserved, not rewritten.

Existing source siblings and historical lessons stay untouched. Backups contain
private career/learning information and are not independent disk-failure copies.
No commit/push, cleanup, install or flash without explicit permission.
