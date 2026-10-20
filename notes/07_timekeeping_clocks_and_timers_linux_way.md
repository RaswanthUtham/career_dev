# Timekeeping, Hardware `stimecmp` (`Sstc`), & Interrupt Top-Half vs. Bottom-Half (`softirq`)

This guide covers **Milestone 5: Time, Hardware `stimecmp`, and Softirq Bottom-Half Pipeline** in the upstream Linux kernel way:
1. **Bare-Metal (`-bios none` at `0x80000000`) vs. OpenSBI (`-bios default` at `0x80200000`) Dual-Boot** — How to unlock PMP, trap delegation, and the modern **`stimecmp` (`Sstc` CSR `0x14D`)** in Machine Mode yourself!
2. **The 12-Year-Old Clock Math (`timebase-frequency` vs. `HZ`)** — Why `HZ` (`100`) is an OS configuration choice (`100,000` ticks per `10 ms` jiffy), NOT a hardware number derived from `10 MHz`!
3. **`struct clocksource` vs. `struct clock_event_device` Using Modern `stimecmp` (`CSR 0x14D`)** — How modern RISC-V CPUs program the next timer deadline directly in S-mode in **1 instruction** (`csrw stimecmp, next_tval`) with zero M-mode `ecall` overhead!
4. **Interrupt Top-Half vs. Bottom-Half (`kernel/softirq.c`: `TIMER_SOFTIRQ`)** — Why Linux splits every interrupt into a fast **Top Half** (Hard IRQ) and a deferred **Bottom Half** (`softirq`), and how `struct timer_list` callbacks execute in `TIMER_SOFTIRQ`!

---

## PART 1: Answering Your 2 Foundational Boot Questions!

### Question 1: "With `-bios none`, doesn't the CPU start at `0x80000000`? How could `vmlinux` linked at `0x80200000` work, or should `-bios none` start at `0x80000000`?"

You are **100% right** about real silicon and raw binary images!
Let's explain what happens in both cases in simple terms:

```text
CASE A: Real Silicon / Raw Binary Image (.bin) with No OpenSBI (-bios none)
  - On real hardware (or when passing a raw binary to QEMU), the reset ROM at
    0x1000 ALWAYS jumps to the start of RAM: 0x80000000!
  - Because OpenSBI is NOT there, our kernel MUST be linked at 0x80000000!

CASE B: Why QEMU's '-kernel vmlinux' (ELF file) Can Jump to Any Address
  - When you pass an ELF file ('vmlinux') to QEMU's '-kernel' flag, QEMU has a
    built-in ELF loader that reads 'ENTRY(_start)' from the ELF header and
    rewrites the target address at 0x1018 in the reset ROM!
  - HOWEVER, relying on QEMU's ELF loader to jump to 0x80200000 with '-bios none'
    is a simulator shortcut! On real bare-metal hardware, without OpenSBI at
    0x80000000, the first stage at RAM base MUST live at 0x80000000!
```

#### How We Do It Cleanly in `my-linux/arch/riscv/kernel/vmlinux.lds.S`:
Because `vmlinux.lds.S` is preprocessed by the C preprocessor (`gcc -E`), we use `#ifdef CONFIG_RISCV_M_MODE`:
```ld
#ifdef CONFIG_RISCV_M_MODE
    . = 0x80000000;     /* Bare-Metal (-bios none): Starts right at RAM base 0x80000000! */
#else
    . = 0x80200000;     /* OpenSBI (-bios default): Starts at 0x80200000 after OpenSBI!  */
#endif
```
Look how clean that is:
- When you run **`make qemu-baremetal`** (`-DCONFIG_RISCV_M_MODE` with `-bios none`), `vmlinux` is linked at **`0x80000000`** and runs our M-mode bare-metal setup (`mstatus`, `medeleg`, `mideleg`, `pmpaddr0`, `pmpcfg0`, `mcounteren`, `menvcfg.STCE`, `mret`)!
- When you run **`make qemu`** (with `-bios default`), `vmlinux` is linked at **`0x80200000`** and boots via OpenSBI!

---

### Question 2: Generic Boot Process of a Bare-Metal Microcontroller vs. an Application Processor (From Power-On to OS!)

In interviews, being able to contrast a **Microcontroller (MCU)** boot flow against an **Application Processor (SoC)** boot flow in simple English sets you apart immediately.

Here is the complete picture from the moment electricity hits the chip:

