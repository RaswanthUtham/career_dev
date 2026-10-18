# Learning Evidence And Recall Log

Schedule: [authoritative twenty-week curriculum](five_month_development_roadmap.md). Current module status: [training tracker](training_tracker.md). Environment: [workspace readiness](../docs/workspace_setup.md). Teaching protocol: [Feynman coach](../.github/skills/learning-feynman-coach/SKILL.md). Dated entries below preserve history; their old next-action statements do not override the tracker.

## Current Position

- Active phase: rebaselined week 3 xv6/RISC-V foundation and learner-authored Linux-aligned kernel, default Teach mode. Questions are paused until explicitly resumed; implementation requests do not resume testing.
- Learner reports intermediate C proficiency and 10+ years using C. Concrete code examples and terms defined before use are required; no elementary syntax or `printf` restart.
- Current topic: [new kernel milestone 4 Stage 3 & milestone 5](scratch_riscv_os_plan.md#milestones-and-evidence), Sub-Page Slab Allocator (`kmalloc`/`kfree`), High-Half Kernel Relocation, and Timekeeping (`clocksource`, `clock_event_device`, `jiffies_64`, `timer_list`). Milestone 2 (S-mode boot), Milestone 3 (Device Tree discovery), and Milestone 4 Stages 1 & 2 (Trap vector, Linux-style Physical Page Allocator, Sv39 3-level MMU, High-Half Linear Mapping with runtime `va_pa_offset`, `walk_va_to_pa`, and Page Fault handling) verified live in QEMU via OpenSBI.
- Architecture policy: The learner created `my-linux/reference/` preserving the legacy implementation. Active files are authored by the learner in `my-linux/`. Hardware reset (`0x1000` ROM), dual-boot firmware options (custom M-mode firmware stub vs production OpenSBI S-mode entry), and the user-mode interactive shell are integrated into Milestone 7. Use xv6-riscv for OS mechanisms and Linux for accurate subsystem naming, firmware/DTB contracts and API boundaries; a reduced teaching kernel is not Linux.
- Historical xv6 progress from retired schedules is preserved below; no completed work has been erased or automatically counted as retained mastery.
- Budget from week 3: confirmed 22 hours/week (five 2-hour weekdays plus two 6-hour weekend days, including breaks).
- Setup: installed tools and running Podman/image availability checked on 2026-09-22; runtime and new-clone gates are recorded separately in the setup guide.

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