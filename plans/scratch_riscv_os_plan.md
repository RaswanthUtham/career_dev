# Linux-Aligned RISC-V Teaching Kernel

Updated 2026-10-07. This is a dependency-ordered **project guide**, not a second calendar. The [curriculum](five_month_development_roadmap.md) owns weekly hours and scheduling; the [tracker](training_tracker.md) owns current status; the [log](learning_log.md) preserves prior evidence. The previous 35-day schedule and its Day 1-8 QEMU results belong to the prior scratch iteration, not the new build.

## Ownership And Boundaries

- The learner creates `C:\workspace\career_ws\my-linux`, its folders, and every active source file after understanding the relevant lesson. Preserve the old `riscv-kernel` source under an excluded `my-linux/reference/` for comparison. Do not silently move, delete, build or overwrite learner code; first inventory source and generated output. Never include `reference/` in new Makefile inputs.
- Use the independent `xv6-riscv` fork for concrete OS mechanism comparisons, not as a source to copy. Use the checked-out `linux` tree for naming, boot ABI, subsystem ownership, documentation and code review. The teaching kernel is a reduced original implementation, not Linux and not an API-compatible reimplementation.
- **Hardware Reset & Firmware Duality**: In QEMU Virt, the hardware reset vector is fixed in QEMU's internal Mask ROM (MROM) at `0x1000`. The reset ROM places the DTB address in `a1` and jumps directly to `0x80000000` in Machine Mode. To achieve complete end-to-end understanding:
  1. *Self-Contained Firmware / Mini-SBI (`-bios none`)*: The kernel begins at `0x80000000` in M-mode, executes an early firmware setup stub (`head.S` / `firmware.c`) that configures PMP, delegates traps (`medeleg`, `mideleg`), handles SBI runtime calls, and drops to S-mode via `mret`.
  2. *Production OpenSBI Boot (`-bios default`)*: OpenSBI runs at `0x80000000` in M-mode, configures the hardware, and drops to our kernel entry in S-mode at `0x80200000` with `a0 = hartid, a1 = dtb_pa`.
  Mastering both ensures zero black boxes from bare silicon to supervisor mode.
- A milestone begins with what/why/failure, a short actual-source walkthrough and concrete trace. The learner then types from understanding. Review submitted code before suggesting edits; build and run it only on request. Preserve history and distinguish student report, inspected source and actual QEMU evidence. Quizzes remain paused until explicitly resumed.

## Mainline Map, Built Only When Needed

| Teaching responsibility | Upstream reference | Active teaching directory when reached |
| --- | --- | --- |
| Entry, linker, early boot, privilege and MMU transition | `linux/arch/riscv/kernel/head.S`, `vmlinux.lds.S`; `linux/arch/riscv/mm/init.c` | `arch/riscv/kernel/`, later `arch/riscv/mm/` |
| Generic initialization and early output | `linux/init/main.c`, `linux/kernel/printk/` | `init/`, `kernel/printk/` |
| ABI, CSR, traps and IRQ entry | `linux/arch/riscv/include/asm/`, `linux/arch/riscv/kernel/entry.S`, `irq.c` | `arch/riscv/include/asm/`, `arch/riscv/kernel/` |
| Firmware description and timebase | `linux/arch/riscv/kernel/setup.c`, `time.c`; `linux/arch/riscv/boot/dts/` | `arch/riscv/kernel/`, later `arch/riscv/boot/dts/` |
| Early memory, page tables, task and syscall core | `linux/mm/`, `linux/arch/riscv/mm/`, `linux/kernel/sched/`, `linux/kernel/` | `mm/`, `arch/riscv/mm/`, `kernel/` |
| Timer hardware, IRQ controller, device and deferred work | `linux/drivers/clocksource/timer-riscv.c`, `linux/drivers/irqchip/irq-sifive-plic.c`, `linux/kernel/softirq.c` | `drivers/clocksource/`, `drivers/irqchip/`, later `drivers/tty/` and `kernel/` |
| Build/configuration and hardware description | `linux/Makefile`, `linux/arch/riscv/Makefile`, `linux/kernel/Kconfig.hz`, DTS and bindings | `Makefile`, `arch/riscv/Makefile`, later `Kconfig` and DTS |

