# Career Development Workspace

Start with [workspace setup](docs/workspace_setup.md), then the [weekly curriculum](plans/five_month_development_roadmap.md) and [Linux-aligned teaching-kernel milestones](plans/scratch_riscv_os_plan.md). Use the [training tracker](plans/training_tracker.md) for current status and evidence pointers; the [learning log](plans/learning_log.md) preserves dated history. The former eight-week schedule is a compatibility redirect. Advanced applied C and implementation ownership are the starting point, not elementary syntax.

The goal is a validation-to-firmware/embedded-development transition, targeting INR 70-90 LPA CTC in Bengaluru, with Hyderabad secondary; no pay or employment outcome is guaranteed. Weeks 1 and 2 are complete by learner report. The confirmed future allocation is 22 hours/week: xv6 and the learner-authored `my-linux` foundation -> Cortex-M/Zephyr -> embedded Linux -> kernel drivers -> hardening. Original week-3-to-20 dates are historical targets pending rebaseline; do not compress later prerequisites to preserve them. Progress means explaining, tracing, implementing and debugging, not merely reading material.

## Current Teaching Approach

The learner reports 10+ years using C and intermediate proficiency. Teach advanced problems in accessible language: a real driver/parser problem, concrete C code, definitions at first use, a worked trace, one failure and repair, then a short recap. Priorities from the conversation are allocation lifetime, overflow-safe buffer APIs and thread synchronization. Do not infer a global proficiency level from years or isolated answers.

**Questions are paused until explicitly resumed.** Default to Teach, review volunteered answers when provided, and use Assess only on request. Continuing lessons or implementing files does not restart quizzes. Optional private Feynman practice means explaining the mechanism on blank paper to an imagined 12-year-old; no submission is required. Retention and runtime results stay unverified without actual evidence.

## Teaching Skills

The versionable teaching package lives in this repository, with no dependency on a particular user-profile directory:

| Skill | Use |
| --- | --- |
| [learning-feynman-coach](.github/skills/learning-feynman-coach/SKILL.md) | Shared Teach/Review/Assess modes, question control, optional blank-paper practice and evidence-based feedback |
| [learning-c-cpp](.github/skills/learning-c-cpp/SKILL.md) | C/C++, pointers, lifetime, ownership and interview coding |
| [learning-riscv-xv6](.github/skills/learning-riscv-xv6/SKILL.md) | Source-grounded RISC-V and xv6 |
| [learning-arm-rtos](.github/skills/learning-arm-rtos/SKILL.md) | Arm, MCU peripherals, Zephyr and RTOS |
| [learning-linux-development](.github/skills/learning-linux-development/SKILL.md) | Linux kernels/drivers, debugging and career preparation |

[Workspace instructions](.github/copilot-instructions.md) route lessons to the shared method and the relevant domain. These are revised workspace counterparts to the earlier personal teaching skills, not edits to global profile files. Distinct names avoid same-name collisions; they do not automatically disable older skills.

In a fresh VS Code chat, check Configure Chat -> Chat Customizations -> Skills (or `/skills`) and invoke `/learning-feynman-coach` for a worked lesson, such as "Explain one-time producer/consumer publication with a concrete C example; no questions." If discovery has not refreshed, reload the window. When studying code in another workspace root, explicitly invoke the appropriate `learning-*` skill or attach its file. Do not assume cross-root automatic activation solely because files exist.

Smoke test: request the worked lesson above; expect the problem, C example, plain-language terms, order trace and recap, with no compulsory question or exercise at the end. Submit an answer voluntarily; expect explicit grading and correction without an automatic next quiz. Explicitly request assessment to test one-scenario-at-a-time behaviour. For selected source, expect actual definition/caller inspection before explanation. Verify loaded paths in chat customization diagnostics where available. Fresh-chat discovery and interactive behaviour are not proven by document/link checks alone.

## Repository Layout And Migration

Keep the learning repository and source forks separate. The approved target layout is:

```text
career_ws/
  career_dev/             plans, notes, own exercises, .github/skills
  xv6-riscv/              your xv6 fork
  linux/                  your Linux fork
  my-linux/               learner-created teaching kernel (planned, not yet present)
  zephyr-workspace/
    zephyr/               your fork as the west manifest repository
    modules/              fetched by west as required
```

Source-clone availability and build status are recorded in the [setup guide](docs/workspace_setup.md). The [prepared workspace](career_dev.code-workspace) selects the four existing reference roots; add `my-linux` only after the learner creates it. The six migrated xv6 utilities passed earlier QEMU smoke checks. Zephyr west/SDK and Linux guest/module readiness remain pending. Do not vendor OS trees into this repository.

| Source | Action |
| --- | --- |
| [MIT xv6-RISC-V](https://github.com/mit-pdos/xv6-riscv) | Fork for source exercises; course-specific lab starters/graders may differ |
| [Zephyr](https://github.com/zephyrproject-rtos/zephyr) | Fork; initialize using `west` and the chosen release's setup guide |
| [Linux](https://github.com/torvalds/linux) | New local clone uses RaswanthUtham/linux-mainline; the separate BeagleBoard fork RaswanthUtham/linux is unchanged |
| [GitHub repositories](https://github.com/RaswanthUtham) | career_dev SSH origin and source-fork remotes verified locally; review content before publishing |

Before moving, inspect status, local branches/commits and remotes in each existing repository. Preserve modified and untracked files, local container recipes and teaching material. Set `origin` to the user's fork and `upstream` to the original only after reviewing existing configuration. Validate fresh builds before removing old roots from VS Code. Removing a workspace root is not disk deletion.

The user supplied the source clones and explicitly authorized retirement of four old folders after preservation. Full private recovery copies contain 114,383 hash-verified files. Old Zephyr is deleted; three empty old directory shells remain locked by Windows processes, as detailed in the setup guide. Employer directories remain on disk, outside this workspace. No commits/pushes or profile-skill changes were performed. Each fork is pushed separately from career_dev. Exact SDK/kernel/flash commands depend on the selected release and board.

## Before Publishing

- Obsolete schedules were removed after progress preservation. Their private backup still contains personal information; it must not be uploaded. Review remaining notes and learning records before publishing too.
- Review existing notes and downloaded book material for licensing and attribution. Link to official references rather than republishing documents without permission.
- Exclude employer CDD code, test data, logs, confidential schematics, credentials and private interview/application records. A neighbouring repository being accessible is not permission to publish it.
- Do not add binaries, build trees, downloaded toolchains or operating-system clones to the learning repository.
- Preserve licenses and attribution for any reused public code. Clearly distinguish original projects, adaptations, simulated results and measured hardware evidence.

Existing original deep-dive notes remain references. The old schedules no longer compete with the new curriculum; their recorded progress is preserved in the learning log, with historical status clearly separated from fresh verification.