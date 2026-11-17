# Active Lesson Library

Read the lesson Markdown first, then ask questions in chat using its title or
section number. Lessons are published progressively, one manageable topic at a
time. Publication does not mean that you have completed or mastered the topic.

The [roadmap](../../plans/five_month_development_roadmap.md) owns dates and hours.
The [tracker](../../plans/training_tracker.md) owns learning status;
the [log](../../plans/learning_log.md) records actual evidence.
This index is a reading guide, not another schedule.

## Week 1 Reading Path

The new project-track Week 1 started early on 2026-10-23. Its planned calendar
and 22-hour allocation remain in the roadmap; historical completed weeks are
not being restarted.

| Order | Topic | Note status |
| --- | --- | --- |
| 1 | [Linux process, kernel and device boundaries](W01_L01_process_kernel_device_boundaries.md) | Published, source-reviewed; revised style approved by learner; understanding not assessed |
| 2 | [System calls and waiting for buffered data](W01_L02_system_calls_and_waiting_for_data.md) | Published; learner-edited explanation preserved; understanding not assessed |
| 3 | [Privilege and address spaces: RISC-V to Arm](W01_L03_privilege_and_address_spaces.md) | Published, source-reviewed; updated in concise Feynman style |
| 4 | [C01: checked header-plus-payload size calculation](W01_L04_c01_checked_packet_size.md) | Published; learner submitted and repaired [C01_packet_size.c](C01_packet_size.c) (source-reviewed; runtime check pending) |
| 5 | [From `start_kernel()` to driver init and QEMU preflight](W01_L05_linux_boot_to_driver_init_and_qemu_preflight.md) | Published, source-reviewed; bridges Week 1 boot/ownership trace to Week 2 QEMU module lifecycle |

Readiness work uses the [setup guide](../../docs/workspace_setup.md) alongside
these topics. No installation, build or board experiment is implied by reading.
Questions and clarifications can be incorporated into the relevant lesson;
new major topics get their own note instead of making the first note endless.

## Reading A Lesson

1. Start with **Know This First**.
2. Follow the problem, design and worked example.
3. Use source links for the small section being explained, not the entire tree.
4. Treat expected/illustrative results differently from measured results.
5. Use the recap and interview explanation for optional private reconstruction.

The learner approved L01's revised approach: explain terms through the project,
use a starting-values table and small worked cases, then minimal source excerpts
with the concrete behavior explained inline. Continue this approach in new
lessons; do not replace it with long code dumps or abstract prerequisite lists.
Preserve the learner's L01/L02 edits. Explain correctness concerns separately
and obtain approval for minimal repairs instead of automatically rewriting notes.

**First implementation:** L04/C01 is a small C function plus boundary tests,
still within Week 1. It does not require Yocto or a full driver. The learner
types the code after explanation; creation/editing of implementation files and
execution require the relevant permission.

Quizzes remain paused. You do not need to submit answers to continue learning.
No future lesson is marked complete merely because its title appears here.

## Authoring Convention

Use `Wxx_Lyy_topic.md` in this folder. `Wxx` refers to the active project-track
week; `Lyy` is the reading order within that week. Every lesson names its scope,
prerequisites, source/evidence status and corrections. Do not pre-generate the
whole curriculum or overwrite historical notes to populate this library.

The [older note guide](../README.md) identifies historical references and
superseded recipes. Learner implementation code remains in its own repositories
or exercises; this folder contains teaching material.