Matching paths express **ownership**, not permission to invent matching Linux interfaces. Introduce headers, Kconfig symbols, generic frameworks and directories only when a runnable step needs them; document each simplification relative to upstream. No complete filesystem or interactive shell is required before moving to real Linux driver work.

## Milestones And Evidence

1. **Protect history and orient:** The learner inventories old source versus build output, moves the old implementation to excluded `reference/` and creates the new root. Compare xv6 `kernel/start.c`, `entry.S` and trap flow with Linux's S-mode boot. Evidence: source inventory, no legacy object in the new build inputs, and an annotated reset-to-first-C map naming mode, stack and register owners. Do not claim a build before one runs.
2. **Firmware entry and smallest boot:** Teach OpenSBI versus a bare-metal `-bios none` image, S-mode entry ABI, image/linker placement, stack and BSS before first console output. The learner creates only the linker script, boot entry, build rules and first C entry it needs. Check firmware version and QEMU boot contract, then when requested verify the S-mode entry registers and first output. A temporary fixed-address early console must be explicitly labelled a bootstrap assumption.
3. **Device description and discovery:** Clarify production role boundaries: in semiconductor/BSP roles (Qualcomm, NVIDIA, AMD, TI), engineers do *not* author low-level binary FDT byte parsers from scratch (that belongs to generic kernel infrastructure like `drivers/of/fdt.c` in Linux, or compile-time codegen in Zephyr). However, engineers *must* master authoring DTS/DTSI files, official YAML Devicetree bindings (`Documentation/devicetree/bindings/`), and driver probe parsing (`of_match_table`, `of_property_read_*`, `irq_of_parse_and_map`). In `my-linux`, inspect QEMU's live generated DTB (`qemu-system-riscv64 -machine virt,dumpdtb=...` and `dtc -I dtb -O dts`), understand the 40-byte FDT header, Big-Endian tokens (`be32_to_cpu`), `/memory`, `/cpus/timebase-frequency`, and UART nodes. Implement a lightweight FDT header and property scan to demystify the `a1` firmware handoff without bogging down in low-value parser plumbing. Production Devicetree authoring is anchored in Capstone 1 (Zephyr overlays & bindings, W09) and Capstone 2 (Linux IIO client driver with YAML bindings, W14-W16).
4. **Memory and trap boundary (`mm/`, `arch/riscv/mm/`):** Teach early physical allocation (`struct page mem_map[]`, `free_page_list`, `alloc_page()`, `free_page()`, `get_zeroed_page()`), Sv39 page tables and permissions, `satp`/TLB synchronization, S-mode trap entry/return and system-call `sepc` handling. Follow upstream Linux's **Two-Stage Identity + High-Half Linear Mapping (`PAGE_OFFSET = 0xFFFFFFC000000000UL`)**:
   - **Stage 1 (Early Boot, `satp = 0`)**: Global `va_pa_offset` starts at `0UL` (matching `linux/arch/riscv/mm/init.c`). `vm_init()` populates `swapper_pg_dir` with **both** the Low-Half Identity Map (`Entries 0..255`: `VA 0x80000000 -> PA 0x80000000`, plus MMIO UART/PLIC, so the CPU does not fault on the instruction immediately after `csrw satp`) **and** the High-Half Kernel Linear Map (`Entries 256..511`: `VA 0xFFFFFFC080000000 -> PA 0x80000000`).
   - **Stage 2 (MMU Activation in `enable_mmu()`)**: Immediately after writing `satp` and executing `sfence.vma`, update `va_pa_offset = PAGE_OFFSET` (`0xFFFFFFC000000000UL`). From that point onward, `__va(pa)` returns High-Half virtual addresses (`0xFFFFFFC08xxxxxxx`) and `__pa(va)` subtracts `va_pa_offset`, while `walk_va_to_pa()` translates arbitrary dynamic mappings (`vmalloc` / `ioremap` / user pages).
   - **Stage 3 (Sub-page SLUB `kmalloc` & Upstream `head.S` Pre-`start_kernel` High-Half Relocation)**: Implement an upstream-aligned two-level SLUB object allocator (`mm/slab.c`: 9 power-of-two caches `8B..2048B` using `set_freepointer`/`get_freepointer` inside objects and `struct list_head partial` on `struct page`), and invoke `setup_vm` + `relocate_enable_mmu` directly from `_start` in `arch/riscv/kernel/head.S` **before** `tail start_kernel` (matching `linux/arch/riscv/kernel/head.S`). This ensures `start_kernel(hartid, dtb_va)` and all C stack frames enter at High-Half `PC = 0xFFFFFFC08020xxxx` and `sp = 0xFFFFFFC08020xxxx` from Line 1, leaving the entire Low Half (`Entries 0..255`) clean for user space while every user `pgd` shares `Entries 256..511` of `swapper_pg_dir`.
   Verify identity + High-Half linear mapping, `va_pa_offset` transition, `walk_va_to_pa()`, one controlled page fault and one returning trap before adding processes.