```text
===================================================================================
TYPE 1: MICROCONTROLLER (MCU) — e.g., STM32 Cortex-M4 or Small RISC-V RV32 MCU
        (Runs Bare-Metal Firmware or RTOS like Zephyr / FreeRTOS)
===================================================================================

  [1. Power-On Reset]
      - Electricity stabilizes; hardware reset pin goes HIGH.
      - Built-in On-Chip SRAM (e.g., 128 KB) and Internal Flash (e.g., 512 KB)
        work immediately with ZERO training needed!

  [2. Hardware Vector Fetch (0x00000000 / 0x08000000 Internal Flash)]
      - CPU hardware reads the first 2 words of Internal Flash:
          Word 0: Initial Stack Pointer (sp = top of SRAM)
          Word 1: Address of Reset_Handler() in Flash

  [3. Reset_Handler() (Startup Assembly / C in Flash)]
      - Copies initial values of global variables (.data) from Flash into SRAM.
      - Zeroes out uninitialized global variables (.bss) in SRAM.
      - Configures CPU clock PLL (e.g., 16 MHz crystal -> 180 MHz CPU clock).
      - Calls main()! (Total boot time: < 2 milliseconds! No U-Boot, no DRAM!)


===================================================================================
TYPE 2: APPLICATION PROCESSOR (SoC) — e.g., RISC-V 64 / ARM64 (Qualcomm, TI, RPi)
        (Runs Full Linux Kernel in Virtual Memory)
===================================================================================
  Why can't an Application Processor boot like an MCU?
  Because a 2 GB external DDR4 DRAM chip is DEAD at power-on (needs clock/signal
  training), and a 20 MB Linux kernel lives on an SD card / eMMC / Flash chip!

  [Stage 0: On-Chip Mask ROM (Boot ROM — Etched into Silicon at Factory)]
      - CPU powers on in highest privilege (M-mode on RISC-V / EL3 on ARM).
      - DRAM is still dead! CPU uses tiny 64 KB on-chip SRAM.
      - Mask ROM reads physical boot pins (SD card? SPI Flash? eMMC? USB?)
        and copies a tiny 40 KB program called SPL from SD/Flash into on-chip SRAM.

  [Stage 1: SPL (Secondary Program Loader / FSBL — Runs in On-Chip SRAM)]
      - Initializes system clocks (PLLs) and trains the DDR DRAM controller!
      - Now the external 128 MB / 2 GB DRAM chip (at 0x80000000) is ALIVE!
      - SPL reads two files from SD card / Flash into DRAM:
          1. Firmware (OpenSBI on RISC-V / TF-A on ARM) -> 0x80000000
          2. U-Boot (Full Bootloader)                   -> 0x80200000
      - SPL jumps to Firmware at 0x80000000.

  [Stage 2: Machine Firmware (OpenSBI in M-Mode at 0x80000000)]
      - Unlocks Physical Memory Protection (PMP) so S-mode can access DRAM.
      - Delegates traps/interrupts (medeleg / mideleg) to S-mode.
      - Unlocks hardware timer CSRs (mcounteren & menvcfg.STCE for 'stimecmp').
      - Drops privilege from M-mode to S-mode ('mret') and jumps to U-Boot!

  [Stage 3: U-Boot (Universal Bootloader — Runs in S-Mode in DRAM)]
      - Has full drivers for Display, Ethernet (TFTP), USB, FAT32/ext4 filesystems.
      - Reads 'vmlinux'/'Image' (Kernel) and 'board.dtb' (Device Tree) from SD card
        or network into DRAM, sets a0 = hartid, a1 = dtb_addr, and jumps to Kernel!
      *(Note: In fast-boot embedded systems or QEMU, U-Boot is skipped and Stage 2
        jumps straight into the Kernel!)*

  [Stage 4: OS Kernel ('my-linux' / Linux — Runs in S-Mode)]
      - head.S masks interrupts, zeroes BSS, builds Sv39 page tables (setup_vm),
        turns on MMU (satp), and relocates PC/SP to High-Half (0xFFFFFFC08xxxxxxx).
      - start_kernel() initializes DTB, SLUB kmalloc, Timers (stimecmp), PLIC IRQs,
        Scheduler, and launches '/bin/sh' in User Mode (U-mode)!
```

---

## PART 2: The Clock Math Explained to a 12-Year-Old (`timebase-frequency` vs. `HZ`)

### 1. What Are the Two Numbers in Every OS Timer?

Imagine you have a wall clock in your classroom and a teacher who claps their hands to check on the students:

```text
+-------------------------------------------------------------------------------+
| 1. THE HARDWARE STOPWATCH (riscv_timebase / timebase-frequency):              |
|    - Inside the RISC-V chip, a hardware counter ('time' CSR) ticks upward     |
|      automatically at a fixed crystal speed.                                  |
|    - Remember when our FDT parser scanned '/cpus' in QEMU Virt?               |
|      It found:  timebase-frequency = 10,000,000 (10 MHz)!                     |
|    - That means the hardware stopwatch ticks 10,000,000 times every 1 second! |
|    - Can software change this speed? NO! It is wired in silicon hardware!     |
+-------------------------------------------------------------------------------+
| 2. THE OS HEARTBEAT RATE (CONFIG_HZ / HZ):                                    |
|    - How many times per second does the OS Kernel want a timer interrupt?     |
|    - This is called 'HZ' (defined in linux/kernel/Kconfig.hz).                |
|    - The kernel engineer CHOOSES 'HZ' when configuring the kernel:            |
|        * HZ = 100  (100 ticks/sec -> 1 tick every 10 milliseconds)            |
|        * HZ = 250  (250 ticks/sec -> 1 tick every 4 milliseconds, Linux def)  |
|        * HZ = 1000 (1000 ticks/sec -> 1 tick every 1 millisecond)             |
|    - CRITICAL RULE: 'HZ = 100' is a human choice, NOT calculated from 10 MHz! |
+-------------------------------------------------------------------------------+
```

