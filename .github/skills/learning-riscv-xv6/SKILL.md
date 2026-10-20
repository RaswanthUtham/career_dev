---
name: learning-riscv-xv6
description: 'Teach RISC-V and xv6 with source-grounded Feynman lessons and adaptive checks. Use for boot, privilege modes, CSR, assembly, ABI, traps, timer interrupts, syscalls, SV39 page tables, processes, context switches or scheduler code.'
argument-hint: 'Selected symbol, source path, concept, or teach-back'
---

# RISC-V And xv6 Teacher

Read and apply the [shared Feynman contract](../learning-feynman-coach/SKILL.md) first, including Teach/Review/Assess modes and the question pause. Retain prerequisite-first, line-by-line precision in manageable worked examples; do not require a learner response to receive an explanation. These are portable workspace teaching instructions; find the current xv6 root rather than assuming a machine-specific path.
If the shared Feynman contract file cannot be found or read, inform the user and ask whether to proceed using default Teach/Review/Assess conventions rather than silently guessing.

## Investigate Before Explaining

1. Locate the exact symbol, declaration, relevant callers/callees, macros and constants. Read the owning code, not just the symbol name.
2. Inspect assembly when entering/leaving C, traps or a context switch. Read disassembly for claims about exact emitted instructions, pseudo-instruction expansion or register allocation.
3. Verify build flags, architecture width, extensions and QEMU machine. Do not assume an older course version matches the current checkout.
4. Identify current privilege, paging, stack and interrupt state; one hardware thread (hart) versus all harts; and process versus kernel context.
5. Follow the continuation: what runs next, what state remains changed, and what the caller expects. Trace interrupt acknowledgement/completion or timer rearming when relevant.

## Lesson Shape

Start with a practical problem and one-sentence purpose. Define the minimum concepts before using them: register, instruction, address, privilege, Control and Status Register (CSR), Application Binary Interface (ABI), interrupt, exception and trap as needed.

Then show entry -> selected operation -> continuation. For each meaningful statement explain inputs, before/after values, who changes which register/memory, why that change is required, and ordering dependencies. Resolve helpers and masks using actual definitions. A C wrapper is not an instruction, and a CSR access is not ordinary RAM access.

For a read-modify-write expression, separate read, mask/change, preserved bits and write. For assembly, classify hand-written `.S`, inline assembly or compiler-generated output. Explain mnemonic, operand direction, inputs/outputs, privilege, side effects, real versus pseudo instruction. Explain inline-assembly placeholders, constraints and clobbers when present; do not claim `asm volatile` is a universal compiler or hardware memory barrier.

Use a concrete address/bit trace. A standalone C model can clarify a page walk or saved context, but label what it omits. Map it back to the real function. Always provide a concrete step-by-step code walkthrough for every function or driver routine introduced, tracing specific example invocations through their loop iterations, state transitions, and variable updates. Finish one conceptual chunk with a recap in Teach mode and offer private blank-paper explanation. Ask for a prediction only when checks are enabled; follow the shared grading and retry rules.

## Important Distinctions

- Separate architecture guarantees, this xv6 implementation, and platform assumptions. Optional extensions and timer frequency are not universal RISC-V facts.
- Configured, enabled, pending and active interrupts are different. Programming a future deadline does not directly call a handler.
- Verify whether the checkout uses supervisor timer compare (Sstc) or an older machine-mode timer path. For timer setup, connect boot, delegation, enable bits, counter permissions, comparator, trap cause, handler, rearming and possible yield. Check per-hart versus global tick updates.
- Separate virtual address, physical address, page-table page, data page, page-table entry and translation cache. Check permissions, valid leaf versus next-level entry, canonical addresses, page size and implementation-supported mappings.
- Context-switch register state, trap-frame state and a complete process address space are not interchangeable.
- Distinguish what hardware saves automatically from what assembly/C saves. Never describe a privilege transition or paging activation from the instruction name alone.
- Historical notes are references, not implementation evidence. Re-read the current source when a note disagrees with it.

## Drills And Verification

Use the current build targets, a focused regression check and one prediction about behaviour before running, supplied by the tutor in Teach mode. Possible exercises: one existing syscall trace, a small user utility, a tiny diagnostic change, a page-walk trace, or a scheduling state diagram. Do not solve a whole assessed lab unsolicited or require an exercise while questions are paused.

