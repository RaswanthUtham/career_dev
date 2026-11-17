# K1a: Firmware and Boot Contract (my-sbi, Dual Boot)

> **Verified:** `riscv64-linux-gnu-gcc` 13.3.0, GNU ld 2.42, QEMU 8.2.2 (`virt`), gdb-multiarch 15.1, 2026-10-22. Tutor-run in a private copy:
> - The firmware builds with `-Werror` and `--orphan-handling=error`. `fw.bin` is 3244 bytes.
> - Your K0 kernel prints `K0 layout: PASS` under my-sbi. With `-smp 4`, hart 3 once won the lottery.
> - GDB dumped the QEMU reset ROM.
> - Negative test: removing PMP rule 2 makes the kernel fault on its first instruction.
>
> The kernel side of the SBI calls is taught in K1b, in three small lessons starting with [K1b-1](K01b1_talk_to_firmware_sbi_ecall.md).

| Item | Value |
| --- | --- |
| Milestone | K1 part a. Next: [K1b-1: talking to the firmware](K01b1_talk_to_firmware_sbi_ecall.md) |
| Repository | `my-linux`, branch `v2`, new folder `firmware/` |
| You write | 15 files in `firmware/` and a new `.gitignore` |
| References | OpenSBI v1.3 (online): [fw_base.S](https://github.com/riscv-software-src/opensbi/blob/v1.3/firmware/fw_base.S), [sbi_init.c](https://github.com/riscv-software-src/opensbi/blob/v1.3/lib/sbi/sbi_init.c), [sbi_hart.c](https://github.com/riscv-software-src/opensbi/blob/v1.3/lib/sbi/sbi_hart.c); QEMU 8.2.2 [hw/riscv/boot.c](https://gitlab.com/qemu-project/qemu/-/blob/v8.2.2/hw/riscv/boot.c); xv6 `kernel/start.c`; [RISC-V specifications](https://docs.riscv.org/) (Privileged ISA, SBI v2.0) |

**How to read this note.** Section 0 gives the background. Read it first, even if a word looks familiar. Each later section starts with the problem, then the idea, then the numbers.

---

## 0. Know This First

### 0.1 Hart

A **hart** (hardware thread) is one CPU that runs instructions. `-smp 4` gives QEMU 4 harts, numbered 0 to 3.

### 0.2 Registers

A **register** is a 64-bit variable inside the CPU. RISC-V has 32 of them, `x0` to `x31`. Each has a nickname that says how C code uses it:

| Nickname | Register | Used for | C view |
| --- | --- | --- | --- |
| `zero` | `x0` | always 0 | the constant `0` |
| `ra` | `x1` | return address | where `return` goes back to |
| `sp` | `x2` | stack pointer | top of the stack |
| `a0`-`a7` | `x10`-`x17` | function arguments; `a0`/`a1` = return value | `f(a0, a1, ...)` |
| `t0`-`t6` | various | temporaries, a call may destroy them | scratch locals |
| `s0`-`s11` | various | saved, a call keeps them | locals that survive a call |
| `gp`, `tp` | `x3`, `x4` | global and thread pointer | not used by K1 |

Example: C calls `fw_main(0, 0x87e00000, 0x1028)`. Before the call, `a0 = 0`, `a1 = 0x87e00000` and `a2 = 0x1028`.

### 0.3 CSRs: the CPU's control switches

A **CSR** (control and status register) is a special register that controls or reports the CPU's behaviour. Normal instructions cannot touch it. Special ones can:

```text
csrr  t0, mcause      t0 = mcause         (read)
csrw  mtvec, t0       mtvec = t0          (write)
csrs  menvcfg, t0     menvcfg |= t0       (set bits)
csrrw sp, mscratch, sp   swap sp and mscratch
```

A name that starts with `m` belongs to M-mode, and a name that starts with `s` belongs to S-mode. These are the CSRs K1a uses:

| CSR | One-line meaning |
| --- | --- |
| `mhartid` | this hart's number |
| `mstatus` | mode bits; `MPP` = the mode that `mret` returns to |
| `mepc` | where to continue after a trap, or where `mret` jumps |
| `mcause` | why the trap happened (a number) |
| `mtval` | extra trap information (for example the bad address) |
| `mtvec` | address of the M-mode trap handler |
| `mscratch` | spare register for the trap handler |
| `medeleg`, `mideleg` | which traps go straight to S-mode |
| `mcounteren` | which counters S-mode may read |
| `menvcfg` | enable switches for S-mode features (Sstc) |
| `pmpcfg0`, `pmpaddr0..2` | memory protection rules (section 4) |
| `stvec`, `satp`, `sie`, `stimecmp` | S-mode handler, MMU, interrupt enable, timer |

### 0.4 Privilege modes

```text
U-mode  user programs           least power
S-mode  kernel (my-linux)
M-mode  firmware (my-sbi)       all power: every CSR, every address
```

The CPU starts in M-mode. Only M-mode can lower the mode, using `mret` (section 6).

### 0.5 Memory map: RAM and devices share one address space

Some addresses are RAM. Others are **devices**: reading or writing them talks to hardware. This is the same idea as a `volatile` pointer in C. QEMU's `virt` board (tutor-run `info mtree`):

```text
0x00001000            reset ROM (first code every hart runs)
0x00100000            test device: write a magic value to power off
0x02000000-0x02003fff ACLINT software interrupts (MSWI)
0x02004000-0x0200bfff ACLINT machine timer (MTIMER)
0x0c000000            PLIC (interrupt controller, K7)
0x10000000            UART (serial console)
0x80000000-0x87ffffff RAM, 128 MB
    0x80000000        my-sbi
    0x80200000        kernel
    0x87e00000        device tree (DTB)
```

**ACLINT** (Advanced Core Local Interruptor) is a small per-board device with two parts:
- **MTIMER**: a counter `mtime` that ticks at a fixed rate, plus one alarm register `mtimecmp` per hart. When `mtime >= mtimecmp`, the hart gets a machine timer interrupt.
- **MSWI**: one bit per hart. Writing 1 interrupts that hart (a doorbell between harts).

Its older name is CLINT. It is an M-mode resource. The kernel uses its own timer CSR `stimecmp` instead (the Sstc extension, section 5).

### 0.6 Traps

A **trap** is the CPU stopping normal code to run a handler:
- an **exception** is caused by an instruction (a bad address, `ecall`);
- an **interrupt** is caused by an outside event (timer, device).

What the CPU does by itself on a trap into M-mode:

```text
mepc   = address of the instruction that trapped
mcause = reason number
mtval  = extra info (bad address or 0)
mode   = M
pc     = mtvec                  -> the handler runs
handler ends with mret          -> pc = mepc, mode = mstatus.MPP
```

Cause numbers used in K1:

| `mcause` | Meaning |
| --- | --- |
| 1 | instruction access fault (not allowed to run code there) |
| 2 | illegal instruction |
| 3 | breakpoint (`ebreak`) |
| 5 / 7 | load / store access fault |
| 8 | `ecall` from U-mode (a system call) |
| 9 | `ecall` from S-mode (an **SBI call**) |
| 12 / 13 / 15 | instruction / load / store page fault (after the MMU is on) |

### 0.7 Reading assembly

The form is `operation destination, source1, source2`. Every instruction used in `fw_base.S`:

| Instruction | C equivalent |
| --- | --- |
| `li t0, 5` | `t0 = 5` |
| `la t0, sym` | `t0 = &sym` (PC-relative, see K0) |
| `mv s0, a0` | `s0 = a0` |
| `add sp, sp, t0` | `sp = sp + t0` |
| `addi sp, sp, -16` | `sp = sp - 16` |
| `mul t0, t0, t1` | `t0 = t0 * t1` |
| `and t0, t0, t1` | `t0 = t0 & t1` |
| `sd t0, 8(sp)` | `*(u64 *)(sp + 8) = t0` (store) |
| `ld t0, 8(sp)` | `t0 = *(u64 *)(sp + 8)` (load) |
| `bnez t0, L` | `if (t0 != 0) goto L` |
| `beq a, b, L` / `bgeu` / `bltu` | `if (a == b)` / `if (a >= b)` / `if (a < b)` (unsigned), then `goto L` |
| `j L` | `goto L` |
| `call f` / `ret` | `f()` / `return` |
| `amoadd.w t1, t1, (t0)` | atomically: `old = *t0; *t0 += t1; t1 = old` |
| `wfi` | sleep until an interrupt |
| `mret` | return from M-mode trap (section 6) |
| `ecall` | trap to the next higher mode |

Assembler directives (not instructions): `.section` picks the output section, `.globl` makes a symbol visible to C, `.balign 4` pads to a multiple of 4, `.word`/`.space` reserve bytes, and `.irp` repeats a line for each value in a list.

`1:` is a numeric local label. `1b` means "label 1 backwards" and `2f` means "label 2 forwards".

### 0.8 Two bit facts used below

- `x >> 2` is `x / 4`.
- A power of two minus one is all ones below its bit. For example, `8 = 0b1000` and `8 - 1 = 0b0111`.

---

## 1. What Firmware Is

Firmware is the M-mode program that runs first. It:
1. prepares the machine (memory protection, trap routing, counters);
2. starts the kernel in S-mode;
3. stays in memory and serves the kernel's requests (**SBI calls**).

```text
 S-mode  my-linux      --- ecall (cause 9) ---> M-mode  my-sbi
         (continues)   <------ mret ----------          (does the job)
```

K1a writes **my-sbi**. It follows the same start-up contract as OpenSBI, so one kernel `Image` boots on either:

```text
make qemu      ROM 0x1000 -> OpenSBI (M) -> kernel (S)
make qemu-fw   ROM 0x1000 -> my-sbi  (M) -> kernel (S)
```

On Arm64 the same job is done by TF-A at EL3, called with `smc` (section 13).

---

## 2. Why It Is Needed

**Analogy.** M-mode is a building manager with master keys. S-mode is a tenant. The manager decides which rooms the tenant may enter (PMP), which problems the tenant fixes alone (delegation), and answers the intercom (`ecall`).

| Step skipped | What happens |
| --- | --- |
| No PMP "allow" rule | The kernel cannot run even its first instruction (tutor-run: endless cause-1 faults at `0x80200000`) |
| No delegation | Every page fault goes to the firmware instead of the kernel |
| No `mcounteren` | Reading the time in S-mode is an illegal instruction |
| No `menvcfg.STCE` | The kernel cannot use its own timer register `stimecmp` |

---

## 3. Boot Flow

### 3.1 The reset ROM (tutor-run GDB dump)

Every hart starts at `0x1000` in M-mode. QEMU wrote this code:

```text
0x1000  auipc t0, 0x0       t0 = 0x1000 (its own address)
0x1004  addi  a2, t0, 40    a2 = 0x1028   -> fw_dynamic_info (below)
0x1008  csrr  a0, mhartid   a0 = hart number
0x100c  ld    a1, 32(t0)    a1 = 0x87e00000  device tree address
0x1010  ld    t0, 24(t0)    t0 = 0x80000000  firmware entry
0x1014  jr    t0            jump to firmware

0x1028  fw_dynamic_info:   magic "OSBI"   version 2   next_addr 0x80200000
                           next_mode 1 (S)   options 0   boot_hart 0
```

So firmware starts with `a0 = hart`, `a1 = DTB` and `a2 = info`. The info block tells it **where the kernel is** and **which mode** to start it in.

QEMU puts the kernel at the firmware's end rounded up to 2 MB (`hw/riscv/boot.c`). my-sbi ends at `0x80000cac`, so the kernel goes to `0x80200000`.

### 3.2 my-sbi, step by step

```text
all harts enter _start
   lottery: the first hart wins, the others sleep (wfi loop)
winner:
   1. mtvec = trap handler, clear .bss, sp = this hart's stack
   2. fw_main():
        check fw_dynamic_info
        PMP rules            (section 4)
        delegation           (section 5)
        counters, Sstc       (section 5)
        mret -> kernel at 0x80200000 in S-mode   (section 6)
later, each kernel ecall:
   trap handler -> do the SBI job -> mret back   (sections 8, 9)
```

Tutor-run banner:

```text
my-sbi: learner M-mode firmware (fw_dynamic handoff)
my-sbi: boot hart         = 0x0
my-sbi: dtb               = 0x87e00000
my-sbi: fw_dynamic_info   = 0x1028
my-sbi:   version         = 0x2
my-sbi:   next_addr       = 0x80200000
my-sbi:   next_mode       = 0x1
my-sbi:   boot_hart       = 0x0
my-sbi: pmpaddr0          = 0x801fff
my-sbi: pmpaddr1          = 0x20007fff
my-sbi: pmpaddr2          = 0xffffffffffffffff
my-sbi: pmpcfg0           = 0x1f1818
my-sbi: medeleg           = 0xb1ff
my-sbi: mideleg           = 0x1666
my-sbi: mcounteren        = 0x7
my-sbi: menvcfg           = 0xa000000000000000
my-sbi: Sstc present, S-mode owns stimecmp
my-sbi: mret to S-mode
```

Sections 4 and 5 explain every number in this banner.

---

## 4. PMP: Which Memory the Kernel May Touch

### 4.1 Problem

The kernel must not:
- read or overwrite the firmware's memory (`0x80000000`, 256 KB);
- touch the ACLINT timer device (`0x02000000`, 64 KB), which belongs to M-mode.

Everything else is allowed.

### 4.2 Idea: a short list of rules

**PMP** (Physical Memory Protection) is a list of up to 16 rules. Each rule is a **region** plus **permissions**: R (read), W (write) and X (execute).

```text
rule 0   ACLINT   0x02000000 - 0x0200ffff   R- W- X-   (deny)
rule 1   my-sbi   0x80000000 - 0x8003ffff   R- W- X-   (deny)
rule 2   all      every address             R  W  X    (allow)
```

**How the CPU checks an S-mode access:**
1. It tries rules 0, 1, 2 ... in order. The **first** matching rule decides.
2. If no rule matches, the access is **denied**.

M-mode is not checked at all, unless a rule is locked (`L` bit, not used here).

Three kernel accesses traced through the rules:

```text
fetch 0x80200000   rule 0? no  rule 1? no (ends 0x8003ffff)  rule 2? yes, X  -> allowed
read  0x80000000   rule 0? no  rule 1? yes, no R                             -> load fault (cause 5)
read  0x02004000   rule 0? yes, no R                                         -> load fault (cause 5)
```

Rule 2 must come **last**: it matches everything, so rules after it would never be reached. Remove rule 2 and nothing allows the kernel, so its first instruction faults (tutor-run).

### 4.3 Writing a region into one register (NAPOT)

**Problem:** each rule has only **one** 64-bit address register, `pmpaddrN`. It must hold both the start and the size of the region.

**Idea:** if the size is a power of two and the start is a multiple of that size, both fit. This is **NAPOT**, "naturally aligned power of two". The register holds:

```text
pmpaddr = (start / 4) with the low bits replaced by ones
          the number of ones tells the size
```

**Small example first:** a 32-byte region at `0x1000`.

```text
Step 1  start / 4             0x1000 >> 2 = 0x400   = 0b 0100 0000 0000
        (the register counts in 4-byte units)
Step 2  size / 8 - 1          32 / 8 - 1  = 3       = 0b 0000 0000 0011
        (a power of two minus one = a run of ones)
Step 3  OR them               0x400 | 0x3 = 0x403   = 0b 0100 0000 0011
```

**What the hardware does (decoding):**

```text
0x403 = 0b 0100 0000 0011
                       ^^ count the ones at the right end: k = 2
size  = 8 x 2^k = 8 x 4 = 32 bytes
start = (0x403 with those ones cleared) x 4 = 0x400 x 4 = 0x1000
```

Each extra 1 doubles the size:

```text
ones k:  0   1   2   3   ...  13      15
size:    8   16  32  64  ...  64 KB   256 KB
```

Why the ones do not damage the start: the start is a multiple of the size, so its low bits are already zero. The ones fill only empty bits.

**The firmware's real rules (same three steps):**

```text
rule 0 ACLINT  start 0x02000000, size 64 KB = 0x10000
       0x02000000 >> 2   = 0x00800000
       0x10000 / 8 - 1   = 0x1fff      (13 ones)
       OR                = 0x00801fff  <- banner: pmpaddr0 = 0x801fff

rule 1 my-sbi  start 0x80000000, size 256 KB = 0x40000
       0x80000000 >> 2   = 0x20000000
       0x40000 / 8 - 1   = 0x7fff      (15 ones)
       OR                = 0x20007fff  <- banner: pmpaddr1 = 0x20007fff

rule 2 all     all bits one = 0xffffffffffffffff
       "as many ones as possible" = the largest region = every address
```

In C (`fw_main.c`):

```c
#define PMP_NAPOT_ADDR(base, size)	(((base) >> 2) | ((size) / 8 - 1))
```

### 4.4 The permission byte (`pmpcfg0`)

Each rule also has one **byte** of settings. One byte = 8 bits:

```text
bit:    7     6  5    4  3     2   1   0
        L     (unused)  A      X   W   R
        lock            mode   execute write read
value:  0x80            see    0x4 0x2 0x1
                        below
```

The mode field `A` (bits 3-4) says how to read `pmpaddr`: `0` = rule off, `3` = NAPOT. `3` placed at bit 3 is `3 << 3 = 0x18`.

**Build each byte:**

```text
rule 0 (deny):   A=NAPOT                = 0x18
rule 1 (deny):   A=NAPOT                = 0x18
rule 2 (allow):  A=NAPOT + X + W + R    = 0x18 + 0x4 + 0x2 + 0x1 = 0x1f
```

**Pack the bytes.** `pmpcfg0` holds the bytes of rules 0-7, with rule 0 in the lowest byte:

```text
            byte 2   byte 1   byte 0
pmpcfg0 =   0x1f     0x18     0x18     = 0x1f1818   <- banner
            rule 2   rule 1   rule 0
```

In C: `0x18 | (0x18 << 8) | (0x1f << 16)`. Shifting by 8 moves a value one byte to the left.

**Write order:** `fw_pmp_init()` writes the three addresses first and `pmpcfg0` last. So no rule turns on before its address is correct. Then `sfence.vma` tells the CPU to drop old cached permissions, as the spec requires.

---

## 5. Delegation, Counters and Sstc

### 5.1 Delegation: which traps skip the firmware

**Problem:** by default **every** trap goes to M-mode, even a kernel page fault.

**Idea:** `medeleg` has one bit per exception cause (section 0.6). Bit N set means "cause N from S/U-mode goes straight to the kernel's `stvec`".

```text
delegated causes: 0 1 2 3 4 5 6 7 8   12 13 15
NOT delegated:    9  (ecall from S-mode = an SBI call; it must reach the firmware)
```

**Compute the value:**

```text
bits 0-8  = 0b1_1111_1111   = 0x01ff
bit 12    = 1 << 12         = 0x1000
bit 13    = 1 << 13         = 0x2000
bit 15    = 1 << 15         = 0x8000
sum                         = 0xb1ff   <- banner: medeleg
```

If bit 9 were set, the kernel's own SBI calls would land back in the kernel, and the firmware could never be reached.

**`mideleg`** does the same for interrupts. my-sbi sets the three S-mode interrupt bits:

```text
bit 1 (software) + bit 5 (timer) + bit 9 (external) = 0x2 + 0x20 + 0x200 = 0x222
```

The banner shows `0x1666`, not `0x222`. QEMU's CPU has the hypervisor extension, which **hard-wires** bits 2, 6, 10 and 12 to 1: `0x222 + 0x444 + 0x1000 = 0x1666`. Writing `0` reads back `0x1444` (tutor-run). Such bits are called **WARL**, "write any, read legal". **Rule: always read a CSR back.**

OpenSBI uses a different `medeleg`, `0xf0b509`: it keeps some faults so it can emulate missing hardware features. QEMU needs no emulation, so my-sbi delegates them (deviation, section 11).

### 5.2 Counters

`mcounteren = 0x7` sets bits 0, 1 and 2, so S-mode may read `cycle`, `time` and `instret`. The kernel needs `time` in K6.

### 5.3 Sstc: the kernel's own timer

`menvcfg` bit 63 (`STCE`) gives S-mode its own alarm register, `stimecmp`, so the kernel needs no SBI call to set a timer.

- The firmware **sets** the bit and **reads it back**. It stays 1 only if the CPU has Sstc.
- Then it writes `stimecmp = -1` (the largest value), so no timer fires before the kernel is ready.
- Banner `0xa000000000000000` = bit 63 (ours) + bit 61 (`ADUE`, already on in QEMU; it matters in K3).

Timers are taught in K6.

---

## 6. `mret`: Starting the Kernel

`mret` lowers the mode and jumps, in one instruction. Before it, the firmware prepares:

| Set before `mret` | Value | Why |
| --- | --- | --- |
| `mstatus.MPP` | 1 (S) | the mode to return to |
| `mepc` | `0x80200000` | where to jump |
| `a0`, `a1` | hart, DTB | the kernel's boot arguments |
| `satp` | 0 | MMU off |
| `stvec` | `0x80200000` | same as OpenSBI (K1b-2 explains why this matters) |
| `sie` | 0 | no S-mode interrupts yet |

After `mret`: mode = S and pc = `0x80200000`. The kernel's `_start` runs.

C cannot place values in specific registers, so `sbi_hart_switch_mode()` uses `register unsigned long a0 __asm__("a0") = arg0;`. That line means "keep this variable in `a0`".

---

## 7. Choosing the Boot Hart

All harts run `_start` at the same time. Only one may set up the machine.

```text
_boot_lottery = 0 (in memory)
hart 2: amoadd -> reads 0, memory becomes 1  -> winner
hart 0: amoadd -> reads 1, memory becomes 2  -> sleeps
hart 1: amoadd -> reads 2 ...                -> sleeps
```

`amoadd.w` is **atomic**: no two harts can read the same old value. The winner may be any hart. Tutor-run with `-smp 4`: hart 3 won once, so the kernel must not assume hart 0.

OpenSBI uses the same idea (`coldboot_lottery` in `sbi_init.c`).

---

## 8. The Trap Handler and `mscratch`

**Problem:** when the kernel does `ecall`, the CPU jumps to `mtvec`, but `sp` still points at the **kernel's** stack. The firmware needs its own stack before it can save anything.

**Idea:** keep the firmware's stack address in the spare CSR `mscratch` and swap it in.

```text
while the kernel runs:   mscratch = firmware stack top     sp = kernel stack

trap -> csrrw sp, mscratch, sp     swap
                                   sp = firmware stack      mscratch = kernel sp
        sp != 0 -> came from the kernel (if 0: trap came from firmware itself, swap back)
        save all registers on the firmware stack (struct sbi_trap_regs)
        mscratch = 0                (marks "inside the firmware")
        call sbi_trap_handler()
        mscratch = firmware stack top again
        restore registers, sp = kernel sp
        mret
```

`sbi_trap_handler()` reads `mcause`:
- 9 (SBI call): run it, then `mepc += 4`, so `mret` continues **after** the 4-byte `ecall` instead of repeating it;
- anything else: print `mcause`/`mepc`/`mtval` and power off with a failure.

---

## 9. SBI Calls

### 9.1 Calling convention

```text
kernel puts:  a7 = extension ID (which service)
              a6 = function ID  (which function)
              a0..a5 = arguments
              ecall
firmware puts: a0 = error (0 = success, negative = error)
               a1 = value
```

| Extension (`a7`) | Name | What my-sbi offers |
| --- | --- | --- |
| `0x10` | BASE | version, implementation ID, "do you have extension X?" |
| `0x4442434E` | DBCN | console write/read (the hex is ASCII "DBCN") |
| `0x53525354` | SRST | power off / reboot ("SRST") |
| `0x01`, `0x02`, `0x08` | legacy | putchar, getchar, shutdown (old SBI v0.1) |

TIME (timer through firmware) comes in K6, and HSM (start other harts) in K8.

### 9.2 Worked call: "do you have DBCN?"

```text
kernel:   a7 = 0x10  a6 = 3 (probe)  a0 = 0x4442434e  ecall
firmware: mcause = 9 -> BASE -> probe(DBCN) -> yes
          saved a0 = 0, saved a1 = 1, mepc += 4, mret
kernel:   a0 = 0, a1 = 1  -> prints "SBI DBCN extension detected"
```

### 9.3 Never trust an address from the kernel

DBCN gets a **physical** address of the text to print. A buggy kernel could pass the firmware's own address and leak its memory. `sbi_buffer_ok()` accepts only RAM above the firmware. Tutor-run: address `0x80000000` returned error `-3` (invalid parameter).

### 9.4 Power off

QEMU's test device at `0x100000`: writing `0x5555` exits QEMU with status 0, and `(1 << 16) | 0x3333` exits with status 1. SRST shutdown uses these values. This lets `make test` (K1b-3) finish by itself.

---

## 10. Files You Write

```text
my-linux/
|-- .gitignore                    replaces the K0 version
`-- firmware/
    |-- Makefile                  builds fw.bin, separately from the kernel
    |-- fw.lds.S                  linker script, address 0x80000000
    |-- fw_base.S                 entry, lottery, stacks, trap handler
    |-- fw_main.c                 PMP, delegation, counters, mret
    |-- sbi_trap.c                which trap is it?
    |-- sbi_ecall.c               the SBI calls; power off
    |-- sbi_console.c             print text and hex
    |-- uart8250.c                serial port driver
    `-- include/
        |-- fw_platform.h         board addresses and sizes
        |-- riscv_encoding.h      CSR bit names
        |-- riscv_asm.h           csr_read / csr_write / csr_set
        |-- sbi_ecall_interface.h SBI numbers
        |-- fw_dynamic.h          struct fw_dynamic_info
        |-- sbi_trap.h            saved-register layout
        `-- fw.h                  function prototypes
```

Save files with **LF** line endings (K0 section 3.11). The license is BSD-2-Clause, like OpenSBI.

### 10.1 `.gitignore`

```gitignore
*.o
*.d
/vmlinux
/vmlinux.map
/Image
/System.map
/test.log
/arch/riscv/kernel/vmlinux.lds
/firmware/fw.lds
/firmware/fw.elf
/firmware/fw.bin
/firmware/fw.map
/reference/
```

### 10.2 `firmware/include/fw_platform.h`

```c
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * QEMU virt platform constants for my-sbi. Usable from C, assembly and the
 * linker script. OpenSBI's generic platform reads these from the device tree;
 * my-sbi hard-codes them (a named deviation).
 */
#ifndef __FW_PLATFORM_H__
#define __FW_PLATFORM_H__

#ifdef __ASSEMBLY__
#define _UL(x)			x
#else
#define _UL(x)			x##UL
#endif

#define FW_BASE			_UL(0x80000000)	/* QEMU loads -bios here */
#define FW_SIZE			_UL(0x40000)	/* 256 KB, protected from S-mode by PMP */
#define FW_HART_COUNT_MAX	8
#define FW_STACK_SIZE		4096		/* per hart; also the M-mode trap stack */

#define PLAT_RAM_BASE		_UL(0x80000000)
#define PLAT_RAM_SIZE		_UL(0x8000000)	/* must match QEMU -m 128M */
#define PLAT_CLINT_BASE		_UL(0x2000000)	/* ACLINT MSWI + MTIMER */
#define PLAT_CLINT_SIZE		_UL(0x10000)
#define PLAT_UART_BASE		_UL(0x10000000)	/* NS16550A */
#define PLAT_TEST_BASE		_UL(0x100000)	/* sifive_test: power off / reset */

#endif /* __FW_PLATFORM_H__ */
```

`_UL(x)` adds `UL` in C only, so one header works for C, assembly and the linker script (like K0's `_AC`).

### 10.3 `firmware/include/riscv_encoding.h`

```c
/* SPDX-License-Identifier: BSD-2-Clause */
/* CSR bit fields used by my-sbi (names follow OpenSBI include/sbi/riscv_encoding.h). */
#ifndef __RISCV_ENCODING_H__
#define __RISCV_ENCODING_H__

#include <fw_platform.h>

/* Privilege levels as encoded in mstatus.MPP */
#define PRV_U			_UL(0)
#define PRV_S			_UL(1)
#define PRV_M			_UL(3)

#define MSTATUS_SIE		_UL(0x00000002)
#define MSTATUS_MIE		_UL(0x00000008)
#define MSTATUS_SPIE		_UL(0x00000020)
#define MSTATUS_MPIE		_UL(0x00000080)
#define MSTATUS_SPP		_UL(0x00000100)
#define MSTATUS_MPP_SHIFT	11
#define MSTATUS_MPP		_UL(0x00001800)
#define MSTATUS_MPRV		_UL(0x00020000)

/* Interrupt bits in mip/mie/mideleg */
#define MIP_SSIP		_UL(0x002)
#define MIP_STIP		_UL(0x020)
#define MIP_SEIP		_UL(0x200)

/* Exception codes in mcause (interrupt bit clear) */
#define CAUSE_MISALIGNED_FETCH		0
#define CAUSE_FETCH_ACCESS		1
#define CAUSE_ILLEGAL_INSTRUCTION	2
#define CAUSE_BREAKPOINT		3
#define CAUSE_MISALIGNED_LOAD		4
#define CAUSE_LOAD_ACCESS		5
#define CAUSE_MISALIGNED_STORE		6
#define CAUSE_STORE_ACCESS		7
#define CAUSE_USER_ECALL		8
#define CAUSE_SUPERVISOR_ECALL		9
#define CAUSE_MACHINE_ECALL		11
#define CAUSE_FETCH_PAGE_FAULT		12
#define CAUSE_LOAD_PAGE_FAULT		13
#define CAUSE_STORE_PAGE_FAULT		15

/* mcounteren: which counters S-mode may read */
#define MCOUNTEREN_CY		_UL(0x1)
#define MCOUNTEREN_TM		_UL(0x2)
#define MCOUNTEREN_IR		_UL(0x4)

/* menvcfg.STCE enables the Sstc stimecmp CSR for S-mode */
#define ENVCFG_STCE		(_UL(1) << 63)

/* One pmpcfg byte */
#define PMP_R			0x01
#define PMP_W			0x02
#define PMP_X			0x04
#define PMP_A_NAPOT		0x18
#define PMP_L			0x80

#endif /* __RISCV_ENCODING_H__ */
```

### 10.4 `firmware/include/riscv_asm.h`

```c
/* SPDX-License-Identifier: BSD-2-Clause */
/* CSR access helpers (same names as OpenSBI include/sbi/riscv_asm.h). */
#ifndef __RISCV_ASM_H__
#define __RISCV_ASM_H__

#define __ASM_STR(x)	#x

/* csrr: read a CSR into a register. */
#define csr_read(csr)							\
	({								\
		unsigned long __v;					\
		__asm__ __volatile__("csrr %0, " __ASM_STR(csr)		\
				     : "=r"(__v) : : "memory");		\
		__v;							\
	})

/* csrw: write a register (or a 5-bit immediate, constraint K) to a CSR. */
#define csr_write(csr, val)						\
	({								\
		unsigned long __v = (unsigned long)(val);		\
		__asm__ __volatile__("csrw " __ASM_STR(csr) ", %0"	\
				     : : "rK"(__v) : "memory");		\
	})

/* csrs: set the bits that are 1 in val. */
#define csr_set(csr, val)						\
	({								\
		unsigned long __v = (unsigned long)(val);		\
		__asm__ __volatile__("csrs " __ASM_STR(csr) ", %0"	\
				     : : "rK"(__v) : "memory");		\
	})

static inline void wfi(void)
{
	__asm__ __volatile__("wfi" : : : "memory");
}

#endif /* __RISCV_ASM_H__ */
```

How `csr_read(mcause)` works:
- `__ASM_STR(mcause)` becomes the text `"mcause"`, so the CPU instruction is `csrr <reg>, mcause`.
- `"=r"(__v)` means "GCC, pick a register for the output and put it in `__v`".
- `({ ... })` is a GCC block whose value is the last line (`__v`).
- `"memory"` stops GCC from moving loads and stores across this line.

### 10.5 `firmware/include/sbi_ecall_interface.h`, `fw_dynamic.h`, `sbi_trap.h`, `fw.h`

```c
/* SPDX-License-Identifier: BSD-2-Clause */
/* SBI calling convention constants (RISC-V SBI specification v2.0). */
#ifndef __SBI_ECALL_INTERFACE_H__
#define __SBI_ECALL_INTERFACE_H__

/* Extension IDs (a7) */
#define SBI_EXT_0_1_CONSOLE_PUTCHAR	0x1
#define SBI_EXT_0_1_CONSOLE_GETCHAR	0x2
#define SBI_EXT_0_1_SHUTDOWN		0x8
#define SBI_EXT_BASE			0x10
#define SBI_EXT_SRST			0x53525354	/* "SRST" */
#define SBI_EXT_DBCN			0x4442434E	/* "DBCN" */

/* BASE function IDs (a6) */
#define SBI_EXT_BASE_GET_SPEC_VERSION	0
#define SBI_EXT_BASE_GET_IMP_ID		1
#define SBI_EXT_BASE_GET_IMP_VERSION	2
#define SBI_EXT_BASE_PROBE_EXT		3
#define SBI_EXT_BASE_GET_MVENDORID	4
#define SBI_EXT_BASE_GET_MARCHID	5
#define SBI_EXT_BASE_GET_MIMPID		6

/* DBCN function IDs */
#define SBI_EXT_DBCN_CONSOLE_WRITE	0
#define SBI_EXT_DBCN_CONSOLE_READ	1
#define SBI_EXT_DBCN_CONSOLE_WRITE_BYTE	2

/* SRST function, types and reasons */
#define SBI_EXT_SRST_RESET		0
#define SBI_SRST_RESET_TYPE_SHUTDOWN	0
#define SBI_SRST_RESET_TYPE_COLD_REBOOT	1
#define SBI_SRST_RESET_TYPE_WARM_REBOOT	2
#define SBI_SRST_RESET_REASON_NONE	0
#define SBI_SRST_RESET_REASON_SYSFAIL	1

/* Spec version 2.0: major in bits 30:24, minor in bits 23:0 */
#define SBI_SPEC_VERSION		((2UL << 24) | 0)

/* Error codes returned in a0 */
#define SBI_SUCCESS			0
#define SBI_ERR_FAILURE			-1
#define SBI_ERR_NOT_SUPPORTED		-2
#define SBI_ERR_INVALID_PARAM		-3
#define SBI_ERR_DENIED			-4
#define SBI_ERR_INVALID_ADDRESS		-5

#endif /* __SBI_ECALL_INTERFACE_H__ */
```

```c
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * fw_dynamic handoff (OpenSBI include/sbi/fw_dynamic.h): the previous boot
 * stage (here the QEMU reset ROM) passes a2 = &struct fw_dynamic_info.
 */
#ifndef __FW_DYNAMIC_H__
#define __FW_DYNAMIC_H__

#define FW_DYNAMIC_INFO_MAGIC_VALUE	0x4942534f	/* "OSBI" */
#define FW_DYNAMIC_INFO_NEXT_MODE_U	0x0
#define FW_DYNAMIC_INFO_NEXT_MODE_S	0x1
#define FW_DYNAMIC_INFO_NEXT_MODE_M	0x3

struct fw_dynamic_info {
	unsigned long magic;
	unsigned long version;
	unsigned long next_addr;	/* entry of the next stage (the kernel) */
	unsigned long next_mode;	/* privilege of the next stage */
	unsigned long options;
	unsigned long boot_hart;	/* version >= 2: preferred boot hart, -1 = any */
};

#endif /* __FW_DYNAMIC_H__ */
```

```c
/* SPDX-License-Identifier: BSD-2-Clause */
/* M-mode trap frame (same layout as OpenSBI struct sbi_trap_regs). */
#ifndef __SBI_TRAP_H__
#define __SBI_TRAP_H__

/* Byte offsets used by fw_base.S: register xN is saved at N * 8. */
#define SBI_TRAP_REGS_MEPC	(32 * 8)
#define SBI_TRAP_REGS_MSTATUS	(33 * 8)
#define SBI_TRAP_REGS_SIZE	(34 * 8)	/* 272: keeps sp 16-byte aligned */

#ifndef __ASSEMBLY__
struct sbi_trap_regs {
	unsigned long zero, ra, sp, gp, tp;
	unsigned long t0, t1, t2;
	unsigned long s0, s1;
	unsigned long a0, a1, a2, a3, a4, a5, a6, a7;
	unsigned long s2, s3, s4, s5, s6, s7, s8, s9, s10, s11;
	unsigned long t3, t4, t5, t6;
	unsigned long mepc;
	unsigned long mstatus;
};

void sbi_trap_handler(struct sbi_trap_regs *regs);
#endif

#endif /* __SBI_TRAP_H__ */
```

Register `xN` is saved at byte `N x 8`. 32 registers x 8 = 256, so `mepc` is at 256 and `mstatus` at 264. The total of 272 bytes keeps `sp` a multiple of 16, as the ABI requires.

```c
/* SPDX-License-Identifier: BSD-2-Clause */
/* my-sbi internal interfaces. */
#ifndef __FW_H__
#define __FW_H__

#include <sbi_trap.h>

#define __noreturn	__attribute__((__noreturn__))

/* uart8250.c: platform console driver */
void uart8250_init(void);
void uart8250_putc(char ch);
int uart8250_getc(void);

/* sbi_console.c: generic console layer */
void sbi_putc(char ch);
void sbi_puts(const char *str);
void sbi_puthex(unsigned long val);
void sbi_print_kv(const char *key, unsigned long val);

/* sbi_ecall.c */
void sbi_ecall_handler(struct sbi_trap_regs *regs);
void __noreturn sbi_system_reset(unsigned long type, unsigned long reason);

#endif /* __FW_H__ */
```

### 10.6 `firmware/fw.lds.S`

```c
/* SPDX-License-Identifier: BSD-2-Clause */
#include <fw_platform.h>

OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
	/* M-mode runs without translation: link address == load address. */
	. = FW_BASE;

	.text : {
		KEEP(*(.entry))
		*(.text .text.*)
	}

	. = ALIGN(8);
	.rodata : {
		*(.rodata .rodata.*)
		*(.srodata .srodata.*)
	}

	.rela.dyn : {
		*(.rela .rela*)
	}

	. = ALIGN(8);
	.data : {
		*(.data .data.*)
		*(.sdata .sdata.*)
	}

	. = ALIGN(16);
	_fw_bss_start = .;
	.bss : {
		*(.sbss .sbss.*)
		*(.scommon)
		*(.bss .bss.*)
		*(COMMON)
	}
	. = ALIGN(16);
	_fw_bss_end = .;
	_fw_end = .;

	ASSERT(_fw_end <= FW_BASE + FW_SIZE, "my-sbi does not fit in its PMP region")

	.comment		0 : { *(.comment) }
	.riscv.attributes	0 : { *(.riscv.attributes) }
	.debug_aranges		0 : { *(.debug_aranges) }
	.debug_info		0 : { *(.debug_info) }
	.debug_abbrev		0 : { *(.debug_abbrev) }
	.debug_line		0 : { *(.debug_line) }
	.debug_frame		0 : { *(.debug_frame) }
	.debug_str		0 : { *(.debug_str) }
	.debug_line_str		0 : { *(.debug_line_str) }
	.debug_loclists		0 : { *(.debug_loclists) }
	.debug_rnglists		0 : { *(.debug_rnglists) }

	/DISCARD/ : {
		*(.eh_frame)
		*(.note.GNU-stack)
	}
}
```

Same rules as K0: every section is named. There is no `AT()`, because M-mode has no MMU and runs where it is linked. `ASSERT` stops the build if the firmware grows past its 256 KB PMP region.

### 10.7 `firmware/fw_base.S`

```asm
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * my-sbi entry and M-mode trap vector (compare OpenSBI firmware/fw_base.S).
 * Linked and run at 0x80000000 with no MMU in M-mode: link address == load address.
 */
