# Learning Evidence And Recall Log

Schedule: [roadmap](five_month_development_roadmap.md). Current module status: [training tracker](training_tracker.md). Kernel milestones: [kernel milestone guide](scratch_riscv_os_plan.md). Environment: [workspace readiness](../docs/workspace_setup.md). Teaching protocol: [Feynman coach](../.github/skills/learning-feynman-coach/SKILL.md). Dated entries below preserve history; their old next-action statements do not override the tracker.

## Current Position

- Active direction approved2026-10-26: MCU/driver firmware, FreeRTOS first and
  bounded Zephyr. P1 cyclic peripheral service/fault recorder; P2 diagnostic
  supervisor/test node. Linux kernel/IIO/Yocto retired from active curriculum.
- The full former foundation plan and five skills are preserved under
  `backup/plan/2026-10-22-foundation` and `backup/skills/2026-10-22-foundation`.
- Existing kernel and C progress remains historical evidence. Do not repeat all
  lessons or treat old next actions as the active schedule.
- C01 was submitted/repaired and recorded source-reviewed Correct; runtime
  remains pending. Prior Linux lessons remain unchanged references.
- MCU topics 10–15 tutoring In progress (`W01-L15`..`W01-L20`, re-ordered
  2026-11-01 so Cortex-M4 instructions [W01-L15](../notes/lessons/W01_L15_thumb_and_arm_instruction_sets.md)
  and registers [W01-L16](../notes/lessons/W01_L16_cortexm4_reset_registers.md)
  precede bare-metal 84 MHz startup and EXTI [W01-L17](../notes/lessons/W01_L17_b1_exti_interrupt_handoff_and_debounce.md),
  MPU/privilege [W01-L18](../notes/lessons/W01_L18_cortexm4_mpu_privilege_psp_svc_and_memmanage.md),
  PendSV/CMSIS/FreeRTOS bridge [W01-L19](../notes/lessons/W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md),
  and BSP [W01-L20](../notes/lessons/W01_L20_bsp_board_support_and_bootstrap_processor.md)).
  Learner approved its register-level revision on 2026-10-28; proposed reference
  not compiled/flashed. Earlier polling implementation remains intact.
  Learner generated and successfully built the F446RE project; existing
  ELF/map inspected on 2026-10-27. Initial debugging now completed-reported;
  initialization observations now include learner-reported 84 MHz software
  clock after setup and 1 ms uwTick progression, not external timing proof.
  Earlier topic understanding
  remains unassessed. Native Windows and
  available hardware selected; no QEMU prerequisite. Ubuntu Server on the other
  HP is installed per learner report and optional. No tutor-observed hardware run.
- The current pre-pivot track is preserved under `backup/plan` and `backup/skills`
  dated2026-10-26, separately from the immutable2026-10-22foundation.
- Learner reports 10+ years C/intermediate proficiency; simple-English
  what/why/how with deep code traces. Quizzes stay paused.
- Budget: 22 hours/week; preparation/interviews and aim for offer by 22 March
  2027, with joining permitted afterward.

## 2026-10-22: K0 Passes on Learner Tree; K1 Published

**K0 `make check` failure (learner report: "syntax error").** Tutor reproduced it on the learner's tree: `scripts/check-vmlinux.sh: 4: set: Illegal option -`. All 14 learner files were byte-identical to the verified reference except for Windows CRLF line endings (for example, 42 CR bytes in the script). Root cause in the lesson: K00 section 3.11 wrongly said `.gitattributes` prevents this, but it only affects files Git writes. Tutor converted the 14 files to LF at the learner's request (line endings only). Tutor-run result on the learner's code: `make check` 5/5 PASS and `K0 layout: PASS` under OpenSBI. K00 was corrected in place (sections 3.11 and 4, Corrections) and gained section 3.12, "Sections versus segments", which answers the learner's `readelf -lW` question.

**K1 published as two notes:** [K1a firmware](../notes/kernel/K01a_firmware_and_boot_contract.md) and K1b SBI client, printk and power off (note removed later the same day, see the next entry). Tutor-run verification (private scratch copy, same toolchain): my-sbi builds (3108 bytes); the same `Image` passes `make test` under OpenSBI (SBI v1.0, legacy console) and `make test-fw` under my-sbi (SBI v2.0, DBCN), with QEMU exit status 0 and 21/21 printf tests; `-smp 4` booted on hart 3; `make check` 6/6.

**Evidence gathered while verifying (all in the notes):**
1. The QEMU reset ROM at `0x1000` and `fw_dynamic_info` (version 2, `boot_hart = 0`) were dumped with GDB. QEMU places `-kernel` at the firmware end rounded up to 2 MB (`hw/riscv/boot.c`).
2. OpenSBI v1.3 uses `boot_hart` only for relocation; the boot hart comes from `coldboot_lottery` (`sbi_init.c` lines 550-551). This explains the hart-1 boot seen in K0.
3. `mideleg` written `0x222` reads `0x1666` (H-extension read-only-one bits); writing `0` reads `0x1444`. `menvcfg` showed ADUE (bit 61) already set at reset.
4. Without a PMP allow entry, S-mode faults on its first fetch at `0x80200000`, endlessly.
5. GCC 13 emits PC-relative jump tables (`R_RISCV_ADD32/SUB32`), so `-fno-jump-tables` is not needed. A stored function pointer does fault (`scause 1`, `sepc = stval =` its link address). `make check` 5 now audits `R_RISCV_64` relocations until K3.
6. Both firmwares leave `stvec = _start`, so early faults looked like boot loops. K1 `head.S` parks early traps as upstream does (`head.S` 294-296).
7. `-Wformat-overflow` rejected a deliberate `NULL` `%s` test. Kept the warning on and hid the value as upstream `OPTIMIZER_HIDE_VAR` does (upstream disables the warning globally).

**Plan changes:** K1 split into a/b; SBI TIME moved to K6 and HSM/IPI to K8; KUnit starts in K3 (direct-call self-tests before that); my-sbi `/reserved-memory` handling decided in K4.

**Next action:** the learner types K1a (15 firmware files and `.gitignore`), boots the K0 kernel under my-sbi, then types K1b and pastes `make check`, `make test-fw` output and `echo $?`.

**Same day, learner feedback on K1a:** the PMP table, ACLINT and the NAPOT/`pmpcfg0` arithmetic were not understandable, because the note assumed RISC-V register and assembly knowledge that the learner does not have yet. Repairs:
- K1a was rewritten with a "Know This First" section (harts, register nicknames, CSRs, privilege modes, the QEMU `virt` memory map from tutor-run `info mtree`, what ACLINT is, traps and cause numbers, an instruction table).
- The NAPOT math is now taught with a 32-byte example at `0x1000` (encode to `0x403`, then decode back) before the real values.
- `pmpcfg0` is built byte by byte, and `medeleg` is computed as a sum of bits.
- `PMP_NAPOT_ADDR` became the equivalent `(base >> 2) | (size/8 - 1)`, and the firmware banner now prints `pmpaddr0-2` (re-verified: `0x801fff`, `0x20007fff`, all ones; `make test`/`make test-fw` still pass).
- K1b gained its own "Know This First" section (calling convention, `ecall`, inline assembly, `va_list`, computed versus stored addresses).

The coach skill, the RISC-V skill and the workspace instructions now require a "Know This First" section, a small-numbers-first walkthrough and minimum words in every lesson.

## 2026-10-22 (evening): K1b Restarted as Three Small Lessons

**Learner feedback:** the K1b note was overwhelming: many C files with too little explanation of each file, and the printing pieces (debug console, UART driver, printk) all looked like they did the same thing. The learner asked for a binding tutor guide and a clean restart.

**Actions (learner-approved choices):**
- **Backup.** The 15 K1b files the learner had typed or pasted (`asm/sbi.h`, `sbi_ecall.c`, `sbi.c`, nine `include/linux/*.h`, `printk.c`, `string.c`, `vsprintf.c`) were copied to `C:\workspace\career_ws\backups\my-linux-k1b-attempt1` (hash-verified), then removed from `my-linux`, along with the build outputs.
- **Checkpoint.** Tutor-run at the learner's request: the K0 kernel boots under OpenSBI and my-sbi (`make check` 5/5, `K0 layout: PASS`, banner read-backs match). Committed as `3fddd0d` on `v2`. Editor files (`.clangd`, `.zed/`, `compile_commands.json`, `.cache/`) were left untracked.
- **Review of the learner's firmware** (no changes made): it matches the reference except for clang-format style and the earlier, equivalent NAPOT formula without the `pmpaddr` banner lines.
- **Lesson split.** The old K1b note was deleted. K1b is now three lessons, each adding one layer: K1b-1 SBI call (`sbi_ecall`, `sbi_console_putchar`); K1b-2 printk, `vsnprintf` and earlycon; K1b-3 discovery, power off and tests.
- **Tutor guide** added to the coach skill ("Tutor Guide For Lessons And Answers", binding):
  - upstream first, one small layer per lesson;
  - one unit per file, with the file path as the heading, covering Why/What/Technical/Code/How (input -> output for every function)/Syntax;
  - a "how it fits together" section listing every piece doing a similar job;
  - a three-part control flow: a one-line chain, a numbered step table following one value, and a privilege timeline. This format comes from a research task covering the xv6 book's trap-path sentence, OSTEP's protocol tables, worked examples and subgoal labelling.

**K1b-1 published:** [K01b1_talk_to_firmware_sbi_ecall.md](../notes/kernel/K01b1_talk_to_firmware_sbi_ecall.md). Tutor-run: no warnings, `make check` 5/5, `K0 layout: PASS` through the firmware under both OpenSBI and my-sbi. `qemu -d int` counted 280 `supervisor_ecall` traps for 280 characters. GDB stopped in `sbi_ecall_handler` with `a7 = 1` and `mepc = __sbi_ecall+6`, and the backtrace runs `_trap_handler -> sbi_trap_handler -> sbi_ecall_handler -> sbi_putc -> uart8250_putc`.

**Process note:** the tutor's file-creation tool wrote CRLF line endings in four new reference files. The pre-publication CR check caught them, and they were converted before the note was generated, so the published code has LF.

**Next action:** the learner types the 4 new and 2 changed files of K1b-1, runs both proofs, and commits. K1b-2 is written after that.

## 2026-10-21: K0 Lesson Published (Tutor-Verified)

**Published:** [K00 Build, toolchain and linker contract](../notes/kernel/K00_build_and_linker_contract.md) and the [lesson index](../notes/kernel/README.md). `my-linux` is on branch `v2` (empty start commit); nothing was written into it by the tutor.

**Tutor-run verification (private scratch copy, `xv6-lab`, GCC 13.3.0, ld 2.42, QEMU 8.2.2, gdb-multiarch 15.1, upstream Linux 7.3-rc4 `f0100363d`):** clean `-Werror` build with `--orphan-handling=error`; `make check` 5/5 PASS; `K0 layout: PASS` under OpenSBI with both `-kernel Image` and `-kernel vmlinux`; PASS on `-smp 4` including a run where OpenSBI chose boot hart 1; GDB stopped at physical `0x80200000` (`head.S:16`, `a1 = 0x87e00000`) using `symbol-file -o 0x100200000`; the negative test (removing `.sbss` from the script) failed at link time with an orphan error.

**Findings while verifying (all now in the note):**
1. `-fno-PIE` in `CFLAGS` alone is not enough: assembly built under default PIE turned `la` into GOT loads (`.got` orphans). Upstream passes `-fno-PIE` to `KBUILD_AFLAGS` as well.
2. ld reports empty `.rela.*` input sections as orphans under `--orphan-handling=error`; upstream names `.rela.dyn` explicitly. K0 does the same and `make check` requires it to be empty.
3. A `bnez a0, park` hartid check in an early draft would hang when OpenSBI's lottery picks a non-zero boot hart; removed to match upstream non-spin-wait boot.
4. Files created on Windows had CRLF endings (`sh: set: Illegal option -`); K0 adds `.gitattributes` with `eol=lf`.

**Next action:** learner types the 14 K0 files on branch `v2`, runs the acceptance checklist, and pastes `make check` output plus the last QEMU lines as learner-run evidence.

## 2026-10-21: Root-Cause Review, v1 Freeze and Rebaseline

**Failure observed (tutor-run QEMU, `-d int`):** after `[MM] Linux mem_map[] array and free_page_list initialized.`, a store access fault (`scause = 7`) occurred at `epc 0x80200a0a` in `alloc_page()` with `stval 0x2492494a2e3d2028`.

**Root cause (from `objdump -h` and `nm -n`):** GCC emits small globals (`mem_map`, `max_pfn`, `min_low_pfn`, `va_pa_offset`, `swapper_pg_dir`, `riscv_timebase`) into `.sbss`/`.sdata`. The linker script only collected `.data` and `.bss`, so `.sbss` became an orphan section placed after `_end` at `0x80209000`. `mm_init()` placed `mem_map[]` at `PAGE_ALIGN(_end)`, the same address, overwriting its own pointer variables; the head.S BSS loop also never cleared `.sbss`. Upstream `vmlinux.lds.S` collects `*(.sdata*)` and its `BSS_SECTION` collects `.sbss`.

**Earlier contributing defects (same week):** linking at a low VMA left statically initialized pointers (`LIST_HEAD`, function pointers in `.data`) at physical addresses after the high-half switch; the default-PIE Ubuntu compiler generated a GOT with virtual addresses used while the MMU was off (fixed by `-fno-PIE`, which upstream also uses).

**Tutor-process findings:** layout changes were taught without a section audit; fixes were chained without evidence; lesson code was not built before publication; notes 06-07 accumulated contradictory patches; v1 ran the page allocator before the MMU was on, unlike upstream (`setup_vm` uses static early tables; `mm_core_init` runs after `start_kernel`).

**Decisions (learner-approved):** tag v1 as `v1-reference` and restart from an empty tree; dual boot (OpenSBI and learner firmware) from the first boot milestone; memory management in upstream order (memblock, buddy, SLUB); supersede notes 05-07 with a new verified series; lessons written in markdown, learner implements. Roadmap, tracker, kernel milestone guide, workspace instructions and skills rewritten the same day. Upstream constants recorded: `KERNEL_LINK_ADDR = 0xffffffff80000000` (kernel mapping) and `PAGE_OFFSET_L3 = 0xffffffd600000000` (linear map) for Sv39.

## 2026-10-18: Sv39 MMU & High-Half Linear Mapping Verification (Milestone 4 Stages 1 & 2)

The learner implemented [arch/riscv/include/asm/pgtable.h](../../my-linux/arch/riscv/include/asm/pgtable.h), [arch/riscv/mm/init.c](../../my-linux/arch/riscv/mm/init.c), updated [arch/riscv/kernel/traps.c](../../my-linux/arch/riscv/kernel/traps.c) for page faults (`scause = 12, 13, 15`), and updated [init/main.c](../../my-linux/init/main.c). Evaluated: **Correct overall** and verified live in QEMU Virt via OpenSBI:
1. `vm_init()` populated `swapper_pg_dir` with both Low-Half Identity mappings (`Rows 0..255`) and High-Half Kernel Linear mappings (`Rows 256..511`, `PAGE_OFFSET = 0xFFFFFFC000000000UL`).
2. `enable_mmu()` loaded `satp` (`MODE = 8` Sv39), executed `sfence.vma`, updated runtime `va_pa_offset = PAGE_OFFSET`, and relocated `swapper_pg_dir` to its High-Half virtual address.
3. Verified global variable `int a = 42` translation across Low-Half VA, High-Half `__va(a_pa)`, `walk_va_to_pa()`, and `__pa(high_a_ptr)`.
4. Verified dynamic virtual page mapping (`VA 0x40000028 -> PA 0x8038c028`) and aliasing read-back via High-Half linear address `0xffffffc08038c028`, plus controlled Load Page Fault (`scause = 13`) recovery at `0x50000000`.
Created next-topic markdown guides [06_slab_kmalloc_and_high_half_relocation.md](../notes/06_slab_kmalloc_and_high_half_relocation.md) and [07_timekeeping_clocks_and_timers_linux_way.md](../notes/07_timekeeping_clocks_and_timers_linux_way.md).

## 2026-10-18: Mainline Linux Subsystem Alignment & Two-Stage High-Half Linear Mapping Plan

Updated [learning-feynman-coach](../.github/skills/learning-feynman-coach/SKILL.md), [learning-riscv-xv6](../.github/skills/learning-riscv-xv6/SKILL.md), [scratch_riscv_os_plan.md](scratch_riscv_os_plan.md), and [05_physical_memory_allocator_linux_way.md](../notes/05_physical_memory_allocator_linux_way.md) to align every upcoming subsystem in `my-linux` with upstream Linux kernel mechanisms:
1. **Two-Stage Identity + High-Half Linear Mapping (`PAGE_OFFSET = 0xFFFFFFC000000000UL`)**:
   - Stage 1 (While `satp = 0`): Global `va_pa_offset` starts at `0UL` (matching `linux/arch/riscv/mm/init.c`). `vm_init()` populates `swapper_pg_dir` with both the Low-Half Identity Map (`Entries 0..255`: `VA 0x80200000 -> PA 0x80200000` + UART MMIO) and the High-Half Kernel Linear Map (`Entries 256..511`: `VA 0xFFFFFFC080000000 -> PA 0x80000000`).
   - Stage 2 (Inside `enable_mmu()`): Immediately after activating `satp` and executing `sfence.vma`, update `va_pa_offset = PAGE_OFFSET` (`0xFFFFFFC000000000UL`) and relocate `swapper_pg_dir` to its High-Half virtual address so `__va(pa)` and `__pa(va)` operate on High-Half addresses (`0xFFFFFFC08xxxxxxx`).
   - Stage 3 (Sub-page Slab/Kmalloc & High-Half Relocation): Add `kmalloc()` / `kfree()` (`mm/slab.c`) on top of `alloc_page()` for sub-page kernel objects, and relocate kernel execution (`PC`, `sp`, `stvec`) to the High Half before user processes so every user `pgd` shares `Entries 256..511` of `swapper_pg_dir`.
2. **Mainline Subsystem Progression**:
   - **Timekeeping (Milestone 5)**: `clocksource` (`rdtime`) vs. `clock_event_device` (`sbi_set_timer` / `Sstc`), `jiffies_64`, and dynamic `struct timer_list` (`TIMER_SOFTIRQ`).
   - **Interrupts & Deferred Work (Milestone 6)**: Generic IRQ layer (`struct irq_desc`, `struct irq_chip`, `request_irq`), SiFive PLIC chip driver, Top-Half hard IRQ (`in_irq()`, `kfifo`) vs. Bottom-Half `softirq` (`kernel/softirq.c`) and process-context `workqueue` (`struct work_struct`, `kworker`).
   - **Locking, Scheduler, VFS & User Space (Milestone 7)**: `spinlock_t` (`spin_lock_irqsave`), `struct mutex`, `wait_queue_head_t` (`wait_event_interruptible`), `struct task_struct` with `current` in `tp`, `struct mm_struct` / `vm_area_struct`, `copy_from_user` / `copy_to_user` (`sstatus.SUM`), and VFS `struct file_operations`.

## 2026-10-14: Physical Page Frame Allocator Verification (Milestone 4 Part 2)

The learner implemented [include/linux/stddef.h](../../my-linux/include/linux/stddef.h), [include/linux/container_of.h](../../my-linux/include/linux/container_of.h), [include/linux/list.h](../../my-linux/include/linux/list.h), [include/linux/mm.h](../../my-linux/include/linux/mm.h), and [mm/page_alloc.c](../../my-linux/mm/page_alloc.c) following strict Linux kernel patterns: intrusive circular doubly-linked list (`struct list_head`, `LIST_HEAD`, `list_add`, `list_add_tail`, `list_del`, `list_first_entry`), pointer arithmetic (`offsetof` and `container_of`), and global physical descriptor array (`struct page mem_map[]`). Implemented the comprehensive verification suite in [init/main.c](../../my-linux/init/main.c) with freestanding decimal and hex printers (`early_print_dec`, `early_print_hex`). Compiled cleanly via `make` and tested live in QEMU Virt via OpenSBI v1.3:
1. `mm_init()` initialized the 128 MB DRAM descriptor pool and threaded usable pages onto `free_page_list`.
2. Allocated Page 1: returned PFN 776 at physical address $0\text{x}80308000$. Wrote verification pattern `0xdeadbeefcafebabeUL`.
3. Allocated Page 2: returned PFN 777 at physical address $0\text{x}80309000$.
4. Freed Page 1 (`free_page(p1)`).
5. Reallocated Page 3: confirmed `p3 == p1` (LIFO free list order verified), and verified that the underlying 4KB DRAM frame was completely zeroed.
Observed live output: `[MM TEST] All page allocator tests passed successfully!`. Created modular topic notes [01_riscv_hardware_reset_and_boot_flow.md](../notes/01_riscv_hardware_reset_and_boot_flow.md), [02_riscv_smode_boot_and_early_console.md](../notes/02_riscv_smode_boot_and_early_console.md), [03_devicetree_discovery_and_role_boundaries.md](../notes/03_devicetree_discovery_and_role_boundaries.md), [04_supervisor_trap_architecture_and_frame.md](../notes/04_supervisor_trap_architecture_and_frame.md), and unified memory & MMU guide [05_physical_memory_allocator_linux_way.md](../notes/05_physical_memory_allocator_linux_way.md). Milestone 4 Part 2 **Verified**. Advanced to Sv39 Virtual Memory & MMU Activation.

## 2026-10-08: Early `stvec` in `head.S` & Trap Vector Verification (Milestone 4 Part 1)

The learner implemented early trap vector registration directly in [arch/riscv/kernel/head.S](../../my-linux/arch/riscv/kernel/head.S) (`la t0, handle_exception; csrw stvec, t0`), implemented [arch/riscv/kernel/entry.S](../../my-linux/arch/riscv/kernel/entry.S), and added breakpoint handling in [arch/riscv/kernel/traps.c](../../my-linux/arch/riscv/kernel/traps.c). Evaluated: **Correct overall**. Initializing `stvec` early in assembly matches upstream Linux (`head.S::.Lsetup_trap_vector`) and closes the vulnerable unmapped trap window before C entry. Live QEMU execution verified that: (1) S-mode `ecall` was intercepted by OpenSBI in M-mode and returned cleanly, and (2) supervisor `ebreak` trapped directly into `stvec` (`handle_exception` $\rightarrow$ `do_trap`), successfully printing `[TRAP] Supervisor Breakpoint (ebreak) trapped to stvec successfully!`. Identified and analyzed the subsequent instruction alignment fault: GCC generated 16-bit compressed `c.ebreak` ($2$ bytes), while unconditional `epc += 4` jumped into the middle of the following 32-bit `auipc` instruction. Taught the RISC-V variable instruction length rule (`(insn & 0x3) == 0x3 ? 4 : 2`). Proceeded to Milestone 4 Part 2: Physical Page Frame Allocator (`mm/page_alloc.c`).