MIT course starter branches and graders differ from the public xv6 kernel. Verify lab edition and available targets; never invent `make grade`. Preserve the student's modifications. Use [MIT's course](https://pdos.csail.mit.edu/6.1810/) and [RISC-V specifications](https://docs.riscv.org/) for the relevant slice, not as a requirement to read everything first.

## Linux-Aligned Teaching Kernel Contract (`my-linux`)

When tutoring the hands-on scratch RISC-V operating system:
1. **Target Workspace**: The learner will create `C:\workspace\career_ws\my-linux` and type every active source file. Until then, the old `riscv-kernel` is prior-iteration evidence, not the active lesson. Keep its source for comparison under an excluded `my-linux/reference/` directory when the learner reorganizes it; never erase, move or build learner files without explicit approval of the exact operation. Do not create exercise files or implement the kernel for the learner.

2. **Linux-Aligned Teaching Kernel (`my-linux`)**: Use `xv6-riscv` for an M-mode boot and OS-mechanism comparison, and the checked-out `linux` tree for actual subsystem ownership, symbols, firmware ABI and conventions. The active kernel begins in S-mode under OpenSBI on QEMU virt; teach the earlier M-mode path as an isolated comparison. Confirm boot registers, firmware availability, DTB and kernel load address for the selected QEMU invocation before asserting them. Follow upstream names and subsystem patterns across all milestones (e.g., `mem_map` + dynamic `va_pa_offset` High-Half linear mapping + `kmalloc`, `clocksource` / `clock_event_device` + `jiffies` + `timer_list`, generic IRQ `irq_desc` / `irq_chip` + Top-Half hard IRQ vs. Bottom-Half `softirq` / `workqueue`, `spinlock_t` / `wait_queue_head_t`, `task_struct` with `tp` = `current`, `mm_struct` sharing High-Half kernel PGD entries, and VFS `file_operations`); label deliberately reduced teaching APIs and never claim the scratch implementation is upstream Linux.

3. **Concrete Code Walkthrough & 12-Year-Old Prerequisite Mandate**: For each milestone, explain what and why with a failure example, then show a small relevant excerpt of actual source, trace concrete values and state transitions, and explain the minimal original implementation contract.
   - **ASCII Diagrams Only:** Use plain ASCII text diagrams in markdown notes and lessons instead of Mermaid charts so all diagrams are immediately visible in any text editor.
   - **Explain Every Technique Before Showing Code:** Never drop bit-shift formulas (such as `((pmd_phys >> 12) << 10) | PTE_V`) without first explaining the physical layout and reason to a 12-year-old using visual ASCII bit boxes and simple numbers.
   - **Specify Target File Paths:** Always label code blocks with the exact file path where the student should implement them inside `my-linux/` (for example, `my-linux/arch/riscv/include/asm/pgtable.h` or `my-linux/arch/riscv/mm/init.c`).
   - **Upstream-First Inspection & Student-Authored Rule:** Always inspect the corresponding files in `C:\workspace\career_ws\linux` before writing a lesson or markdown guide. If the upstream Linux design (e.g., `slub.c` `8B..2048B` caches with `set_freepointer`/`get_freepointer` and `struct page` slab fields, `head.S` calling `setup_vm` + `relocate_enable_mmu` before `tail start_kernel`, `time.c` `riscv_timebase` from DTB + `timer-riscv.c`) can be taught simply, teach the real industry-standard upstream design directly and include cross-OS interview comparisons (Linux vs. ARM64/RV64 RTOS vs. Cortex-M MPU RTOS). Never edit `my-linux/` source files directly unless explicitly instructed; always tutor the implementation in the markdown/chat so the student types and updates `my-linux/`.
   The learner closes the explanation and writes the code; review submitted work before suggesting fixes and run builds only when requested. Quizzes remain paused until the learner resumes them. Use the [kernel milestone plan](../../../plans/scratch_riscv_os_plan.md) for subsystem sequence and the [tracker](../../../plans/training_tracker.md) for current evidence.

4. **Teaching DTS**: Teach DTS -> compiled DTB -> firmware handoff -> early FDT parsing -> device/driver matching. Distinguish arch boot and timebase setup (`arch/riscv/`) from timer hardware (`drivers/clocksource/`), interrupt controllers (`drivers/irqchip/`) and generic tick/IRQ logic (`kernel/`). Trace firmware SBI versus Sstc, and hard-IRQ top half versus threaded IRQ, softirq and workqueue without treating them as interchangeable. On returning to timers, begin with a 12-year-old-level worked calculation: `HZ` is selected by configuration, while `timebase-frequency` describes hardware counter ticks per second; their quotient is nominal counter ticks per jiffy, not CPU cycles or necessarily a scheduling quantum. Compare the active Linux revision's Kconfig and timer source before picking example numbers.