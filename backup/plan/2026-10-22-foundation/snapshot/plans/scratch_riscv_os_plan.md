# my-linux v2: Linux-Aligned RISC-V Teaching Kernel

Updated 2026-10-21. This guide owns **what to build, in which order, against which upstream code, and how each milestone is accepted**. The [roadmap](five_month_development_roadmap.md) owns weeks and hours, the [tracker](training_tracker.md) owns status, and the [learning log](learning_log.md) owns dated history. There is no second calendar here.

`my-linux` v1 is preserved on branches `main` and `v1` and tag `v1-reference` (commit `6610119`), plus an offline bundle at `C:\workspace\career_ws\backups\my-linux-v1.bundle`. v2 is developed on branch `v2`, which started empty. v1 is read-only reference material: concepts may be re-derived from it, code is never copied forward without being re-taught against upstream. The untracked `my-linux/reference/` folder (the older `riscv-kernel`) is kept and must stay out of v2 commits.

## 1. Why This Project Exists

Senior and staff kernel/firmware interviews probe the same chain repeatedly: reset and boot handoff, privilege and traps, virtual memory, allocators, interrupts and deferred work, scheduling and locking, and the user/kernel boundary. Building each link in upstream Linux order, with upstream names and data structures, produces:

- a repository you can walk an interviewer through, link by link;
- precise answers to "what happens between power-on and `start_kernel()`", "how does `kmalloc(8)` work", "top half versus bottom half", "how does a context switch save state";
- a direct mental map onto `linux/arch/riscv`, `mm/`, `kernel/` and, by comparison, `arch/arm64`.

It is a reduced teaching kernel, not Linux. Every deliberate simplification is labelled in the milestone note.

## 2. Rules That Remove Ambiguity

| Rule | Meaning |
| --- | --- |
| Learner-authored | The learner creates every folder and file in `my-linux` and types every line. The tutor never edits `my-linux` unless the learner names the exact files to change. |
| Lesson location | Each milestone is taught in `notes/kernel/Kxx_*.md` inside `career_dev` (new folder; older notes in `notes/` are left unchanged), with explanations, ASCII diagrams, worked traces, target file paths and complete reference code. Index: [notes/kernel/README.md](../notes/kernel/README.md). |
| Tutor verification | Before a lesson note is published, the tutor builds and runs its reference code in a private scratch copy **outside** `my-linux` and records the result at the top of the note as `Verified: <toolchain>, <QEMU>, <date>` or `Unverified: <reason>`. |
| Upstream order | A mechanism is implemented in the same execution context as upstream (MMU off versus on, before or after `start_kernel()`, hard IRQ versus process context). Any deviation is named in a "Deviation from upstream" box. |
| Evidence-first debugging | On failure: collect `qemu -d int` output, `objdump -h`, `nm -n` and `readelf -l` first, state one hypothesis, change one thing, re-run. No chained speculative fixes. |
| Section audit | Every linker-script or layout change is checked with `--orphan-handling=error` and `objdump -h` before it is taught. |
| Note corrections | A wrong statement is corrected in place and recorded in the note's "Corrections" section; no stacked contradictory patches. |
| Tests | K1-K2 use direct-call self-tests in the older upstream `lib/test_*.c` style, because KUnit suite tables are stored pointers that are unusable before the MMU is on. From K3, self-tests use `lib/kunit/` (upstream KUnit naming, reduced runner). Every test target requires a clean QEMU exit through SBI SRST plus a PASS line (`make test`, `make test-fw`). |

## 3. Environment Pins