#include <fw_platform.h>
#include <riscv_encoding.h>
#include <sbi_trap.h>

	.section .entry, "ax"
	.globl	_start
_start:
	/*
	 * QEMU's reset ROM at 0x1000 jumps here on EVERY hart with
	 * a0 = mhartid, a1 = DTB physical address, a2 = &fw_dynamic_info.
	 * The first hart to increment _boot_lottery becomes the boot hart.
	 */
	la	t0, _boot_lottery
	li	t1, 1
	amoadd.w t1, t1, (t0)		/* t1 = old value; only one hart sees 0 */
	bnez	t1, _secondary_park
	li	t0, FW_HART_COUNT_MAX
	bgeu	a0, t0, _secondary_park	/* no stack slot for this hart */

	/* Keep the handoff registers in callee-saved registers. */
	mv	s0, a0
	mv	s1, a1
	mv	s2, a2

	/* Report our own faults from now on. mscratch == 0 means "trap came from M-mode". */
	la	t0, _trap_handler
	csrw	mtvec, t0
	csrw	mscratch, zero
	csrw	mie, zero

	/* Clear .bss (includes .sbss and the stacks). */
	la	t0, _fw_bss_start
	la	t1, _fw_bss_end
	bgeu	t0, t1, 2f
1:	sd	zero, 0(t0)
	addi	t0, t0, 8
	bltu	t0, t1, 1b