## 2026-10-08: Milestone 3 Verified in QEMU (FDT Parsing & Hardware Tree Traversal)

The learner implemented [include/linux/fdt.h](../../my-linux/include/linux/fdt.h), [arch/riscv/kernel/setup.c](../../my-linux/arch/riscv/kernel/setup.c), updated [init/main.c](../../my-linux/init/main.c), and updated [Makefile](../../my-linux/Makefile). Evaluated: **Correct overall**. Implementation cleanly handles the Flattened Device Tree binary format: header magic check ($0\text{xD00DFEED}$), big-endian to little-endian word translation (`be32_to_cpu`), structure block token walking (`FDT_BEGIN_NODE`, `FDT_PROP`, `FDT_END_NODE`, `FDT_END`), 4-byte padding alignment (`fdt_align4`), and strings block property name lookups. Tested live in QEMU Virt via OpenSBI v1.3 (`qemu-system-riscv64 -machine virt -cpu rv64 -m 128M -bios default -kernel vmlinux -nographic -smp 1`). Observed live output: successfully validated DTB at $0\text{x}87E00000$, traversed the entire hardware hierarchy, and enumerated all devices: `/memory@80000000` (128 MB RAM), `/cpus` (`timebase-frequency = 10000000`), `/serial@10000000` (NS16550 UART with properties `reg`, `compatible`, `clock-frequency`, `interrupts`), `/test@100000` (poweroff), and `/plic@c000000` (interrupt controller). Milestone 3 **Verified**. Advanced to Milestone 4: Memory and Trap Boundary.

## 2026-10-07: FDT Header Check Implementation Review (Milestone 3)

The learner implemented [include/linux/fdt.h](../../my-linux/include/linux/fdt.h), [arch/riscv/kernel/setup.c](../../my-linux/arch/riscv/kernel/setup.c), updated [init/main.c](../../my-linux/init/main.c), and updated [Makefile](../../my-linux/Makefile). Source review: **Partially correct overall**. Core C logic is sound: `fdt_header` struct layout matches the Devicetree specification; `be32_to_cpu()` accurately handles big-endian to little-endian byte swapping; `fdt_check_header()` validates null pointer, checks `FDT_MAGIC` ($0\text{xD00DFEED}$), verifies format version $\ge 16$, and prints validation status; `main.c` forwards firmware register `a1` (`dtb`) to `fdt_check_header()`. Three build-system defects identified in `Makefile`: (1) line 26 missing whitespace `$(INCLUDES)-c` merging `-Iinclude` and `-c` into `-Iinclude-c`, causing GCC to invoke linker `ld` prematurely during assembly of `head.S`, (2) line 19 using `-O $@` (capital O optimization flag) instead of `-o $@` (lowercase output flag) when preprocessing `vmlinux.lds.S`, and (3) `arch/riscv/kernel/setup.c` omitted from `SRCS_C`. Learner code preserved. Proceeded to Makefile fixes and live QEMU FDT validation.

## 2026-10-07: Devicetree Industry Role Boundary & Scope Clarification

Clarified Devicetree responsibilities for Staff-level embedded systems and Linux driver roles (Qualcomm, NVIDIA, AMD, TI):
1. **Low-Level FDT Binary Parser (`0xd00dfeed`)**: Part of generic architecture infrastructure (`drivers/of/fdt.c` in Linux; compile-time code generation in Zephyr). Production engineers do *not* author binary byte unpackers from scratch. In `my-linux` Milestone 3, we implement a lightweight header check and property scan strictly to demystify the firmware register `a1` handoff and `be32_to_cpu` mechanics without getting derailed by low-value parser plumbing.
2. **Production Devicetree Responsibilities**: 100% owned by the BSP / Driver Engineer: authoring board/SoC Devicetree nodes (`.dts` / `.dtsi`), writing official YAML hardware binding schemas (`Documentation/devicetree/bindings/`), and retrieving hardware resources inside driver `probe()` via `of_match_table` and `of_property_read_*` APIs.
3. **Curriculum Alignment**: Production Devicetree skills are anchored in Capstone 1 (Zephyr Devicetree overlays and bindings for STM32 SPI DMA telemetry, W09) and Capstone 2 (Linux IIO high-speed SPI/I2C client driver with YAML schema and threaded IRQ, W14-W16).

## 2026-10-07: Linux-Aligned Kernel Rebaseline

The learner elected to restart the **active** RISC-V implementation as `my-linux`, retain the old scratch source for comparison under an excluded `reference/`, and create/type all new folders and source personally after source-grounded lessons. Use xv6's M-mode boot for a short comparison, then OpenSBI S-mode boot for the active project. The project is not yet created; no previous kernel files have been moved, erased, recompiled or reverified by this planning update. Prior Day 1-8 QEMU runs remain historical evidence only; previous Day 9 empty timer files are not a completed lesson.

The 35-day sequence was replaced by [dependency-ordered Linux-aligned milestones](scratch_riscv_os_plan.md); the weekly budget remains 22 hours, with later dates subject to rebaseline rather than compression. Start at M-mode versus S-mode entry, then early firmware boot and DTB before timer and IRQ infrastructure. On revisiting timekeeping, explain to a 12-year-old why Linux `HZ` is configured rather than calculated from the DTB `timebase-frequency`; calculate counter ticks per nominal jiffy from the chosen configuration. Track independent understanding and future builds/runs separately. Career goal INR 70-90 LPA CTC remains aspirational.

## 2026-10-07: Milestone 2 Verified in QEMU (OpenSBI S-Mode Boot & Early Console)

The learner implemented [arch/riscv/kernel/head.S](../../my-linux/arch/riscv/kernel/head.S), [arch/riscv/kernel/vmlinux.lds.S](../../my-linux/arch/riscv/kernel/vmlinux.lds.S), [drivers/tty/serial/8250_early.c](../../my-linux/drivers/tty/serial/8250_early.c), [init/main.c](../../my-linux/init/main.c), [include/linux/types.h](../../my-linux/include/linux/types.h), and [Makefile](../../my-linux/Makefile). Evaluated: **Correct overall**. All earlier syntax defects were resolved cleanly: 4KB section alignment, BSS boundary clearing via 8-byte stores, secondary hart parking (`wfi`), early stack pointer initialization, polled NS16550 UART character transmission (`early_putc` with automatic `\r` insertion on `\n`), and minimal S-mode entry. Compiled cleanly via `make` using `riscv64-linux-gnu-gcc` in the `xv6-lab` container. Verified live in QEMU Virt via OpenSBI v1.3 (`-bios default -kernel vmlinux -m 128M -smp 1 -nographic`): OpenSBI successfully executed M-mode platform init, configured PMP and trap delegation, placed the DTB at $0\text{x}87E00000$, and dropped execution to S-mode `_start` at $0\text{x}80200000$. Observed clean boot banner: `[S-MODE] my-linux: Scratch Kernel Online!`, `Boot Hart ID: 0`, `Linker Script & Boot Stack Verified`. Milestone 2 **Verified**. Advanced to Milestone 3: Device Description and Discovery (Flattened Device Tree / DTB parsing).

## 2026-10-07: S-Mode Boot & Linker Script Review (Milestone 2)

The learner implemented [arch/riscv/kernel/head.S](../../my-linux/arch/riscv/kernel/head.S) and [arch/riscv/kernel/vmlinux.lds.S](../../my-linux/arch/riscv/kernel/vmlinux.lds.S) from memory in `my-linux`. Source review: **Partially correct overall**. Core architecture is sound: supervisor interrupt masking (`csrw sie/sip, zero`), secondary hart parking (`bnez a0`), 16KB 4096-byte aligned boot stack allocation in `.bss.stack`, 8-byte BSS zeroing loop, frame pointer clearing (`li s0, 0`), tail-call to `start_kernel`, and 4KB page alignment across `.text`, `.rodata`, `.data`, and `.bss` sections. Five defects noted for correction: (1) `sd zero 0(t0)` on line 19 of `head.S` is missing the operand comma (`sd zero, 0(t0)`), (2) `li t0, __bss_start` on lines 15-16 of `head.S` uses Load Immediate `li` instead of Load Address `la` for relocatable symbols, (3) symbol naming mismatch between `vmlinux.lds.S` (`__start_bss` / `__stop_bss`) and `head.S` (`__bss_start` / `__bss_stop`), (4) section naming mismatch between `head.S` (`.section .bss_stack`) and `vmlinux.lds.S` (`*(.bss.stack)`), and (5) typo in `vmlinux.lds.S` line 14 (`_extext = .;` instead of `_etext = .;`). Load address $0\text{x}80000000$ noted for custom Mini-SBI versus $0\text{x}80200000$ for standard OpenSBI. Learner code preserved. Proceeded to early C entry (`init/main.c`) and UART console output.

## 2026-10-07: Hardware Reset, Firmware Duality & Shell Integration Analysis

Analyzed the full hardware reset chain and kernel implementation gaps following learner creation of `my-linux/reference/`:
1. **Hardware Reset ($0\text{x}1000$)**: In QEMU Virt, address $0\text{x}1000$ is simulated Mask ROM (MROM) hardwired in QEMU. At CPU power-on, the core enters M-mode at $0\text{x}1000$, places the DTB address in `a1`, and jumps to RAM base $0\text{x}80000000$.
2. **Firmware Duality ("Mini-SBI" vs OpenSBI)**: Clarified that the prior Day 7 `main.c` bare-metal boot (`-bios none`) acted as a minimal Machine-mode firmware (configuring PMP, delegating traps to S-mode, setting `mstatus.MPP = S` and executing `mret`). Upstream Linux boots from OpenSBI (`-bios default`), where OpenSBI handles M-mode platform init and SBI runtime calls (`ecall`). The teaching plan supports both paths to achieve complete end-to-end understanding from raw reset to supervisor mode.
3. **Implementation Gaps Identified**: The prior `riscv-kernel` verified Days 1–8 (boot, serial, printk, CSRs, M-to-S transition, trap vector). Gaps: Day 9 timer/clocksource (empty files), DTB parsing (hardcoded literals used previously), Sv39 MMU/paging (`satp = 0`), PLIC external interrupts, process control (`task_struct`), scheduler (`schedule`), user mode (`sret` to U-mode), syscall dispatcher, and user programs/shell.
4. **Interactive Shell Integration**: Confirmed that Milestone 7 incorporates the full user-space execution bridge: Sv39 user page table, U-mode transition, system call gateway (`sys_read`, `sys_write`, `sys_fork`, `sys_exit`), user runtime (`crt0.S`), and an interactive user shell (`usr/bin/sh.c`) over UART, fulfilling end-to-end OS validation.

## 2026-10-01: Early Trap Safety & Upstream Linux `stvec` Architecture

Analyzed the failure mode if an exception or interrupt occurs prior to `trap_init()`: with `stvec == 0x0`, the CPU fetches from address 0, causing an immediate Instruction Access Fault (`scause = 1`), which re-traps to address 0 in an infinite double-fault loop (silent system lockup). Contrasted with upstream Linux kernel architecture ([linux/arch/riscv/kernel/head.S](../../linux/arch/riscv/kernel/head.S)): Linux configures `CSR_TVEC` (`stvec`) early in assembly via `.Lsetup_trap_vector` before executing any complex C code, and temporarily points `stvec` to `.Lsecondary_park` (`wfi; j .Lsecondary_park`) during early page-table initialization so unexpected faults halt cleanly rather than running wild. In the scratch kernel, setting `stvec` before `mret` or in `head.S` eliminates the vulnerable unmapped trap window.

## 2026-10-01: Scratch RISC-V OS Day 8 Supervisor Trap Handling Verification

The learner implemented [arch/riscv/include/asm/ptrace.h](../../my-linux/reference/riscv/include/asm/ptrace.h), [arch/riscv/kernel/entry.S](../../my-linux/reference/riscv/kernel/entry.S), and [arch/riscv/kernel/traps.c](../../my-linux/reference/riscv/kernel/traps.c). Evaluated: **Correct overall**. Architecture cleanly captures the 288-byte `struct pt_regs` frame (all 31 GPRs + CSRs `sepc`, `sstatus`, `stval`, `scause`), programs `stvec` to `handle_exception` in direct mode, routes through `do_trap(struct pt_regs *regs)`, advances `regs->epc += 4` on synchronous system call `ecall`, restores registers and returns via `sret`. Fixed minor typo in macro closing parenthesis (`(1UL << 63)`). Verified live in QEMU Virt via synchronous `ecall` trap triggering `scause = 9`, printing the syscall log, and resuming execution transparently. Verified. Proceeded to Day 9: Hardware Timer Clocksource & Interrupts.

## 2026-09-29: Scratch RISC-V OS Day 7 M-to-S Mode Transition Verification

The learner completed [arch/riscv/include/asm/csr.h](../../my-linux/reference/riscv/include/asm/csr.h) and implemented the Machine-to-Supervisor transition in [init/main.c](../../my-linux/reference/init/main.c). Evaluated: **Correct overall**. Implementation accurately configured `mstatus.MPP = MSTATUS_MPP_S`, set `mepc = start_kernel_supervisor`, disabled virtual translation (`satp = 0`), delegated all exceptions and interrupts to Supervisor Mode (`medeleg = 0xffff`, `mideleg = 0xffff`), unmasked supervisor interrupts via `sie`, and configured PMP (`pmpaddr0 = 0x3fffffffffffffull`, `pmpcfg0 = 0xf`). Executed `mret` and successfully transitioned the CPU into Supervisor Mode. Compiled via Makefile in the `xv6-lab` Podman container and launched in QEMU Virt. Observed output: M-mode logs, successful `mret` transition, banner in S-mode, reading `sstatus = 0x0`, and clean poweroff via the SiFive test device (`0x100000 = 0x5555`). Verified. Proceeded to Day 8: Supervisor Trap Vector & Register Spill Frame.

## 2026-09-29: Scratch RISC-V OS Day 6 CSR Access Primitives Review

The learner implemented [arch/riscv/include/asm/csr.h](../../my-linux/reference/riscv/include/asm/csr.h) from memory without notes. Source review: **Partially correct overall**. Core architecture is sound: correct GNU statement expression syntax `({ ... })` across all operations; `csr_write` (`csrw`), `csr_set` (`csrs`), and `csr_clear` (`csrc`) correctly pass input `rK` constraint and `"memory"` compiler barrier; `csr_swap` correctly formats `csrrw` instruction; accurate definitions for `mstatus`, `sstatus`, `mie`, `sie` bit fields matching the RISC-V privileged specification. Two defects identified for correction: (1) `csr_read` declares local register variable `_v` but returns undeclared `__v;`, and formats instruction string as `"csrr, %0" __ASM_STR(csr)` which expands to invalid opcode `csrr, %0mstatus` (should be `"csrr %0, " __ASM_STR(csr)` returning `_v`), and (2) `csr_swap` uses non-standard output constraint `"=rw"(_v)` (should be standard `"=r"(_v)`). Added `-Iarch/riscv/include` to [Makefile](../../my-linux/reference/Makefile) `CFLAGS`. Code preserved. Proceeded to Day 7: Machine to Supervisor Mode Transition (`init/main.c`).

## 2026-09-29: Scratch RISC-V OS Day 5 Build System & First Boot Verification

Verified the containerized build system and QEMU execution for Phase 1. The learner corrected the syntax and scoping issues in [kernel/printk/printk.c](../../my-linux/reference/kernel/printk/printk.c) and added formatted test invocations to [init/main.c](../../my-linux/reference/init/main.c). Compiled cleanly via `Makefile` with `riscv64-linux-gnu-gcc` in the `xv6-lab` Podman container. Tested in `qemu-system-riscv64 -machine virt -bios none -kernel vmlinux -m 128M -smp 1 -nographic`. Observed runtime verification output: boot banner, signed decimal with character (`-42 Z`), unsigned hex (`0xdeadbeef`), pointer (`0x80000000`), and hart ID (`0`). Explained Makefile syntax, automatic variables (`$@`, `$<`), incremental timestamp rules, and compiler flags (`-mcmodel=medany`, `-mno-relax`, `-fno-common`, `-ffreestanding`). Explained QEMU terminal escape sequence (`Ctrl+A` then `x`) and hardware poweroff devices. Completed Phase 1; advanced to Phase 2, Day 6: RISC-V CSR Access Primitives (`arch/riscv/include/asm/csr.h`).

## 2026-09-29: Scratch RISC-V OS Day 4 Formatted Print Engine Review

The learner implemented [kernel/printk/printk.c](../../my-linux/reference/kernel/printk/printk.c) from memory without notes. Source review: **Partially correct overall**. Core algorithms are sound: unsigned and signed radix conversion using local stack buffer and reverse-flush to `early_putc()`; handling of negative values with minus prefix; `0x` pointer formatting via `print_pointer()`; length modifier parsing for `%l` / `%ll`; character, string, hex, decimal, unsigned, and pointer format parsing in `vprintk()`; variadic wrappers `early_printk()`, `printk()`, and `panic()`. Three compile-time defects identified for correction: (1) line 1 `#include <cstdint>` is a C++ header invalid in freestanding bare-metal C (should use `<linux/types.h>` and `<linux/stdarg.h>`), (2) reusing the variable name `val` with conflicting types across `switch` cases without enclosing `{ }` compound scopes, causing duplicate symbol redefinition errors in C, and (3) `fmt++` placed inside the `switch` block under `default` after `break`, making it dead/unreachable code and preventing format string advancement. Detailed syntax and Makefile options explained. Proceeded to Day 5 verification.

## 2026-09-29: Code Walkthrough Mandate & Formatted Print Walkthrough

Updated the shared Feynman teaching contract in [learning-feynman-coach](../.github/skills/learning-feynman-coach/SKILL.md) and [learning-riscv-xv6](../.github/skills/learning-riscv-xv6/SKILL.md) to mandate concrete step-by-step code walkthroughs for all code examples, tracing specific representative calls across variables, loop iterations, argument registers, and state mutations. Executed line-by-line mechanical execution trace of `printk("%d %c\n", a, v)` and `printk("hart %u at %p\n", hartid, ptr)`.

## 2026-09-28: Scratch RISC-V OS Day 3 Early Serial Driver Review

The learner implemented [drivers/tty/serial/8250_early.c](../../my-linux/reference/drivers/tty/serial/8250_early.c) from memory without notes. Source review: **Correct overall**. Implementation matches the Linux `earlycon` polled architecture: MMIO access helpers `serial_out` and `serial_in` use `volatile uint8_t *` pointers preventing dead-store/caching compiler optimizations; `early_serial8250_init()` disables interrupts via `UART_IER`, programs the 38400 baud rate divisor (`clock / (16 * baudrate) = 3`) via `UART_LCR_DLAB` / `UART_DLL` / `UART_DLM`, configures 8-N-1 framing (`UART_LCR_WLEN8`), and clears/enables FIFOs via `UART_FCR`; `early_putc()` performs recursive `\r` insertion on `\n` to prevent terminal staircase output, polls `UART_LSR` for `UART_LSR_THRE` ($0\text{x}20$), and writes to `UART_TX`; `early_puts()` safely traverses strings with null-pointer checks. Code preserved. Proceeded to Day 4 Kernel Formatted Print Engine (`kernel/printk/printk.c`).

## 2026-09-28: Scratch RISC-V OS Day 2 Assembly Boot Entry Review

The learner implemented [arch/riscv/kernel/head.S](../../my-linux/reference/riscv/kernel/head.S) from memory without notes. Source review: **Partially correct overall**. Core architectural requirements are met: Machine mode interrupt disable (`csrw mstatus, zero`), hardware thread ID interrogation via `mhartid`, secondary core parking loop (`wfi; j Lsecondary_park`), stack pointer initialization (`la sp, boot_stack_top`), unsigned BSS clearing loop from `__bss_start` to `__bss_stop` with 8-byte steps (`sd zero, 0(t0)`), frame pointer clearing (`li s0, 0`), clean non-returning handoff to C (`tail start_kernel`), and a 16KB 4096-byte aligned boot stack buffer in `.bss.stack`. Three syntax/assembler directive defects identified for correction: (1) line 2 `global _start` is missing the leading period `.` required by GNU assembler directives (`.global _start`), (2) line 19 `bgeu to, t1, Lclear_bss_stop` has typo `to` (letter 'o') instead of register `t0` (number zero), causing an unresolved symbol error at assembly time, and (3) line 40 `space 4096 * 4` is missing the leading period `.` required by the GNU `as` space allocation directive (`.space 4096 * 4`). Learner code preserved. Proceeded to Day 3 early serial console driver (`drivers/tty/serial/8250_early.c`).

## 2026-09-28: Scratch OS Linux Kernel Architectural Alignment

Aligned the 35-day scratch RISC-V kernel to strictly adopt Linux kernel folder structures, file names, and data structures, hosted in `c:\workspace\career_ws\riscv-kernel`. Replacing ad-hoc filenames with standard Linux kernel conventions (`arch/riscv/kernel/head.S`, `arch/riscv/kernel/entry.S`, `init/main.c`, `kernel/sched/core.c`, `mm/page_alloc.c`, `include/linux/sched.h`) provides direct muscle memory for Staff-level semiconductor and Linux systems interviews. Reconciled Day 1 linker script to `arch/riscv/kernel/vmlinux.lds.S` with canonical symbols (`_start`, `_stext`, `_etext`, `__bss_start`, `__bss_stop`, `_end`). Proceeded to Day 2 assembly boot entry (`arch/riscv/kernel/head.S`).

## 2026-09-28: Scratch OS Architectural Policy & Generic Reference Mandate

Formulated the scratch RISC-V kernel architectural principles: `xv6-riscv` serves as an established conceptual baseline for learning OS mechanics, not a rigid template. Across the entire kernel (boot, memory, scheduling, traps, drivers, filesystem), the Adaptive Superiority Principle dictates that if a design is cleaner, more portable, or more intuitive (such as maximizing standard C logic over processor-specific assembly), our superior approach is adopted. When the curriculum advances to Zephyr RTOS and Embedded Linux, the tutor will explicitly contrast these choices against production kernel architectures (OpenSBI, PSCI, BSP/AP sequencing, IPIs, Devicetree).

## 2026-09-28: Scratch RISC-V OS Day 1 Linker Script Review

The learner created `mini_riscv_kernel/kernel.ld` from scratch. Source review: **Partially correct overall**. Core architectural requirements are met: target architecture `OUTPUT_ARCH("riscv")`, entry point `ENTRY(_start)`, base physical address `. = 0x80000000`, section mapping for `.text` and `.rodata` with 4KB page alignment, and export of `_etext`, `_erodata`, `_bss_start`, `_bss_end`, and `_end`. Four syntax/typo issues identified for learner correction: (1) line 19 `.data = {` uses `=` instead of colon `:`, (2) line 21 `. = ALIGH(4096);` has typo `ALIGH` (should be `ALIGN`), (3) line 25 `.bss = {` uses `=` instead of colon `:`, and (4) line 29 `. = ALIGH(4096);` has typo `ALIGH`. Learner code preserved. Proceeded to Day 2 (`boot.S`).

