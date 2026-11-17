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

Follow the coach's **Tutor Guide For Lessons And Answers** (upstream first, one small layer per lesson, one unit per file with the file path as the heading, Why/What/Technical/Code/How/Syntax per unit, input -> output for every function, "how it fits together", then the three-part control flow).

Start with a practical problem and one-sentence purpose. Define the minimum concepts before using them: register, instruction, address, privilege, Control and Status Register (CSR), Application Binary Interface (ABI), interrupt, exception and trap as needed. The learner is new to RISC-V registers and assembly: every `my-linux` note begins with a "Know This First" section, or links to an earlier one such as [K1a section 0](../../../notes/kernel/K01a_firmware_and_boot_contract.md#0-know-this-first), that covers every register, CSR, instruction, device and bit notation the note uses. Name each memory-mapped device in plain words (what it is, what it does and who owns it) before its address appears. For bit fields and encodings (PMP, PTEs, CSR masks), build the value bit by bit from a tiny example, decode it back, then show the real value in program output.

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

The [kernel milestone guide](../../../plans/scratch_riscv_os_plan.md) defines milestones K0-K10, upstream references and acceptance checks; the [tracker](../../../plans/training_tracker.md) holds status. Apply the coach's Engineering Correctness Gates to every milestone.

1. **Ownership.** The learner types every file in `my-linux` on branch `v2`. v1 stays on branches `main`/`v1` and tag `v1-reference` as reference only. Lessons live in `career_dev/notes/kernel/` (older notes in `notes/` are never edited) as markdown with explanation, ASCII diagrams, worked traces, target file paths and complete reference code that the tutor has built and run in a private scratch copy (verification line at the top of each note).
2. **Upstream boot order.** Follow upstream Linux execution order and context: `head.S` -> `setup_vm()` with static early tables (no allocator, MMU off) -> `relocate_enable_mmu()` -> `start_kernel()` -> `setup_arch()` (FDT, memblock, `setup_vm_final()`) -> `mm_core_init()` (buddy, then slab) -> traps, IRQ, time, softirq -> scheduler. Use upstream names and constants (`KERNEL_LINK_ADDR`, `PAGE_OFFSET`, `kernel_map`, `__pa_symbol`, `memblock`, `free_area`, `kmem_cache`). Label every reduced or reordered mechanism with a Deviation box.
3. **Dual boot.** One S-mode kernel image boots under OpenSBI and under the learner's separate M-mode firmware (`firmware/`) that implements the `fw_dynamic` handoff and a small SBI subset. Never place SBI firmware code inside the kernel image with `#ifdef`. Confirm boot registers, load addresses and firmware behaviour from QEMU output before asserting them.
4. **Build contract.** Use the upstream flags that matter for early boot (`-mcmodel=medany`, `-fno-PIE`, `-mno-save-restore`, no unwind tables), link with orphan sections treated as errors, and include `.srodata*`, `.sdata*`, `.sbss*` and `COMMON` explicitly. Re-audit sections whenever the layout changes.
5. **Explain before code.** Explain bit layouts, assembly idioms and pointer expressions with ASCII boxes and concrete numbers before showing them. Each milestone note ends with interview answers and an Arm64 comparison line.
6. **Device tree and time.** Teach DTS -> DTB -> firmware handoff -> early FDT parsing -> device/driver matching. Keep arch timebase setup (`arch/riscv/`), timer hardware (`drivers/clocksource/`), interrupt controllers (`drivers/irqchip/`) and generic tick/IRQ code (`kernel/`) separate. Teach Sstc `stimecmp` as the primary path and SBI TIME as the fallback. Begin timers with the worked calculation: `HZ` is a configuration choice; `timebase-frequency` is counter ticks per second; their quotient is counter ticks per jiffy.
7. **Review and run.** Review submitted code before suggesting fixes; build or run learner code only when requested. Quizzes stay paused until resumed.