2:
	/* sp = _fw_stacks + (hartid + 1) * FW_STACK_SIZE: the top of this hart's stack. */
	la	sp, _fw_stacks
	addi	t0, s0, 1
	li	t1, FW_STACK_SIZE
	mul	t0, t0, t1
	add	sp, sp, t0

	mv	a0, s0
	mv	a1, s1
	mv	a2, s2
	call	fw_main			/* does not return: it ends with mret */

_secondary_park:
	/* Other harts wait here; K8 adds SBI HSM to start them. */
	csrw	mie, zero
3:	wfi
	j	3b

/*
 * M-mode trap vector (mtvec MODE = direct, so it must be 4-byte aligned).
 * While S-mode runs, mscratch holds this hart's M-mode stack top.
 * While the firmware runs, mscratch is 0.
 */
	.text
	.balign	4
	.globl	_trap_handler
_trap_handler:
	csrrw	sp, mscratch, sp	/* sp <-> mscratch */
	bnez	sp, 4f
	csrrw	sp, mscratch, sp	/* came from M-mode: undo, keep using current sp */
4:
	addi	sp, sp, -SBI_TRAP_REGS_SIZE
	/* Save x1 and x3..x31 at offset N * 8. x2 (sp) is saved below. */
	.irp	n, 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	sd	x\n, \n * 8(sp)
	.endr
	csrr	t0, mscratch		/* interrupted sp (S-mode) or 0 (M-mode) */
	bnez	t0, 5f
	addi	t0, sp, SBI_TRAP_REGS_SIZE