## 2026-09-28: RISC-V Architecture, Privilege Modes & Boot Entry (W03-A Completion)

Taught RISC-V 64-bit privilege hierarchy (Machine, Supervisor, User modes), Control and Status Registers (`mstatus`, `mepc`, `satp`, `medeleg`, `mideleg`, `pmpcfg0`, `pmpaddr0`, `tp`), and bare-metal entry execution. Traced per-hart stack pointer math in `entry.S` (`sp = stack0 + (hartid + 1) * 4096`). Traced the `mret` privilege-drop trick in `start.c` setting `mstatus.MPP = S-mode` and `mepc = main()`. W03-A completed by learner report. Transitioned to W03-B.

## 2026-09-27: Linux Kernel Reverse Goto Unwind & Week 2 Close

Taught the canonical Linux kernel reverse `goto` error unwinding pattern (LIFO release of partially acquired resources: allocation $\rightarrow$ MMIO mapping $\rightarrow$ clock enable $\rightarrow$ IRQ registration). Compared with C++ RAII destructors and Linux managed device resources (`devm_`). Week 2 is completed by learner report. Transitioned to Week 3 (RISC-V / xv6 architecture).

## 2026-09-27: Cancellation and Teardown Completion (W02-C & W02-D)

The learner implemented [the cancellation teardown exercise](../Lessons/07_cancellation_teardown.c) without notes, covering both monotonic timed wait and cancellation/drain. Source review: **Partially correct overall**. Core architectural contracts are sound: state machine transitions (`REQ_PENDING`, `REQ_COMPLETED`, `REQ_CANCELLED`, `REQ_TIME_OUT`) are serialized under `lock`; `clock_gettime(CLOCK_MONOTONIC)` with carry normalization correctly used; predicate loop `while (!req->worker_fineshed && rc != ETIMEDOUT)` prevents spurious wake-ups; cancel path synchronously drains `worker_fineshed` via condition wait; and destruction correctly joins the worker thread once without double-join or double-destroy. Specific compilation/syntax bugs identified: `stderror` typo (should be `strerror`), `pthread_cond_attr_t` typo (should be `pthread_condattr_t`), `pthread_cond_init` passing value `attr` instead of pointer `&attr`, missing paren in `check(pthread_create(...);`, `MONOTONIC` instead of `CLOCK_MONOTONIC`, `timeout` instead of `timeout_ms`, struct field name mismatches (`req->state` vs `status`, `req->mutex` vs `lock`), and missing semicolon on line 124. Exercise `06_timed_wait.c` was removed as it is fully subsumed. Learner code preserved.

## 2026-09-24: Condition Variable Completion

The learner implemented [the condition-variable exercise](../Lessons/05_condition_variable.c) without notes. Source review: **Partially correct overall**. Core concurrency invariants are correct: shared telemetry and state predicate (`ready`) updated under `lock`; `pthread_cond_signal` emitted under lock; consumer evaluates predicate in a `while (!ready)` loop preventing spurious wake-ups and stolen conditions; telemetry read safely in critical section; `pthread_join` precedes destruction; destroying `cv` before `lock` is valid once worker joins. Specific syntax/portability defects noted: stray single quote on line 43 (`printf(...)'`), non-standard macro `PTHREAD_LOCK_INITIALIZER` (should be `PTHREAD_MUTEX_INITIALIZER`), missing `#include <stdlib.h>` for `exit` and `EXIT_*`, missing forward declaration of `check()`, and missing `return NULL;` in `producer()`. Learner code was left untouched.

## 2026-09-23: Curriculum Consolidation

The learner confirmed week 1 completed, week 2 partially completed, 22 hours/week going forward, free official resources, Bengaluru primary/Hyderabad secondary, and INR 70-90 LPA CTC as an aspiration. Hardware reported: STM32F446RE with onboard ST-LINK and BeagleBone Black; ESP32/Pico/Zero variants remain unknown. Simulation is the fallback if physical setup stalls, not evidence of actual bus/DMA/power behavior.

The five-month document now owns the detailed twenty-week curriculum and official training mapping. The former weeks 1-8 document is a redirect; the tracker owns live module status. Weeks 1-2 educational scope and all earlier evidence remain preserved. The superseded sequence placed xv6/Zephyr/Linux/interview launch in weeks 3/4/5-7/8; the new sequence gives xv6 three weeks, Cortex-M/Zephyr four weeks, then embedded Linux prerequisites before driver work. Earlier weeks 9-20 topics are explicitly mapped to new weeks or marked stretch/deferred in the curriculum. This is schedule consolidation, not completion of additional training.

Before replacement, the three original plan/log documents were copied and SHA-256 verified under `%LOCALAPPDATA%/CareerLearningBackups/curriculum-20260923-181532`. This private recovery copy is outside publication trees. No lesson code was changed, compiled or run by the planning update.

Provisional future calendar: week 3 starts 2026-09-28 after remaining week-2 work; weeks 3-20 allocate 396 hours and end 2027-01-31, with 2027-02-01 through 2027-02-22 reserved for recovery/interviews. Firmware checkpoint is 2026-11-16 through 2026-11-22; driver-focused applications expand as later evidence supports them. Shift dates if prerequisites take longer. No guaranteed pay band or hiring outcome was found in sampled official job descriptions.

Provider research retrieved on 2026-09-23: MIT 2025 schedule (2026 schedule under construction), Bootlin embedded Linux/kernel/debugging courses, Zephyr setup/kernel/device-model and board documentation, and optional Nordic fundamentals. The retrieved AMD 91919 and Qualcomm 3095332 roles support C/C++, architecture, drivers, buses, debugging and ownership priorities; other sampled platform/power roles are stretch or adjacent, not promised matches. No course enrollment, hardware purchase, install, flash, commit or push was performed.

To target the INR 70-90 LPA CTC Senior 2 / Staff Systems Engineer band (Qualcomm, AMD, NVIDIA, TI in Bengaluru/Hyderabad) from a 36 LPA automotive CDD validation baseline, the plan anchors on 3 Flagship Capstone Artifacts centered on the industry's #1 most in-demand combination: High-Speed SPI/I2C with DMA, Regmap, and IIO:
1. Capstone 1 (Zephyr RTOS & MCU, W09): High-Speed SPI sensor telemetry pipeline with STM32 circular DMA (`dma_stm32`, `STM32_DMA_MODE_CYCLIC`), devicetree bindings, double-buffering, and zero-copy lockless ring buffer (`sys_ring_buf`), referencing [drivers/spi/spi_stm32.c](../../zephyr-workspace/zephyr/drivers/spi/spi_stm32.c). Solves dropped samples under high-frequency interrupts.
2. Capstone 2 (Linux Kernel Peripheral Driver, W14 & W16): Linux Industrial I/O (IIO) high-speed SPI/I2C client driver with `regmap` transport abstraction, hardware FIFO integration, threaded interrupt handling (`request_threaded_irq`), devicetree YAML bindings, and Runtime PM, referencing [drivers/iio/imu/inv_mpu6050/inv_mpu_spi.c](../../linux/drivers/iio/imu/inv_mpu6050/inv_mpu_spi.c) and [inv_mpu_core.c](../../linux/drivers/iio/imu/inv_mpu6050/inv_mpu_core.c). Solves transport abstraction and hard-IRQ latency budgets.
3. Capstone 3 (Linux Kernel DMA Engine & Cache Coherency, W17 & W18): Streaming DMA driver with cache management (`dma_map_single`, `dma_sync_single_for_device/cpu`), 64-byte L1 cache-line alignment (`IIO_DMA_MINALIGN`), and race/latency triage using `ftrace` and `lockdep`, referencing [drivers/iio/adc/nxp-sar-adc.c](../../linux/drivers/iio/adc/nxp-sar-adc.c). Solves silent data corruption across Point of Coherency (PoC) vs Point of Unification (PoU).

Simulation-First Policy: All driver and firmware logic is developed and verified first in simulation (QEMU / `native_sim` / virtual guest) on weeknights to ensure zero-blocker momentum; physical deployment to STM32 NUCLEO-F446RE (logic analyzer) and BeagleBone Black (serial) takes place during dedicated weekend lab blocks.

## 2026-09-23: Memory Ordering And Mutex Completion

The learner reports completing both implementations. No compiler or runtime commands were run for this review; source inspection and expected outcomes are not observed test results. No study hours or delayed-retention scores were supplied. Learner code was preserved unchanged.

| Exercise | Completion status | Source review | Runtime evidence |
| --- | --- | --- | --- |
| [One-time publication](../Lessons/03_memory_ordering.c) | Completed, learner-reported | Correct: producer writes 45 before release-storing true; consumer waits with acquire before reading; no subsequent payload writes; create/join errors checked. The trailing-semicolon loop is an intentional empty busy-wait, not a spinlock. | Not tool-verified; expected measurement 45 |
| [Mutex counter](../Lessons/04_mutex.c) | Completed, learner-reported | Correct for the Linux host target: worker and main each perform 100000 increments under the same mutex; successful join precedes destruction and the final read; pthread errors checked. | Not tool-verified; expected counter 200000 |

Next: [condition-variable completion](../Lessons/05_condition_variable.c), an empty learner-owned exercise file. Teach the predicate, same-mutex rule, release-and-wait, reacquisition, spurious wake-ups and notifications not being stored before the full chat example. Week-2 session 2 remains in progress until that portion is addressed. Questions remain paused.

## 2026-09-22: Allocation Ownership Practice

The learner reports completing private Feynman practice and implementing [the allocation example](../Lessons/01_alloc_ownership.c) after closing the notes. Source review: **Partially correct overall**. The temporary realloc result, success-only owner/capacity update, index-based borrowed-pointer reconstruction and final owner cleanup are correct. Initial malloc failure is unchecked before writing samples[0]; check for NULL and return EXIT_FAILURE before any element access. Prefer size_t for capacities/indexes in general buffer APIs. The source was left unchanged; no compilation, runtime or allocation-failure tests were performed during this review. Closed-notes practice is student-reported; delayed retention remains unverified.

Next: worked overflow-safe byte-range validation, including exact-fit and zero-length policy, before size calculations and decoder integration. Questions remain paused. Optional private revisit on 2026-09-23; no required submission or automatic reminder.

## 2026-09-22: Workspace Migration Verification

The user's new source clones are now referenced by both workspace files. Full private backups of all four old folders verified 114,383 files by SHA-256 before authorized cleanup. Old Zephyr is deleted; old xv6, Linux and learning folder contents are removed, with three empty Windows-locked directory shells remaining. Employer folders and the 13 pre-existing modified files in the new Linux clone were preserved. See [setup details](../docs/workspace_setup.md) for recovery location and pending environment gates.

Six preserved xv6 utility sources and the container recipe were migrated to revision `9e3161a`. Removing three unused xargs helpers resolved the `-Werror` build failure. The kernel/filesystem build and bounded QEMU shell smoke checks for sleep, pingpong, find, xargs, fdtest and linktest passed. This verifies migration operation, not exhaustive utility correctness, independent authorship, retained mastery or completion of new curriculum exercises. Questions remain paused. No commits or pushes were performed.

## 2026-09-22: Source-Sample Assessment

Read-only sampling of prior validation artifacts showed parameterized device tests, structures referring to response arrays, callback/error counters, topology-dependent byte decoding and state-targeted fault injection. This supports advanced applied embedded C rather than elementary syntax. These observations concern artifacts; individual authorship, complete framework contracts and runtime execution were not verified.

Priorities: explicit length/ID contracts, visible timeout versus completion outcomes, asynchronous buffer ownership, failure unwind and Linux driver lifecycle. A missing local check is not proof that the framework lacks it; callback code is not proof of a data race without execution-context evidence. Linux production ownership, C++ depth, practical debugging and delayed retention remain unverified. Labs use independently written generic examples, not proprietary source, protocols or test data. Questions remain paused.

## Preserved xv6 History

Migrated from retired September boot/utilities and SV39 schedules before deletion. Status/hours below are **historically recorded**, not reverified by this migration. Earlier claims of mastery or verification are not newly endorsed. The full original schedules remain in the hash-verified private backup. The original week-1 roll-up and individual rows both total 11.0 hours. Blocker/carry-over fields were blank.

| ID | Recorded item/evidence | Hours | Recorded status/date |
| --- | --- | ---: | --- |
| W1-00 | Tutor session; book HTML/PDF downloaded | 0.5 | Done 2026-09-15 |
| W1-01 | xv6-lab image built from local container recipe | 0.5 | Done 2026-09-15 |
| W1-02 | xv6 shell boot; ls and cat used | 0.5 | Done 2026-09-15 |
| W1-03 | Chapter 1: syscall, fork/exec, descriptors, pipes, filesystem interfaces | 1.0 | Done 2026-09-15 |
| W1-04 | sleep implemented and tried in shell | 0.5 | Done 2026-09-15 |
| W1-05 | pingpong implemented; pipe IPC exercised | 0.5 | Done 2026-09-17 |
| W1-06 | find implemented; recursion and manual string/memory operations | 1.0 | Done 2026-09-17 |
| W1-07 | xargs implemented; token parsing and shell test recorded | 1.0 | Done 2026-09-18 |
| W1-08 | Chapter 2 skimmed; OS organization | 0.5 | Done 2026-09-18 |
| W1-09 | Entry assembly and per-hart stack trace | 1.0 | Done 2026-09-18 |
| W1-10 | M/S/U privilege roles | 1.0 | Done 2026-09-18 |
| W1-11 | start and machine-to-supervisor transition | 1.0 | Done 2026-09-18 |
| W1-12 | CSR wrappers versus instructions | 0.5 | Done 2026-09-18 |
| W1-13 | Main initialization, hart roles and started flag | 0.5 | Done 2026-09-18 |
| W1-14 | Five interview prompts/recitation recorded | 0.5 | Done 2026-09-18 |
| W1-15 | Four utilities and boot teach-back recorded | 0.5 | Done 2026-09-18 |
| W2-01 | SV39 virtual/physical layout, VPN splitting and offset | 1.0 | In progress; completion date blank |
| W2-02 | Page-table flags/permissions | Not recorded | Not started |
| W2-03 | Physical allocator/free list | Not recorded | Not started |
| W2-04 | Software page-table walk | Not recorded | Not started |
| W2-05 | Mapping and kernel address space | Not recorded | Not started |
| W2-06 | User address-space lifecycle | Not recorded | Not started |
| W2-07 | Page-table printer exercise | Not recorded | Not started |
| W2-08 | Closed-book page walk | Not recorded | Not started; assessment now paused |

Do not assign all utilities again. Inspect preserved source and revisit only needed concepts in new week 3. Old salary assumptions, study caps and six-month dates are not carried forward. Existing dated conversational records below remain intact.

## 2026-09-22: Advanced C Evidence And Pacing Revision

This summary records actual conversational answers and corrections, not compiler/runtime verification or a global C level. Immediate correct follow-ups after teaching are distinguished from independent derivation and delayed recall. No elapsed study time has been supplied.

| Area | Observed evidence | Feedback or remaining gap | Current teaching action |
| --- | --- | --- | --- |
| Allocation lifetime | Recognized invalid interior pointer when `realloc` moves, but believed same-address success preserved it and failure lost the allocation | Partially correct: successful `realloc` ends the old lifetime even at the same address; nonzero-size failure preserves the original allocation. The shown failure branch deliberately freed it | Worked before/after allocation and ownership table; confirmation on a fresh case deferred |
| Retained references | Proposed saving indexes before resizing and rebuilding pointers afterward | Correct mechanism with a complete update/ownership contract; value snapshots do not retain a selection, and external pointer copies need handling | Concrete owner/index example; distinguish position from logical identity |
| Logical identity | Proposed comparing values and initially associated generation with resize count; after explanation correctly answered generation 12 before removal, 13 afterward | Value equality cannot prove identity. Per-slot generation follows an application-defined removal/reuse policy; last answer applied the taught policy correctly | Keep the request-slot C example available; no extra generation quiz now |
| Allocation-size arithmetic | Initially trusted `malloc` failure alone; later correctly calculated `0x10000000c` wrapping to `0x0c` in the stated unsigned 32-bit arithmetic | Correct wrap calculation after instruction; allocation cannot diagnose a size already calculated incorrectly | Derive size guards with byte/count units, actual arithmetic types and worked boundary cases |
| Header plus payload | Proposed adding header bytes to count in a guard; after correction correctly derived four elements for a 31-byte limit, seven-byte header and six-byte elements | Initial guard mixed units; later exact-fit reasoning correct. General guard derivation needs independent verification later | Worked subtraction/division guard, with zero-size and allocation-failure policy stated |
| Range checks | Correctly calculated wrap to eight; proposed a safe fixed-32 guard still using addition; correctly evaluated end/zero-length true and end/one-byte false after general repair | Fixed-size guard did not meet generic no-addition requirement. Short-circuit offset bound before subtracting was taught | Apply numerical bounds separately from actual pointer/object validity |
| Shared data | Correctly said `volatile` does not guarantee atomicity, but said it prevents compiler optimization generally and that payload result may vary | Partially correct: volatile access rules do not provide thread synchronization; conflicting unordered ordinary accesses cause undefined behaviour, not a guaranteed set of outputs. Acquire/release remains unclear by explicit self-report | Teach with concrete C, definitions, an order trace and one worked later-write failure; no quiz |
| Representation and practical work | No independent advanced aliasing/alignment/restrict implementation, build, debugger or concurrent test evidence supplied | Unassessed, not failed. Earlier chat punctuation is not a proficiency proxy | Cover later inside parser/driver examples; retain independent C++ and architecture pacing |

**Learning preference:** advanced subject matter with accessible explanation, followed by optional private blank-paper teaching to an imagined 12-year-old. Review volunteered answers explicitly. Do not force a new prediction or diagnostic after every correction. Current assessments and recall questions are paused; old due dates are suggestions, not required submissions or automatic reminders.

## Initial Assessment: 2026-09-22

Historical starting snapshot. Later entries and the current summary above supersede its pending actions and initial judgments; do not restart these questions.

| Concept | Student evidence | Evaluation and correction already discussed | Next check | Due | State |
| --- | --- | --- | --- | --- | --- |
| Pointer versus value | "pointer stores a memory location" | Correct introductory model for a valid object pointer; distinguish address from pointed-to contents | Explain `cursor` versus `*cursor` in a fresh example | Next lesson | Pending |
| Pointer arithmetic | "incrementing it points to next memory address" | Partially correct: next element, not necessarily next byte; element size and allowed array range matter | Trace an integer array, then a different element type | 2026-09-23 | Recall pending |
| Executable versus execution | "flasing it in mcu and executing it" | Partially correct MCU workflow; an executable is stored code/data, execution has live state, and Linux processes do not require MCU flashing | Explain two runs of one executable and what state can differ | 2026-09-23 | Recall pending |
| C++, Python, Rust and Linux | Self-reported familiarity | Not assessed; self-report is not a performance score | Assess the prerequisite needed for the selected lesson | When needed | Unassessed |

Dates are suggested review dates, not automatic reminders. If the next session is later, review due items then; do not create a catch-up backlog of every missed interval.

## Session Record Template

Append one small entry after actual work. Keep employment details, confidential code and application records elsewhere/private. Record the learner's original explanation before the correction, not a tutor-written answer presented as the learner's.

```text
Date / available minutes / actual minutes (only if known):
Concept / exercise / source revision:
Student explanation or prediction:
Evidence (code, command, trace, drawing or test result):
Evaluation: Correct / Partially correct / Incorrect
What was right:
Specific gap and authoritative correction:
Fresh retry and result:
Scores: explain __ / trace __ / implement __ / debug __ / transfer __
Next action:
Next recall date:
```

Score `0 = not yet`, `1 = with hints`, `2 = independently`, `3 = independently on a fresh delayed problem`; use `N/A` when unassessed. Initial conversational corrections above do not establish an independent practical score.

## 2026-09-22: First Pointer Trace

- Evidence: student answered, "it points to 0x1004 that is samples[1] and *cursor reads 20. values inside samples didnot change."
- Conditions: three-element integer array `{10, 20, 30}`, pointer initially at the first element, then incremented once; illustrative base `0x1000`, `sizeof(int) == 4`.
- Evaluation: **Correct** for both questions. Element, address, dereferenced value and unchanged array contents are correctly identified.
- Feedback: pointer increment changes the pointer's stored address by one pointed-to element, not the array values. The four-byte step follows the stated element size, not pointer size.
- Scores: trace 2 for this immediate example; explain/implement/debug/transfer N/A. No delayed retention or code execution has been assessed. Study minutes not recorded.
- Next action: explain element-size stepping in plain words, then trace a write through the pointer.
- Next recall: 2026-09-23, using a fresh example without notes.

## 2026-09-22: Pointed-To Write And Step Size

- Evidence: student answered, "{10, 99, 30} are the contents. cursor still points to 0x1004" and "cursor++ advances to the next memorylocation which is sizeof(type), type is the type of cursor."
- Evaluation: first answer **Correct**. Second answer **Partially correct**: element-size stepping is recognized, but the type of `cursor` is `int *`; the step uses the pointed-to type `int`, not the pointer type.
- Feedback: increment advances one element, `sizeof(*cursor)` bytes within permitted array bounds, not `sizeof(cursor)` bytes. For this example the step is four bytes; pointer storage size is a separate property.
- Scores: trace 2 for the immediate write example; explain 1 (type distinction needs feedback); implement/debug/transfer N/A. No execution or delayed retention assessed.
- Next action: contrast `sizeof(cursor)` with `sizeof(*cursor)` in a fresh wider-element example and request a plain-language explanation.
- Next recall: 2026-09-23. Actual study time not recorded.

## 2026-09-22: Double-Element Stepping

- Evidence: first answer was "position = 0x2000, *position is 1.5"; second explained that eight-byte elements make `position++` point to `0x2008`, reading `2.5`.
- Conditions: `double readings[3] = {1.5, 2.5, 3.5}`, pointer initially at the first element, then incremented once; illustrative base `0x2000`, element size eight bytes and pointer storage size four bytes.
- Evaluation: answer 1 **Incorrect for the requested post-increment state**; it describes the initial state. Answer 2 **Correct**; its explanation and final state repair the first answer. Pointer storage size does not determine element stepping.
- Feedback: explicitly distinguish before (`0x2000`, `1.5`) and after (`0x2008`, `2.5`). The array contents remain unchanged.
- Scores: explain 2 for element-size stepping; trace consistency needs a fresh check due to conflicting answers in this response; implement/debug N/A. No delayed retention or execution assessed.
- Next action: label states around one additional pointer increment and distinguish `position + 1` from modifying `position`.
- Next recall: 2026-09-23. Actual study time not recorded.

