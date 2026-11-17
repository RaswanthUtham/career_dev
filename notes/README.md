# Active Lesson Navigation

Start with the [roadmap](../plans/five_month_development_roadmap.md) and
[project contracts](../plans/project_portfolio.md).
The [active lesson library](lessons/README.md) is the home for MCU/FreeRTOS-first
Markdown teaching. The current topic is the
[register-level clock and EXTI lesson](lessons/W01_L17_b1_exti_interrupt_handoff_and_debounce.md),
with [C01 checked sizing](lessons/W01_L04_c01_checked_packet_size.md) runtime
evidence still pending. Use native Windows and available hardware; no QEMU
prerequisite. The learner-reported installed Ubuntu Server is optional.
Linux-oriented L01-L03/L05 and the kernel series
are preserved references, not required next lessons. New notes name their
project, prerequisite, source revision and verification status.

## Existing Notes Are References

The [kernel series](kernel/README.md) and earlier notes preserve valuable
architecture work. Their old "current lesson", "next", "done" and checklist
statements do not override the active tracker. In particular:

- [Assembly](00_riscV_asm.md) and [S-mode boot](02_riscv_smode_boot_and_early_console.md)
  contain v1 implementation recipes, not architecture-only background.
- Notes 05-07 describe superseded allocator/MMU/timer implementations.
- K1b-1 needs the inline-assembly explanation corrections listed in the
  [tracker](../plans/training_tracker.md) before reuse.
- Supplementary SBI API notes are historical explanations, not independently
  verified new lessons.

Do not silently copy code from these references into the learner's repository.
Re-read the selected current upstream source, explain the differences and
verify any new reference implementation separately.