5:	sd	t0, 2 * 8(sp)
	csrr	t0, mepc
	sd	t0, SBI_TRAP_REGS_MEPC(sp)
	csrr	t0, mstatus
	sd	t0, SBI_TRAP_REGS_MSTATUS(sp)
	csrw	mscratch, zero		/* now running firmware code */

	mv	a0, sp
	call	sbi_trap_handler	/* may change a0/a1/mepc in the frame */

	ld	t0, SBI_TRAP_REGS_MEPC(sp)
	csrw	mepc, t0
	ld	t0, SBI_TRAP_REGS_MSTATUS(sp)
	csrw	mstatus, t0
	/* Returning to S/U-mode: re-arm mscratch with the stack top. */
	li	t1, MSTATUS_MPP
	and	t0, t0, t1
	beq	t0, t1, 6f
	addi	t0, sp, SBI_TRAP_REGS_SIZE
	csrw	mscratch, t0
6:
	.irp	n, 1,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	ld	x\n, \n * 8(sp)
	.endr
	ld	sp, 2 * 8(sp)
	mret

	.data
	.balign	4
_boot_lottery:
	.word	0

	.section .bss.stacks, "aw", @nobits
	.balign	16
	.globl	_fw_stacks
_fw_stacks:
	.space	FW_HART_COUNT_MAX * FW_STACK_SIZE
