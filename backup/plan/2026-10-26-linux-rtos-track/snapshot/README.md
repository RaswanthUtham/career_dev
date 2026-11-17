# Embedded Systems Development Workspace

Start with [workspace readiness](docs/workspace_setup.md), then the
[active five-month roadmap](plans/five_month_development_roadmap.md).
The [project briefs](plans/project_portfolio.md) define the products and
acceptance gates; the [tracker](plans/training_tracker.md) records evidence.
The [learning log](plans/learning_log.md) preserves dated history.

## Goal And Projects

Prepare for embedded Linux/driver and firmware roles, with Staff/Principal-level
design reasoning and an aspirational INR 90 LPA total CTC goal. Preparation and
active interviews run through 22 March 2027; an offer is the aim, not a promise.
Joining may follow. The weekly budget is 22 hours.

The focus is embedded software, not general desktop/backend/server development.
OS and systems-programming concepts are supporting tools for device access,
firmware concurrency, buffer ownership and embedded Linux services, not a
separate career track.

- **A: Embedded fault/event recorder.** STM32 acquisition firmware and a BBB
  Linux IIO client driver, packaged with Yocto. Record low-voltage signals around
  an event and diagnose data loss, latency and reset behavior.
- **B: Standalone Zephyr telemetry and health-monitoring node.** Multi-rate
  tasks, bounded queues, overload policy, priority inversion, watchdogs and
  recovery. A second MCU/CAN extension depends on confirmed hardware.

  The [2026-10-25 photo inventory](docs/board_inventory.md) identifies the available
  boards and displays. Recommended roles: BBB/F446RE for A, F767ZI for B, with
  Pico/ESP32 as conditional spares or fixtures. The
  [board failure/fallback plan](docs/board_project_plan.md) keeps software learning
  moving while failed hardware gates remain explicit.

QEMU comes first for software mechanisms. Hardware then verifies what simulation
cannot: actual peripherals, timing, wiring and DMA behavior. Neither project is
claimed implemented, production-qualified or evidence of mastery yet.

## Teaching

Use simple English: what, why with a failure, design alternatives, how with deep
code traces, then recap. Quizzes stay paused unless requested. The learner
authors implementation; the tutor explains and reviews it.

The learner plans an HP x360 running Ubuntu Server 26.04.1 as a build machine,
with source edited on the current laptop and synchronized through GitHub.
Remote-SSH is a suggested faster edit/build loop; neither workflow is configured
yet. See [environment choices and readiness](docs/workspace_setup.md#planned-hp-linux-build-host).
The current Podman environment remains a fallback. The build host is not the
embedded target and installing Ubuntu Server does not add server-development
topics to the curriculum.

Read substantial lessons in the [Markdown lesson library](notes/lessons/README.md),
then ask questions in chat. Start with
[Week 1's process/kernel/device lesson](notes/lessons/W01_L01_process_kernel_device_boundaries.md).
Future notes are published progressively; reading/publication does not
automatically complete a topic.

| Active skill | Purpose |
| --- | --- |
| [Feynman coach](.github/skills/learning-feynman-coach/SKILL.md) | Shared teaching and evidence contract |
| [C/C++](.github/skills/learning-c-cpp/SKILL.md) | Applied C, ownership, concurrency and interview coding |
| [Linux](.github/skills/learning-linux-development/SKILL.md) | Linux drivers, Yocto/BSP, tracing and design |
| [Arm/RTOS](.github/skills/learning-arm-rtos/SKILL.md) | STM32 and standalone Zephyr project |
| [RISC-V/xv6](.github/skills/learning-riscv-xv6/SKILL.md) | Existing architecture foundations, on demand |
| [Lesson reviewer](.github/skills/learning-lesson-review/SKILL.md) | Read-only correctness/fact/example audit; comments only for evidenced checklist violations |

The [workspace instructions](.github/copilot-instructions.md) route these skills.
Reload/check VS Code skill discovery after changes; file validation alone does
not prove fresh-chat activation. Keep source repositories as independent siblings.

## Preserved Long-Term Foundation

The previous working material, including uncommitted edits, was preserved before
the active rewrite:

- [Plans, notes and context](backup/plan/2026-10-22-foundation/README.md)
- [Five original teaching skills](backup/skills/2026-10-22-foundation/README.md)

All 37 copied files were hash-verified. The snapshot is a long-term reference,
not another active calendar; it is not an off-disk backup. Existing historical
notes remain unchanged and may contain superseded code or known defects.
Use the [active note guide](notes/README.md), not old next-action statements.

No employer material, source clones, downloaded books or dependency trees were
added to the backup. Review personal records, licenses and attribution before
publication. No commit, push, hardware purchase or flash is implied.