5. **Time, Hardware `stimecmp` (`Sstc`), and Softirq Bottom-Half Pipeline (`arch/riscv/kernel/time.c`, `drivers/clocksource/timer-riscv.c`, `kernel/softirq.c`):**
   - **Dual-Boot Bare-Metal (`-bios none` at `0x80000000`) & OpenSBI (`-bios default` at `0x80200000`)**: Support `#ifdef CONFIG_RISCV_M_MODE` in `vmlinux.lds.S` and `head.S` (matching upstream `linux/arch/riscv/kernel/head.S`). When built with `make qemu-baremetal` (`-bios none`), `_start` boots at `0x80000000` in Machine Mode, configures PMP, delegates traps (`medeleg`/`mideleg`), unlocks `time` (`mcounteren |= 2`) and the modern **`Sstc` extension (`menvcfg |= ENVCFG_STCE`, `1ULL << 63`)** so S-mode can program hardware **`stimecmp` (`CSR 0x14D`)** directly without any runtime M-mode `ecall`, and drops to `.Ls_mode_entry` via `mret`. When built with `make qemu` (`-bios default`), OpenSBI performs M-mode setup and enters `_start` in S-mode at `0x80200000`.
   - **Clock Math (`timebase-frequency` vs. `HZ`)**: Teach the clock math to an imagined 12-year-old. `riscv_timebase` is discovered dynamically from the Device Tree `/cpus` `"timebase-frequency"` property (`10,000,000` Hz on QEMU Virt); `HZ` (`100`) is a **chosen Linux configuration** (`100,000` counter ticks per `10 ms` jiffy).
   - **Modern `stimecmp` (`CSR 0x14D`) Clocksource & Clockevent**: Follow upstream `drivers/clocksource/timer-riscv.c` by separating **`struct clocksource`** (`rdtime`) from **`struct clock_event_device`** (writing deadlines directly to hardware `CSR_STIMECMP = 0x14D` in 1 instruction).
   - **Interrupt Top-Half vs. Bottom-Half (`kernel/softirq.c`)**: Introduce Linux's Top-Half / Bottom-Half split on our very first interrupt:
     1. *Top Half (`riscv_timer_interrupt` in Hard IRQ context)*: Increments `jiffies_64++`, rearms `stimecmp` (`rdtime() + riscv_timebase / HZ`), and calls `raise_softirq(TIMER_SOFTIRQ)`.
     2. *Bottom Half (`kernel/softirq.c`: `open_softirq`, `raise_softirq`, `irq_exit` -> `__do_softirq`)*: Runs at the end of `do_trap()`, invokes `run_timer_softirq()`, and fires expired dynamic `struct timer_list` callbacks (`timer_setup`, `mod_timer`, `del_timer`).