```

**Trace for the boot hart (hart 0, `-smp 1`):**

```text
la t0,_boot_lottery; li t1,1; amoadd.w t1,t1,(t0)   t1 = 0 (old), memory = 1
bnez t1, park                                       not taken: winner
li t0,8; bgeu a0,t0, park                           0 < 8: not taken
mv s0,a0 / s1,a1 / s2,a2                            keep 0, 0x87e00000, 0x1028
csrw mtvec / mscratch=0 / mie=0
clear .bss loop                                     t0 from _fw_bss_start to _fw_bss_end
la sp,_fw_stacks; t0=(0+1)*4096; sp+=t0             sp = top of hart 0's stack
call fw_main                                        never returns (ends with mret)
```

Other details:
- `.irp n, 1,3,...,31` writes 30 `sd` lines, one per register. `x2` (`sp`) is skipped and saved separately.
- `_boot_lottery` is in `.data`, not `.bss`. The winner clears `.bss`, and that must not reset the lottery while other harts are still arriving.
- The trap handler is 4-byte aligned, because the low 2 bits of `mtvec` are a mode field.

### 10.8 `firmware/uart8250.c` and `firmware/sbi_console.c`

```c
// SPDX-License-Identifier: BSD-2-Clause
/* Minimal NS16550A driver (compare OpenSBI lib/utils/serial/uart8250.c). */
#include <fw_platform.h>
#include <fw.h>