## 2026-09-22: Pointer Addition Without Mutation

- Evidence: student answered, "position is at 0x2008 and next is at 0x2010, *position is 2.5 and *next is 3.5. creating next doesn't move position."
- Conditions: continuing the three-element double array, `position` designates its second element; `double *next = position + 1;` initializes a separate pointer.
- Evaluation: **Correct** on all parts. Both addresses and dereferenced values are correct; computing `position + 1` does not modify `position`.
- Feedback: addition computes a new pointer value, whereas increment changes the pointer operand. The hexadecimal step from `0x2008` to `0x2010` is eight bytes in this example.
- Scores: trace 2 and explain 2 for this immediate distinction; implement/debug N/A. The previous trace inconsistency was repaired on this fresh check, but delayed retention remains unassessed.
- Next action: introduce one-past formation versus dereference, then assess the boundary with a small paper trace.
- Next recall: 2026-09-23. No execution or actual study time recorded.

## 2026-09-22: One-Past Formation Versus Reading

- Evidence: student correctly identified `0x2018` and allowed increment, then said reading is possible but either crashes or reads garbage; attributed crashes to absent physical memory or missing read permission.
- Evaluation: answer 1 **Correct**. Answer 2 **Partially correct**: these are possible machine-level outcomes, but reading through a one-past pointer is undefined behaviour in C regardless of whether the numerical address is mapped/readable. Outcomes are not limited to crash or garbage.
- Feedback: forming the one-past pointer is permitted; reading or writing an element through it is not. Hardware permissions do not establish C object bounds. Undefined behaviour can appear to work or affect surrounding optimized code. A hardware fault may be handled, and a missing resident page may be supplied by an OS; a fault is not automatically a program crash.
- Scores: trace 2 for one-past formation; explain 1 for bounds versus accessibility; implement/debug/transfer N/A for this topic. No runtime or delayed retention assessed.
- Next action: contrast a legal last-element read with an illegal end-marker read even when the address is readable; request a plain-language explanation.
- Next recall: 2026-09-23. Actual study time not recorded.

## 2026-09-22: Last Element Versus Array Extent

- Evidence: student answered, "last is valid and it obtains 3.5 and outside is invalid" and "it is not a valid read, because that memory belongs to something lese not the array. array ends at 0x2010."
- Evaluation: first answer **Correct** when classifying the expressions: `*(end - 1)` designates the last element, whose value is `3.5`; reading `*end` is undefined behaviour. Second answer **Partially correct**: outside the array is the relevant boundary, but no adjacent object's existence or ownership is established, and `0x2010` is the last element's starting address, not the last byte.
- Feedback: in the stated byte-addressed model with eight-byte doubles, the last element occupies `0x2010` through `0x2017`; the full array occupies `0x2000` through `0x2017`, with one-past at `0x2018`. Reading through the one-past pointer is invalid regardless of adjacent storage or hardware readability. The snippet as a whole has undefined behaviour if the invalid read is executed, so classification of its valid expression is not a guarantee about the whole program's execution.
- Scores: trace 2 for classifying the two reads; explain 1 for exact storage extent/adjacent-memory reasoning; implement/debug N/A. No execution or delayed retention assessed.
- Next action: draw the complete byte extent and end marker for a fresh array with a different element count/size.
- Next recall: 2026-09-23. Actual study time not recorded.

## 2026-09-22: Fresh Short-Array Boundary Check

- Evidence: student answered, "starting address is 0x3006. final byte is 0x3007" and "0x3008 is one past address".
- Conditions: four-element short array at illustrative base `0x3000`, with `sizeof(short) == 2`; the requested starting address was that of the last element.
- Evaluation: **Correct** on both questions. Last-element start, final occupied byte and one-past position are correctly distinguished. The array itself still begins at `0x3000`.
- Feedback: the four elements occupy byte pairs `0x3000-0x3001`, `0x3002-0x3003`, `0x3004-0x3005` and `0x3006-0x3007`; `0x3008` marks the end, not a readable fifth element.
- Scores: trace 2 for the fresh boundary calculation; the previous extent mistake is repaired on this immediate check. Independent plain-language teach-back, implementation, debugging and delayed retention remain to be assessed for this topic.
- Next action: review a small end-pointer traversal with one incorrect comparison and propose its repair before execution.
- Next recall: 2026-09-23. No runtime or actual study time recorded.

## 2026-09-22: End-Pointer Loop Repair

- Evidence: student answered, "it enters the loop but there is no element in it" and "it should be < instead of <=".
- Conditions: four initialized short samples `{10, 20, 30, 40}`, `cursor` at the first element, `end = samples + 4`, and a loop incorrectly allowing `cursor <= end` before dereference and increment.
- Evaluation: answer 1 **Correct**: equality admits the one-past pointer into a body that reads it. Answer 2's condition repair **Correct**: `<` excludes the end marker. The requested corrected total was not supplied; do not count it as demonstrated.
- Feedback: the corrected loop adds four elements for a total of 100, then stops with `cursor == end`. Its final increment forms the permitted one-past pointer. The original body reads one-past and, if followed literally, also attempts an increment beyond one-past; neither operation is valid and execution has no guaranteed result.
- Scores: debug 2 for this local paper repair, trace 2 for the equality case; full implementation, total prediction and delayed retention not assessed.
- Next action: request a brief closed-book explanation of typed stepping, pointer versus pointed-to mutation, and the end marker; then a learner-written runnable traversal.
- Next recall: 2026-09-23. No runtime or actual study time recorded.

## 2026-09-22: Consolidated Pointer Teach-Back

- Student explanation: "A pointer holds a memory address, which can contain a value or another memory address"; increment travels to the next element by its byte size; changing a pointer changes its location, while writing replaces the value at the pointed-to location; an end pointer has no element.
- Evaluation: **Correct** for all four ideas in the studied valid-object/array context. The description of writing clearly intends writing through the pointer. No drawing was submitted, and closed-book conditions were not independently verified.
- Precision feedback: a pointer stores an address value; the designated object may itself be a pointer object. Say "assign to the pointer" versus "write through `*pointer`" to avoid ambiguity. The step uses the pointed-to type's size, and array arithmetic must stay within the permitted range. The end marker here means one-past, not a pointer to the last element. Not every pointer value designates a live accessible object.
- Scores: explain 2 for the consolidated immediate teach-back; prior paper traces and local boundary repair remain evidence, not runtime verification. Independent implementation and delayed retention are pending; no overall C-mastery claim.
- Next action: learner writes a short C fragment to sum four initialized integers using pointer traversal and a one-past stopping condition, then predicts the total before execution.
- Next recall: 2026-09-23 with a fresh example. Actual study time not recorded.

## 2026-09-22: First Traversal Implementation

- Evidence: student supplied `int samples[4] = {1, 2, 3, 4};`, `int *cursor = samples;`, `int *end = samples + 4;`, and `while (cursor < end) { cursor++; }`.
- Evaluation: **Partially correct** for the requested summation task. Pointer initialization, one-past construction, loop bound and increment are correct; the array is unchanged. There is no accumulator or element read, so no sum is computed. No predicted total was supplied.
- Feedback: at loop exit `cursor == end`, reached through four permitted increments without dereferencing one-past. Add `int total = 0;` before the loop, then add the current pointed-to value into it before advancing `cursor`. Incrementing first would skip the first sample and eventually read one-past.
- Input change: learner used `{1, 2, 3, 4}` instead of the assigned `{3, 6, 9, 12}`; this is acceptable for testing the same concept. The corrected sum for the submitted input is 10, not the original input's 30. Neither total was student-predicted yet.
- Scores: implementation 1 for the summation task, with correct traversal structure acknowledged. No compile/run or delayed retention assessed.
- Next action: learner adds the accumulator and current-element addition, then submits corrected code and predicts its result.
- Next recall: 2026-09-23. Actual study time not recorded.

## 2026-09-22: Summation Completed With Syntax Fixes Pending

- Evidence: student added `int total = 0` before the loop and `total += *cursor` before `cursor++;`, predicting "total is 10" for `{1, 2, 3, 4}`. Both added statements omitted their terminating semicolons.
- Evaluation: logic **Correct**, predicted total **Correct**, submitted C syntax **Partially correct**. Add a semicolon after the initialization and accumulation statements; no algorithm change is needed.
- Feedback: total progresses `0 -> 1 -> 3 -> 6 -> 10`. Each element is read before the pointer advances; the final pointer equals the one-past end and is not dereferenced. The array remains unchanged.
- Scores: implementation 1 (completed after targeted feedback, with syntax correction still required); trace 2 for the total prediction. No compiler or runtime result has been observed; delayed retention and independent implementation on a fresh exercise remain pending.
- Next action: correct the two semicolons and put the fragment inside a complete host C program that prints the total; compile/run and compare actual output with the prediction.
- Next recall: 2026-09-23. Actual study time not recorded.

## Weekly Review

| Week | Hours actually used | Demonstrated output | Weak prerequisite | Next adjustment | Gate evidence |
| --- | --- | --- | --- | --- | --- |
| 1 | Not recorded | Pointer teach-back plus corrected arithmetic, exact-fit and range follow-ups demonstrated | Allocation guarantees, independent guard derivation and thread synchronization need focused teaching; runtime and retention unassessed | Advanced worked C examples; no quizzes until explicitly resumed | Assessment paused; no mastery claim |

Every weekend: choose the next small deliverable and remove one stretch task if behind. Private recall is optional while questions are paused; do not turn old due dates into a compulsory backlog. Keep first attempts, tutor corrections, immediate follow-ups and delayed evidence distinguishable. After a successful lesson, suggest +1/+3/+7/+14/+30-day retrieval adjusted to results, without claiming it happened.

## 2026-10-22: Foundation Preserved; Fresh Two-Project Track

**Learner decisions.** Preserve the existing plan/skills as valuable long-term
foundations, create fresh active material, use QEMU before board-specific work
and introduce Yocto for actual Linux builds. Project A (embedded fault/event
recorder) selected. Learner additionally requested a separate MCU RTOS project;
the tutor recommends B, a standalone Zephyr telemetry/health node, with
two-board CAN gated on hardware. INR 90 LPA total CTC remains aspirational.
The five-month deadline concerns preparation/interviews and aim for an offer,
not a guaranteed job or joining date. No quizzes were resumed.

**Hardware update, learner-reported.** Two STM32 boards (one F446RE Nucleo,
second model pending), BBB, Saleae logic analyser, Arduino and an unresolved
Pico-or-Zero. Exact extra models expected in about two days; no automatic
reminder promised. Purchase budget/BOM deferred until inventory confirmation.
No hardware capability, wiring or electrical measurement verified.

**Preservation, tool-verified.** 37 working files, including uncommitted edits,
were copied and SHA-256 compared before the rewrite. At the learner's request
the snapshot was relocated into
[backup/plan](../backup/plan/2026-10-22-foundation/README.md) and
[backup/skills](../backup/skills/2026-10-22-foundation/README.md); all 37 hashes
matched again. The manifest records paths relative to `backup`. No downloaded
books, dependency trees, Git metadata or sibling source were copied. This is an
in-repository reference, not independent disaster recovery.

**Source inspection, not new execution.** Available branches `main` and `v1`
both point to `6610119`; `v2` points to `3fddd0d`. Earlier v1 allocator/MMU
evidence remains valuable, but the final state is superseded. Its test runner
prints overall PASS without aggregating per-test failures, and the timer test
has an unbounded wait; do not reuse those patterns. K0 committed code is not
proof of all outstanding acceptance tests. K1b-1 source is not implemented on
the inspected v2 tree. No checkout, source edit, build or QEMU run performed.

**Corrections to active guidance.** Historical K1b-1 prose overstates volatile
assembly, fixed-register locals and memory clobbers. These do not generally
prevent all motion, reserve registers or provide hardware memory fences.
Firmware DBCN buffer writes block through UART despite the SBI nonblocking
buffer-write contract; repair requires learner-owned exact-file approval.
Historical boot/MMU examples and missing old-source links must not be treated
as active recipes. Old lesson files remain unchanged.

**Market correction.** The earlier log's "industry #1 most-in-demand" claim for
SPI/I2C/DMA/regmap/IIO is unsupported. Current official role samples span many
subsystems and demand relevant ownership and debugging. Public Freelancer
briefs supplied examples of useful integration, waveform capture, resource
budgets and test deliverables, not salary evidence. Upwork returned 403.
Secondary compensation aggregates are not employer offers or personal forecasts.

**Fresh material.** New roadmap, project contracts, tracker, setup/routing,
README and five skills retain useful teaching principles without adopting the
old calendar. Saved workspace now includes the existing kernel root. The
curriculum covers 21 full weeks at 22 hours/week (462 total hours), with buffers
and protected contingency. Reference versions, build readiness, projects,
independent mastery and production qualification remain unverified until
appropriate evidence exists.

**Next action.** Confirm remaining board/probe/BOM details when available and
start the environment/readiness gate. Keep implementations learner-authored;
no installations, purchases, flashing, commits or pushes authorized by this
document update.

**Document validation, tool-run.** Active/index local file links passed across
16 files (historical body links intentionally excluded). Five skill names,
quoted descriptions, hints and frontmatter structure passed static checks.
Workspace JSON resolves all five roots. The 21 weekly dates advance in
seven-day steps; weekly blocks total 22 hours and 462 hours overall, ending
21 March 2027. All 37 backup hashes remained unchanged; whitespace checks
passed for edited tracked files. Kernel status remained on v2 with only the
three previously untracked editor/configuration paths. No fresh-chat skill
discovery, project build, emulator or hardware run was performed.

## 2026-10-23: Podman Workflow And Resource Inventory

**Learner clarification.** Use Podman commands; its existing WSL2 backend is
acceptable, but no separate WSL distribution/development workflow is wanted.
Update environment facts/guidance only. The broader reviewed curriculum
refinements remain unapplied pending approval. No quiz resumed.

**Read-only host observations.** Windows 11 Enterprise; 31.69 GiB physical RAM,
7.97 GiB available at inspection, eight logical processors; C: 102.13 GiB free.
`Get-CimInstance`, `Get-PSDrive` and `Get-Command` supplied this inventory.
Native `qemu-system-arm` and `qemu-system-aarch64` both report 9.1.1.
The Arm machine list includes `virt`, `mps2-an386` and other STM32-related
models; these do not establish exact BBB/F446 peripheral emulation.

**Podman metadata versus effective guest.** `podman machine list --format json`
reports an existing running WSL-backed machine with four CPUs, memory field
2 GiB and disk field 100 GiB. Through `podman machine ssh`,
`free -b` reports 16,607,719,424 bytes total RAM, 15,941,533,696 bytes available
and 4,294,967,296 bytes swap. Thus the earlier metadata must not be treated as
a verified 2 GiB effective guest limit.

Guest `df -B1 /` reports 1,081,101,176,832 bytes capacity and
1,015,760,351,232 bytes available. This is virtual filesystem capacity, not
additional physical host disk space. Container limits, build-volume location
and its backing-store capacity remain unverified. Host and guest memory figures
are not independent physical pools that can be added together.

**Outcome.** Setup, roadmap environment guidance and tracker now reflect the
approved Podman workflow. Existing relevant image names were listed, but no
container toolchain compatibility was certified. No container or guest was
launched for project execution, and no backend/resource setting was changed.
No download, installation, build, flash, source edit, commit or push occurred.
Yocto readiness remains blocked on supported-container/storage verification;
about 100 GB free host disk is limited headroom, not a guarantee of success.

## 2026-10-23: Planned 200 GB Storage And Mac Build-Host Comparison

**Learner report.** Free disk space can be increased to approximately 200 GB
by removing unwanted files personally. This is not completed cleanup or a
tool-verified capacity. The last observed C: value remains 102.13 GiB.
No files were deleted, moved or selected for cleanup.

**Assessment.** With that space available to the actual Linux build storage,
the existing 32 GB-class Windows laptop and observed 15.47 GiB Podman guest
memory are a reasonable basis for a minimal Yocto workflow. Validate the
container distribution/dependencies, memory limits and Linux-filesystem volume;
start with one image and monitor growth. Multiple targets/SDKs/caches can
require more space. This does not close the Yocto build-readiness gate.

**Official documentation checked.** Yocto 5.0.14 system requirements specify
90 GB disk and 8 GB RAM for the documented example and list supported Linux
distributions, not macOS. The quick-build guide describes a container option
for non-Linux hosts. Unix ancestry alone does not make macOS a native supported
builder. A MacBook Air M3 alternative requires a suitable Linux VM or remote
Linux builder, with AArch64 host/layer/tool compatibility checked; its RAM/disk
were not inspected. No recommendation to migrate away from the approved
Podman workflow.

**Changes.** Updated setup, roadmap environment guidance and tracker, preserving
measured versus planned values. No cleanup, installation, build, VM/backend
change, source edit or revision to the curriculum schedule was performed.

## 2026-10-23: Approved Curriculum Review Fixes

**Permission and scope.** Learner requested "Fix the review comments" after
reviewing the non-environment refinements. Applied document/teaching changes
only; no learner implementation edits or runs. Existing Podman/environment
facts and planned-versus-measured storage remain unchanged.

**Roadmap and tracking.** Moved driver model/lifecycles and IRQ/locking/teardown
to Weeks 3-4, followed by Yocto image/packaging in Weeks 5-6. Retained early
resource preflight. Added bounded architecture/task-switch/PendSV traces inside
the existing concept hours, C01-C21 exercises inside existing C hours, portfolio
review targets at Weeks 9/12/18 and an execution-only tracker. New-track week
numbers do not erase historical completed-reported Weeks 1-2. Dates, 22-hour
weekly budget and 462-hour total are unchanged; no results marked complete.

**Project contracts.** Added incremental design decisions and fault/timing
records for A/B, qualified SPI-core/controller mapping and ownership traces,
split/full-duplex protocol comparison, observational PIO/DMA tests and optional
genuine upstream work. Source thresholds are per-transfer and conditional;
protocol correctness comes before DMA eligibility. Managed allocation does not
quiesce asynchronous activity; alignment alone is insufficient. The precise
introduction release of the DMA-group helpers was not established (earlier
tag checks were rate-limited); no "introduced in 6.12" claim was adopted.

**Teaching.** Clarified tiny encode/decode examples, concrete execution-table
columns, call chains and relevant actor timelines. C practice distinguishes
ISO C, POSIX, kernel/GNU and MCU contexts, teaches lifetime early, and does not
silently extend SPSC proofs to drop-oldest queues. Linux guidance follows the
version-aware project rules. Short answers remain short; quizzes stay paused.

**Preservation.** Historical notes, v1/v2 source and the 37-file foundation
snapshot were not edited. No new project ADR/evidence files were fabricated;
the plan specifies them for actual implementation work. No installation,
build, hardware test, commit, push or public submission performed.

**Validation, tool-run.** Checked all 21 weekly dates and the reordered Weeks
3-6, the unchanged 22-hour budget/462-hour total, all 21 C IDs against the
execution register, and all three checkpoint dates. Changed-document local
links/anchors, skill metadata/size and scoped whitespace checks passed.
All 37 foundation hashes remained unchanged; kernel status remained v2 with
only the previously untracked editor/configuration paths. No learner runtime,
hardware result or fresh-chat skill activation was inferred from these checks.

## 2026-10-23: Week 1 Started: Process, Kernel And Device Boundaries

**Learner request.** Start tutoring, explain the Week 1 plan and mark the
current topic in progress. Actual start is 2026-10-23; the roadmap's planned
2026-10-26 start and 22-hour allocation remain unchanged. Early teaching counts
toward Week 1. No elapsed study time supplied.

**Current topic.** Linux process, kernel and device boundaries, using Project A
as the concrete example. Teach mode; no quiz or compulsory submission.
Distinguish a running recorder process, privileged kernel services, device
driver responsibilities, MCU firmware and hardware. Explain a buffered read:
data acquisition and delivery to the process are separate operations; an
application read does not necessarily initiate a fresh SPI transfer.

**Source evidence, tutor-inspected.** Linux checkout `f0100363d`; selected IIO
files had no worktree modifications. `drivers/iio/industrialio-core.c` registers
the buffered file operations; `drivers/iio/industrialio-buffer.c` implements
`iio_buffer_read`, including device/buffer validation, readiness/waiting and
the buffer backend's read callback. This supports the conceptual explanation,
not a complete syscall-to-hardware trace or an executed Project A experiment.
Illustrative sample sizes/values in teaching are not hardware measurements.

**Status.** Week 1 and the current topic set to In progress in the tracker.
No learner answer, independent understanding, C01 completion, guest boot or
hardware result inferred. C01 and the bounded architecture comparison remain
planned; existing environment inventory is retained without rerunning it.
No implementation file, historical note or foundation backup changed; no build,
installation, flash, commit or push performed.

**Next teaching step.** Explain how an application requests kernel services and
how a running task can wait for buffered data; then trace the relevant source
in a small chunk. Continue readiness work only with the appropriate execution
permission. Record C01 and other evidence separately when actually attempted.

## 2026-10-23: Markdown-First Lesson Library

**Learner preference.** Create a lesson folder and Markdown teaching material
to read independently, with questions afterward in chat. Updated the shared
coach and workspace instructions so all domain skills inherit Markdown-first
delivery; narrow questions remain direct chat answers.

**Published.** [Lesson index](../notes/lessons/README.md) and
[W01-L01: process/kernel/device boundaries](../notes/lessons/W01_L01_process_kernel_device_boundaries.md).
The note captures the current lesson with prerequisites, problem/solution,
roles, a concrete buffered-read example, source anchors, control-flow table,
boundary timeline, limits and recap/interview explanations. Follow-up topics
are listed as planned, not pre-generated or marked complete.

**Evidence.** Tutor inspected Linux `f0100363d`: buffered file operations in
`industrialio-core.c`, `CONFIG_IIO_BUFFER` mapping in `iio_core.h`, and the read
wrapper/readiness/handler in `industrialio-buffer.c`. Readiness may request a
hardware FIFO flush; the note does not claim that reads never cause hardware
activity. The 4-byte scans and 8-byte successful read are explicit illustrative
assumptions, not a measurement or a guaranteed complete result for every read.

**Progress.** Week 1/current topic stay In progress; actual hours and independent
competence remain unknown. No builds, hardware tests or learner implementation
edits. Historical lesson files, C exercises and foundation snapshots unchanged.
The new folder sits under `notes` rather than colliding with the existing
`Lessons` C-exercise folder on Windows. No commit or push.