### 2. How Many Hardware Ticks Make 1 OS Heartbeat ("Jiffy")?

In Linux, one OS heartbeat interval (`1 / HZ` of a second) is called **1 Jiffy**, and **`jiffies_64`** counts how many heartbeats have happened since boot:

```text
THE JIFFY DELTA FORMULA:

                       riscv_timebase          10,000,000 ticks/sec
  Ticks per Jiffy  =  ----------------     =  ----------------------  =  100,000 ticks!
                             HZ                  100 jiffies/sec
```
- Every time the hardware `time` counter goes up by **`100,000`**, **10 milliseconds (1 Jiffy)** have passed!

---

## PART 3: Modern Hardware `stimecmp` (`Sstc` CSR `0x14D`) vs. Old OpenSBI `ecall`

### 1. Why Modern RISC-V Added `stimecmp` (`CSR 0x14D`)
- **The Old Way (2017–2021 RISC-V)**: Only Machine Mode had a timer comparator (`mtimecmp`). Every 10 ms, the S-mode kernel had to execute an `ecall` trap into M-mode (OpenSBI) just to set the next timer tick!
- **The Modern Way (`Sstc` Extension — Upstream `linux/drivers/clocksource/timer-riscv.c` & `xv6-riscv/kernel/start.c`)**:
  Modern RISC-V CPUs give Supervisor Mode its own 64-bit hardware comparator CSR: **`stimecmp` (CSR `0x14D`)**!

```text
HOW HARDWARE 'stimecmp' (CSR 0x14D) WORKS IN SILICON:

  +----------------------+          Hardware Comparator           +----------------------+
  |  'time' CSR (0xC01)  | ------>  Is (time >= stimecmp)?  <---- | 'stimecmp' (0x14D)   |
  |  (Ticks at 10 MHz)   |                   |                    | (Written by S-mode!) |
  +----------------------+                   |                    +----------------------+
                                             +-- If YES: Sets sip.STIP = 1
                                                         (Fires S-Mode Timer IRQ 5!)
                                             +-- If NO:  Clears sip.STIP = 0 automatically!
```

Look at the two huge benefits of `stimecmp`:
1. **1 Single Instruction in S-Mode**: Writing `csrw 0x14d, next_tval` arms the next timer interrupt in 1 CPU cycle—**zero `ecall` to Machine Mode!**
2. **Automatic Interrupt Clearing**: The instant S-mode writes a future value (`rdtime() + 100000`) or `U64_MAX` (`~0ULL`) into `stimecmp`, the hardware sees `time < stimecmp` and **automatically clears the pending timer interrupt (`sip.STIP = 0`)**!

### 2. What Unlocks `stimecmp` in Machine Mode (When Booting Bare-Metal `-bios none`)?
When the CPU powers on in Machine Mode at `0x80000000`, `stimecmp` is locked by default until M-mode sets two CSR bits (just like `xv6-riscv/kernel/start.c` and OpenSBI do):
1. **`mcounteren |= 0x2`**: Allows S-mode to read the `time` counter (`rdtime`).
2. **`menvcfg |= (1ULL << 63)` (`ENVCFG_STCE`)**: Enables **S**upervisor **T**imer **C**ounter **E**xtension (`stimecmp` CSR `0x14D`)!

---

## PART 4: Interrupt Top-Half vs. Bottom-Half (`kernel/softirq.c`) — The Linux Way!

### 1. Why Does Linux Split Every Interrupt into a "Top Half" and a "Bottom Half"?

Imagine an emergency room doctor when an ambulance arrives:
1. **The Top Half (Hard IRQ Context — Emergency Triage)**:
   - When a hardware interrupt fires and the CPU jumps to `stvec` (`handle_exception` $\rightarrow$ `do_trap`), the CPU automatically **disables all interrupts (`sstatus.SIE = 0`)**!
   - While inside this Hard IRQ handler, **no other interrupt can be serviced**.
   - So the **Top Half** must do only the bare minimum hardware work (update `jiffies_64++`, write the next deadline to `stimecmp`, and raise a flag `raise_softirq(TIMER_SOFTIRQ)`), finishing in a few nanoseconds!
2. **The Bottom Half (Softirq Context — Deferred Processing)**:
   - What about walking the list of 50 software timers (`struct timer_list`) and running their callback functions (`run_timer_softirq()`)?
   - That is deferred to the **Bottom Half (`kernel/softirq.c`)**, which runs inside **`irq_exit()`** right as the interrupt handler finishes!