#define UART_RBR	0	/* receive buffer (read) */
#define UART_THR	0	/* transmit holding (write) */
#define UART_IER	1	/* interrupt enable */
#define UART_FCR	2	/* FIFO control (write) */
#define UART_LCR	3	/* line control */
#define UART_LSR	5	/* line status */

#define UART_LCR_8N1	0x03
#define UART_FCR_ENABLE	0x07	/* enable FIFOs, clear RX and TX FIFOs */
#define UART_LSR_DR	0x01	/* a received byte is waiting */
#define UART_LSR_THRE	0x20	/* transmit holding register empty */

static volatile unsigned char *const uart = (volatile unsigned char *)PLAT_UART_BASE;

void uart8250_init(void)
{
	/* QEMU ignores the baud divisor, so it is not programmed here. */
	uart[UART_IER] = 0;		/* polled mode: the kernel owns UART interrupts later */
	uart[UART_LCR] = UART_LCR_8N1;
	uart[UART_FCR] = UART_FCR_ENABLE;
}

void uart8250_putc(char ch)
{
	while (!(uart[UART_LSR] & UART_LSR_THRE))
		;
	uart[UART_THR] = ch;
}

int uart8250_getc(void)
{
	if (uart[UART_LSR] & UART_LSR_DR)
		return uart[UART_RBR];
	return -1;
}
```

```c
// SPDX-License-Identifier: BSD-2-Clause
/* Console layer above the UART driver (compare OpenSBI lib/sbi/sbi_console.c). */
#include <fw.h>

void sbi_putc(char ch)
{
	if (ch == '\n')
		uart8250_putc('\r');
	uart8250_putc(ch);
}

void sbi_puts(const char *str)
{
	while (*str)
		sbi_putc(*str++);
}

/* Hexadecimal with a 0x prefix and no leading zeros. */
void sbi_puthex(unsigned long val)
{
	static const char digits[] = "0123456789abcdef";
	int shift = 60;

	sbi_puts("0x");
	while (shift > 0 && !(val >> shift))
		shift -= 4;
	for (; shift >= 0; shift -= 4)
		sbi_putc(digits[(val >> shift) & 0xf]);
}

void sbi_print_kv(const char *key, unsigned long val)
{
	sbi_puts("my-sbi: ");
	sbi_puts(key);
	sbi_puts(" = ");
	sbi_puthex(val);
	sbi_puts("\n");
}
```

The driver knows the chip's registers. The console layer adds `\r` before `\n` and prints hex.

### 10.9 `firmware/fw_main.c`

```c
// SPDX-License-Identifier: BSD-2-Clause
/*
 * my-sbi cold boot: prepare M-mode state for the S-mode kernel, then mret.
 * Compare OpenSBI lib/sbi/sbi_init.c, sbi_hart.c and sbi_hart_switch_mode().
 */
#include <fw_platform.h>
#include <riscv_encoding.h>
#include <riscv_asm.h>
#include <fw_dynamic.h>
#include <sbi_ecall_interface.h>
#include <fw.h>

/*
 * NAPOT pmpaddr for a region that is a power of two in size (at least 8 bytes)
 * and starts at a multiple of its size: the address divided by 4, with the
 * low bits filled with ones. The number of ones tells the hardware the size.
 */
#define PMP_NAPOT_ADDR(base, size)	(((base) >> 2) | ((size) / 8 - 1))

extern char _fw_stacks[];

static void __noreturn fw_fatal(const char *msg)
{
	sbi_puts("my-sbi: FATAL: ");
	sbi_puts(msg);
	sbi_puts("\n");
	sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_SYSFAIL);
}

/*
 * The lowest-numbered matching entry wins. L = 0, so the entries restrict
 * only S-mode and U-mode; M-mode keeps full access.
 *   entry 0: ACLINT  -> no access (S-mode uses Sstc, not the CLINT)
 *   entry 1: my-sbi  -> no access
 *   entry 2: all     -> RWX
 */
static void fw_pmp_init(void)
{
	csr_write(pmpaddr0, PMP_NAPOT_ADDR(PLAT_CLINT_BASE, PLAT_CLINT_SIZE));
	csr_write(pmpaddr1, PMP_NAPOT_ADDR(FW_BASE, FW_SIZE));
	csr_write(pmpaddr2, -1UL);	/* NAPOT, all ones: the whole address space */
	csr_write(pmpcfg0, PMP_A_NAPOT |
			   (PMP_A_NAPOT << 8) |
			   ((unsigned long)(PMP_A_NAPOT | PMP_R | PMP_W | PMP_X) << 16));
	/* Privileged spec: synchronise PMP changes with translation caches. */
	__asm__ __volatile__("sfence.vma" : : : "memory");
}

/*
 * Delegated traps go straight to the kernel's stvec. ecall from S-mode
 * (cause 9) is NOT delegated: it is how the kernel calls this firmware.
 */
static void fw_deleg_init(void)
{
	csr_write(mideleg, MIP_SSIP | MIP_STIP | MIP_SEIP);
	csr_write(medeleg, (1UL << CAUSE_MISALIGNED_FETCH) |
			   (1UL << CAUSE_FETCH_ACCESS) |
			   (1UL << CAUSE_ILLEGAL_INSTRUCTION) |
			   (1UL << CAUSE_BREAKPOINT) |
			   (1UL << CAUSE_MISALIGNED_LOAD) |
			   (1UL << CAUSE_LOAD_ACCESS) |
			   (1UL << CAUSE_MISALIGNED_STORE) |
			   (1UL << CAUSE_STORE_ACCESS) |
			   (1UL << CAUSE_USER_ECALL) |
			   (1UL << CAUSE_FETCH_PAGE_FAULT) |
			   (1UL << CAUSE_LOAD_PAGE_FAULT) |
			   (1UL << CAUSE_STORE_PAGE_FAULT));
}

/* Returns 1 if the hart implements Sstc. */
static int fw_counters_init(void)
{
	/* S-mode may read cycle, time and instret. */
	csr_write(mcounteren, MCOUNTEREN_CY | MCOUNTEREN_TM | MCOUNTEREN_IR);

	/* WARL probe: the bit stays set only if Sstc exists. */
	csr_set(menvcfg, ENVCFG_STCE);
	if (!(csr_read(menvcfg) & ENVCFG_STCE))
		return 0;
	csr_write(stimecmp, -1UL);	/* no S-mode timer interrupt pending at handoff */
	return 1;
}

static void __noreturn sbi_hart_switch_mode(unsigned long arg0, unsigned long arg1,
					    unsigned long next_addr, unsigned long next_mode)
{
	unsigned long val = csr_read(mstatus);

	val &= ~(MSTATUS_MPP | MSTATUS_MPIE | MSTATUS_MPRV | MSTATUS_SIE);
	val |= next_mode << MSTATUS_MPP_SHIFT;	/* mret drops to this mode */
	csr_write(mstatus, val);
	csr_write(mepc, next_addr);		/* mret jumps here */
	csr_write(stvec, next_addr);
	csr_write(sscratch, 0);
	csr_write(sie, 0);
	csr_write(satp, 0);			/* the kernel starts with the MMU off */

	register unsigned long a0 __asm__("a0") = arg0;
	register unsigned long a1 __asm__("a1") = arg1;
	__asm__ __volatile__("mret" : : "r"(a0), "r"(a1));
	__builtin_unreachable();
}

void __noreturn fw_main(unsigned long hartid, unsigned long dtb,
			struct fw_dynamic_info *info)
{
	int sstc;