**Validation, tool-run.** Lesson/index/routing local links and Markdown anchors,
referenced source-line bounds, shared-coach metadata, Week 1 In-progress status,
code-fence balance and scoped whitespace checks passed. All 37 preserved
foundation hashes still match. This validates publication/navigation, not
fresh-chat skill activation, executable behavior or learner understanding.

## 2026-10-23: W01-L01 Revised For Feynman Clarity (Watermark, Assumptions And Code Trace)

**Learner feedback.** Requested a simpler, basic-to-deep Feynman rewrite of
[W01-L01](../notes/lessons/W01_L01_process_kernel_device_boundaries.md) because
the watermark explanation, abstract assumptions list and earlier prose were
unclear.

**Repairs in place.**
- Grounded every term in `Know This First` directly in Project A (`ch0` + `ch1` =
  `4 bytes` per scan / `datum`).
- Added a visual bucket-fill-line explanation and a 3-row table for `watermark`
  (`to_wait = min_t(size_t, n / datum_size, rb->watermark)`).
- Replaced the abstract bullet list of assumptions with a variable-by-variable
  starting-snapshot table mapped directly to `iio_buffer_read()` (`n = 8`,
  `datum_size = 4`, `watermark = 2`, `avail = 3`).
- Embedded the actual inspected C functions from `drivers/iio/iio_core.h`,
  `drivers/iio/industrialio-core.c` and `drivers/iio/industrialio-buffer.c`
  (Linux `f0100363d`) and walked through both the ready (`avail = 3`) and
  sleeping (`avail = 1`) paths step by step.
- Follow-up refinement (revision 3): removed unnecessary boilerplate code not
  being implemented now, kept only the core `iio_buffer_fileops` and
  `iio_buffer_read()` lines, and moved the step-by-step what/how trace directly
  into inline C comments. Updated `learning-feynman-coach` so future lessons
  avoid overwhelming code dumps and explain code inline in comments.
- Also revised [W01-L02](../notes/lessons/W01_L02_system_calls_and_waiting_for_data.md)
  into the same concise Feynman format: visual two-queue diagram (`kfifo` data
  queue versus `rb->pollq` wait queue), starting-snapshot table (`fd = 3`,
  `n = 8`, `avail = 1 -> 2`, `to_wait = 2`), and three short inline-commented
  excerpts (`ksys_read`/`vfs_read`, `iio_buffer_read`/`kfifo_to_user`, and
  `iio_push_to_buffer`).
- Updated the interview Q&A sections in both `W01-L01` (5 questions) and
  `W01-L02` (4 questions) into simple, spoken-English bullets (`Simple answer`,
  `How it works / Why`, `Trade-off / Key takeaway`).
- Week 1 status remains In progress; quizzes remain paused.

## 2026-10-23: Preserve Learner Wording; L03 And Implementation Entry

**Learner direction.** Restored the simpler L01 explanation, especially its
watermark analogy and Feynman recap, and edited L02. Preserve conceptually
sound wording; report incorrect explanations separately and wait for approval
before updating existing lessons. Continue in the same simple style and explain
when implementation starts. Updated the shared coach/workspace instruction
precedence and reading index accordingly; L01/L02 are not edited in this turn.

**Next lesson.** Published
[W01-L03](../notes/lessons/W01_L03_privilege_and_address_spaces.md), a bounded
RISC-V/Arm64 privilege/address-space comparison using the same read request,
simple role tables, annotated result-slot lines, toy VA/PA mapping and short
recap. BBB's Armv7-A User/Supervisor naming is distinguished from Arm64
EL0/EL1 and MCU Thread/Handler terminology. No complete architecture ladder.

**Evidence.** Tutor inspected Linux `f0100363d`: RISC-V trap/syscall helpers,
Arm64 syscall/result helpers and boot/memory documentation; BBB/AM33xx device
tree and 32-bit Arm mode definitions. Selected paths showed no local source
changes. Actual target register state, memory mappings and build behavior were
not measured. No source or hardware execution occurred.

**Existing-note review, pending approval only.** Core buffered-read and
wait/recheck examples are useful. Wording to flag: L01's watermark is described
as a producer-wakeup threshold despite the successful-push notification path;
kernel buffering is described as guaranteeing no loss; L01/L02 say other-task
switches happen only on sleeping (preemption is another possibility). L02's
inline VFS comment places descriptor lookup inside `vfs_read`, although
`ksys_read` passes the resolved file to it. Preserve the notes while presenting
these concrete corrections to the learner, not a broad stylistic rewrite.

**Implementation timing.** L04/C01 is the next small C implementation within
Week 1: checked header-plus-payload sizing and boundary tests. It does not wait
for Yocto or a completed kernel. The learner authors code after explanation;
implementation paths and execution permission are agreed separately. Week 2
adds resource-unwind/module workflow; Weeks 3-4 hold the first driver slice.

**Status.** Week 1/L03 In progress; C01 still planned. No lesson mastery,
elapsed hours or runtime results invented. No installation, build, flash,
commit or push; foundation snapshots and historical kernel notes untouched.

**Validation, tool-run.** L03/index/routing/tracker local links and anchors,
source-line bounds, code fences, shared-coach metadata, progress status and
scoped whitespace passed. L01 and L02 SHA-256 hashes match the start of this
turn exactly. All 37 foundation snapshot hashes remain unchanged. These are
document checks, not execution evidence or proof of understanding.

## 2026-10-23: W01-L03 Updated And W01-L04 (C01) Published

**Learner request.** Update Lesson 3 (`W01-L03`) into the same concise Feynman
style and create Lesson 4 (`W01-L04` / `C01`).

**Actions.**
- Updated [W01-L03](../notes/lessons/W01_L03_privilege_and_address_spaces.md)
  with the 4-platform privilege comparison table (RISC-V `U/S/M`, Arm64
  `EL0/EL1/EL2/EL3`, BBB 32-bit Armv7-A `User/SVC`, STM32 Cortex-M4
  `Thread/Handler`), inline-commented `syscall_handler` and
  `syscall_set_return_value` excerpts, visual `0x00401003 -> 0x81234003` page
  translation diagram, and spoken-English interview Q&As.
- Created [W01-L04 (C01)](../notes/lessons/W01_L04_c01_checked_packet_size.md)
  covering checked header-plus-payload frame sizing (`c01_frame_size_bytes`):
  8-bit wraparound hand trace (`8 + 63 * 4 = 260 -> 4`), subtraction and
  division guards before addition/multiplication, 7-case boundary table,
  comparison with Linux `<linux/overflow.h>` (`check_mul_overflow` /
  `check_add_overflow`), and spoken-English interview Q&As.
- **Scratch verification (tutor-run):** Verified the `C01` reference code and 7
  boundary assertions inside an ephemeral Podman container
  (`localhost/xv6-lab:latest`, `gcc 13.3.0 -std=c11 -O2 -Wall -Wextra -Werror -pedantic`,
  `C01 scratch verification: 7/7 checks PASS`, exit status `0`). No learner
  source file was created or modified in `Lessons/`.
- Updated [lesson index](../notes/lessons/README.md) and
  [tracker](training_tracker.md). Week 1 and `C01` remain `In progress` until
  the learner writes/runs their own implementation.
- **Technical review fixes applied across W01-L01..L04 (2026-10-23):**
  1. `W01-L04`: Clarified C integer promotion (`uint8_t` promotes to `int` in expressions unless narrowed back to `uint8_t total`, whereas `size_t` is unpromoted and wraps modulo `SIZE_MAX + 1`), refined the `out_total_bytes` pointer contract (`NULL` vs. valid writable `size_t *`), and added addition-only overflow and `bytes_per_sample == 0` checks (`9/9` scratch checks `PASS` in `localhost/xv6-lab:latest`).
  2. `W01-L01`: Clarified that scan grouping does not prove electrical simultaneity, buffering prevents loss only within capacity, preemption can also switch threads, BBB uses Armv7-A `SVC` mode, and watermark governs `read()` completion batching while the producer still notifies `rb->pollq` per push.
  3. `W01-L02`: Moved `fd = 3` lookup comment to `ksys_read()`, refined busy-wait power/starvation and preemption wording, and clarified `-ERESTARTSYS` automatic restart (`SA_RESTART`) vs. `EINTR` / `SIGINT` handler behavior.
  4. `W01-L03`: Clarified that `pt_regs` stores general-purpose registers and exception state (not FP/vector registers) in both the glossary and Section 4 intro, distinguished address translation from memory protection (MMU vs. MPU), and traced `regs->orig_a0 = 3` and `regs->a0 = -ENOSYS` before `regs->a0 = fn(regs)`.
  5. Second-pass polish on `W01-L01`, `W01-L03`, and `W01-L04`: updated the L01 watermark table and Feynman recap to use simple "read finishes / waiting room does not overflow" wording (and noted `requested_scans >= 64` in the 64-scan Q5 example), aligned L03 Section 4's `pt_regs` sentence with the glossary, and stated the exact C integer promotion rule (`int` if representable, else `unsigned int`) and target ABI qualification for `size_t` in L04.

## 2026-10-23: Embedded Scope And Planned HP Ubuntu Builder

**Learner clarification.** Focus remains embedded software: firmware, RTOS,
embedded Linux and drivers, not general systems/server application development.
Recorded that OS and systems-programming mechanisms are necessary supporting
skills for these products, with examples tied to device access, data ownership,
timing and recovery. No planned embedded prerequisites removed.

**New host plan, not execution.** Learner plans Ubuntu Server 26.04.1 on an
HP x360, writing original code on the current laptop, pushing to GitHub and
pulling on the HP for builds. HP CPU architecture, RAM, SSD space, installed
OS and connectivity remain unverified. Earlier Windows/Podman measurements
must not be applied to this machine.

**Official sources checked.** Ubuntu's 26.04 release directory lists a 26.04.1
server image. Yocto 6.0 requirements list Ubuntu 26.04 and give 140 GB disk /
32 GB RAM for their documented Sato/qemux86-64 example. Previously researched
Yocto 5.0.14 lists Ubuntu 24.04, not 26.04, and quotes different example
resources (90 GB / 8 GB). Neither figure is a measured requirement of our
future minimal image. Select the release/BSP/layers with the host, not simply
the newest Ubuntu. A compatible Podman user-space container on native Linux
is an alternative for retaining an older release environment.

**Recommendation.** Use HP as primary builder after readiness checks, keep
existing Windows/Podman as fallback. The proposed GitHub workflow is valid;
VS Code Remote-SSH offers a faster local-UI/remote-files/build loop without a
push/pull for every edit. GitHub remains version control and explicit approved
publication/checkpointing, not an automatic transport of private or employer
material. The learner's workflow choice is not silently replaced.

**Updates and limits.** Setup/roadmap/tracker/README and active teaching scope
instructions now reflect this direction. Existing lessons and learner
implementation files are unchanged. No new installation, SSH login, repository
creation, clone, commit, push, remote change or build performed; no curriculum
hours, learner progress or previously dated evidence overwritten.

**Validation, tool-run.** Changed-document local links/anchors, code fences,
active skill metadata and scoped whitespace checks passed. The 21 weekly dates,
22-hour budget and all 37 foundation hashes are preserved. HP installation,
host compatibility in practice, SSH access and target builds remain unverified.

## 2026-10-23: Package Plan Reviewed; SSH-First Guide

**Request.** Review the supplied apt/Yocto/Zephyr installation steps and provide
SSH setup first for Windows-to-HP access. The supplied plan describes 16 GB RAM
and a 220 GB SSD; these remain reported/proposed, not measured HP resources.

**Review.** Keep staged dependencies, headless tools, locale checks and a Python
virtual environment. Avoid an unattended broad upgrade and installing all cross
toolchains now. Yocto 5.0.14 packages match its guide but do not establish native
Ubuntu 26.04 support. A compatible container does not automatically bypass
AppArmor/user-namespace policy; buildtools is not a universal fix. Version
pipelines through `head` can hide a missing compiler; use direct checks.
Pin Zephyr/Python/SDK rather than claiming the combined apt list is complete.

**Published.** [HP SSH-first guide](../docs/hp_linux_ssh_setup.md) separates HP
console and Windows PowerShell steps, verifies the server fingerprint, installs
only the public client key without overwriting existing authorized keys, tests
a new key-only session and retains console recovery. Optional Remote-SSH and
incremental package checks follow working terminal access. No port forwarding,
blanket firewall/AppArmor disablement or forced authentication changes.

**Limits.** Commands are instructions, not executed results. No installs,
upgrades, firewall changes, SSH logins, key generation, credentials, downloads
to the HP, builds or publication performed. Existing lessons and implementation
files unchanged; hardware/SSH readiness remains planned.

**Static validation.** Guide/setup/tracker local links and anchors, fence
balance, embedded PowerShell syntax and scoped whitespace checks passed.
All 37 foundation snapshot hashes match. Linux commands, SSH authentication,
firewall behavior and package installation were not executed or verified on HP.

## 2026-10-25: Photo Inventory And Board-Backed Project Fallbacks

**Request.** Inspect the available-board photos, create Markdown descriptions,
research useful embedded career projects and plan for board failures.

**Photo evidence.** Inspected all 11 images without modifying or uploading them.
Identified NUCLEO-F446RE / MB1136 rev C, NUCLEO-F767ZI / MB1137 Rev B, BBB family,
original Raspberry Pi Pico (not Zero), ESP32-family HW-463, ESP8266 NodeMCU-style,
UNO-style board views, a 1.8-inch SPI 128x160 V1.1 TFT/SD module and a probable
parallel character LCD. Exact UNO quantity, display controllers, ESP variants,
BBB revision and all working states remain unverified. Saleae was not pictured.

**Research.** Read official Zephyr/Nucleo/Pico documentation and local board
files at `55f132b58`, BeagleBoard overview, Arduino family documentation,
Espressif SPI-target/ESP8266 guides, Bootlin labs, IIO/I2C-model and Zephyr
mutex/watchdog/CAN documentation. Local F767 docs/DTS expose a PA7 default
SPI/Ethernet conflict and a silicon-cut-specific Ethernet warning; the
photographed PCB revision does not identify the silicon cut.

**Published.** [Hardware inventory](../docs/board_inventory.md), including all
photo links, and [project/fallback plan](../docs/board_project_plan.md).
Recommend A on BBB/F446RE and independent B on F767ZI; Pico/ESP32 are conditional
fixtures/spares. Displays, Ethernet and physical CAN remain optional/gated, not
new flagship scope. Every fallback names what software work can continue and
which physical evidence remains missing. No photographed MCU replaces BBB as
a Linux SBC.

**Current guidance.** Updated setup, project contracts, roadmap hardware
paragraph, tracker, root navigation and hardware-routing instructions. No dates,
weekly hours, lesson text or learner completion changed. Photos support model
identification, not boot/ADC/DMA/CAN/measurement acceptance. No firmware, source
clone, environment, board or backup changed; no purchases, wiring, flashes,
builds, commits or pushes performed.

**Document validation.** All 11 image files are linked once in the photo index.
Local documentation/source links, Markdown anchors, fences, active Arm-skill
metadata and scoped whitespace checks passed. All 37 foundation hashes and the
21-week/22-hour budget remain unchanged. Kernel worktree status remained v2
with only its previously untracked editor/configuration paths. No functional
hardware result is inferred from these document checks.

## 2026-10-25: Simplified HP SSH And Staged Package Guide With Command Syntax

**Learner request.** Simplify [docs/hp_linux_ssh_setup.md](../docs/hp_linux_ssh_setup.md),
explain the syntax and flags of each command in plain English, add inline
comments inside the command blocks, and replace unnecessarily complex command
wrappers with simpler equivalents.

**Updates.**
- Replaced redundant/complex commands (`findmnt -T "$HOME"`, `'sport = :22'`,
  and multi-line PowerShell `Join-Path`/`Split-Path`/`Test-Path` wrapper logic)
  with simpler standard commands (`df -Th / ~`, `uname -rm`, `sudo ss -tlnp | grep :22`,
  direct `ssh-keygen -t ed25519 -f ...`, and a 5-line `~/.ssh/config` `Host hp`
  alias so `ssh hp` works everywhere).
- Added inline `#` comments above every command and a **Command & Syntax
  Breakdown** table under each step explaining every flag (`-a`, `-rm`, `-h`,
  `-Th`, `-br`, `-4`, `enable --now`, `--no-pager`, `-tlnp`, `-lf`, `-t ed25519`,
  `chmod 700/600`, `tr -d '\r'`, `-i`, `-o PasswordAuthentication=no`).
- Preserved all safety and staging rules (staged tool installation by week,
  direct `--version` checks without `head`, Yocto 5.0.14 vs. 6.0 host distro
  checks, and non-root/non-global `venv` rules). No commands were executed on
  the HP; readiness remains planned until the learner runs them.

## 2026-10-25: Review Of Learner `C01_packet_size.c` And Publication Of `W01-L05`

**Learner submission.** Learner completed reading through `W01-L04` and wrote
[notes/lessons/C01_packet_size.c](../notes/lessons/C01_packet_size.c).

**Review of `notes/lessons/C01_packet_size.c` (Evaluation: Partially correct).**
- **What was right:** Properly checked `out_total_bytes == NULL` before
  dereferencing, checked `header_bytes > max_frame_bytes` before subtracting
  `max_frame_bytes - header_bytes`, and used division
  (`max_paylod_bytes / bytes_per_sample`) to guard the multiplication before
  computing `header_bytes + (sample_count * bytes_per_sample)`.
- **Specific gaps to repair before marking C01 Verified:**
  1. **Missing `return true;` at the end of `c01_frame_size_bytes()`:** Control
     reaches the closing `}` on the success path without returning a value
     (undefined behavior in C; rejected by `-Wall -Werror=return-type`).
  2. **`*out_total_bytes = 0;` placed after early failure returns:** When
     `bytes_per_sample == 0` or `header_bytes > max_frame_bytes`, the function
     returns `false` before zeroing `*out_total_bytes`, leaving the caller's
     previous value intact on failure. Move `*out_total_bytes = 0;` immediately
     after the `NULL` check.
  3. **`if (bytes_per_sample == 0) return false;` vs. Case 8 contract:** Case 8
     in `main()` tests `!c01_frame_size_bytes(8, 100, 0, 260, &total) || total != 8`
     (expecting `true` and `8`). Guarding the division with
     `if (bytes_per_sample != 0 && sample_count > max_payload_samples)` handles
     `0`-byte samples without dividing by zero and satisfies Case 8.
  4. **Redundant check (minor):** Since `max_paylod_bytes <= max_frame_bytes`,
     `max_payload_samples` is always `<= max_total_samples`; only
     `sample_count > max_payload_samples` is needed.
- Per ownership rules, `notes/lessons/C01_packet_size.c` was not modified by the
  tutor.

**Next lesson published.** Created
[W01-L05: From `start_kernel()` To Driver Init And QEMU Guest Preflight](../notes/lessons/W01_L05_linux_boot_to_driver_init_and_qemu_preflight.md),
completing Week 1's source-grounded boot/ownership trace (`init/main.c` at
`f0100363d`: `start_kernel -> rest_init -> kernel_init -> driver_init -> do_initcalls -> free_initmem -> run_init_process`)
and introducing the 4 ingredients required for Week 2's disposable QEMU Linux
guest.

## 2026-10-25: Applied Lesson Review Findings (`LR1`–`LR6`) And Reviewed Repaired `C01_packet_size.c`

**Review analysis.** Evaluated all 6 findings (`LR1`–`LR6`) from the
`learning-lesson-review` audit against `linux` (`f0100363d`) and accepted all 6:
1. **`LR1` (`W01-L05` init binary selection):** Updated Step C, diagrams, step
   table, and recap in [W01-L05](../notes/lessons/W01_L05_linux_boot_to_driver_init_and_qemu_preflight.md)
   to show `kernel_init()`'s actual order (`ramdisk_execute_command` / `rdinit=`,
   `execute_command` / `init=`, `CONFIG_DEFAULT_INIT`, then `/sbin/init`,
   `/etc/init`, `/bin/init`, `/bin/sh` fallbacks).
2. **`LR2` (`W01-L05` `__init` lifetime and post-free behavior):** Distinguished
   built-in `free_initmem()` reclamation from loadable-module init-section
   reclamation in `do_init_module()`, and noted that calling freed init code can
   fault or execute overwritten/reused memory.
3. **`LR3` (`W01-L05` `kthreadd` scope):** Scoped `kthreadd` (`PID 2`) to
   servicing normal `kthread_create()` requests after `PID 0` bootstrap.
4. **`LR4` (`W01-L05` Section 6 QEMU recipe):** Framed the 4-flag table as our
   chosen Week 2 direct-kernel boot recipe (`-M virt -nographic`, `-kernel`,
   `-initrd`, `-append`), noting built-in initramfs/cmdline alternatives.
5. **`LR5` (`W01-L02` Step B item 5 `kfifo_to_user` comment):** Separated
   `kfifo_to_user()`'s status return (`0` on success) from `copied = 8` and
   `iio_read_kfifo()`'s return value (`8`).
6. **`LR6` (`W01-L04` learner-status header):** Updated the header in
   [W01-L04](../notes/lessons/W01_L04_c01_checked_packet_size.md),
   [notes/lessons/README.md](../notes/lessons/README.md), and
   [training_tracker.md](training_tracker.md) to reflect that the learner has
   also applied all 3 repairs in [C01_packet_size.c](../notes/lessons/C01_packet_size.c)
   (source-reviewed **Correct**; runtime execution pending).
- **2026-10-26 (`W01-L05` expanded to complete boot movie with Before/After & `task_struct`):**
Expanded [W01-L05](../notes/lessons/W01_L05_linux_boot_to_driver_init_and_qemu_preflight.md)
per learner approval to cover the full 6-scene story (Mask Boot ROM ->
SPL/OpenSBI -> U-Boot -> `head.S` -> `start_kernel` -> `rest_init` ->
`driver_init` / `do_initcalls` -> `kernel_execve` -> `fork + execve` user
app), added the `struct task_struct` diagram (`init_task` in
`init/init_task.c`), `fork`/`clone` vs. `exec` vs. `spawn` visuals, and
explicit **Before -> After** ASCII boxes for `head.S`, `setup_arch()` +
`mm_core_init()`, `rest_init()`, `driver_init()`, `do_initcalls()`,
`free_initmem()`, and `kernel_execve()`.

## 2026-10-26: Approved MCU And FreeRTOS-First Career Pivot

**New experience evidence, learner-reported.** Professional AURIX motor/BMS IC
driver work includes architecture/interface SWA reviews, cyclic driver-function
implementation, AUTOSAR-module integration with CDD, integration tests and team
mentoring. Safety-related work includes FMEA/HAZOP meetings and assessments with
safety engineers, Polyspace/MISRA analysis and requirement traceability. Earlier
"validation-only" positioning was incomplete. Architecture authorship or
independent safety-case ownership is not inferred from review/participation.