```text
UPSTREAM LINUX INTERRUPT FLOW (TOP HALF -> irq_exit() -> BOTTOM HALF):

  [Hardware Timer Interrupt Fires (time >= stimecmp, scause = 5)]
                   |
                   v
  +---------------------------------------------------------------------------+
  | 1. TOP HALF (Hard IRQ Context in drivers/clocksource/timer-riscv.c):      |
  |    riscv_timer_interrupt() -> tick_handle_periodic():                     |
  |      a) jiffies_64++                                                      |
  |      b) Rearm stimecmp:  csrw 0x14d, get_cycles64() + (riscv_timebase/HZ) |
  |      c) Schedule Bottom Half:  raise_softirq(TIMER_SOFTIRQ);              |
  +---------------------------------------------------------------------------+
                   |
                   v
  +---------------------------------------------------------------------------+
  | 2. BOTTOM HALF (Softirq Context in kernel/softirq.c):                     |
  |    At the end of do_trap(), irq_exit() checks 'softirq_pending':          |
  |      - Sees Bit 1 (TIMER_SOFTIRQ) is set!                                 |
  |      - Calls __do_softirq() -> softirq_vec[TIMER_SOFTIRQ].action()        |
  |      - Executes run_timer_softirq() to fire expired struct timer_list!    |
  +---------------------------------------------------------------------------+
```

### 2. How `kernel/softirq.c` Works in Upstream Linux (`include/linux/interrupt.h`)
In upstream Linux (`include/linux/interrupt.h` and `kernel/softirq.c`):
1. There is a small table of 10 statically-defined Softirq types (`HI_SOFTIRQ = 0`, **`TIMER_SOFTIRQ = 1`**, `NET_TX_SOFTIRQ = 2`, `NET_RX_SOFTIRQ = 3`, ...):
   ```c
   struct softirq_action {
       void (*action)(struct softirq_action *);
   };
   static struct softirq_action softirq_vec[NR_SOFTIRQS];
   ```
