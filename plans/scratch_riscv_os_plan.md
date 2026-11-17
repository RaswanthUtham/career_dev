# Teaching Kernel: Preserved Long-Term Foundation

The full previous milestone guide is preserved unchanged in the
[foundation snapshot](../backup/plan/2026-10-22-foundation/snapshot/plans/scratch_riscv_os_plan.md).
Its paired [original teaching skills](../backup/skills/2026-10-22-foundation/README.md)
are long-term references.

The [active roadmap](five_month_development_roadmap.md) is the only schedule.
Linux kernel development is outside the active MCU/FreeRTOS-first track.
Do not make completion of K0-K10 a prerequisite for current firmware applications.
Use [current project briefs](project_portfolio.md) and
[tracker](training_tracker.md) for active scope and evidence.

Keep `my-linux` v2 learner-owned and unchanged during this planning task.
`main`/`v1` and `v1-reference` remain historical; no checkout, move, build or
repair is implied. Preserve original scratch-kernel Days 1-8 and weeks 1-2
evidence without treating it as proof about a new build.

On an explicitly requested kernel lesson, trace current xv6 and upstream Linux
first; the learner creates/types all active files. Start the main kernel in
S-mode under OpenSBI, with M-mode firmware as a separate comparison.
Explain timebase-frequency versus configured HZ before timer code.

Before resuming old milestones, correct the known plan hazards: no stored
high-linked function-pointer dispatch before the MMU unless address handling
is proven; derive virtual entry from the actual linked symbol, not an extra
physical-load offset; apply acceptance tests only after their infrastructure
exists. Historical source and note defects are recorded in the tracker.