**Approval.** Learner approved MCU/firmware/RTOS focus, FreeRTOS first followed
by bounded Zephyr; remove Linux kernel and Yocto from active preparation. Any
interesting role with good pay is acceptable, including MCU applications.
Keep INR90LPA aspirational,22h/week and preparation/interviews by22March2027.

**Research.** Current Bengaluru official examples include TI25009764 RTOSLead
(FreeRTOS/drivers/leadership), Qualcomm3092665 management-controller firmware
(RTOS/Zephyr/FreeRTOS/diagnostics), InfineonHRC1641593 applications
(AURIX/peripherals/customerdebug), and Qualcomm3094816 (Zephyr but also display
specialization). NVIDIAJR2023363 requires direct safety-lifecycle plus
QNX/Linux/hypervisor experience and is not used as an MCU-only readiness gate.
Employee-reporting sources were separated from official job requirements;
broad India software medians do not predict an individual firmware offer.
Glassdoor retrieval returned403; no salary value invented.

**Private production-style project interpretation.** Learner wants technical
depth, interview readiness and an idea potentially useful at work. P1 is a
cyclic peripheral-service/diagnostic framework and recorder; P2 is a standalone
Zephyr supervisor/programmed test node. Public/synthetic protocols and safe
low-voltage bench behavior only. Initial work is personal learning; an employer
project/architecture claim requires actual authorization/adoption/ownership.
No confidential implementation, high-energy motor/BMS control or certification
is implied. No GitHub showcase/publication required.

**Preservation.** Before active edits, copied43workingfiles including current
plans,skills,docs and notes into the dated2026-10-26snapshot, verifiedSHA256.
Original37file2026-10-22snapshot unchanged. Archived Linux/xv6 skills removed
from active discovery; source sibling repositories and all lesson bodies remain.

**Applied.** Replaced the active roadmap/projects/setup/tracker/boardallocation/
README; updated shared,C,MCU andreviewer routing plus lesson index. Preserved
the21week dates and22hweekly budget with21boundedCtopics. Current entry is C01
runtime/readiness and MCU fundamentals, not another Linux boot lesson.
No code implementation, source move, download/install, build/run, flash,
commit/push or system configuration change performed.

**Validation.** Active links/anchors, Markdown fences, four active skill
metadata entries and scoped whitespace checks passed. All21weeklydates,
22h/week/462h total and C01-C21 tracker mappings passed. All37original and
43pre-pivot snapshot hashes remained unchanged. Existing lesson bodies and
learner C01 source match the pre-pivot snapshot exactly. Kernel worktree status
remained v2 with only the previous untracked editor/configuration files.
Fresh-chat skill discovery and any MCU/runtime behavior were not tested.

## 2026-10-26: MCU Topic 1 Tutoring Started

**Request and publication.** Learner asked to start the updated plan with a
short introduction and the first topic's Markdown. Created
[W01-L06: MCU Driver Contracts And Buffer Ownership](../notes/lessons/W01_L06_mcu_driver_contracts_and_buffer_ownership.md).
L06 is the next unused Week 1 ID, not a restart or overwrite of earlier lessons.

**Teaching scope.** Week 1 driver requirements/ownership, using a synthetic
two-byte reply: acceptance versus completion, bounded cyclic work, buffer
handoff, validation, timeout versus confirmed stopping and planned tests.
Simple-English explanations, Before/After diagram, starting values, success/
failure traces and Feynman recap. FreeRTOS APIs and actual hardware are deferred
until source/configuration/readiness gates; no proprietary protocol is used.

**Evidence and progress.** Tutor authored a conceptual note and updated the
index/tracker. Topic tutoring In progress; no submitted explanation, measured
timing, executed model or learning-completion evidence. Actual study hours
remain unknown and quizzes paused. Existing lesson bodies, C01 learner source
and project implementations unchanged. No build, install, flash or board action.

**Next action.** Read the new note and ask follow-up questions if needed.
C01 runtime evidence and board/toolchain readiness remain the Week 1 practical
thread, with execution requiring the relevant permission.

## 2026-10-26: Hardware-First Choice And MCU Topic 2

**Learner direction.** Proceed to the next lesson using available hardware;
no QEMU required. Updated the active roadmap, project contracts, setup,
fallbacks, repository guidance and teaching skills accordingly. Optional host-C
logic tests remain useful; no emulator installation or fallback is assigned.
Old lesson bodies and dated log entries remain intact; their emulator-first
directions are superseded in the active index, not silently rewritten.

**Publication.** Created
[W01-L07: From C Source To Running Firmware On The F446RE](../notes/lessons/W01_L07_f446re_build_flash_and_debug_path.md).
Explains host/probe/target, cross-compilation and linking, ELF versus raw binary
and map, startup initialization of example values 7/0, and separate build,
programming and runtime evidence. Clarifies that Remote-SSH does not
automatically forward USB and a container build does not prove probe access.

**Evidence.** Inspected the local F446RE board reference at Zephyr `55f132b58`
for Cortex-M4/ST-LINK facts and official Arm CMSIS/GNU documentation for
startup/build responsibilities. Direct ST board/tool pages failed retrieval;
no physical power/jumper sequence was invented. The future application's exact
startup, tool versions, clock and generated code remain unverified.

**Status and next action.** Topic 2 tutoring In progress; Topic 1 completion,
actual hours and mastery unassessed. Confirm intended host, board/probe
readiness and existing firmware preservation before commands. C01 runtime
remains pending. No implementation edits, installations, builds, hardware
connections, flashes, commits or pushes performed. Quizzes remain paused.

## 2026-10-26: Windows-Primary Workflow And MCU Topic 3

**Direction.** Learner asked to continue with the Windows-first recommendation.
Use the current HP Windows laptop for P1 and initially P2. The other HP now has
Ubuntu Server installed per learner report; exact release, SSH, tools and free
space remain unverified. It is optional, not a prerequisite or automatic move
at the Zephyr stage. No QEMU, separate WSL or container is required for board work.

**Tutor inspection.** Read standard Windows installation records and scoped tool
directories: CubeIDE 1.16.1, CubeProgrammer 2.18.0 and their executables present;
GNU Tools for STM32 12.3.rel1-labelled plugin with compiler executable present;
CubeMX 6.12.1-labelled integration; STM32CubeF4 V1.28.1 folder in the default
package repository. Searched executable names were absent from PATH, not absent
from disk. Package completeness and actual executable runtime were not tested.
No IDE, compiler, programmer or debug-server invocation was performed.

**Documentation evidence.** Direct ST web retrieval failed. Inspected bundled
UM2553 Rev 3 workspace/project sections and UM2609 Rev 12 workspace/device-
configuration sections. Python PDF-reader packages and PATH `pdftotext` were
unavailable; existing repository Node PDF tooling read the installed manuals
locally. No dependency was installed or document uploaded.

**Publication.** Created
[W01-L08: A Clean Windows CubeIDE Project For The F446RE](../notes/lessons/W01_L08_windows_cubeide_project_baseline.md).
Explains workspace versus source project, exact board selection, existing tool/
package candidates, generation versus compilation, and generated-code ownership.
New workspace/project locations are proposals only; no implementation files or
directories created. The note stops before build/debug/programmer connection.

**Persistent updates.** Aligned setup, roadmap, fallbacks, instructions/skills,
index and tracker with native Windows primary and optional Ubuntu. Updated the
SSH guide's scope/link only. Earlier lessons and dated history retained; current
index supersedes their old host/emulator next actions.

**Next action and limits.** Agree new project location, open the existing IDE
and inspect/generate the baseline in the learner-owned workflow. Build and board
operations remain separately gated. C01 runtime still pending; no mastery or
elapsed hours inferred from continuing. No install, project generation, source
implementation, build, flash, commit or push performed. Quizzes remain paused.

## 2026-10-27: Learner Generated F446RE Baseline; Topic 4 Published

**Learner report.** Generated the F446RE project with the agreed name/location.
Tutor read-only inspection confirmed
[p1_f446re_baseline](../../p1_f446re_baseline), project identity and generated
configuration/source. This establishes source availability and corroborates
generation, not successful compilation, hardware operation or overall mastery.

**Observed configuration.** NUCLEO-F446RE / STM32F446RETx; CubeMX 6.12.1;
selected STM32Cube FW_F4 V1.28.3, with matching package folder present.
Yesterday's V1.28.1 was an installed candidate; no downgrade/regeneration was
performed. Debug and Release both select the FLASH linker script, declaring
512K FLASH at 0x08000000 and 128K RAM at 0x20000000. Debug metadata selects
fpv4-sp-d16/hard floating-point ABI; actual executed flags remain pending.
Source requests a nominal 84 MHz clock through HSI/PLL, initializes GPIO and
USART2, then enters an empty loop; no blink/transmit behavior was added.

**Build status.** No object, ELF, map, binary or hex files were found in the
project during inspection. The tutor did not run a build or invoke any board
tool. Source/configuration is not executable/disassembly evidence. Physical
clock, wiring, target condition and programming readiness remain unverified.

**Lesson.** Created
[W01-L09: Your First Debug Build And Memory Map](../notes/lessons/W01_L09_first_debug_build_and_memory_map.md).
Explains the learner's deliberate Debug build, compiler/linker command evidence,
actual memory declarations, map interpretation, source-only normal/failure
paths and stage-specific error investigation. Stops before Run/Debug/flash.
Index, tracker and setup now distinguish generation from first-build readiness.
Existing lessons and all project files remain unchanged.

**Next action.** Learner builds the unchanged Debug configuration and retains
the build console/ELF/map outcome. Review actual errors or results before the
probe/programming gate. C01 runtime remains pending; quizzes paused; hours and
independent mastery unknown. No project edits, installations, builds, flashes,
commits or pushes performed by the tutor.

## 2026-10-27: Learner Build Succeeded; First Hardware Debug Lesson

**Learner report and approval.** Learner reports the build succeeded. In the
hardware-readiness form, confirmed the F446RE firmware can be replaced, nothing
needs preserving, board is bare with onboard ST-LINK/no external wiring or
power, and a matching USB data cable is available for the Windows host.
Approval is for learner-operated lesson steps, not tutor flashing/access.
Current power/debug jumper state and physical working condition are not
inferred from those confirmations.

**Artifact inspection, tutor-run read-only tools.** Existing
[Debug ELF/map/list](../../p1_f446re_baseline/Debug) are present. Used the installed
`arm-none-eabi-size`, `arm-none-eabi-readelf -h`, and `arm-none-eabi-objdump`
on the existing ELF, not a build or target execution:
- ELF32 little-endian Arm executable, hard-float ABI metadata.
- main at 0x080004f4; compiled HAL/clock/GPIO/UART calls then empty loop.
- Size summary text8408/data12/bss1644 bytes; bss includes the 0x604-byte
  heap/stack/alignment section, not just the 0x68-byte ordinary bss section.
- Map: SystemCoreClock at0x20000000, huart2 at0x20000028, uwTick at0x20000070.
  Initial data contains SystemCoreClock=16000000; target values not observed.
- ELF SHA256:
  `F0C40B172F20D8C530FA20475573BC18A242B1C1171C12FEB4A19A930B6E7935`.
Generated rules name GNU Tools for STM3212.3.rel1 and -O0/-g3. The original
compiler-console/warning history was not supplied, so none was fabricated.

**Teaching evidence.** Installed UM2609 Rev12 debug launch/startup sections
inspected with existing local PDF tooling; current local board reference and
earlier F446 photo consulted. Official UM1724 retrieval failed on primary and
regional hosts. An old Mbed board URL now redirects to its sunset notice;
search retrieval did not yield usable board instructions. No manual-specific
jumper positions, power modifications or guessed recovery commands prescribed.

**Publication.** Created
[W01-L10: First ST-LINK Debug Session - Stop At main](../notes/lessons/W01_L10_first_stlink_debug_stop_at_main.md).
Includes learner's preservation approval, bare-board prechecks, matching image/
symbols, local ST-LINK/SWD launch, stop-at-main versus skipping startup,
expected initial RAM values, failure-stage distinctions and safe stop behavior.
Records pending target observations separately from static artifact facts.

**Next and limits.** Learner performs prechecks and the first programming/debug
session, recording actual connection/programming messages, breakpoint and RAM
values. Do not infer standalone boot, clock accuracy or peripheral correctness
from one breakpoint. Topic5 In progress; quizzes paused; hours and mastery
unknown. No tutor rebuild, project edit, probe connection, flash, firmware
update, installation, commit or push.

## 2026-10-27: Debug Exercise Completed-Reported; Initialization Lesson

**Learner report.** "Done, ad debugged" in response to the first ST-LINK lesson.
Record L10 completed-reported and continue teaching. No specific breakpoint,
console output, values, clock accuracy or standalone boot result was supplied;
none is inferred from that short report.

**Current evidence.** Main/source files and existing ELF inspected read-only.
ELF SHA256 remains
`F0C40B172F20D8C530FA20475573BC18A242B1C1171C12FEB4A19A930B6E7935`.
The new saved launch selects the Debug ELF, stop-at-main and resume, with
connect_under_reset strategy. No unique probe identifier was reproduced.
Launch metadata supports configuration availability, not observed target state.

**Source trace.** Inspected HAL_Init/InitTick/IncTick, CMSIS SysTick_Config,
clock update/reconfiguration, generated low-level callbacks, GPIO/USART2 setup
and SysTick_Handler. Current HAL tick default increment is1ms at1kHz:
16000 counter clocks/LOAD15999 at nominal16MHz, then84000/83999 at84MHz.
Clock setup recalculates the timer but does not intentionally reset uwTick.
HAL_Init's unchecked tick-return and the later callback's priority-group0
override are explained without changing vendor/generated code. FreeRTOS
interrupt configuration remains a separate future gate.

**Publication.** Created
[W01-L11: Follow Initialization From main To The Empty Loop](../notes/lessons/W01_L11_trace_initialization_clock_gpio_uart.md).
Uses four after-call debugger checkpoints, expected versus observed values,
clock-frequency versus tick-period reasoning, GPIO/UART configuration versus
actual behavior, and asynchronous tick handling despite the empty main loop.
No new implementation, register pokes, external wiring or fault injection.

**Status and next action.** Topic6 In progress. Learner follows unchanged
initialization and records actual B/C/D/E state plus a brief tick-progress
observation. Source review does not certify target timing, pins or serial data.
C01 runtime remains pending; quizzes paused; hours and mastery unknown.
Existing lessons and project/source/build artifacts retained. No tutor build,
probe connection, flash, installation, code change, commit or push.

## 2026-10-27: Initialization Values Reported; First LD2 Lesson

**Learner report and review.** Learner reports completing the observations:
uwTick increments every1ms and the system clock value is84MHz after clock
initialization. These agree with the source's intended HAL time base and
software clock bookkeeping. No external instrument/method/raw trace was
supplied; do not record measured clock accuracy, exact wall-clock cadence or
unreported UART/GPIO values. Mark L11 completed-reported, not mastery verified.

**Validation choice.** Offered a compile check of the proposed four-call
LED loop in an isolated copy without touching the learner tree/board.
Learner selected source review only and will build it themselves.
No reference copy, compilation or code execution was performed.

**Evidence.** Current main loop remains empty and ELF fingerprint unchanged.
Inspected actual HAL_GPIO_WritePin (BSRR set/reset writes), GPIO/device headers,
HAL_Delay/GetTick/IncTick and existing GPIO setup. Installed ST Nucleo BSP and
local Zephyr board wiring corroborate LD2=PA5 active-high; the Zephyr clock/
RTOS configuration is not imported. Delay500 uses wait501 with current
uwTickFreq1; busy-wait semantics and dependency on tick progress are explicit.

**Publication.** Created
[W01-L12: Drive LD2 And Understand The Cost Of A Blocking Delay](../notes/lessons/W01_L12_ld2_gpio_output_and_blocking_delay.md).
One proposed learner edit: four calls in Core/Src/main.c's existing USER CODE3
region. Explains pin number versus mask, SET/RESET command semantics, exact
counter examples, same-pin ownership, visible-output evidence and why this
bring-up pattern is unsuitable for the bounded P1 cyclic service.
Updated navigation, setup and tracker without changing prior lesson bodies.

**Next and limits.** Learner types/builds the example and uses the established
bare-board procedure to observe LD2, then reports actual behavior/errors.
New code remains uncompiled/unexecuted by tutor, not certified ready-to-run.
No project/source/build artifacts changed; no tutor build, flash, board access,
install, commit or push. C01 runtime remains pending; quizzes paused; hours and
independent mastery unknown.

## 2026-10-27: Rewrote W01-L05 Through W01-L12 In Direct Story Mode And Audited Technical Accuracy

**Learner request.** Analyse and update `W01-L05` through `W01-L12` in
[notes/lessons](../notes/lessons/README.md) using a simple story-mode
explanation, removing defensive/negative filler (such as "not intended to do"
or "what it does not prove") so each section states directly what the hardware
and software do, then double-check technical accuracy.

**Updates applied (`W01-L05`..`W01-L12`).**
- [W01-L05](../notes/lessons/W01_L05_linux_boot_to_driver_init_and_qemu_preflight.md):
  Polished the 6-scene boot movie (`Mask ROM -> SPL/OpenSBI -> U-Boot -> head.S -> start_kernel -> rest_init -> driver_init/do_initcalls -> kernel_execve`), `task_struct` diagram, and Before/After boxes into direct positive prose.
- [W01-L06](../notes/lessons/W01_L06_mcu_driver_contracts_and_buffer_ownership.md):
  Rewrote as the 4-scene "Notebook Handoff Story" (`READY -> WAITING -> READY` on `[1, 42]`, and `WAITING -> STOPPING -> FAILED` on `3 ms` timeout / `4 ms` stop confirmation) with Before/After scene boxes and an annotated C state-machine model.
- [W01-L07](../notes/lessons/W01_L07_f446re_build_flash_and_debug_path.md):
  Rewrote as the 4-scene "Silicon Journey" from `main.c` on Windows through `arm-none-eabi-gcc`/linker, USB/ST-LINK/SWD flashing, Cortex-M4 hardware vector-table fetch (`0x08000000` `_estack`, `0x08000004` `Reset_Handler`), and the annotated `startup_stm32f446retx.s` `.data`/`.bss` initialization loops.
- [W01-L08](../notes/lessons/W01_L08_windows_cubeide_project_baseline.md):
  Rewrote as the workshop setup story for `p1_f446re_baseline` (`STM32CubeIDE 1.16.1`, `CubeMX 6.12.1`, `STM32Cube FW_F4 V1.28.3`), file roles, and `/* USER CODE BEGIN / END */` preservation zones.
- [W01-L09](../notes/lessons/W01_L09_first_debug_build_and_memory_map.md):
  Rewrote as the 3-scene Debug build story covering compiler/FPU flags (`-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard -O0 -g3`), exact `STM32F446RETX_FLASH.ld` memory regions (`FLASH 0x08000000..0x0807FFFF`, `RAM 0x20000000..0x2001FFFF`, `_estack = 0x20020000`), `text = 8408 / data = 12 / bss = 1644` breakdown, and the `16 MHz / 16 * 336 / 4 = 84 MHz` PLL diagram.
- [W01-L10](../notes/lessons/W01_L10_first_stlink_debug_stop_at_main.md):
  Rewrote as the 3-scene ST-LINK debug story halting at `main()` (`0x080004f4`) before `HAL_Init()`, explaining the exact SRAM addresses and starting values verified in `p1_f446re_baseline.map` (`SystemCoreClock = 16000000` at `0x20000000`, `huart2` at `0x20000028` / `huart2.Init.BaudRate = 0` at `0x2000002c`, and `uwTick = 0` at `0x20000070`).
- [W01-L11](../notes/lessons/W01_L11_trace_initialization_clock_gpio_uart.md):
  Rewrote as the 5-scene initialization walkthrough (`Checkpoints A..E`) with Before/After boxes for `HAL_Init()` (`SysTick->LOAD = 15,999` at `16 MHz`), `SystemClock_Config()` (`84 MHz` PLL, `SysTick->LOAD = 83,999`), `MX_GPIO_Init()` (`PA5` pre-cleared low then push-pull output; `PC13` falling-edge interrupt), `MX_USART2_UART_Init()` (`PA2/PA3` `AF7`, `115200` baud, `HAL_UART_STATE_READY`), and `SysTick_Handler()` in Handler mode.
- [W01-L12](../notes/lessons/W01_L12_ld2_gpio_output_and_blocking_delay.md):
  Rewrote as the 4-scene `LD2` (`PA5`) blink story showing pin mask `1U << 5 = 0x0020`, atomic set (`0x00000020`) and reset (`0x00200000`) writes to `GPIOA->BSRR`, `HAL_Delay(500U)`'s `wait = 501` tick loop, and why cyclic drivers replace blocking `HAL_Delay()` calls with non-blocking elapsed-time checks.

**Verification.** Audited all 8 lesson files against the `learning-lesson-review`
checklist (`C01`–`C12`), verified every relative link and `#L` line anchor
against `linux` and `p1_f446re_baseline`, cross-checked symbol addresses against
`Debug/p1_f446re_baseline.map`, and confirmed clean `git diff --check`.
- **Follow-up review fixes applied (`R1`–`R14`, 2026-10-27):**
  1. `R1` (`W01-L06`): Checked `timed_out = ((now_ms - ctx->start_ms) >= 3U)` before accepting a collected reply in `p1_driver_cyclic_1ms()` so a reply collected at or after `3 ms` reports `DRV_ERR_TIMEOUT` and transitions to `DRV_STATE_FAILED` rather than publishing `DRV_OK`.
  2. `R2` & `R3` (`W01-L07`): Distinguished the reset-vector slot at `0x08000004` from the `Reset_Handler` entry address (`0x08000888`), and separated raw `.bin` files from address/checksum-bearing Intel `.hex` records.
  3. `R4` & `R5` (`W01-L09`): Refined `-O0` description and updated `.text` start address in the Flash diagram from `0x08000188` to `0x080001c4` (matching `.isr_vector` size `0x1c4` in `p1_f446re_baseline.map`).
  4. `R6` (`W01-L10`): Distinguished `main` function symbol address `0x080004f4` (prologue) from GDB's `break main` source-line breakpoint address `0x080004f8` (`HAL_Init();`).
  5. `R7` (`W01-L11`): Distinguished `HAL_MspInit()` (`SYSCFG`/`PWR` clocks + NVIC grouping) from `HAL_UART_MspInit()` (`USART2` clock + `PA2`/`PA3` `AF7` pins) in the Section 0 glossary.
  6. `R8` & `R9` (`W01-L12`): Described `GPIOA->ODR` as a 32-bit register (`uint32_t ODR`) whose lower 16 bits hold the output-latch states, and specified that mask `5` selects `ODR` bits `0` and `2` (while `IDR` reports input states).
  7. `R10`–`R14` (`W01-L05`): Clarified `user_mode_thread()` for `PID 1` (no `PF_KTHREAD`), `kernel_clone(.fn)` vs. user-space `fork()`, RISC-V vs. 32-bit Arm `head.S` `.bss`/MMU ordering, initcall-level order + `-EPROBE_DEFER`, and `-initrd rootfs.cpio`.