2. A 32-bit bitmap (`uint32_t softirq_pending`) tracks which softirqs have work waiting:
   - **`open_softirq(nr, action)`**: Registers the callback function in `softirq_vec[nr].action`.
   - **`raise_softirq(nr)`**: Sets bit `nr` in `softirq_pending |= (1U << nr);` (Called by the Top Half!).
   - **`irq_exit()` $\rightarrow$ `__do_softirq()`**: Loops over any set bits in `softirq_pending`, clears the bit, and calls `softirq_vec[nr].action()`! (And uses an `in_softirq` guard flag so nested interrupts don't re-enter `__do_softirq()`).

---

## PART 5: Step-by-Step Implementation in `my-linux/`

Here are the exact files for you to update or create in `my-linux/`.

---

### Step 1: Dual-Boot Bare-Metal (`0x80000000`) & OpenSBI (`0x80200000`) in `vmlinux.lds.S` and `head.S`

#### A. In `my-linux/arch/riscv/kernel/vmlinux.lds.S`:
Replace `. = 0x80200000;` with `#ifdef CONFIG_RISCV_M_MODE`:
```ld
/* Target File: my-linux/arch/riscv/kernel/vmlinux.lds.S */
#ifdef CONFIG_RISCV_M_MODE
    . = 0x80000000;
#else
    . = 0x80200000;
#endif
```

#### B. In `my-linux/arch/riscv/kernel/head.S`:
At the top of `_start`, add the `#ifdef CONFIG_RISCV_M_MODE` bare-metal Machine-Mode setup block (matching `linux/arch/riscv/kernel/head.S` and `xv6-riscv/kernel/start.c`) that unlocks PMP, delegates traps, unlocks `time` (`mcounteren |= 2`) and **`stimecmp` (`menvcfg |= (1 << 63)`)**, and executes `mret` into `.Ls_mode_entry`:

```assembly
/* Target File: my-linux/arch/riscv/kernel/head.S */
/* SPDX-License-Identifier: GPL-2.0-only */

.section .text.init
.global _start

_start:
#ifdef CONFIG_RISCV_M_MODE
    /*
     * BARE-METAL MACHINE MODE ENTRY (-bios none at 0x80000000):
     * Perform the exact hardware initialization that OpenSBI does!
     */
    /* 1. Read hardware thread ID into a0 (a1 already holds DTB from 0x1000 ROM) */
    csrr a0, mhartid
    bnez a0, .Lsecondary_park

    /* 2. Configure Physical Memory Protection (PMP) to allow S-mode full access */
    li   t0, 0x3fffffffffffff
    csrw pmpaddr0, t0
    li   t0, 0xf
    csrw pmpcfg0, t0

    /* 3. Delegate all exceptions and interrupts to Supervisor Mode (S-mode) */
    li   t0, 0xffff
    csrw medeleg, t0
    csrw mideleg, t0

    /* 4. Unlock 'time' CSR (mcounteren bit 1) and 'stimecmp' (menvcfg bit 63 STCE)! */
    csrr t0, mcounteren
    ori  t0, t0, 2
    csrw mcounteren, t0

    li   t0, (1 << 63)          /* ENVCFG_STCE: Unlock S-mode stimecmp (0x14D)! */
    csrs menvcfg, t0

    /* 5. Set mstatus.MPP = S-Mode (1 << 11) and mepc = .Ls_mode_entry, then mret! */
    csrr t0, mstatus
    li   t1, ~(3 << 11)
    and  t0, t0, t1
    li   t1, (1 << 11)          /* MPP = 01 (Supervisor Mode) */
    or   t0, t0, t1
    csrw mstatus, t0

    la   t0, .Ls_mode_entry
    csrw mepc, t0
    mret
#endif

.Ls_mode_entry:
    /* 1. Mask all supervisor interrupts and clear pending flags */
    csrw sie, zero
    csrw sip, zero

    /* 2. Enable time CSR (rdtime) access */
    li   t0, 0x2
    csrw scounteren, t0

    /* 3. Park secondary cores (only Hart 0 runs the boot sequence) */
    bnez a0, .Lsecondary_park
```
*(Keep the rest of `head.S` from `/* 4. Load initial boot stack pointer */` downward identical!)*

---

### Step 2: Dynamically Read `"timebase-frequency"` in `my-linux/arch/riscv/kernel/setup.c`

In **`my-linux/arch/riscv/kernel/setup.c`**, extract `"timebase-frequency"` (`10000000`) from the Device Tree into `unsigned long riscv_timebase` (matching upstream `linux/arch/riscv/kernel/time.c`):

```c
/* Target File: my-linux/arch/riscv/kernel/setup.c */

void early_print_dec(unsigned long val);

/* Global hardware timebase frequency (ticks/sec) discovered from DTB /cpus */
unsigned long riscv_timebase = 0;

static int fdt_streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/* Inside fdt_scan_nodes(), inside the 'else if (token == FDT_PROP)' block: */
        } else if (token == FDT_PROP) {
            uint32_t prop_len = be32_to_cpu(*p++);
            uint32_t name_offset = be32_to_cpu(*p++);
            const char *prop_name = strings + name_offset;

            /* Dynamically extract timebase-frequency from /cpus node! */
            if (fdt_streq(prop_name, "timebase-frequency") && prop_len == 4) {
                riscv_timebase = (unsigned long)be32_to_cpu(*p);
                early_puts("  [FDT] Discovered riscv_timebase = ");
                early_print_dec(riscv_timebase);
                early_puts(" Hz\n");
            }

            /* Advance pointer past property value and 4-byte padding */
            p += fdt_align4(prop_len) / sizeof(uint32_t);
```

---

### Step 3: Create `my-linux/arch/riscv/include/asm/csr.h` (With Hardware `CSR_STIMECMP = 0x14D`!)

**What this file does:**
Defines the interrupt bits and the modern `stimecmp` CSR (`0x14D`, matching `linux/arch/riscv/include/asm/csr.h`).
Why do we write `"csrw 0x14d, %0"` using the numeric CSR address `0x14d`?
Because numeric CSR addresses (`0x14d`) assemble cleanly on every RISC-V binutils version without needing special `-march=rv64gc_sstc` compiler flags!

```c
/* Target File: my-linux/arch/riscv/include/asm/csr.h */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_RISCV_CSR_H
#define _ASM_RISCV_CSR_H

#include <linux/types.h>

/* sstatus CSR bits */
#define SR_SIE              (1UL << 1)  /* Supervisor Interrupt Enable */
#define SR_SPIE             (1UL << 5)  /* Previous Supervisor Interrupt Enable */
#define SR_SPP              (1UL << 8)  /* Previously Supervisor (1=S, 0=U) */
#define SR_SUM              (1UL << 18) /* Supervisor User Memory access */

/* sie / sip CSR bits */
#define SIE_SSIE            (1UL << 1)  /* Software Interrupt */
#define SIE_STIE            (1UL << 5)  /* Timer Interrupt */
#define SIE_SEIE            (1UL << 9)  /* External Interrupt (PLIC) */

/* Interrupt cause codes (without the top bit 63) */
#define RV_IRQ_SOFT         1
#define RV_IRQ_TIMER        5
#define RV_IRQ_EXT          9

/* Sstc Extension: Supervisor Timer Compare CSR (0x14D) */
#define CSR_STIMECMP        0x14d

static inline unsigned long get_cycles64(void)
{
    unsigned long val;
    __asm__ volatile("rdtime %0" : "=r"(val));
    return val;
}

/* Write deadline directly to hardware stimecmp CSR (0x14D) in S-mode! */
static inline void csr_write_stimecmp(uint64_t val)
{
    __asm__ volatile("csrw 0x14d, %0" : : "r"(val) : "memory");
}

static inline void local_irq_enable(void)
{
    __asm__ volatile("csrs sstatus, %0" : : "r"(SR_SIE) : "memory");
}

static inline void local_irq_disable(void)
{
    __asm__ volatile("csrc sstatus, %0" : : "r"(SR_SIE) : "memory");
}

#endif /* _ASM_RISCV_CSR_H */
```

---

### Step 4: Create `my-linux/include/linux/interrupt.h` & `my-linux/kernel/softirq.c` (Bottom-Half Engine!)

#### A. Create `my-linux/include/linux/interrupt.h`:
```c
/* Target File: my-linux/include/linux/interrupt.h */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_INTERRUPT_H
#define _LINUX_INTERRUPT_H

#include <linux/types.h>

/* Upstream Linux Softirq Indices (include/linux/interrupt.h) */
enum {
    HI_SOFTIRQ = 0,
    TIMER_SOFTIRQ,
    NET_TX_SOFTIRQ,
    NET_RX_SOFTIRQ,
    NR_SOFTIRQS
};

struct softirq_action {
    void (*action)(struct softirq_action *);
};

void softirq_init(void);
void open_softirq(int nr, void (*action)(struct softirq_action *));
void raise_softirq(unsigned int nr);
void irq_enter(void);
void irq_exit(void);

#endif /* _LINUX_INTERRUPT_H */
```

#### B. Create `my-linux/kernel/softirq.c`:
```c
/* Target File: my-linux/kernel/softirq.c */
/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/interrupt.h>

void early_puts(const char *s);

static struct softirq_action softirq_vec[NR_SOFTIRQS];
static volatile uint32_t softirq_pending = 0;
static volatile int irq_nest_count = 0;
static volatile int in_softirq = 0;

void open_softirq(int nr, void (*action)(struct softirq_action *))
{
    if (nr >= 0 && nr < NR_SOFTIRQS) {
        softirq_vec[nr].action = action;
    }
}

void raise_softirq(unsigned int nr)
{
    if (nr < NR_SOFTIRQS) {
        softirq_pending |= (1U << nr);
    }
}

static void __do_softirq(void)
{
    in_softirq = 1;

    uint32_t pending = softirq_pending;
    softirq_pending = 0;

    for (int i = 0; i < NR_SOFTIRQS; i++) {
        if (pending & (1U << i)) {
            if (softirq_vec[i].action) {
                softirq_vec[i].action(&softirq_vec[i]);
            }
        }
    }

    in_softirq = 0;
}

void irq_enter(void)
{
    irq_nest_count++;
}

void irq_exit(void)
{
    if (irq_nest_count > 0) {
        irq_nest_count--;
    }

    /* Run pending Bottom-Half softirqs when returning from the outer Hard IRQ! */
    if (irq_nest_count == 0 && !in_softirq && softirq_pending != 0) {
        __do_softirq();
    }
}

void softirq_init(void)
{
    for (int i = 0; i < NR_SOFTIRQS; i++) {
        softirq_vec[i].action = NULL;
    }
    softirq_pending = 0;
    early_puts("[SOFTIRQ] Bottom-Half softirq subsystem initialized.\n");
}
```

---

### Step 5: Create `my-linux/include/linux/timer.h` & `my-linux/drivers/clocksource/timer-riscv.c`

#### A. Create `my-linux/include/linux/timer.h`:
```c
/* Target File: my-linux/include/linux/timer.h */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#include <linux/types.h>
#include <linux/list.h>

/* Configured OS heartbeat rate: 100 ticks per second (10 ms per jiffy) */
#define HZ                  100UL
#define TICK_NSEC           (1000000000UL / HZ)
#define U64_MAX             (~0ULL)

extern unsigned long riscv_timebase;
extern volatile uint64_t jiffies_64;

struct clocksource {
    const char *name;
    int rating;
    uint64_t (*read)(struct clocksource *cs);
};

struct clock_event_device {
    const char *name;
    int rating;
    int (*set_next_event)(unsigned long delta, struct clock_event_device *ce);
    int (*set_state_shutdown)(struct clock_event_device *ce);
    void (*event_handler)(struct clock_event_device *ce);
};

struct timer_list {
    struct list_head entry;
    uint64_t expires;
    void (*function)(struct timer_list *t);
};

void timer_setup(struct timer_list *timer, void (*callback)(struct timer_list *), unsigned int flags);
int mod_timer(struct timer_list *timer, uint64_t expires);
int del_timer(struct timer_list *timer);

void time_init(void);
void riscv_timer_interrupt(void);

#endif /* _LINUX_TIMER_H */
```

#### B. Create `my-linux/drivers/clocksource/timer-riscv.c`:
Look at how clean `riscv_clock_event_stop()` and `riscv_clock_next_event()` are now:
- They write directly to **`csr_write_stimecmp()` (`CSR 0x14D`)**!
- **Top Half (`tick_handle_periodic`)**: Increments `jiffies_64++`, rearms `stimecmp`, and calls **`raise_softirq(TIMER_SOFTIRQ)`**!
- **Bottom Half (`run_timer_softirq`)**: Registered via `open_softirq(TIMER_SOFTIRQ, run_timer_softirq);` and executed automatically by `irq_exit()`!

```c
/* Target File: my-linux/drivers/clocksource/timer-riscv.c */
/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <asm/csr.h>

void early_puts(const char *s);
void early_print_dec(unsigned long val);

/* Global jiffies counter (incremented at HZ = 100 times per second) */
volatile uint64_t jiffies_64 = 0;

static LIST_HEAD(active_timers);

/*
 * riscv_clock_event_stop - Stop timer comparator by writing U64_MAX to stimecmp
 * (Matches upstream linux/drivers/clocksource/timer-riscv.c::riscv_clock_event_stop)
 */
static void riscv_clock_event_stop(void)
{
    csr_write_stimecmp(U64_MAX);
}

/*
 * riscv_clock_next_event - Program next timer deadline directly into stimecmp (CSR 0x14D)
 * (Matches upstream linux/drivers/clocksource/timer-riscv.c::riscv_clock_next_event)
 */
static int riscv_clock_next_event(unsigned long delta, struct clock_event_device *ce)
{
    (void)ce;
    uint64_t next_tval = get_cycles64() + delta;
    csr_write_stimecmp(next_tval);
    return 0;
}

static int riscv_clock_shutdown(struct clock_event_device *evt)
{
    (void)evt;
    riscv_clock_event_stop();
    return 0;
}

static uint64_t riscv_clocksource_rdtime(struct clocksource *cs)
{
    (void)cs;
    return get_cycles64();
}

struct clocksource riscv_clocksource = {
    .name   = "riscv_clocksource",
    .rating = 400,
    .read   = riscv_clocksource_rdtime,
};

/*
 * TOP HALF: tick_handle_periodic - Runs in Hard IRQ context!
 * Does ONLY fast hardware rearming + jiffies_64++, then raises TIMER_SOFTIRQ!
 */
static void tick_handle_periodic(struct clock_event_device *ce)
{
    /* 1. Advance the OS heartbeat counter */
    jiffies_64++;

    /* 2. Rearm hardware stimecmp for the next jiffy (riscv_timebase / HZ)! */
    unsigned long ticks_per_jiffy = riscv_timebase / HZ;
    ce->set_next_event(ticks_per_jiffy, ce);

    /* 3. Defer software timer_list processing to the Bottom Half (TIMER_SOFTIRQ)! */
    raise_softirq(TIMER_SOFTIRQ);
}

struct clock_event_device riscv_clock_event = {
    .name               = "riscv_timer_clockevent",
    .rating             = 450,
    .set_next_event     = riscv_clock_next_event,
    .set_state_shutdown = riscv_clock_shutdown,
    .event_handler      = tick_handle_periodic,
};

/*
 * TOP HALF Entry: riscv_timer_interrupt - Called from do_trap() on RV_IRQ_TIMER (scause = 5)
 */
void riscv_timer_interrupt(void)
{
    struct clock_event_device *evdev = &riscv_clock_event;

    riscv_clock_event_stop();
    if (evdev->event_handler) {
        evdev->event_handler(evdev);
    }
}

void timer_setup(struct timer_list *timer, void (*callback)(struct timer_list *), unsigned int flags)
{
    (void)flags;
    INIT_LIST_HEAD(&timer->entry);
    timer->expires = 0;
    timer->function = callback;
}

int mod_timer(struct timer_list *timer, uint64_t expires)
{
    timer->expires = expires;
    if (list_empty(&timer->entry)) {
        list_add_tail(&timer->entry, &active_timers);
    }
    return 0;
}

int del_timer(struct timer_list *timer)
{
    if (!list_empty(&timer->entry)) {
        list_del(&timer->entry);
        INIT_LIST_HEAD(&timer->entry);
        return 1;
    }
    return 0;
}

/*
 * BOTTOM HALF: run_timer_softirq - Registered on TIMER_SOFTIRQ!
 * Executed by irq_exit() -> __do_softirq() after the Top Half finishes!
 */
static void run_timer_softirq(struct softirq_action *h)
{
    (void)h;
    struct list_head *curr = active_timers.next;
    while (curr != &active_timers) {
        struct timer_list *t = list_entry(curr, struct timer_list, entry);
        struct list_head *next = curr->next;

        if (jiffies_64 >= t->expires) {
            list_del(&t->entry);
            INIT_LIST_HEAD(&t->entry);
            if (t->function) {
                t->function(t);
            }
        }
        curr = next;
    }
}

void time_init(void)
{
    if (riscv_timebase == 0) {
        early_puts("[TIME PANIC] No timebase-frequency discovered in DTB!\n");
        return;
    }

    /* Register TIMER_SOFTIRQ Bottom-Half handler */
    open_softirq(TIMER_SOFTIRQ, run_timer_softirq);

    unsigned long ticks_per_jiffy = riscv_timebase / HZ;
    early_puts("[TIME] Initializing Sstc stimecmp clockevent (ticks_per_jiffy = ");
    early_print_dec(ticks_per_jiffy);
    early_puts(")...\n");

    /* Clear any pending timer interrupt and arm the first tick via stimecmp */
    riscv_clock_event_stop();
    riscv_clock_event.set_next_event(ticks_per_jiffy, &riscv_clock_event);

    /* Enable Supervisor Timer Interrupt bit (SIE_STIE) in sie CSR */
    unsigned long sie_val = SIE_STIE;
    __asm__ volatile("csrs sie, %0" : : "r"(sie_val) : "memory");

    /* Enable Master Supervisor Interrupts (SR_SIE) in sstatus CSR */
    local_irq_enable();

    early_puts("[TIME] Sstc stimecmp Timer Interrupts Enabled!\n");
}
```

---

### Step 6: Update `my-linux/arch/riscv/kernel/traps.c` with `irq_enter()` and `irq_exit()`!

Look at how `do_trap()` in **`my-linux/arch/riscv/kernel/traps.c`** now connects the **Top Half** and **Bottom Half**:
```c
/* Target File: my-linux/arch/riscv/kernel/traps.c */
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <asm/csr.h>

/* Inside do_trap(struct pt_regs *regs): */
    if (is_interrupt) {
        irq_enter(); /* Enter Hard IRQ Top-Half context */

        if (code == RV_IRQ_TIMER) {
            riscv_timer_interrupt(); /* Top Half: jiffies_64++, stimecmp, raise_softirq */
        } else {
            early_puts("[TRAP] Supervisor Interrupt received!\n");
        }

        irq_exit();  /* Exit Hard IRQ -> automatically runs pending Softirq Bottom Halves! */
    }
```

---

### Step 7: Add `Makefile` Rules (Including `make qemu-baremetal`), `main.c`, and KUnit Timer Test!

1. **In `my-linux/Makefile`**:
   - Add `kernel/softirq.c` and `drivers/clocksource/timer-riscv.c` to `SRCS_C`:
     ```makefile
     SRCS_C  += kernel/softirq.c
     SRCS_C  += drivers/clocksource/timer-riscv.c
     ```
   - Add the `qemu-baremetal` target so you can test **both** `-bios default` (`make qemu`) and bare-metal M-mode `-bios none` (`make qemu-baremetal`):
     ```makefile
     qemu-baremetal:
     	$(MAKE) clean
     	$(MAKE) CFLAGS="$(CFLAGS) -DCONFIG_RISCV_M_MODE"
     	qemu-system-riscv64 -machine virt -cpu rv64 -m 128M -bios none -kernel vmlinux -nographic -smp 1
     ```
2. **In `my-linux/init/main.c`**:
   - Include `<linux/interrupt.h>` and `<linux/timer.h>`, and call `softirq_init();` and `time_init();` before `kunit_run_all_tests();`.
3. **In `my-linux/lib/kunit/test_kernel.c`**:
   - Add `kunit_test_timer()` to schedule a dynamic `struct timer_list` callback 5 jiffies (`50 ms`) in the future, wait for the Bottom-Half `TIMER_SOFTIRQ` callback to fire, and print `[PASS]`!

```c
/* Target File: my-linux/lib/kunit/test_kernel.c */
#include <linux/timer.h>

static volatile int timer_callback_fired = 0;

static void test_timer_callback(struct timer_list *t)
{
    (void)t;
    timer_callback_fired = 1;
    early_puts("  [TIMER_SOFTIRQ] Bottom-Half timer_list callback fired at jiffies = ");
    early_print_dec((unsigned long)jiffies_64);
    early_puts("!\n");
}

static void kunit_test_timer(void)
{
    early_puts("[KUNIT] 5. Testing Sstc stimecmp Top-Half & TIMER_SOFTIRQ Bottom-Half...\n");

    struct timer_list my_timer;
    uint64_t start_jiffies = jiffies_64;
    timer_callback_fired = 0;

    timer_setup(&my_timer, test_timer_callback, 0);
    mod_timer(&my_timer, start_jiffies + 5); /* Fire 5 jiffies (50 ms) from now */

    while (!timer_callback_fired) {
        __asm__ volatile("wfi");
    }

    if (jiffies_64 >= start_jiffies + 5) {
        early_puts("  [PASS] Hardware stimecmp IRQ + TIMER_SOFTIRQ Bottom-Half verified!\n\n");
    }
}
```
*(Note: When testing `ecall` in `kunit_test_traps()` under bare-metal `-bios none`, remember that in `head.S` we delegated `medeleg = 0xffff` including Bit 9! So under `-bios none`, S-mode `ecall` traps directly into our own `do_trap()` `code == 9` handler too!)*

---

## PART 6: Plain-Language Feynman Recap

1. **Dual-Boot (`make qemu` & `make qemu-baremetal`)**: With `-DCONFIG_RISCV_M_MODE`, `vmlinux` links at `0x80000000`, runs our own M-mode setup in `head.S` (unlocking PMP, delegating traps, and setting `menvcfg.STCE = 1` to unlock `stimecmp`), and drops to S-mode via `mret`.
2. **Hardware `stimecmp` (`CSR 0x14D`)**: S-mode programs the next timer deadline in 1 instruction (`csrw 0x14d, next_tval`) with zero runtime M-mode `ecall`.
3. **Top Half vs. Bottom Half**:
   - **Top Half (`riscv_timer_interrupt`)**: Runs in Hard IRQ context (`irq_enter`), increments `jiffies_64++`, rearms `stimecmp`, and calls `raise_softirq(TIMER_SOFTIRQ)`.
   - **Bottom Half (`kernel/softirq.c`)**: Runs in `irq_exit()` $\rightarrow$ `__do_softirq()`, invoking `run_timer_softirq()` to execute expired `struct timer_list` callbacks!