6. **Generic IRQ Layer, PLIC, and Top-Half vs. Bottom-Half Deferred Execution (`kernel/irq/`, `drivers/irqchip/irq-sifive-plic.c`, `kernel/softirq.c`, `kernel/workqueue.c`):** Discover the interrupt controller from DT and build the upstream Linux interrupt architecture:
   - **Generic IRQ Layer (`kernel/irq/`)**: `struct irq_desc`, `struct irq_chip` (`.irq_enable`, `.irq_disable`, `.irq_eoi`), `request_irq()`, and `generic_handle_irq()`.
   - **PLIC Driver (`drivers/irqchip/irq-sifive-plic.c`)**: Implement `struct irq_chip` for SiFive PLIC, claim pending hardware IRQ via `PLIC_CLAIM`, dispatch to the registered handler, and complete via `PLIC_COMPLETE`.
   - **Top-Half vs. Bottom-Half Split**:
     1. *Top Half (Hard IRQ Context, `in_irq()`)*: Runs with interrupts disabled; must never sleep. Reads incoming bytes from UART RX FIFO into a `kfifo` ring buffer, schedules a bottom half, and returns `IRQ_HANDLED`.
     2. *Bottom Half — Softirq (`kernel/softirq.c`)*: Statically registered `softirq_vec` (`TIMER_SOFTIRQ`, `RX_SOFTIRQ`), raised via `raise_softirq()` and executed in `__do_softirq()` on `irq_exit()` (atomic context, cannot sleep).
     3. *Bottom Half — Workqueue (`kernel/workqueue.c`)*: `struct work_struct`, `INIT_WORK()`, `schedule_work()`, executed by a kernel worker thread (`kworker`) in **process context** (can sleep and acquire mutexes).
   Verify a UART receive event, Top-Half capture, Bottom-Half processing, correct PLIC EOI acknowledgement, and safe teardown.
7. **Synchronization, Tasks, User Isolation, VFS, System Calls & Interactive Shell (`kernel/locking/`, `kernel/sched/`, `fs/`, `usr/`):** Build the multi-tasking and user-space subsystems using upstream Linux patterns:
   - **Synchronization Primitives**: `spinlock_t` (`spin_lock_irqsave` / `spin_unlock_irqrestore` for data shared between Hard IRQ Top-Half and process context), `struct mutex` (sleeping lock for process context), and `wait_queue_head_t` (`wait_event_interruptible` / `wake_up_interruptible` so blocking `sys_read` sleeps until the UART bottom half wakes it).
   - **Scheduler & Tasks**: `struct task_struct`, `struct thread_struct`, `current` pointer held in the RISC-V `tp` register (`arch/riscv/include/asm/current.h`), `switch_to(prev, next, last)` in `entry.S`, kernel threads (`kthread_create`), and preemptive scheduling (`TIF_NEED_RESCHED` on trap exit).
   - **User Address Space & VFS**: Per-process `struct mm_struct` and `struct vm_area_struct` (VMA) mapping user code/heap/stack in the Low Half (`PTE_U = 1`) while sharing the High-Half kernel PGD entries (`Entries 256..511` of `swapper_pg_dir`); safe `copy_from_user()` / `copy_to_user()` gating `sstatus.SUM`; VFS `struct file`, `struct inode`, and `struct file_operations` (`.read`, `.write`, `.open`, `.release`).
   - **System Calls & Interactive Shell**: `sys_call_table` handling U-mode `ecall` (`sys_read`, `sys_write`, `sys_clone`/`sys_fork`, `sys_execve`, `sys_wait4`, `sys_brk`, `sys_exit`), standalone user C runtime (`usr/lib/crt0.S`), and interactive command shell (`usr/bin/sh.c`).
   Verify two runnable tasks, one `wait_queue` blocking/wakeup path, a user-mode system call round-trip, and interactive shell execution.