| Item | Pinned value | How to re-check |
| --- | --- | --- |
| Container | `localhost/xv6-lab` (Podman) | `podman images` |
| Compiler | `riscv64-linux-gnu-gcc` 13.3.0 | `riscv64-linux-gnu-gcc --version` |
| Emulator | QEMU 8.2.2, `-machine virt -cpu rv64 -m 128M -smp 1` | `qemu-system-riscv64 --version` |
| CPU features seen by OpenSBI | `rv64imafdch`, extensions `time,sstc` | OpenSBI banner |
| Upstream reference | `C:\workspace\career_ws\linux` | Linux 7.3-rc4, commit `f0100363d` (shallow clone without tags; read `VERSION`/`PATCHLEVEL` in `linux/Makefile`) |
| OS comparison | `C:\workspace\career_ws\xv6-riscv` | - |

## 4. Boot Architecture: Two Firmwares, One Kernel

Dual boot is required from K1. The kernel is always the same S-mode image linked as upstream does; only the M-mode firmware changes.

```text
Path A: production firmware
  QEMU reset ROM 0x1000 --> OpenSBI (M-mode, 0x80000000) --mret--> my-linux _start (S-mode, phys 0x80200000)

Path B: learner-written firmware ("mini-SBI", separate image in my-linux/firmware/)
  QEMU reset ROM 0x1000 --> firmware/fw.bin (M-mode, 0x80000000) --mret--> my-linux _start (S-mode, phys 0x80200000)

Both paths hand over:  a0 = hartid   a1 = DTB physical address   satp = 0   S-mode
```

Why a separate firmware image instead of `#ifdef` code inside the kernel: real platforms ship firmware separately (OpenSBI on RISC-V, TF-A on Arm64), upstream `CONFIG_RISCV_M_MODE` means "Linux itself without an MMU in M-mode" (for example K210), not "SBI inside the kernel". QEMU passes `a2 = &fw_dynamic_info` (next address, next mode) to whatever runs at `0x80000000`, which is exactly the contract OpenSBI's `fw_dynamic` uses; the learner implements that contract.

## 5. Upstream Boot Order That v2 Follows

```text
head.S _start (MMU off, physical PC)
  |-- mask interrupts, park secondary harts, clear .bss/.sbss, early stack
  |-- setup_vm(dtb_pa)            arch/riscv/mm/init.c  static early_pg_dir/early_pmd, 2 MB mappings, NO allocator
  |-- relocate_enable_mmu()       arch/riscv/kernel/head.S  trampoline_pg_dir + stvec redirect, then early_pg_dir
  `-- start_kernel()              init/main.c  (virtual PC at KERNEL_LINK_ADDR)
        |-- setup_arch()          arch/riscv/kernel/setup.c
        |     |-- parse_dtb()                 memory, chosen, timebase
        |     `-- paging_init()               setup_bootmem() -> memblock; setup_vm_final() -> swapper_pg_dir linear map
        |-- mm_core_init()        mem_map, buddy allocator, then slab (kmem_cache_init)
        |-- trap_init(), init_IRQ(), time_init(), softirq_init()
        `-- rest_init() -> kernel threads -> init process
```

Memory layout (Sv39, matching upstream constants in `arch/riscv/include/asm/page.h` and `pgtable.h`):

```text
0xffffffff80000000  KERNEL_LINK_ADDR  kernel image (.text .rodata .data .bss), va_kernel_pa_offset
0xffffffd600000000  PAGE_OFFSET_L3    linear map of all RAM, va_pa_offset, __va()/__pa()
        ...         VMALLOC / FIXMAP  introduced only when a milestone needs them