	uart8250_init();
	sbi_puts("\nmy-sbi: learner M-mode firmware (fw_dynamic handoff)\n");
	sbi_print_kv("boot hart        ", hartid);
	sbi_print_kv("dtb              ", dtb);
	sbi_print_kv("fw_dynamic_info  ", (unsigned long)info);
	if (info->magic != FW_DYNAMIC_INFO_MAGIC_VALUE)
		fw_fatal("bad fw_dynamic_info magic");
	sbi_print_kv("  version        ", info->version);
	sbi_print_kv("  next_addr      ", info->next_addr);
	sbi_print_kv("  next_mode      ", info->next_mode);
	sbi_print_kv("  boot_hart      ", info->boot_hart);
	if (info->next_mode != FW_DYNAMIC_INFO_NEXT_MODE_S)
		fw_fatal("only an S-mode next stage is supported");

	fw_pmp_init();
	fw_deleg_init();
	sstc = fw_counters_init();

	sbi_print_kv("pmpaddr0         ", csr_read(pmpaddr0));
	sbi_print_kv("pmpaddr1         ", csr_read(pmpaddr1));
	sbi_print_kv("pmpaddr2         ", csr_read(pmpaddr2));
	sbi_print_kv("pmpcfg0          ", csr_read(pmpcfg0));
	sbi_print_kv("medeleg          ", csr_read(medeleg));
	sbi_print_kv("mideleg          ", csr_read(mideleg));
	sbi_print_kv("mcounteren       ", csr_read(mcounteren));
	sbi_print_kv("menvcfg          ", csr_read(menvcfg));
	sbi_puts(sstc ? "my-sbi: Sstc present, S-mode owns stimecmp\n" :
			"my-sbi: no Sstc\n");
	sbi_puts("my-sbi: mret to S-mode\n\n");

	/* From now on, traps from S-mode run on this hart's stack top. */
	csr_write(mscratch, (unsigned long)_fw_stacks + (hartid + 1) * FW_STACK_SIZE);
	sbi_hart_switch_mode(hartid, dtb, info->next_addr, PRV_S);
}
```

`mscratch` is set last, just before `mret`. Once it is non-zero, a trap would use the stack top. That is safe only when `fw_main` is about to leave for good.

### 10.10 `firmware/sbi_trap.c` and `firmware/sbi_ecall.c`

```c
// SPDX-License-Identifier: BSD-2-Clause
/* C side of the M-mode trap vector (compare OpenSBI lib/sbi/sbi_trap.c). */
#include <riscv_encoding.h>
#include <riscv_asm.h>
#include <sbi_ecall_interface.h>
#include <fw.h>

static void __noreturn sbi_trap_fatal(struct sbi_trap_regs *regs, unsigned long mcause)
{
	sbi_puts("\nmy-sbi: unexpected trap\n");
	sbi_print_kv("mcause ", mcause);
	sbi_print_kv("mepc   ", regs->mepc);
	sbi_print_kv("mtval  ", csr_read(mtval));
	sbi_print_kv("from   ", (regs->mstatus & MSTATUS_MPP) >> MSTATUS_MPP_SHIFT);
	sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_SYSFAIL);
}

void sbi_trap_handler(struct sbi_trap_regs *regs)
{
	unsigned long mcause = csr_read(mcause);

	if (mcause == CAUSE_SUPERVISOR_ECALL) {
		sbi_ecall_handler(regs);
		regs->mepc += 4;	/* resume after the 4-byte ecall */
		return;
	}
	/* Everything else is either delegated to S-mode or a bug. */
	sbi_trap_fatal(regs, mcause);
}
```

```c
// SPDX-License-Identifier: BSD-2-Clause
/*
 * SBI call dispatch (compare OpenSBI lib/sbi/sbi_ecall*.c).
 * Calling convention: a7 = extension ID, a6 = function ID, a0..a5 = arguments.
 * Return: a0 = error code, a1 = value (legacy v0.1 calls: a0 only).
 */
#include <fw_platform.h>
#include <riscv_asm.h>
#include <sbi_ecall_interface.h>
#include <fw.h>

#define SBI_IMPL_ID_MY_SBI	0x4d59	/* "MY": not a registered SBI implementation ID */
#define SBI_IMPL_VERSION	0x1

/* QEMU sifive_test device */
#define FINISHER_FAIL		0x3333
#define FINISHER_PASS		0x5555
#define FINISHER_RESET		0x7777

void sbi_system_reset(unsigned long type, unsigned long reason)
{
	volatile unsigned int *test = (volatile unsigned int *)PLAT_TEST_BASE;

	if (type == SBI_SRST_RESET_TYPE_SHUTDOWN)
		*test = reason == SBI_SRST_RESET_REASON_SYSFAIL ?
			(1 << 16) | FINISHER_FAIL : FINISHER_PASS;	/* QEMU exit 1 or 0 */
	else
		*test = FINISHER_RESET;
	while (1)
		wfi();
}

/* S-mode buffers must be RAM outside the firmware (OpenSBI checks its domain). */
static int sbi_buffer_ok(unsigned long addr, unsigned long size)
{
	unsigned long start = FW_BASE + FW_SIZE;
	unsigned long end = PLAT_RAM_BASE + PLAT_RAM_SIZE;

	return addr >= start && addr <= end && size <= end - addr;
}

static unsigned long sbi_ecall_probe(unsigned long eid)
{
	switch (eid) {
	case SBI_EXT_0_1_CONSOLE_PUTCHAR:
	case SBI_EXT_0_1_CONSOLE_GETCHAR:
	case SBI_EXT_0_1_SHUTDOWN:
	case SBI_EXT_BASE:
	case SBI_EXT_DBCN:
	case SBI_EXT_SRST:
		return 1;
	default:
		return 0;
	}
}

static long sbi_ecall_base(unsigned long fid, struct sbi_trap_regs *regs,
			   unsigned long *out)
{
	switch (fid) {
	case SBI_EXT_BASE_GET_SPEC_VERSION:
		*out = SBI_SPEC_VERSION;
		break;
	case SBI_EXT_BASE_GET_IMP_ID:
		*out = SBI_IMPL_ID_MY_SBI;
		break;
	case SBI_EXT_BASE_GET_IMP_VERSION:
		*out = SBI_IMPL_VERSION;
		break;
	case SBI_EXT_BASE_PROBE_EXT:
		*out = sbi_ecall_probe(regs->a0);
		break;
	case SBI_EXT_BASE_GET_MVENDORID:
		*out = csr_read(mvendorid);
		break;
	case SBI_EXT_BASE_GET_MARCHID:
		*out = csr_read(marchid);
		break;
	case SBI_EXT_BASE_GET_MIMPID:
		*out = csr_read(mimpid);
		break;
	default:
		return SBI_ERR_NOT_SUPPORTED;
	}
	return SBI_SUCCESS;
}

static long sbi_ecall_dbcn(unsigned long fid, struct sbi_trap_regs *regs,
			   unsigned long *out)
{
	unsigned long num = regs->a0, addr = regs->a1, i;
	char *buf = (char *)addr;	/* a PHYSICAL address: M-mode does not translate */
	int ch;

	switch (fid) {
	case SBI_EXT_DBCN_CONSOLE_WRITE:
	case SBI_EXT_DBCN_CONSOLE_READ:
		/* a2 = upper XLEN bits of the address: always 0 on RV64 */
		if (regs->a2 || !sbi_buffer_ok(addr, num))
			return SBI_ERR_INVALID_PARAM;
		if (fid == SBI_EXT_DBCN_CONSOLE_WRITE) {
			for (i = 0; i < num; i++)
				sbi_putc(buf[i]);
		} else {
			for (i = 0; i < num && (ch = uart8250_getc()) >= 0; i++)
				buf[i] = ch;
		}
		*out = i;		/* bytes written or read */
		return SBI_SUCCESS;
	case SBI_EXT_DBCN_CONSOLE_WRITE_BYTE:
		sbi_putc(regs->a0 & 0xff);
		return SBI_SUCCESS;
	default:
		return SBI_ERR_NOT_SUPPORTED;
	}
}

static long sbi_ecall_srst(unsigned long fid, struct sbi_trap_regs *regs)
{
	unsigned long type = regs->a0, reason = regs->a1;

	if (fid != SBI_EXT_SRST_RESET)
		return SBI_ERR_NOT_SUPPORTED;
	if (type > SBI_SRST_RESET_TYPE_WARM_REBOOT ||
	    reason > SBI_SRST_RESET_REASON_SYSFAIL)
		return SBI_ERR_INVALID_PARAM;
	sbi_system_reset(type, reason);
}