## 2026-10-27: Updated Learning Skills With Story-Mode Authoring And 10-Point Pre-Publication Self-Audit

**Learner request.** Update the Markdown lesson-generation skills so future
lessons are written in a simple, easy-to-understand story mode with inline-
commented code and complex syntax explanations, and codify generic technical
guardrails so past review defects are not repeated.

**Updates applied.**
- [learning-feynman-coach/SKILL.md](../.github/skills/learning-feynman-coach/SKILL.md):
  Added **Structure, Story-Mode Style And Inline Code Explanations** (Scene-by-Scene
  story flow, direct positive prose without negative/defensive filler, side-by-side
  `Before -> After` ASCII boxes, inline `/* ... */` value traces, and **Complex
  Syntax** breakdowns) and **Section 4: Engineering Correctness And Pre-Publication
  Self-Audit** (10 generic pre-publication rules covering state-machine/deadline
  guard ordering, C integer promotions vs. fixed-width types, pointer preconditions
  and early `*out = 0` initialization, register widths/latch vs. pin muxing/`N - 1`
  counter math, vector slots vs. handler addresses & symbol vs. post-prologue
  breakpoints, `.elf`/`.bin`/`.hex`/`.map` formats, caller/callee attribution,
  notification vs. readiness & preemption, architecture-specific boot order, and
  whole-file consistency).
- [learning-arm-rtos/SKILL.md](../.github/skills/learning-arm-rtos/SKILL.md),
  [learning-c-cpp/SKILL.md](../.github/skills/learning-c-cpp/SKILL.md),
  [learning-lesson-review/SKILL.md](../.github/skills/learning-lesson-review/SKILL.md),
  and [copilot-instructions.md](../.github/copilot-instructions.md): Aligned
  domain checklists and workspace instructions with the updated story-mode
  authoring contract and 10-point pre-publication self-audit.

## 2026-10-27: Published W01-L13 (Non-Blocking Elapsed-Time Scheduling And `HAL_GPIO_TogglePin()`)

**Learner request.** Create the next lesson (`W01-L13`).

**Published.** Created
[W01-L13: Non-Blocking Elapsed-Time Scheduling And How `HAL_GPIO_TogglePin()` Works](../notes/lessons/W01_L13_nonblocking_elapsed_time_and_gpio_toggle.md)
following the Story-Mode authoring contract and 10-point Pre-Publication
Self-Audit:
- **3-Scene Story & `Before -> After` Comparison:** Shows how replacing
  `HAL_Delay(500U)` with `uint32_t now_ms = HAL_GetTick(); if ((now_ms - last_toggle_ms) >= 500U)`
  frees `while (1)` to run cyclic driver work every millisecond.
- **Rollover Math (Small 8-Bit Example -> 32-Bit `uwTick`):** Traces `start = 250`,
  `period = 10` across `255 -> 0` wraparound (`(uint8_t)(4 - 250) = 10`),
  explaining C's `int` promotion on `uint8_t` vs. native modulo-`2^32`
  subtraction on `uint32_t`.
- **`HAL_GPIO_TogglePin()` Bitwise Trace (`stm32f4xx_hal_gpio.c#L436`):** Traces
  `GPIOx->BSRR = ((odr & GPIO_Pin) << GPIO_NUMBER) | (~odr & GPIO_Pin)` for both
  `PA5 = 0` (`writes 0x00000020`) and `PA5 = 1` (`writes 0x00200000`) while `PA3`
  remains `1`, plus a Complex Syntax breakdown (`~`, `&`, `<< 16U`, `|`) and
  same-pin vs. different-pin ISR concurrency analysis.
- Updated [notes/lessons/README.md](../notes/lessons/README.md) and
  [training_tracker.md](training_tracker.md). No learner project files modified.
- **Follow-up review fixes on `W01-L13` (`R1`–`R4`, 2026-10-27):**
  1. `R1`: Labelled `42,000,000` as the nominal `84 MHz * 0.5 s` product and replaced unmeasured `"~5 cycles"` / `"< 1 us"` figures with `"short check"` and `"one masked BSRR write"`.
  2. `R2` + 8-bit overflow deep dive: Expanded Gem 1 with (a) the `250 + 10` addition-overflow bug (`uint8_t deadline = 250 + 10 = 4`, firing at `now = 251` after 1 tick), (b) the full-counter-period wrap ambiguity counterexample (`259` total elapsed ticks vs. `3` ticks when `elapsed >= 256`), and (c) the 3 rules to prevent timer overflow bugs (subtract `now - start`, keep total elapsed time since `start` `< 2^N`, and check frequently enough before `2^N` ticks elapse).
  3. `R3`: Clarified in Interview Q2 that setting `last_toggle_ms = now_ms` at sampled check tick `603` makes the next toggle **become due at tick `1103`** and occur when the loop next checks at or after `1103`.
  4. `R4`: Stated that the non-blocking LED check leaves room in `while (1)` for future P1 service checks, which will have their own schedule and execution budget.
  5. Added the learner's `last_toggle = 16_bit_max - 400`, `now = 100` trace to `W01-L13` showing (a) why modular subtraction underflow cancels counter rollover (`(100 - 65136) + 65536 = 500`), and (b) why a 16-bit `uint16_t` counter on a 32-bit CPU requires `(uint16_t)(now_16 - last_16) >= 500U` to avoid C's 32-bit signed `int` promotion + `500U` unsigned conversion bug.

## 2026-10-27: Published W01-L14 (Reading `B1` On `PC13` With Non-Blocking Debounce And Edge Detection)

**Eligibility check.** Verified against the active roadmap (Week 1–2 bare-metal
polling peripheral -> interrupt handoff progression) and `learning-arm-rtos`
(`polling -> interrupts -> buffering/deferred work`). Topic 9 (`W01-L14`) is the
exact eligible next lesson after `W01-L13`'s non-blocking output scheduler.

**Published.** Created
[W01-L14: Reading `B1` (`PC13`) With Non-Blocking Debounce To Change The `LD2` Blink Speed](../notes/lessons/W01_L14_b1_button_polling_and_nonblocking_debounce.md):
- Traces `HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13)` reading 32-bit `GPIOC->IDR` bit
  13 (`0x2000`) and active-low `B1` wiring (`GPIO_PIN_RESET` when pressed,
  `GPIO_PIN_SET` when released).
- Walks through the 4-scene story and `Before -> After` diagrams for mechanical
  contact bounce (`20 ms` non-blocking timer `now_ms - debounce_start_ms >= 20U`)
  and `false -> true` press-edge detection so holding `B1` changes
  `blink_period_ms` (`500 ms <-> 150 ms`) once per press without blocking `LD2`.
- Updated [notes/lessons/README.md](../notes/lessons/README.md) and
  [training_tracker.md](training_tracker.md). No learner project files modified.
- **Applied review findings (`R1`–`R5`, 2026-10-27; kept minor `R6`–`R7` as-is):**
  1. `R1` & `R2` (`W01-L13`): Labelled the FreeRTOS `250 + 10 = 4` trace as an illustrative 8-bit model assuming a `1 ms` tick (`xLastWakeTime` initialized to `250`), and clarified that tick expiry moves the task to the **Ready** list to run when scheduled by priority.
  2. `R3` (`W01-L13`): Qualified the Saturating Countdown and Sticky Flag table with ISR progress, safe rearming, and event-coalescing notes.
  3. `R4` & `R5` (`W01-L14`): Clarified sampled-observation debounce semantics (`>= debounce_window + polling_interval` to guarantee spanning the full `20 ms` window), attributed bounce rejection to the debounce algorithm, and fixed the Section 3B ASCII waveform line so `t = 204..224 ms` stays on the `LOW (Pressed)` row.

## 2026-10-28: Reviewed And Verified W01-L14 `main.c` Fix; Published W01-L15 (`B1` `EXTI15_10` Interrupt Handoff)

**Code review (`W01-L14` in `p1_f446re_baseline/Core/Src/main.c`):**
- Learner reported `LD2` did not blink after typing the Lesson 14 code.
- Inspected `Core/Src/main.c` read-only and identified the root cause: line 96
  declared `uint32_t debounce_start_time_ms = last_toggle_ms;` while lines 113
  and 115 referenced `debounce_start_ms`, causing a compile error (`'debounce_start_ms' undeclared`)
  so the new binary could not build/flash. Also noted moving the loop body into
  `USER CODE BEGIN 3` so CubeMX regeneration preserves it.
- Learner applied the fix in `Core/Src/main.c` (verified read-only at lines
  96 and 111–137: `debounce_start_ms` matches across declaration and use, and
  loop code is inside `USER CODE BEGIN 3`) and reported "Done."

**Published Topic 10 (`W01-L15`):**
- Created
  [W01-L15: `B1` (`PC13`) External Interrupt (`EXTI15_10`) Handoff From Handler Mode To Thread Mode](../notes/lessons/W01_L15_b1_exti_interrupt_handoff_and_debounce.md)
  following the roadmap's `polling peripheral -> interrupt handoff` progression:
  - Explains the 3 hardware interrupt gates (`SYSCFG->EXTICR[3]` + `EXTI->FTSR`/`IMR`/`PR`,
    `NVIC->ISER[1]` bit 8 for `EXTI15_10_IRQn = 40`, and vector slot `56` at
    `0x080000E0`).
  - Traces why `EXTI->PR = B1_Pin` uses direct `=` assignment on a Write-1-to-Clear
    (`rc_w1`) register instead of `|=`.
  - Shows how `EXTI15_10_IRQHandler()` (`Core/Src/stm32f4xx_it.c`) and
    `HAL_GPIO_EXTI_Callback()` (`Core/Src/main.c`) hand off `b1_irq_tick_ms` and
    `b1_irq_pending` from Handler mode to Thread mode (`while (1)`), using a
    brief `__disable_irq()` / `__enable_irq()` critical section and a `20 ms`
    post-edge quiet window before checking `HAL_GPIO_ReadPin()`.
- Updated [notes/lessons/README.md](../notes/lessons/README.md) and
  [training_tracker.md](training_tracker.md). No learner project files modified.

## 2026-10-28: Approved Register-Level Rewrite Of L15

**Review and approval.** Learner requested accuracy/prerequisite review and
removal of the HAL teaching path. Specific issues explained before approval:
priority group0 does not implement the claimed preemption priority5; a
falling-edge-only quiet-window check lacks stable-release rearming; vector
dispatch is not an enable gate; fixed instruction-count/frame claims needed
conditions. Learner approved rewriting L15 and its index/tracker/log, not
firmware, and chose the full84MHz regulator/Flash/PLL sequence rather than an
HSI-only first layer.

**Changed teaching.** Replaced L15's proposed implementation with one original
register-level source unit for a separate RegistersDebug configuration.
The working HAL main/interrupt/support files are preserved and excluded only
in the proposed new configuration, not erased. Device startup/linker and CMSIS
register names/intrinsics remain. Reference covers bounded readiness polling
with fault codes, 84MHz clock sequence, direct SysTick/GPIO/EXTI/NVIC, both-edge
activity, coherent main-context decision, boot-held policy and release-to-rearm.
Added Cortex-M modes/privilege, MSP/PSP, vector numbering, masking/grouping,
exception frames/return, and bounded previews of SVC/PendSV/faults/MPU/FPU.
SBI is correctly identified as RISC-V, not a Cortex-M prerequisite.

**Evidence and access.** Several source reads and original user-home manual
folders were denied by host policy; no alternate path was used to read those
blocked files. PowerShell could not start because the sandbox local proxy was
unavailable. Used permitted file reads and browser documentation instead.
Initial public RM0390 Rev3 review was superseded by learner-supplied local
[RM0390 Rev6](../docs/stm32/um-f446re/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf),
[UM1724 Rev14](../docs/stm32/um-f446re/UM1724.pdf) and
[DUI0553B](../docs/stm32/um-cortexm4/DUI0553.pdf), read in the local PDF viewer.
Checked voltage-scaling/PLL ordering, Flash timing/4-bit LATENCY, RCC and EXTI
fields, board connections, core modes and EXC_RETURN cases.
Corrected the draft's LATENCY mask to0xF during validation.

**Limits and next action.** This reference has not been compiled or executed.
A current ES0298 sheet for the actual silicon was not identified in the
supplied folder listing; that remains a pre-flash check along with supply/
board configuration and exact build inputs. Learner implements/builds only
after those checks. No project source/configuration, hardware, installation,
commit or push changed by the tutor. No study hours, measurements or mastery
invented. Existing earlier lessons and dated history preserved.
- **Sequential Story-Mode expansion (2026-10-28):** Expanded `W01-L15` into a
  continuous 6-scene Story-Mode walk from Power-On Reset (`Reset_Handler` ->
  `SystemInit` -> `main()`) through `clock_init_84mhz()`, `io_interrupt_init()`
  (GPIO clocks & pins -> `SYSCFG`/`EXTI` -> `SCB`/`NVIC` -> `SysTick`), hardware
  exception entry (`EXC_RETURN`), and the `for (;;)` release-before-arm state
  machine + `LD2` toggle, adding side-by-side `Before -> After` ASCII boxes,
  inline C comments, Complex Syntax breakdowns, and the 3-part control-flow
  section while preserving the exact reference code and review repairs.
- **Hybrid Bare-Metal + MPU Pre-FreeRTOS Decision (2026-10-28):**
  - Learner selected the **Hybrid Approach**: complete bare-metal using **only
    the linker script (`STM32F446RETX_FLASH.ld`) and `Core/Src/register_app.c`**
    (custom C `.isr_vector` table `g_pfnVectors[113]`, `Reset_Handler()` with
    FPU `CPACR` enable + `.data`/`.bss` init, manual-derived MMIO register
    structs, and inline assembly `cpsid i`/`mrs`/`msr`/`dsb`/`isb`—zero CMSIS
    headers, zero `startup_stm32f446retx.s`, zero `system_stm32f4xx.c`) for
    `W01-L15` and the next lesson (`W01-L16: Cortex-M4 MPU, Privilege (CONTROL),
    PSP/MSP, and MemManage Faults`), then transitioning to standard CMSIS
    headers (`stm32f446xx.h` / `core_cm4.h`) when entering `P1`/FreeRTOS.
  - Updated [W01-L15](../notes/lessons/W01_L15_b1_exti_interrupt_handoff_and_debounce.md),
    [notes/lessons/README.md](../notes/lessons/README.md),
    [five_month_development_roadmap.md](five_month_development_roadmap.md), and
    [training_tracker.md](training_tracker.md).
  - Added Section 3.D (*Hardware Document Detective Guide*) to `W01-L15` showing
    when to use each of the 5 hardware documents (`mb1136` Schematic, `UM1724`
    Board Manual, `stm32f446mc.pdf` Datasheet, `RM0390` Reference Manual, and
    `DUI0553B` Cortex-M4 Generic User Guide), how to `Ctrl+F` search and derive
    `Base Address + Offset` (including clarifying `0x4002 0000` `AHB1` base vs.
    `0x4004 0000` `USB OTG HS`), and a Master Lookup Table mapping every constant
    in `STM32F446RETX_FLASH.ld` and `register_app.c` to its exact document/table.
  - Applied review fixes `R1`–`R7` to `W01-L15` (`(SysTick->CTRL & 0x7U) == 0x7U`
    + `COUNTFLAG`, `EXTI->PR` read-instant semantics under `PRIMASK = 1`, `< 20`-tick
    bounce threshold assumption, `0x00000000/04` boot-alias reset fetch + Thumb
    bit 0 vs. `0x08000000/04` linked Flash and `VTOR`, initial `MSP = 0x20020000`
    vs. live `MSP` after C stack frames, `R29 = 100 Ohm` / `C15 = 100 nF` for
    `B1` on `MB1136 C03`, and Datasheet Section `3.6` for SRAM).
- **Published Topics 11 & 12 (`W01-L16` & `W01-L17`, 2026-10-28):**
  - Created
    [W01-L16: Complete Bare-Metal Cortex-M4 MPU, Privilege (`CONTROL`), `PSP` vs. `MSP`, `SVC`, And `MemManage` Faults (Linker Script Only)](../notes/lessons/W01_L16_cortexm4_mpu_privilege_psp_svc_and_memmanage.md)
    (`Core/Src/mpu_privilege_app.c` with 3 MPU regions, naked `PSP` +
    unprivileged switch `CONTROL = 0x03`, `SVC #1`/`#2` escalation, and
    `MemManage_Handler` capturing `MMFSR = 0x82`, `MMFAR`, and stacked `PC`).
  - Created
    [W01-L17: From Bare-Metal `PendSV` Context Switching To Official CMSIS Headers & FreeRTOS Tasks (`P1` Bridge)](../notes/lessons/W01_L17_pendsv_context_switch_cmsis_and_freertos_bridge.md)
    (`Core/Src/pendsv_cmsis_bridge_app.c` mapping our manual structs/intrinsics
    1-to-1 to `#include "stm32f446xx.h"`, forging 16-word initial task stack
    frames on `task0_stack` and `task1_stack`, launching Task 0 via `SVC_Handler`,
    and context-switching between `task_button_service` and `task_led_service`
    in `PendSV_Handler` at priority `15`).
  - Updated [notes/lessons/README.md](../notes/lessons/README.md) and
    [training_tracker.md](training_tracker.md). No learner project files modified.
- **Rewrote `W01-L15` in Interleaved Concept + Code Scene Format (2026-10-28):**
  - Per learner's visual example and feedback, restructured
    [W01-L15](../notes/lessons/W01_L15_b1_exti_interrupt_handoff_and_debounce.md)
    into 7 sequential scenes (`Scene 1: POR & Vector Table`, `Scene 2: Startup Memory Init & Reset_Handler`, `Scene 3: Clock Tree & 84 MHz PLL`, `Scene 4: GPIO Clocks & PA5/PC13`, `Scene 5: SYSCFG/EXTI/NVIC/SysTick & ISRs`, `Scene 6: main() Release-Before-Arm Debounce & LD2 Loop`, `Scene 7: Document Lookup Cheat-Sheet & RegistersDebug Checklist`).
  - Every scene presents a simple numbered ASCII diagram -> concise numbered
    bullet points -> the exact corresponding C/linker code right below it,
    removing separate theory/code splits, analogies, and meta-review tables while
    retaining all technical details and `R1`–`R7` accuracy conditions. Updated
    [learning-feynman-coach/SKILL.md](../.github/skills/learning-feynman-coach/SKILL.md)
    to enforce this interleaved scene format.
- **Rewrote `W01-L16` & `W01-L17` in Interleaved Format & Audited `W01-L15`..`L17` (`learning-lesson-review`, 2026-10-28):**
  - Rewrote
    [W01-L16](../notes/lessons/W01_L16_cortexm4_mpu_privilege_psp_svc_and_memmanage.md)
    and
    [W01-L17](../notes/lessons/W01_L17_pendsv_context_switch_cmsis_and_freertos_bridge.md)
    in the interleaved `ASCII Diagram -> Bullet Points -> Immediate C Code`
    scene format.
  - Audited `W01-L15`, `W01-L16`, and `W01-L17` against the `learning-lesson-review`
    checklist (`C01`–`C12`) and fixed 3 subtle issues during the audit:
    1. Serialized `SysTick`, `EXTI15_10`, and `SVCall` at preemption priority `5`
       (`0x50`, with `MemManage` at priority `0`) in `W01-L16` so kernel handlers
       never race on `b1_irq_pending` / `g_kernel_vault`.
    2. Added `__attribute__((used))` to C functions branched to from naked
       assembly (`svc_handler_c`, `memmanage_handler_c`, `unprivileged_app_loop`
       in `W01-L16`, and `svc_first_task_psp`, `pendsv_switch_context` in `W01-L17`)
       so `-Wall` / `--gc-sections` never warn or discard them.
    3. In `W01-L17`, set `SVCall` to priority `0` and held `BASEPRI = (1U << 4)`
       across `__enable_irq(); svc #0;` until `SVC_Handler` initializes `PSP`
       and clears `BASEPRI = 0`, preventing `SysTick` or `EXTI15_10` from pending
       `PendSV` before `PSP` is loaded.
- **Applied Cross-Lesson Redundancy Labels & Technical Review Fixes `R1`–`R4` (2026-10-29):**
  1. Added a **"What Changes From The Previous Lesson (And What Is Reused Unchanged)"**
     overview and `/* BEGIN/END UNCHANGED FROM W01-L15 */` markers to `W01-L16`
     and `W01-L17` while keeping each file standalone and preserving the distinct
     vector, priority, and debounce ownership logic.
  2. `R1` (`W01-L15:158`): Replaced `"PRIMASK = 1: mask IRQs 0..15"` with
     `"PRIMASK = 1: mask configurable-priority exceptions; NMI and HardFault remain unmasked"`.
  3. `R2` (`W01-L17`): Clarified that writes to `CONTROL.SPSEL` are ignored in
     Handler mode, and that returning with `EXC_RETURN = 0xFFFFFFFD` (`bit 2 = 1`)
     selects `PSP` (loaded via `MSR PSP`) for resumed Thread mode.
  4. `R3` (`W01-L17`): Labelled the 16-word stack layout as the initial/basic
     frame without alignment padding and explained the optional `STKALIGN` pad
     word + stacked `xPSR` bit 9 (`xPSR[9] = 1`).
  5. `R4` (`W01-L17`): Updated the FreeRTOS notification mapping row to show
     both sides (`task waits via ulTaskNotifyTake()/xTaskNotifyWait() -> ISR notifies via xTaskNotifyFromISR() + portYIELD_FROM_ISR() -> task becomes Ready -> scheduler runs it`).
  6. Updated [learning-feynman-coach/SKILL.md](../.github/skills/learning-feynman-coach/SKILL.md)
     and [learning-arm-rtos/SKILL.md](../.github/skills/learning-arm-rtos/SKILL.md)
     with generic authoring rules covering cross-lesson `What Changes / Reused
     Unchanged` labelling, Document Lookup Cheat-Sheets, read-to-clear status
     bits (`COUNTFLAG`), boot-alias vs. linked Flash vs. `VTOR`, initial vs. live
     stack pointers, `PRIMASK` scope and unprivileged `CPSID`/`MSR` restrictions,
     `CONTROL.SPSEL` in Handler mode vs. `EXC_RETURN`, `STKALIGN` frame padding,
     hardware edges during critical sections, filter threshold assumptions, and
     both-side RTOS producer/consumer mappings.
  7. Upgraded [W01-L15](../notes/lessons/W01_L15_b1_exti_interrupt_handoff_and_debounce.md)
     and [learning-feynman-coach/SKILL.md](../.github/skills/learning-feynman-coach/SKILL.md)
     so every numbered step follows the **"1. What & Why -> Otherwise... -> Set `<REGISTER>` bit `<N>`"**
     pattern, split `gpio_init()` (Scene 4) from `exti_nvic_systick_init()` (Scene 5)
     so Scene 4 has its own complete C function right below its explanation, and
     added the 5-block **STM32 MCU Peripheral (`GPIOC -> SYSCFG -> EXTI`) to Arm
     Cortex-M4 Core (`NVIC -> CPU Core & VTOR`)** interrupt-routing diagram and
     block-by-block explanation.