0x0000003fffffffff  top of user half
```

## 6. Milestones

Each milestone lists: upstream references, files the learner writes, acceptance checks, and the interview questions it prepares. Status lives in the tracker.

### K0. Build, Toolchain and Linker Contract

- **Upstream:** `linux/Makefile` (`-fno-PIE`, `-fno-strict-aliasing`), `arch/riscv/Makefile` (`-mcmodel=medany`, `-mno-save-restore`, `-fno-asynchronous-unwind-tables`, `-mstrict-align`), `arch/riscv/kernel/vmlinux.lds.S`, `include/asm-generic/vmlinux.lds.h` (`RW_DATA`, `BSS_SECTION`), `arch/riscv/boot/Makefile` (`Image` via objcopy).
- **Learner writes:** `Makefile` with `-MMD -MP` dependency tracking and `Image`, `System.map`, `qemu`, `qemu-gdb`, `gdb` and `check` targets (`qemu-fw` arrives with the firmware in K1); `.gitattributes` (LF) and `.gitignore`; `arch/riscv/kernel/vmlinux.lds.S` with VMA at `KERNEL_LINK_ADDR`, LMA via `AT()`, explicit `.srodata*`, `.sdata*`, `.sbss*`/`.scommon`, `COMMON`, an empty-checked `.rela.dyn`, non-loaded debug/`.comment`/`.riscv.attributes` sections as upstream, `/DISCARD/` only for `.eh_frame` and `.note.GNU-stack`; `head.S` with the upstream Image header, interrupt masking, BSS clear, boot arguments saved after the clear, boot stack and `tail start_kernel`; a temporary `init/main.c` layout probe using a polled UART (removed in K1); `scripts/check-vmlinux.sh` and `scripts/gdb-early.gdb`.
- **Accept:** link succeeds with `--orphan-handling=error`; `make check` passes (first `LOAD` VirtAddr `0xffffffff80000000`/PhysAddr `0x80200000`, every `.bss`/`.sbss` symbol inside `[__bss_start, __bss_stop)`, no GOT/PLT/rela/eh_frame); QEMU prints `K0 layout: PASS` under OpenSBI; GDB stops at physical `0x80200000` (Path B after K1); the `.sbss` negative test fails at link time.
- **Interview:** VMA versus LMA, PIC/PIE and the GOT, RISC-V code models, small-data sections, why kernels disable PIE and unwind tables.

### K1. Firmware and Boot Contract (Dual Boot)

Taught in [K1a firmware](../notes/kernel/K01a_firmware_and_boot_contract.md), then K1b in three small lessons, each adding one layer and ending with something runnable: [K1b-1 talking to the firmware](../notes/kernel/K01b1_talk_to_firmware_sbi_ecall.md), K1b-2 printk and early console, K1b-3 SBI discovery, power off and tests.

- **Upstream / references:** xv6 `kernel/start.c`; OpenSBI v1.3 `firmware/fw_base.S`, `fw_dynamic.S`, `lib/sbi/sbi_init.c` (online, not in the workspace); QEMU 8.2.2 `hw/riscv/boot.c` (reset ROM, `fw_dynamic_info`, kernel placement); RISC-V SBI v2.0 (BASE, legacy, SRST, DBCN); Linux `arch/riscv/kernel/sbi_ecall.c`, `sbi.c`, `reset.c`, `drivers/tty/serial/earlycon-riscv-sbi.c`, `head.S` early `stvec` park.
- **K1a, learner writes (`firmware/`, separate image, BSD-2-Clause):** `fw_base.S` (lottery, per-hart stacks, M-mode trap vector with `mscratch`), `fw.lds.S` (`0x80000000`, `ASSERT` within the PMP region), `fw_main.c` (PMP NAPOT entries, `medeleg = 0xb1ff` with S-mode `ecall` kept in M-mode, `mideleg`, `mcounteren`, `menvcfg.STCE` probe plus `stimecmp = -1`, `fw_dynamic_info` check, `mret`), `sbi_trap.c`, `sbi_ecall.c` (BASE, DBCN with buffer validation, SRST via `sifive_test`, legacy putchar/getchar/shutdown), `sbi_console.c`, `uart8250.c` and seven headers.
- **K1b, learner writes (kernel), in three lessons:**
  - **K1b-1:** `include/linux/types.h`, `asm/sbi.h`, `sbi_ecall.c`, `sbi.c` (`sbi_console_putchar`); the K0 report prints through the firmware; `make qemu-fw`.
  - **K1b-2:** `lib/string.c`, `lib/vsprintf.c`, `printk.c` with log-buffer replay, `earlycon-riscv-sbi.c` (DBCN or legacy), the `head.S` early `stvec` park, `make check` 5 (no stored absolute addresses until K3).
  - **K1b-3:** version-gated `sbi_init()`, `reboot.c`/`reset.c` power off, `setup_arch()`, `lib/test_printf.c`, `make test`/`make test-fw`.
- **Accept:** the identical `Image` passes `make test` (OpenSBI, legacy console, SBI v1.0) and `make test-fw` (my-sbi, DBCN, SBI v2.0) with QEMU exit status 0; firmware read-backs match (`pmpcfg0 0x1f1818`, `medeleg 0xb1ff`, `mideleg 0x1666`); `-smp 4` passes with a non-zero boot hart; `make check` 6/6.
- **Moved out of K1:** SBI TIME (fallback timer) to K6; SBI HSM/IPI/RFENCE to K8; DTB `/reserved-memory` for my-sbi decided in K4.
- **Interview:** boot chain BootROM -> SPL -> firmware -> bootloader -> kernel; M/S/U privilege; PMP; trap delegation and WARL read-back; SBI calling convention and discovery; early `printk` replay; stored versus computed addresses before the MMU; SBI versus Arm PSCI/TF-A; why firmware is a separate image.

### K2. Traps and Fatal Diagnostics (MMU Still Off)

- **Upstream:** `arch/riscv/kernel/entry.S` (`handle_exception`, `ret_from_exception`), `arch/riscv/kernel/traps.c` (`do_trap_*`, exception vector table), `asm/ptrace.h` (`struct pt_regs`), `die()`/`show_regs()`.
- **Learner writes:** `struct pt_regs`, `entry.S` save/restore, cause-indexed dispatch table, `do_trap_break` (2/4-byte instruction length), `do_trap_insn_illegal`, `do_page_fault` stub, `die()` printing `scause`, `sepc`, `stval`, `ra`, `sp`; `panic()`.
- **Accept:** `ebreak` returns to the next instruction; an illegal instruction and a load access fault each print a correct register dump and halt with a non-zero QEMU exit.
- **Interview:** synchronous versus asynchronous traps, what hardware saves versus software, direct versus vectored `stvec`, nested traps, comparison with Arm64 `VBAR_EL1`/`ESR_EL1` and Cortex-M hardware stacking.

### K3. Early MMU and Virtual Entry

- **Upstream:** `arch/riscv/mm/init.c` (`setup_vm`, `early_pg_dir`, `early_pmd`, `trampoline_pg_dir`, `create_pgd_mapping`, `create_pmd_mapping`, `kernel_map`), `arch/riscv/kernel/head.S` (`relocate_enable_mmu`), `asm/page.h`, `asm/pgtable-bits.h`, `asm/pgtable-64.h`.
- **Learner writes:** `asm/page.h` (`struct kernel_mapping`, `__pa_symbol`, `__va`/`__pa` placeholders), `asm/pgtable-bits.h`, `asm/pgtable.h`, `asm/csr.h`; `setup_vm(dtb_pa)` using only static page-aligned arrays and 2 MB leaf PMDs for the kernel and the DTB; `relocate_enable_mmu` with the trampoline `stvec` redirect; `head.S` calls both before `start_kernel`.
- **Accept:** `start_kernel` runs at `0xffffffff8020xxxx`; `sp`, `stvec` and static data pointers are virtual; no allocator or linked list is touched while `satp = 0`; DBCN passes `__pa(bytes)`; `make check` 5 (stored-address audit) is removed and the printf self-test moves to KUnit.
- **Deviation box required if:** any 4 KB mapping or allocator is used before the MMU is on.
- **Interview:** Sv39 walk, PTE bits, 2 MB megapages, why a trampoline instead of an identity map, `sfence.vma`, kernel versus linear mapping, Arm64 `TTBR0/TTBR1` comparison.

### K4. `setup_arch`: Device Tree, memblock and Final Page Tables

- **Upstream:** `arch/riscv/kernel/setup.c` (`setup_arch`, `parse_dtb`), `drivers/of/fdt.c`, `mm/memblock.c`, `arch/riscv/mm/init.c` (`setup_bootmem`, `setup_vm_final`, `create_linear_mapping_page_table`).
- **Learner writes:** FDT walker (header, tokens, `/memory`, `/chosen`, `/cpus/timebase-frequency`, UART node), `memblock` (add, reserve, `memblock_phys_alloc`), reserve kernel/DTB/firmware, `setup_vm_final()` building `swapper_pg_dir` with the linear map at `PAGE_OFFSET` using memblock pages.
- **Accept:** RAM size comes from the DTB, not a constant; `__va(pa)`/`__pa(va)` round-trip for any RAM address; a software page walk matches hardware translation; a memblock dump lists reserved regions.
- **Interview:** device tree role boundaries, early allocator versus page allocator, reserving firmware memory, linear map design.

### K5. `mm_core_init`: Buddy Allocator and SLUB

- **Upstream:** `include/linux/mm_types.h` (`struct page`), `mm/page_alloc.c` (`free_area`, `__rmqueue_smallest`, `expand`, `__free_one_page`), `mm/mm_init.c`, `include/linux/slab.h`, `mm/slub.c` (`kmem_cache`, `freelist`, `get_freepointer`/`set_freepointer`, partial lists), `mm/slab_common.c` (kmalloc size classes).
- **Learner writes:** `mem_map`, memblock-to-buddy handover, buddy `alloc_pages(order)`/`__free_pages()` with split and merge for orders 0-10, `kmem_cache_create`, `kmem_cache_alloc`/`free`, `kmalloc`/`kfree` from 8 B with large-allocation fallback to pages.
- **Accept:** split/merge tests restore the original free counts; `kmalloc(8)` slot reuse; `kfree` finds its cache through `struct page`; allocator statistics print correctly.
- **Interview:** external versus internal fragmentation, buddy math, slab/SLUB design, per-CPU caches (concept), why `kfree` needs no size.

### K6. Time, Timer Interrupt and Softirq Bottom Half

- **Upstream:** `arch/riscv/kernel/time.c` (`riscv_timebase`, `time_init`), `drivers/clocksource/timer-riscv.c` (Sstc `stimecmp` path, SBI fallback), `kernel/time/` (jiffies, `timer_list`), `kernel/softirq.c` (`raise_softirq`, `irq_exit`, `__do_softirq`).
- **Learner writes:** clocksource and clockevent, `riscv_timer_interrupt` top half, `TIMER_SOFTIRQ` bottom half, `timer_setup`/`mod_timer`/`del_timer`, `irq_enter`/`irq_exit`, `local_irq_save`/`restore`; SBI TIME in my-sbi (M-mode `mtimecmp` plus `mip.STIP` forwarding) as the non-Sstc fallback.
- **Accept:** jiffies advance at `HZ`; a `timer_list` callback fires in softirq context at the requested jiffy; `del_timer` prevents a callback; works under both firmware paths (Sstc unlocked by OpenSBI and by the learner firmware).
- **Interview:** timebase versus `HZ`, clocksource versus clockevent, tickless idle (concept), top half versus softirq, why timer callbacks must not sleep.

### K7. Interrupt Subsystem, PLIC and Interrupt-Driven UART

- **Upstream:** `kernel/irq/` (`irq_desc`, `irq_chip`, `irq_domain`, `generic_handle_irq`, `request_irq`), `drivers/irqchip/irq-riscv-intc.c`, `drivers/irqchip/irq-sifive-plic.c`, `drivers/tty/serial/8250/`, `kernel/softirq.c` (tasklets), `kernel/workqueue.c` (concept introduced here, worker thread in K8).
- **Learner writes:** INTC and PLIC `irq_chip`, hwirq-to-virq mapping, `request_irq`, UART RX top half into a ring buffer, tasklet bottom half, claim/complete ordering.
- **Accept:** typed characters arrive through the interrupt path; no repeated or lost interrupt after completion; spurious-IRQ counter stays zero.
- **Interview:** interrupt controller layering, level versus edge, claim/complete, threaded IRQ versus softirq versus tasklet versus workqueue, Arm GICv3 comparison.

### K8. Tasks, Context Switch, Scheduling and Locking

- **Upstream:** `include/linux/sched.h` (`task_struct`, `thread_info`), `arch/riscv/kernel/entry.S` (`__switch_to`), `arch/riscv/include/asm/current.h` (`tp`), `kernel/sched/core.c` (`schedule`, `__schedule`, preemption counters), `include/linux/spinlock.h`, `kernel/locking/mutex.c`, `include/linux/wait.h`, `kernel/kthread.c`, `kernel/workqueue.c`.
- **Learner writes:** `task_struct`, kernel stacks, `__switch_to`, `kthread_create`, round-robin scheduler with timer-driven preemption (`TIF_NEED_RESCHED`), `spinlock_t` (`amoswap`), `preempt_count`, wait queues, completions, mutex, a workqueue worker thread consuming K7 UART work; SBI HSM (`hart_start`) and IPI in my-sbi with the matching kernel callers, so parked secondary harts can be started.
- **Accept:** two kthreads interleave under preemption; a waiter sleeps and is woken by the UART bottom half; spinlock and mutex tests pass; lock held in IRQ context uses the `irqsave` variant.
- **Interview:** context switch register set, preemption points, spinlock versus mutex rules, sleeping in atomic context, priority inversion, wait/wakeup races.

### K9. User Space and Shell (after Phase 3; optional extension)

- **Upstream:** `mm_struct`/VMA, `arch/riscv/kernel/entry.S` (`ret_from_exception` to U-mode), syscall table, `copy_to_user`/`copy_from_user` with `sstatus.SUM`, `fs/binfmt_elf.c` (concept).
- **Learner writes:** per-process page tables sharing the kernel half, U-mode entry, `read`/`write`/`exit`/`getpid`/`sched_yield`/`brk`, embedded initramfs ELF loader, `init` and a minimal shell.
- **Accept:** shell runs commands through syscalls; a bad user pointer returns `-EFAULT` instead of crashing the kernel.

### K10. Arm64 Transfer (taught alongside Phase 3)

- **Upstream:** `arch/arm64/kernel/head.S`, `arch/arm64/mm/mmu.c`, `drivers/irqchip/irq-gic-v3.c`, `drivers/clocksource/arm_arch_timer.c`.
- **Output:** a comparison note mapping every K1-K8 mechanism to Arm64 (EL3/EL2/EL1, TF-A/PSCI, `TTBR0/1`, `VBAR_EL1`, GICv3, generic timer). No new kernel code required.

## 7. Definition of Done (every milestone)

1. Lesson note published with verification status and corrections section.
2. Learner code builds without warnings using the K0 flags and passes `make check` (section audit).
3. Milestone self-tests print `PASS` under both firmware paths from K1 onward.
4. Evidence recorded in the tracker and log (learner-run versus tutor-run distinguished).
5. The milestone's interview questions are answered in the note's "Interview answers" section in 3-5 sentences each.

## 8. Next Lesson

**K1b-1: Talking to the Firmware.** Published as [K01b1_talk_to_firmware_sbi_ecall.md](../notes/kernel/K01b1_talk_to_firmware_sbi_ecall.md). K0 and K1a are done and committed on `my-linux` branch `v2` (`3fddd0d`). K1b-2 and K1b-3 are written after K1b-1 runs on the learner's tree.
