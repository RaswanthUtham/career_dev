---
name: learning-riscv-xv6
description: "Use for requested RISC-V/xv6 architecture foundations: boot, privilege/CSR/assembly, ABI, traps, timers, Sv39/MMU, context switching and learner teaching-kernel source. Preserve prior progress without making a full kernel rebuild mandatory."
argument-hint: "RISC-V mechanism, xv6 function or selected teaching-kernel code"
---

# On-Demand RISC-V And xv6 Foundations

Apply the [shared coach](../learning-feynman-coach/SKILL.md) and consult
[current status](../../../plans/training_tracker.md). The
[active roadmap](../../../plans/five_month_development_roadmap.md) owns time;
the [kernel reference guide](../../../plans/scratch_riscv_os_plan.md) points to
preserved long-term milestones, not a mandatory second calendar.

## Preserve Progress And Ownership

Earlier boot/FDT/trap/allocator/MMU work remains historical evidence. Do not
restart everything because v2 was created. `my-linux` v2 is learner-owned;
`main`/`v1` and tag `v1-reference` are historical. The learner creates/types every
active file after a worked xv6/upstream trace. Do not edit, move, build or run
implementation without permission. Quizzes remain paused.

## Before Explaining

Confirm source roots/revisions, build flags, ISA width/extensions and emulator
machine. Read exact symbols, definitions, masks, callers and continuation.
Inspect assembly at C/privilege boundaries and disassembly before claims about
pseudo-instructions, register allocation or exact PCs.

Trace mode, stack, paging, interrupt state and per-hart versus global behavior.
Start from OpenSBI S-mode handoff for the main teaching kernel; xv6 M-mode and
separate learner firmware are comparisons, not merged code in the kernel.

## Worked Trace

Explain register nicknames, CSR, privilege, ABI, instruction operand direction
and bit notation before use. Identify each device in plain language before an
address. Tiny encode/decode example -> real value -> observed output when
available. For read-modify-write separate read, preserved bits, changed bits
and write; CSR access is not ordinary RAM access.

Follow entry -> selected operation -> continuation with concrete values.
Label architectural guarantees, platform assumptions and reduced implementations.
For inline assembly explain operands/constraints/clobbers: `asm volatile` is
not a universal motion prohibition; local register variables select asm operand
registers rather than reserving them permanently; a memory clobber is a compiler
effect, not a CPU fence or cache flush.

## Important Boundaries

- Timer counter frequency versus configured HZ before timer code. Verify Sstc
  versus SBI timer path, delegation, permissions, pending/enabled state,
  comparator, cause, handler and rearming.
- Virtual/physical address, page-table page, leaf PTE, permission and TLB are
  different. Check canonical addresses, mapping size and actual implementation.
- Before MMU activation, stored high-linked pointers are not usable merely
  because a PC-relative call works. Match upstream execution context and audit
  relocation/linker sections before proposing a transition.
- Derive virtual entry from the linked symbol and mapping; do not add the
  physical load offset twice.
- Hardware trap state, software trap frame and context-switch state are not
  interchangeable. Verify exactly what each saves and restores.
- Do not assume only hart zero boots, or a specific OpenSBI version, or that
  every exception from S-mode is delegated to S-mode.

Use existing build/test targets only after inspection; public xv6 is not always
a course lab with `make grade`. New reference code must be tested in a private
scratch copy or labelled unverified. Historical notes are not current source
evidence; consult active warnings before reuse. End a substantial lesson with
recap and relevant interview/architecture transfer, not a compulsory quiz.