**Static validation.** Checked the proposed PLL field mask0x7F437FFF and
value0x27015410, nominal84MHz/48MHz outputs, SysTick LOAD83999, vector offset
0xE0 and IRQ40 enable index1/bit8. Confirmed the local reference links and
current index/tracker point to the register-level lesson. These are document/
arithmetic checks, not compilation or a target run; the reference retains that
explicit limitation.

## 2026-10-28: Thumb And Arm Instruction-Set Reference Published

- **Request:** explain Thumb, the other main Arm instruction sets, why they
  exist and when to select each in simple language.
- **Tutor action:** published
  [W01-L18](../notes/lessons/W01_L18_thumb_and_arm_instruction_sets.md);
  linked it from the lesson index and tracker. Existing lesson bodies and
  learner implementation files were preserved.
- **Source inspection:** read local Arm DUI0553B sections 1.1, 2.1.3 and 3.1
  using the existing local PDF reader; read GCC Arm option documentation.
  Read-only search of the existing P1 generated compiler rule confirmed
  `-mcpu=cortex-m4 -mthumb`. Its saved disassembly includes `2301`,
  `movs r3, #1`. Inspection performed by tutor on the Windows host; no
  compilation, firmware execution or hardware measurement.
- **Explanation:** distinguish A32/T32/A64, Thumb-2 versus a separate execution
  state, instruction width versus register width, and CPU/ABI constraints.
  The small assembly/state trace is illustrative rather than a measured run.
- **Limits / next action:** publication only; learner understanding, retention
  and study time remain unknown. Read the note and discuss requested
  clarifications. Quizzes remain paused.

## 2026-10-28: Reset-Register Meanings And Values Published

- **Request:** explain the purpose and possible values of CONTROL, PRIMASK,
  BASEPRI, SCB, RCC and the other registers in the quoted CPU-reset paragraph.
- **Tutor action:** published the concept-only
  [W01-L19 companion](../notes/lessons/W01_L19_cortexm4_reset_registers.md);
  linked it from the index and tracker. Existing startup lessons were not
  rewritten.
- **Evidence:** tutor read Arm DUI0553B and STM32F446 RM0390 Rev 6 through
  the existing local PDF reader on Windows. Read-only device-header searches
  confirmed four implemented priority bits, FLASH latency encodings, PLL
  controls and STM32F446's PLL-R system-clock selection. No installation,
  compilation, source edit or hardware operation was performed.
- **Explanation:** separates CPU registers from memory-mapped register
  blocks; decodes CONTROL and BASEPRI; distinguishes mask-zero from
  interrupt-enabled, vector slots from handlers, and clock enable from
  readiness/selection. Values and transitions are illustrative.
- **Limits / next action:** source-reviewed publication, not learner mastery
  or a observed reset trace. Actual study hours remain unknown. Discuss
  requested clarifications; quizzes remain paused.

## 2026-10-29: BSP Meanings, Board-Support Work And Prerequisites

- **Request:** distinguish Board Support Package from Bootstrap Processor;
  explain the work involved in board support and the concepts to strengthen.
- **Tutor action:** published
  [W01-L20](../notes/lessons/W01_L20_bsp_board_support_and_bootstrap_processor.md)
  and updated its index/tracker links. Earlier lesson bodies and project
  implementations were preserved.
- **Evidence:** inspected ST's public Nucleo BSP README/header/source, local P1
  HAL GPIO write implementation, Zephyr's board-porting guide and EDK II's
  MP Services interface. Public branch content was read on 2026-10-29, not
  installed or adopted as a pinned dependency.
- **Explanation:** separated software-package scope from processor boot roles;
  traced LED2 -> board pin mapping -> HAL -> output latch; outlined board
  bring-up, drivers, RTOS integration, debugging, failure tests and maintenance.
  Mapped prerequisites to the existing MCU/FreeRTOS-first P1/P2 track.
- **Limits / next action:** tutor source review and illustrative reasoning
  only; no build, board operation or completion claim. Learning time and
  independent competence remain unknown. Discuss requested follow-ups;
  quizzes remain paused.

## 2026-10-29: Upgraded W01-L16 Through W01-L20 And Skills With "What & Why -> Otherwise -> Register/Bit -> Code" Steps And STM32-To-Arm Interrupt Routing

- **Request:** update `W01-L16`, `W01-L17`, `W01-L18`, `W01-L19`, and `W01-L20`
  in the same `"1. What & Why -> Otherwise... -> Register / Bit -> Immediate Code"`
  and explicit STM32-Peripheral-to-Arm-Core interrupt-routing format used in
  `W01-L15`, and add the rule to the skills for future lessons.
- **Updates applied:**
  1. [W01-L16](../notes/lessons/W01_L16_cortexm4_mpu_privilege_psp_svc_and_memmanage.md):
     Upgraded Scenes 1–5 to the `"What & Why -> Otherwise... -> Register / Bit"`
     step format, split `gpio_init()` from `exti_nvic_systick_init()`, and added
     the STM32-Peripheral (`GPIOC -> SYSCFG -> EXTI`) to Arm-Core (`NVIC ->
     SysTick / SVCall Priority 5 serialization & MemManage Priority 0`) routing
     diagram and explanation.
  2. [W01-L17](../notes/lessons/W01_L17_pendsv_context_switch_cmsis_and_freertos_bridge.md):
     Upgraded Scenes 1–3 to the `"What & Why -> Otherwise... -> Register / Bit"`
     step format, split `gpio_init()` from `rtos_vectors_and_irq_init()`, and
     added the STM32-Peripheral (`PC13 -> SYSCFG -> EXTI13`) to Arm-Core (`NVIC
     IRQ 40 / SysTick #15 -> SCB->ICSR PENDSVSET -> PendSV #14 Priority 15`)
     routing diagram and explanation.
  3. [W01-L18](../notes/lessons/W01_L18_thumb_and_arm_instruction_sets.md):
     Upgraded Scenes 1–4 with `"What & Why -> Otherwise... -> Encoding / Bit / Flag"`
     steps and corresponding C/assembly/build snippets (`g_reset_preview`,
     `make_initial_xpsr_thumb`, `enable_gpioa_clock_32bit_reg`, `thumb2_add_demo`,
     and `-mcpu=cortex-m4 -mthumb` compile-time guard).
  4. [W01-L19](../notes/lessons/W01_L19_cortexm4_reset_registers.md):
     Upgraded Scenes 1–7 (`MRS`/`MSR` vs. MMIO structs, `CONTROL`/`MSP`/`PSP`,
     `IPSR`, `PRIMASK`/`BASEPRI`/`FAULTMASK`, `SCB->VTOR`, `RCC->CR`/`CFGR`,
     `FLASH->ACR`) with `"What & Why -> Otherwise... -> Register / Bit"` steps
     and immediate corresponding C code snippets.
  5. [W01-L20](../notes/lessons/W01_L20_bsp_board_support_and_bootstrap_processor.md):
     Upgraded Scenes 1–6 with `"What & Why -> Otherwise... -> Register / Bit"`
     steps, added the complete `bsp_b1_exti_init()` STM32-to-Arm interrupt
     routing helper in Scene 4, and added corresponding C code snippets to every
     scene.
  6. [learning-feynman-coach/SKILL.md](../.github/skills/learning-feynman-coach/SKILL.md)
     and [learning-arm-rtos/SKILL.md](../.github/skills/learning-arm-rtos/SKILL.md):
     Codified the mandatory `"What & Why -> Otherwise... -> Set <REGISTER> bit <N> -> Immediate Code"`
     step rule and explicit MCU-Peripheral-to-Arm-Core interrupt-routing rule
     for all future lessons.

## 2026-10-31: Streamlined W01-L16 With Simple Unified Example, Per-Point Code Snippets, And Complete Code At The End

- **Request:** `W01-L16` was too large and lacked a simple unified example at
  the beginning; add a simple unified example of everything explained before
  providing the actual code, put focused code snippets directly under each new
  explanation point, and provide the complete standalone code at the end.
- **Updates applied:**
  1. Added the **Simple Unified Example** at the top of
     [W01-L16](../notes/lessons/W01_L16_cortexm4_mpu_privilege_psp_svc_and_memmanage.md)
     walking end-to-end from Privileged Startup (`CONTROL = 0x00`, `MSP`, MPU
     Regions 0/1/2) -> Unprivileged `PSP` Loop (`CONTROL = 0x03`, Action A legal
     reads, Action B `svc #1`/`#2` system calls on presses 1 & 2, and Action C
     deliberate unprivileged write blocked by MPU Region 2 and caught by
     `MemManage_Handler` on press 3).
  2. Placed a focused C/assembly snippet immediately below each individual
     numbered point (`1.`, `2.`, `3.`) in Scenes 1–5 (removing repeated blocks
     of unchanged `W01-L15` startup/clock code from the explanation scenes).
  3. Provided the complete standalone `Core/Src/mpu_privilege_app.c` once at
     the end of the lesson, reducing the file from `803` lines to `662` lines
     while improving readability, and updated
     [learning-feynman-coach/SKILL.md](../.github/skills/learning-feynman-coach/SKILL.md).

## 2026-11-01: Rewrote W01-L16 In Plain Feynman MCU Wording ("Simple Words First -> ASCII Diagram -> Code Snippet -> Complete Code")

- **Request:** the unified example in `W01-L16` was not understandable; rewrite
  everything in `W01-L16` using simple understandable MCU words first (as
  Feynman would explain to a 12-year-old, without analogies), followed by a
  simple ASCII diagram if needed, then the focused code snippet under each
  explanation, and the complete standalone code at the end.
- **Updates applied:**
  1. Rewrote the **Unified Example** and **Scenes 1–5** of
     [W01-L16](../notes/lessons/W01_L16_cortexm4_mpu_privilege_psp_svc_and_memmanage.md)
     so every section explains the mechanism in plain, direct MCU sentences
     first (before any ASCII diagram or register bit-math), followed by a
     compact ASCII diagram where helpful, and then the exact C/assembly snippet
     for that point, with the complete `Core/Src/mpu_privilege_app.c` at the end.
  2. Updated [learning-feynman-coach/SKILL.md](../.github/skills/learning-feynman-coach/SKILL.md)
     to enforce the `Simple Understandable Words First -> Simple ASCII Diagram (if needed) -> Focused Code Snippet -> Complete Code at End` order.

## 2026-11-01: Approved Feynman Explanation Style Applied Across W01-L15..L20 And Re-Ordered Cortex-M4 Prerequisites First

- **Request:** learner approved the rewritten explanation style; asked to:
  1. Rename former `L18` and `L19` to `L15` and `L16` so Arm Cortex-M4
     instruction sets, important assembly instructions, and important core/reset
     registers come first, and continue sequentially from `L17` (`L15 -> L17`,
     `L16 -> L18`, `L17 -> L19`, `L20 -> L20`).
  2. Update `W01-L15` through `W01-L20` with the approved approach (`Unified
     Example in simple MCU words first -> Simple ASCII Diagram -> Scenes with
     Simple Words First + Focused Code Snippet -> Complete Code at the End`),
     removing unnecessary "what is reused" meta-sections.
- **Updates applied:**
  1. [W01-L15](../notes/lessons/W01_L15_thumb_and_arm_instruction_sets.md)
     (formerly `W01-L18`): Added Unified Example and sequential Scene 3 guide
     covering all important Cortex-M4 instructions (`movs`, `add.w`, `ldr`/`str`,
     `mrs`/`msr`, `cpsid`/`cpsie`, `dsb`/`isb`, `nop`, `push`/`pop`,
     `stmdb`/`ldmia`, `svc`, `tst`/`ite`, `bx lr`) while preserving all existing
     instruction-set explanations.
  2. [W01-L16](../notes/lessons/W01_L16_cortexm4_reset_registers.md)
     (formerly `W01-L19`): Added Unified Example and Scene 1 complete overview
     of the 16 CPU core registers (`R0..R15`, `xPSR`) vs. memory-mapped register
     blocks (`SCB`, `SysTick`, `NVIC`, `MPU`, `RCC`, `FLASH`) while preserving
     all existing reset-register explanations.
  3. [W01-L17](../notes/lessons/W01_L17_b1_exti_interrupt_handoff_and_debounce.md)
     (formerly `W01-L15`),
     [W01-L18](../notes/lessons/W01_L18_cortexm4_mpu_privilege_psp_svc_and_memmanage.md)
     (formerly `W01-L16`),
     [W01-L19](../notes/lessons/W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md)
     (formerly `W01-L17`), and
     [W01-L20](../notes/lessons/W01_L20_bsp_board_support_and_bootstrap_processor.md):
     Updated with the Unified Example, per-point focused code snippets, and
     complete standalone code at the end, with "what is reused" meta-sections
     removed.
  4. Synchronized [notes/lessons/README.md](../notes/lessons/README.md),
     [notes/README.md](../notes/README.md),
     [five_month_development_roadmap.md](five_month_development_roadmap.md),
     [training_tracker.md](training_tracker.md), and teaching skills.

## 2026-11-03: Rewrote W01-L19 With Complete 16-Slot Stack Breakdown (`Return_PC` vs. `LR`), Why `SVC_Handler` Pops After `task_stack_init()` Pre-Pushes, And Simplified Task Code

- **Request:** in `W01-L19`, explain in simple English followed immediately by
  each step's code: (1) explain what all 16 slots (`R0..xPSR` and `R4..R11`)
  contain in Step 1 (`task_stack_init`) and the difference between `Return_PC`
  and `LR`, (2) explain why `SVC_Handler` in Step 2 pops from `task0_stack`
  instead of pushing, and (3) keep the task code simple so the focus stays on
  understanding `SVC` and `PendSV` context switching.
- **Updates applied:**
  1. Rewrote [W01-L19](../notes/lessons/W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md)
     into 5 sequential steps (`Step 1 -> Step 1 Code -> ... -> Step 5 -> Step 5 Code -> Complete Code`).
  2. Added the complete 16-slot table (`sp[15]` down to `sp[0]`) and plain-English
     distinction between `Return_PC` (`sp[14]`, loaded into `PC` on exception
     exit to start/resume the task) and `LR` (`sp[13]`, loaded into the task's
     `LR` in case the task function ever executes `return;`).
  3. Explained why `task_stack_init()` pre-pushes the 16 words in `main()`, why
     `SVC_Handler` only pops `task0_stack` (discarding `main()`'s finished frame
     on `MSP`), and how `PendSV_Handler` pushes outgoing `Task 0` then pops
     incoming `Task 1`.
  4. Simplified `task_button_check()` (`Task 0`) and `task_led_blink()` (`Task 1`)
     so the code focuses directly on the 2-task context switch.
  5. Added the complete boot + button press (`t = 1000 ms`) + bounce/repeat press
     within `200 ms` (`t = 1050 ms`) scenario trace and the two hardware Q&As
     (`SCB->ICSR = PENDSVSET` + `__DSB()`/`__ISB()` pipeline timing, and the
     learner's 6-step `Active = 1, Pending = 1` nested `EXTI15_10` -> tail-chained
     second `PendSV_Handler` trace).

## 2026-11-04: Published W02-L21 (`C02` Three-Resource Init & Reverse Unwind) And W03-L22 (FreeRTOS Task States, Priorities, `vTaskDelayUntil`, And Notifications)

- **Request:** prepare the next two topics in the approved Staff/Principal
  ground-up style:
  1. `C02` Three-Resource Initialization and Reverse-Order Unwind (`W02-L21`),
     covering Linux kernel (`goto` ladder), FreeRTOS (`P1` on STM32 Cortex-M4),
     and AUTOSAR / MISRA-C (`goto`-free where Rule 15.1 prohibits `goto`).
  2. FreeRTOS from the ground up (`W03-L22`): `tskTCB` & lists, task states,
     task vs. NVIC priority rules (`configMAX_SYSCALL_INTERRUPT_PRIORITY`),
     `vTaskDelayUntil`, and `ulTaskNotifyTake` / `vTaskNotifyGiveFromISR`.
- **Publications:**
  1. [W02-L21](../notes/lessons/W02_L21_c02_three_resource_init_and_reverse_unwind.md)
  2. [W03-L22](../notes/lessons/W03_L22_freertos_tasks_states_delays_and_notifications.md)
  Updated [notes/lessons/README.md](../notes/lessons/README.md) and
  [training_tracker.md](training_tracker.md).

## 2026-11-07: Pico BOOTSEL Detection Reported; Extension Setup Pending

- **Learner evidence:** after the instructed BOOTSEL USB connection check,
  learner reports "it appears", referring to the expected `RPI-RP2` drive
  in Windows. Recorded as completed-reported USB bootloader detection.
- **Tutor inspection:** read-only Windows commands `code --version` and
  `code --list-extensions --show-versions` completed successfully. Observed
  VS Code 1.144.0 x64; the queried CLI profile lists CMake Tools and no
  Raspberry Pi Pico extension. This does not inventory every VS Code profile
  or establish whether Pico SDK/compiler files exist elsewhere.
- **Next step:** learner installs the official Raspberry Pi Pico extension
  in the intended VS Code profile before project creation/build guidance.
- **Limits:** no tutor install, implementation change, firmware upload or
  target access. Existing Pico firmware replacement remains unapproved;
  SDK readiness, firmware behavior and dual-core execution remain unverified.
  Study hours unknown; quizzes remain paused. P1/P2 scope unchanged.

## 2026-11-07: Pico Extension Installed; Blink Example Creation Next

- **Learner evidence:** reports installing the Raspberry Pi Pico extension.
- **Tutor verification:** read-only `code --list-extensions --show-versions`
  confirms `raspberry-pi.raspberry-pi-pico@0.23.0`. Inspected the installed
  package metadata: single-folder workspace support, New Example Pico Project
  and Compile Pico Project commands. The packaged example list includes
  `blink`; the public example CMake definition generates extra firmware outputs.
- **Next step:** learner creates the original-Pico (`pico`, RP2040) blink
  example in a separate VS Code window/folder, preserving its supplied source.
  Record SDK/toolchain selection; leave firmware upload for a later step.
- **Limits:** installation verified, not SDK readiness or successful project
  generation/build. No tutor install, project-file edit, build or hardware
  operation; Pico firmware replacement permission still pending. No learning
  hours or mastery inferred.

## 2026-11-07: Pico Project Creation Reported; First Compile Pending

- **Learner evidence:** "Project created." This follows the official `blink`
  example instructions for the original RP2040 Pico.
- **Status:** project creation completed-reported. Exact project location,
  generated board configuration and SDK/toolchain versions were not supplied
  or inspected; no successful build is inferred.
- **Next step:** learner runs **Raspberry Pi Pico: Compile Pico Project**
  in the separate project window, preserves the supplied example source and
  checks the build result/generated UF2. Firmware upload remains a later step.
- **Limits:** no tutor build, source edit, upload or target access. Existing
  Pico firmware replacement permission remains pending. No study hours,
  physical behavior or multicore execution evidence reported.

## 2026-11-07: STM32 Primary Plan Reaffirmed; Pico UF2 And SWD Details Recorded

- **Learner decision:** approved retaining F446RE/FreeRTOS for P1 and
  F767ZI/Zephyr for P2, with a bounded Pico setup check and optional later
  multicore exercise rather than replacing P1.
- **New learner evidence:** "The uf2 file is available." Recorded as
  artifact-available-reported, not an inspected successful build or target
  run. Exact path, matching source/configuration, build log and SDK/toolchain
  versions remain unknown.
- **Debug clarification:** learner identifies `SWCLK`, `GND` and `SWDIO` on
  the Pico. These are SWD connections; a compatible external probe and verified
  pinout, signaling, target-power and host-tool configuration are required
  for hardware debugging. USB loading alone is not an onboard debug probe.
- **Plan changes:** updated the roadmap, project contracts, allocation plan,
  setup, inventory and tracker. A single-core-baseline-to-multicore bounded
  producer/consumer experiment may replace another Week 17 optional extension
  after P1/P2 core gates. Preserve 22 hours/week and existing projects/lessons.
  Interview evidence remains engineering depth and genuine work, not board
  brand/core count or a guaranteed compensation outcome.
- **Next checkpoint:** record Pico artifact/configuration details; any upload
  needs explicit replacement permission and confirmed safe setup. Keep the
  main learning path on STM32. Defer multicore work if core tasks need the time.
- **Limits:** plan/document updates only. No firmware source/configuration
  edits, builds, uploads, probe connections, wiring, installations or purchases.
  Physical blink, SWD debugging and dual-core execution remain unverified.

## 2026-11-17: Published W03-L23 (`C03` Event Producer/Consumer Ownership Model — Payload Lifetime, Copy Queues, And Zero-Copy Slot Pools)

- **Request:** proceed with the next topic according to the active STM32F446RE /
  FreeRTOS (`P1`) roadmap.
- **Publication:** created
  [W03-L23](../notes/lessons/W03_L23_c03_producer_consumer_payload_ownership.md)
  covering the Week 3 Applied C ownership exercise (`C03`):
  1. Why notification (`vTaskNotifyGiveFromISR`) signals *when* an event
     occurred without owning payload bytes, and the 3 classic C firmware bugs
     (dangling stack pointers, torn multi-word reads on a `volatile` global
     struct, and premature ISR buffer reuse).
  2. Model A: bounded copy-by-value ring queue with `Drop-Newest` overload
     accounting (`dropped_count`) and zeroed output before early failure return.
  3. Model B: static zero-copy slot pool (`SLOT_FREE -> SLOT_FILLING ->
     SLOT_READY -> SLOT_IN_USE -> SLOT_FREE`) with `generation`-guarded handles
     to reject stale/double releases, mapped directly onto FreeRTOS
     `xQueueSendFromISR` / `xQueueReceive` for `P1`, plus a standalone C11
     verification test harness and scenario trace.
- **Next topic:** Week 4 main RTOS lesson (`W04-L24`: FreeRTOS Queues, Mutexes
  vs. Binary Semaphores, ISR API Rules, Critical Sections, and Priority
  Inversion).
- **Limits:** teaching Markdown and tracker/index/log updates only; no learner
  implementation edits, builds, or hardware runs performed. Quizzes remain paused.