void sbi_ecall_handler(struct sbi_trap_regs *regs)
{
	unsigned long eid = regs->a7, fid = regs->a6, out = 0;
	long ret;

	switch (eid) {
	case SBI_EXT_0_1_CONSOLE_PUTCHAR:
		sbi_putc(regs->a0);
		regs->a0 = 0;
		return;
	case SBI_EXT_0_1_CONSOLE_GETCHAR:
		regs->a0 = uart8250_getc();
		return;
	case SBI_EXT_0_1_SHUTDOWN:
		sbi_system_reset(SBI_SRST_RESET_TYPE_SHUTDOWN, SBI_SRST_RESET_REASON_NONE);
	case SBI_EXT_BASE:
		ret = sbi_ecall_base(fid, regs, &out);
		break;
	case SBI_EXT_DBCN:
		ret = sbi_ecall_dbcn(fid, regs, &out);
		break;
	case SBI_EXT_SRST:
		ret = sbi_ecall_srst(fid, regs);
		break;
	default:
		ret = SBI_ERR_NOT_SUPPORTED;
		break;
	}
	regs->a0 = ret;
	regs->a1 = out;
}
```

`sbi_ecall_srst()` has no `return` after `sbi_system_reset()`, and the legacy shutdown case has no `break`. That is fine because `sbi_system_reset()` is marked `__noreturn` in `fw.h`, so GCC knows it never comes back.

### 10.11 `firmware/Makefile`

```make
# SPDX-License-Identifier: BSD-2-Clause
# my-sbi: a separate M-mode firmware image, built independently of the kernel
# (like OpenSBI's fw_dynamic.bin). Same safety flags as the kernel.

CROSS_COMPILE ?= riscv64-linux-gnu-
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy

ARCH_FLAGS := -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany

CPPFLAGS := -nostdinc -isystem $(shell $(CC) -print-file-name=include) -Iinclude

CFLAGS := $(ARCH_FLAGS) -std=gnu11 -O2 -g -Wall -Werror \
	  -ffreestanding -fno-common -fno-PIE -fno-pic \
	  -fno-strict-aliasing -fno-stack-protector -fno-omit-frame-pointer \
	  -fno-asynchronous-unwind-tables -fno-unwind-tables \
	  -mno-save-restore -mstrict-align -mno-relax \
	  -mno-riscv-attribute -Wa,-mno-arch-attr

AFLAGS := $(ARCH_FLAGS) -D__ASSEMBLY__ -fno-PIE -g -mno-relax \
	  -mno-riscv-attribute -Wa,-mno-arch-attr

LDFLAGS := -nostdlib --orphan-handling=error -z noexecstack --no-warn-rwx-segments

DEPFLAGS = -MMD -MP

objs := fw_base.o fw_main.o sbi_trap.o sbi_ecall.o sbi_console.o uart8250.o

all: fw.bin

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CPPFLAGS) $(AFLAGS) $(DEPFLAGS) -c $< -o $@

fw.lds: fw.lds.S
	$(CC) $(CPPFLAGS) -D__ASSEMBLY__ -E -P -x c $(DEPFLAGS) -MT $@ -MF $@.d $< -o $@

fw.elf: $(objs) fw.lds
	$(LD) $(LDFLAGS) -T fw.lds -Map=fw.map $(objs) -o $@

fw.bin: fw.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f $(objs) $(objs:.o=.d) fw.lds fw.lds.d fw.elf fw.bin fw.map

-include $(objs:.o=.d) fw.lds.d

.PHONY: all clean
```

---

## 11. Build, Run, Experiment

From the `my-linux` root, inside the container:

```sh
make -C firmware        # builds firmware/fw.bin
make                    # your K0 kernel, unchanged
qemu-system-riscv64 -machine virt -cpu rv64 -m 128M -smp 1 -nographic \
    -bios firmware/fw.bin -kernel Image          # quit: Ctrl-A then x
```

Expected: the banner from section 3.2, then `K0 layout: PASS`.

**GDB on the firmware.** Firmware addresses equal load addresses, so ordinary breakpoints work:

```sh
qemu-system-riscv64 -machine virt -cpu rv64 -m 128M -smp 1 -nographic \
    -bios firmware/fw.bin -kernel Image -s -S < /dev/null > /tmp/qemu.log 2>&1 &
gdb-multiarch -q firmware/fw.elf -ex 'target remote :1234'
(gdb) x/6i 0x1000        # reset ROM, section 3.1
(gdb) break fw_main
(gdb) continue
(gdb) info registers a0 a1 a2      # expect 0, 0x87e00000, 0x1028
```

**Experiments (tutor-verified results):**
1. Remove rule 2: set `pmpaddr2` to 0 and take `0x1f << 16` out of `pmpcfg0`. Result: `pmpcfg0 = 0x1818`, and the kernel faults forever at `0x80200000` (see it with `qemu ... -d int -D /tmp/int.log`).
2. Write `mideleg = 0`. Read back: `0x1444`.
3. Boot `-smp 4` several times and watch the boot hart change.

**Deviations from OpenSBI:**

| my-sbi | OpenSBI v1.3 | Why |
| --- | --- | --- |
| Addresses fixed in `fw_platform.h` | Reads them from the device tree | No device tree parser in firmware |
| Delegates illegal-instruction and access faults | Keeps them to emulate features | QEMU needs no emulation |
| One lottery | Separate relocation and boot lotteries | Simpler, same visible result |
| No `/reserved-memory` node added to the DTB | Adds one for its own memory | K4 must keep `0x80000000-0x801fffff` away from the allocator |
| No TIME, IPI, HSM | Implemented | Added in K6 and K8 |

---

## 12. Acceptance Checklist

- [ ] 15 firmware files and `.gitignore` typed, saved with LF.
- [ ] `make -C firmware` has no warnings. `nm firmware/fw.elf` shows `_start` at `0x80000000`.
- [ ] K0 kernel boots under my-sbi: `K0 layout: PASS`.
- [ ] Banner matches: `pmpaddr0 0x801fff`, `pmpaddr1 0x20007fff`, `pmpcfg0 0x1f1818`, `medeleg 0xb1ff`, `mideleg 0x1666`.
- [ ] `-smp 4` passes, including a run with a non-zero boot hart.
- [ ] GDB shows the ROM at `0x1000`.
- [ ] Continue with [K1b-1](K01b1_talk_to_firmware_sbi_ecall.md).

---

## 13. Arm64 Comparison

| RISC-V | Arm64 |
| --- | --- |
| M-mode | EL3 |
| OpenSBI / my-sbi | TF-A BL31 |
| `ecall`, SBI | `smc`, SMCCC/PSCI |
| SRST power off | PSCI `SYSTEM_OFF` |
| HSM hart start | PSCI `CPU_ON` |
| `medeleg`/`mideleg` | `SCR_EL3`, `HCR_EL2` routing |
| PMP | TrustZone controllers, Stage 2 tables |
| `mret` with `MPP`, `mepc` | `eret` with `SPSR_EL3`, `ELR_EL3` |

---

## 14. Interview Answers

**Q1. What does OpenSBI do before the kernel starts?**
It runs in M-mode and picks a boot hart. It sets PMP so the kernel cannot touch firmware memory, delegates page faults and S-mode interrupts to the kernel, and enables counters and Sstc. Then it uses `mret` to enter the kernel in S-mode with `a0 = hart` and `a1 = DTB`. It stays resident to serve SBI calls.

**Q2. Why is S-mode `ecall` not delegated?**
It is how the kernel calls the firmware. If it were delegated, the call would trap back into the kernel itself.

**Q3. Explain PMP NAPOT with an example.**
A power-of-two region at an aligned start fits in one register: `(start >> 2) | (size/8 - 1)`. For 32 bytes at `0x1000` that is `0x403`: two trailing ones mean `8 x 2^2 = 32` bytes. Rules are checked in order, the first match wins, and an S-mode access with no match is denied.

**Q4. Why did `mideleg` read back `0x1666`?**
Some of its bits are hard-wired to 1 by the hypervisor extension (WARL). Firmware should read CSRs back instead of trusting the write.

**Q5. How does the firmware get a stack on a trap from the kernel?**
`mscratch` holds the firmware stack top while the kernel runs. The handler swaps it with `sp`, saves the registers, and sets `mscratch = 0` while inside the firmware. It restores everything before `mret`.

**Q6. Why must firmware check addresses the kernel passes?**
M-mode can access all physical memory. An unchecked address would let the kernel read or write firmware memory through an SBI call.

**Q7. SBI versus PSCI?**
Both are calling conventions from the OS to firmware. SBI (`ecall`) covers console, timer, IPI, fences, harts and reset. PSCI (`smc`) mainly covers CPU and system power. On Arm, timers and IPIs do not go through firmware.

---

## 15. Corrections

- **2026-10-22, revision.** Rewritten after learner feedback: added section 0 (prerequisites) and the small-number NAPOT and `pmpcfg` walkthroughs. `PMP_NAPOT_ADDR` changed to the equivalent `(base >> 2) | (size/8 - 1)`. Both forms give the same values, re-verified: `pmpaddr0 0x801fff`, `pmpaddr1 0x20007fff`. The banner now prints `pmpaddr0-2`, and `fw.bin` grew from 3108 to 3244 bytes. Removed one unverified remark, that the hardware keeps only 54 bits of `pmpaddr2`: QEMU reads back all 64 ones.