8. **Transfer to real Linux:** Keep the [curriculum's](five_month_development_roadmap.md) Zephyr/embedded Linux and IIO/DMA capstones. Compare every teaching simplification with real kernel Kconfig/kbuild, DT bindings, device/driver matching, IRQ lifecycle and locking; then implement focused changes in an isolated QEMU guest or learning board as appropriate. Log explanation, trace, implementation, debug evidence and limitations separately. The INR 70-90 LPA CTC target is an aspiration, not a kernel-build acceptance test.

## Next Lesson

Milestone 2 (S-mode boot and early console) and Milestone 3 (Device Tree discovery and token traversal) are verified in QEMU via OpenSBI. Proceed to **Milestone 4: Memory and Trap Boundary** (physical page allocator, Sv39 page table construction, activating `satp`, trap vector `stvec`, and saving `struct pt_regs`). Return to timer calculations when milestone 5 is reached.

---

# 35-Day Scratch RISC-V 64-bit OS Implementation Plan (Linux Kernel Architecture)

Updated 2026-10-07. This plan outlines the day-by-day, instruction-by-instruction construction of a complete multi-tasking operating system with an interactive user shell from bare metal on RV64, hosted in `c:\workspace\career_ws\my-linux`.

## Architectural Target & Environment
- **Architecture**: RISC-V 64-bit (`rv64gc`).
- **Target Location**: `c:\workspace\career_ws\my-linux`.
- **Platform**: QEMU Virt machine (`qemu-system-riscv64 -machine virt -cpu rv64 -m 128M -bios none -nographic`).
- **Build Environment**: Podman container (`xv6-lab`) with `riscv64-unknown-elf-gcc`, `ld`, and `make`.
- **Architectural & Naming Standard**: Strict Linux Kernel Architecture. All directory structures, file names, exported symbols, data structures (`struct task_struct`, `struct pt_regs`, `struct page`), and subsystem interfaces mirror the upstream Linux kernel (`v6.x`). This gives the student immediate muscle memory and exact terminology required for Staff-level Linux Kernel / Systems interviews (Qualcomm, AMD, NVIDIA, TI).
- **Reference Model**: `xv6-riscv` is used generically as a conceptual reference for OS mechanisms, not as a rigid implementation template.
- **Adaptive Superiority Principle**: Across the entire kernel—boot, memory management, scheduling, traps, drivers, and filesystem—whenever our approach is superior, cleaner, or more intuitive than the implementation in xv6 (such as maximizing standard C logic over processor-specific assembly or adopting cleaner data structures), we adopt our superior approach.
- **Pedagogical Approach**: 1 atomic topic per day. What-Why-How first-principles methodology with concrete analogies, hardware dilemma breakdown, and learner-authored code.

## Linux-Aligned Directory Hierarchy

```text
my-linux/
├── Makefile                                # Top-level kernel build orchestrator
├── arch/
│   └── riscv/
│       ├── include/
│       │   └── asm/
│       │       ├── csr.h                   # Hardware CSR access inline assembly primitives
│       │       ├── page.h                  # Page sizes, shift constants, PFN conversions
│       │       ├── pgtable.h               # Sv39 3-level page table descriptors and PTE bits
│       │       └── ptrace.h                # struct pt_regs (CPU register frame on trap)
│       ├── kernel/
│       │   ├── entry.S                     # Low-level trap entry/exit vector
│       │   ├── head.S                      # Bare-metal reset entry point (_start)
│       │   ├── process.c                   # Context switch primitives (switch_to)
│       │   ├── sys_riscv.c                 # Architecture-specific syscall dispatcher
│       │   ├── time.c                      # Hardware timer clocksource and tick irq
│       │   ├── traps.c                     # Supervisor exception & interrupt demux
│       │   └── vmlinux.lds.S               # Linker script with QEMU Virt memory layout
│       └── mm/
│           ├── fault.c                     # Page walk logic and MMU page-fault handler
│           └── init.c                      # Kernel early address space & identity maps
├── drivers/
│   ├── block/
│   │   └── ramdisk.c                       # In-memory RAM block device driver
│   ├── irqchip/
│   │   └── irq-sifive-plic.c               # Platform-Level Interrupt Controller driver
│   └── tty/
│       └── serial/
│           └── 8250_early.c                # Early NS16550 UART polled console driver
├── fs/
│   ├── exec.c                              # Binary loader and user stack setup (do_execve)
│   ├── minfs.c                             # Minimal inode-based disk filesystem
│   └── vfs.c                               # Virtual Filesystem Switch (struct file, open/read/write)
├── include/
│   └── linux/
│       ├── fs.h                            # VFS struct file, struct inode, struct super_block
│       ├── init.h                          # __init and early boot markers
│       ├── mm.h                            # struct page and physical memory interfaces
│       ├── printk.h                        # Early and formatted printk prototypes
│       ├── sched.h                         # struct task_struct, task states, scheduler headers
│       └── types.h                         # Fixed-width uint64_t, size_t, pid_t definitions
├── init/
│   └── main.c                              # Core kernel entry point (start_kernel)
├── kernel/
│   ├── exit.c                              # Process termination and zombie reaping (do_exit)
│   ├── fork.c                              # Process creation and address space cloning (do_fork)
│   ├── printk/
│   │   └── printk.c                        # Formatted string engine (printk / early_printk)
│   ├── sched/
│   │   └── core.c                          # Runqueue, scheduler loop, waitqueues (schedule, sleep)
│   ├── sys.c                               # Generic system call implementations
│   └── time/
│       └── timer.c                         # Software timers and jiffies accounting
├── mm/
│   ├── bootmem.c                           # Early boot bump allocator
│   └── page_alloc.c                        # 4KB physical page frame allocator (alloc_pages)
└── usr/
    ├── bin/
    │   └── sh.c                            # Interactive user-space command shell
    └── lib/
        └── crt0.S                          # User C runtime entry stub (_start)
```

## Architecture Progression & Production OS Comparison
The scratch kernel builds OS fundamentals from raw hardware up. As the roadmap progresses into later phases, the tutor will explicitly contrast the scratch kernel's architectural patterns against production operating systems:
- **Firmware Abstraction**: Bare-metal reset (`-bios none`) vs. standard firmware interfaces (OpenSBI on RISC-V, ARM Trusted Firmware / PSCI on ARM64).
- **Multi-Core Boot Sequencing**: Simultaneous bare-metal boot vs. the production Leader-Follower (BSP/AP) model where secondary cores are awakened via Inter-Processor Interrupts (IPIs) or firmware calls (`sbi_hart_start`, `CPU_ON`).
- **Hardware Description & Device Models**: Hardcoded MMIO addresses vs. Devicetree bindings (`.dts`/`.dtsi`), unified driver models (`struct device` in Linux, `DEVICE_DT_DEFINE` in Zephyr), and runtime bus registration.

---

## The 35-Day Curriculum (Linux-Aligned)

### Phase 1: Bare-Metal Boot & Early Console (Days 1–5) — [COMPLETED & VERIFIED]
- **Day 1: Linker Script Architecture (`arch/riscv/kernel/vmlinux.lds.S`)** — [Verified]
  - QEMU Virt DRAM memory layout (`0x80000000`), 4KB page alignment (`ALIGN(4096)`), segment boundaries (`.text`, `.rodata`, `.data`, `.bss`), and canonical Linux linker boundary symbols (`_start`, `_stext`, `_etext`, `__bss_start`, `__bss_stop`, `_end`).
- **Day 2: Assembly Boot Entry (`arch/riscv/kernel/head.S`)** — [Verified]
  - Bare-metal reset state machine in Machine Mode; hardware thread (hart) ID interrogation via `mhartid`; secondary core parking loop (`wfi`); early stack initialization (`boot_stack_top`); memory zeroing of BSS bounds; clean tail-call branch into `start_kernel()`.
- **Day 3: Early NS16550 Serial Console (`drivers/tty/serial/8250_early.c`)** — [Verified]
  - Memory-mapped I/O (MMIO) base address (`0x10000000`), Line Control Register (LCR), FIFO Control Register (FCR), Line Status Register (LSR), divisor latch registers (DLL/DLM), polled character transmission (`early_putc`).
- **Day 4: Kernel Formatted Print Engine (`kernel/printk/printk.c`)** — [Verified]
  - Self-contained string formatting engine without standard libc; `%d`, `%x`, `%p`, `%s`, `%c`; `early_printk()` and `printk()` implementation.
- **Day 5: Containerized Build System (`Makefile`) & First Boot Verification** — [Verified]
  - Compilation flags (`-nostdlib -fno-builtin -mcmodel=medany`), linking against `vmlinux.lds.S`, QEMU invocation via Podman, and verifying the Linux-style kernel boot banner and formatted tests.

### Phase 2: Privilege Modes, Traps & Hardware Timers (Days 6–10) — [IN PROGRESS]
- **Day 6: RISC-V CSR Access Primitives (`arch/riscv/include/asm/csr.h`)** — [Verified]
  - M-mode vs S-mode vs U-mode hardware boundaries; inline assembly primitives for atomic CSR operations (`csr_read`, `csr_write`, `csr_set`, `csr_clear`, `csr_swap`).
- **Day 7: Machine to Supervisor Mode Transition (`init/main.c`)** — [Verified]
  - Faking exception return via `mret`: configuring `mstatus.MPP = S`, setting `mepc = start_kernel_supervisor`, trap delegation (`medeleg`, `mideleg`), and Physical Memory Protection (PMP) initialization. Verified live in QEMU with clean poweroff.
- **Day 8: Supervisor Trap Vector & Register Frame (`arch/riscv/kernel/entry.S`, `traps.c`, `ptrace.h`)** — [Verified]
  - Hardware trap behavior; programming `stvec`; saving 31 general-purpose registers to `struct pt_regs` on the kernel stack; trap demuxing via `scause` in `do_trap()`. Verified live in QEMU via synchronous `ecall` trap and return.
- **Day 9: Hardware Timer Clocksource & Interrupts (`arch/riscv/kernel/time.c`, `kernel/time/timer.c`)** — [In Progress]
  - RISC-V Sstc extension (`stimecmp`) vs machine timer; handling timer interrupts; incrementing `jiffies` global tick counter; time quantum accounting.
- **Day 10: SiFive PLIC & Asynchronous UART RX (`drivers/irqchip/irq-sifive-plic.c`)**
  - Routing external hardware interrupts through the Platform-Level Interrupt Controller (PLIC); priority/threshold registers; interrupt completion claims; non-blocking UART RX ring buffer.

### Phase 3: Physical Memory Management & Sv39 Virtual Memory (Days 11–16)
- **Day 11: Early Boot Memory Bump Allocator (`mm/bootmem.c`)**
  - Tracking free memory from linker `_end` to DRAM ceiling (`0x88000000`); writing an early bump allocator (`bootmem_alloc`) for early page table structures.
- **Day 12: Page Frame Allocator (`mm/page_alloc.c`, `include/linux/mm.h`)**
  - 4096-byte chunking; `struct page` metadata array; intrusive singly-linked free list; thread-safe `alloc_pages()` and `free_pages()`.
- **Day 13: RISC-V Sv39 Paging Descriptors (`arch/riscv/include/asm/pgtable.h`, `page.h`)**
  - 3-level page table hierarchy (PGD $\rightarrow$ PMD $\rightarrow$ PTE); 39-bit virtual address bit-field extraction; PTE permission flags (`PTE_V`, `PTE_R`, `PTE_W`, `PTE_X`, `PTE_U`).
- **Day 14: Page Table Walker & Mapping Engine (`arch/riscv/mm/fault.c`)**
  - Allocating intermediate page directory levels dynamically; mapping arbitrary virtual pages to physical pages with explicit permissions (`map_page()`).
- **Day 15: Kernel Address Space & Identity Mappings (`arch/riscv/mm/init.c`)**
  - Constructing root kernel page directory (`swapper_pg_dir`); identity mapping UART MMIO, PLIC, kernel code/data, and physical DRAM.
- **Day 16: Activating the MMU (`satp` & `sfence.vma`)**
  - Programming root PGD physical address and MODE=8 (Sv39) into `satp`; executing TLB flush (`sfence.vma`); transitioning CPU into full virtual memory execution.

### Phase 4: Processes, Context Switching & Multi-Tasking (Days 17–22)
- **Day 17: Process Control Block (`include/linux/sched.h`)**
  - Defining `struct task_struct` (PID, state flags `TASK_RUNNING` / `TASK_INTERRUPTIBLE` / `TASK_ZOMBIE`, kernel stack pointer, user page directory, trapframe pointer).
- **Day 18: Callee-Saved Context Switch (`arch/riscv/kernel/process.c`, `swtch.S`)**
  - Saving current execution context (`ra`, `sp`, `s0`–`s11`) into `struct thread_struct`; stack pivoting to target `task_struct`; `switch_to(prev, next)` contract.
- **Day 19: Core Runqueue Scheduler (`kernel/sched/core.c`)**
  - Task runqueue; round-robin CPU scheduling loop (`schedule()`); scanning for `TASK_RUNNING` tasks; switching MMU contexts via `satp`.
- **Day 20: Wait Queues & Cooperative Sleeping (`kernel/sched/core.c`)**
  - Wait queues; transitioning tasks to `TASK_INTERRUPTIBLE`; calling `schedule()`; `wake_up()` primitives for I/O completion.
- **Day 21: Task Duplication (`kernel/fork.c`)**
  - `do_fork()` implementation; allocating child `task_struct`; duplicating user address space and page tables; configuring child return value (`a0 = 0`) and parent return value (`a0 = child_pid`).
- **Day 22: Task Termination & Reaping (`kernel/exit.c`)**
  - `do_exit()` state transition to `TASK_ZOMBIE`; reparenting orphan tasks; parent reclamation and zombie reaping in `sys_wait4()`.

### Phase 5: User Mode, System Calls & The Trampoline (Days 23–27)
- **Day 23: Trampoline Page Architecture (`arch/riscv/kernel/entry.S`)**
  - Mapping a shared trampoline page at virtual address `0x3ffffff000` in both kernel and user page directories; per-process trapframe mapping.
- **Day 24: Transitioning into User Space (`ret_to_user`)**
  - Restoring user registers from `struct pt_regs`; switching `satp` to user page directory; executing `sret` to drop to User Mode (U-mode).
- **Day 25: The System Call Gateway (`arch/riscv/kernel/sys_riscv.c`)**
  - Trapping from U-mode on `ecall`; decoding system call number from register `a7`; marshalling arguments in `a0`–`a5`; advancing `sepc += 4`.
- **Day 26: Fundamental System Calls (`kernel/sys.c`)**
  - Implementing `ksys_write` (console output), `ksys_read` (keyboard input), `ksys_getpid`, and `ksys_sched_yield`.
- **Day 27: Dynamic Heap Syscall (`kernel/sys.c`: `ksys_brk`)**
  - Tracking user heap break pointer; allocating physical pages on demand; expanding user page table mappings dynamically.

### Phase 6: Block Storage, Filesystem & User Shell (Days 28–35)
- **Day 28: In-Memory RAM-Disk Block Driver (`drivers/block/ramdisk.c`)**
  - Block I/O interface; sector read/write transactions backed by contiguous RAM buffer.
- **Day 29: Simple Inode Filesystem (`fs/minfs.c`, `include/linux/fs.h`)**
  - `struct super_block`, `struct inode`, directory entry layout, block allocation bitmap.
- **Day 30: Virtual Filesystem (VFS) Layer (`fs/vfs.c`)**
  - `struct file` descriptor table; implementing `vfs_open`, `vfs_close`, `vfs_read`, and `vfs_write`.
- **Day 31: User Program Executable Loader (`fs/exec.c`)**
  - `do_execve()`: parsing executable header; clearing old user address space; loading text/data segments; setting up user stack with `argc` and `argv`.
- **Day 32: Standalone User C Runtime & Library (`usr/lib/crt0.S`)**
  - Writing user CRT0 (`_start`), system call assembly stubs, and minimal libc string helpers for user-space applications.
- **Day 33: Interactive Command Shell (`usr/bin/sh.c`)**
  - Interactive REPL command prompt; reading character input from UART; parsing arguments (`argv`).
- **Day 34: Fork + Exec Pipeline in User Shell**
  - Executing user utilities (`echo`, `cat`, `ls`) via `fork()` and `exec()`; parent waiting on child process exit.
- **Day 35: Capstone Review, Architecture Documentation & Interview Portfolio**
  - Complete architecture walkthrough; memory map diagram; Staff-level interview narratives covering boot, virtual memory, scheduling, and driver subsystems.
