# Supervisor Trap Architecture, Register Frames, and Hardware Delegation

## 1. The Hardware Trap Mechanism

When an interrupt or exception occurs while executing in Supervisor Mode, the RISC-V hardware state machine performs the following actions atomically in hardware:
1. **`sepc` (Supervisor Exception Program Counter)**: Stores the $\text{PC}$ of the interrupted instruction.
2. **`scause` (Supervisor Cause)**: Stores the trap reason. The highest bit (Bit 63 on RV64) indicates whether the trap is an asynchronous **Interrupt** ($1$) or a synchronous **Exception** ($0$). The remaining bits define the exact cause code.
3. **`stval` (Supervisor Trap Value)**: Stores exception-specific data (e.g., the bad virtual address on a page fault, or the invalid instruction opcode).
4. **`sstatus` (Supervisor Status)**:
   - Saves `sstatus.SIE` (Supervisor Interrupt Enable) into `sstatus.SPIE` (Previous Interrupt Enable).
   - Clears `sstatus.SIE = 0`, automatically masking supervisor interrupts to prevent nested traps.
   - Saves the previous privilege mode into `sstatus.SPP`.
5. **Program Counter ($\text{PC}$)**: Jumps to the address stored in the **`stvec` (Supervisor Trap Vector Base Address)** register.

---

## 2. The 288-Byte Register Frame (`struct pt_regs`)

Unlike architectures with hardware auto-stacking (such as ARM Cortex-M or x86), RISC-V does not automatically push registers to the stack. Software must save all 31 general-purpose registers ($x1$ through $x31$) before calling any C code; otherwise, compiler-generated code will overwrite active variables in the interrupted context.

```text
Stack Offset (sp)       Saved Field             Description
----------------------------------------------------------------------
0x00 (0)                sepc                    Interrupted PC
0x08 (8)                ra (x1)                 Return Address
0x10 (16)               sp (x2)                 Original Stack Pointer
0x18 (24)               gp (x3)                 Global Pointer
0x20 (32)               tp (x4)                 Thread Pointer
0x28 - 0x38 (40-56)     t0 - t2 (x5-x7)         Temporary Registers 0-2
0x40 - 0x48 (64-72)     s0 - s1 (x8-x9)         Saved Registers / Frame Pointer
0x50 - 0x88 (80-136)    a0 - a7 (x10-x17)       Function Arguments / Syscall ID
0x90 - 0xD8 (144-216)   s2 - s11 (x18-x27)      Saved Registers 2-11
0xE0 - 0xF8 (224-248)   t3 - t6 (x28-x31)       Temporary Registers 3-6
0x100 (256)             sstatus                 Status Register
0x108 (264)             stval                   Bad Virtual Address / Trap Value
0x110 (272)             scause                  Trap Cause Code
0x118 (280)             orig_a0                 Original a0 before Syscall Overwrite
----------------------------------------------------------------------
Total Size = 36 words * 8 bytes = 288 bytes (PT_SIZE)
```

---

## 3. The Dual Role of `a0` and Why `orig_a0` is Essential

### The Problem
Under the RISC-V calling convention:
- **On System Call Entry**: Register `a0` holds the 1st input argument (e.g., file descriptor `fd = 3`).
- **On System Call Exit**: Register `a0` holds the return value (e.g., number of bytes read: `ret = 6`).

When a system call completes, the kernel overwrites the saved slot: `regs->a0 = ret`.

### What Breaks Without `orig_a0`?
If a system call is interrupted mid-execution by an asynchronous signal (e.g., `SIGWINCH`), the kernel sets `regs->a0 = -ERESTARTSYS`.
Once the signal handler finishes, the kernel wants to transparently restart the system call for the user (`SA_RESTART`):
- It decrements `regs->epc -= 4` to point back to the `ecall` instruction.
- It must restore the original arguments (`a0 = 3`).
- If `regs->a0` was already overwritten with `-ERESTARTSYS`, the original argument is permanently lost.
- With `regs->orig_a0`, the kernel restores `regs->a0 = regs->orig_a0;`, allowing seamless system call restart.
- Additionally, `orig_a0` allows debugging tools like `strace` to log both the input argument and the return value at syscall exit (`read(3, ...) = 6`).

---

## 4. Why S-Mode `ecall` Bypassed `stvec` to OpenSBI

When executing `__asm__ volatile("ecall");` in S-mode:
- The CPU did not jump to `stvec`.
- It jumped to OpenSBI in Machine Mode (`mtvec`).

### Root Cause: OpenSBI `MEDELEG` Configuration
OpenSBI sets `MEDELEG = 0x00f0b509`:
- **Bit 3**: Breakpoint (`ebreak`) $\rightarrow$ **Delegated to S-mode**
- **Bit 8**: U-mode `ecall` (User System Calls) $\rightarrow$ **Delegated to S-mode**
- **Bit 9**: S-mode `ecall` $\rightarrow$ **NOT DELEGATED (Bit 9 = 0)**

In RISC-V, S-mode `ecall` is the dedicated communication channel from the operating system to firmware (SBI calls: timer, console, IPI, reset). OpenSBI retains ownership of Bit 9 so it can service kernel requests.

---

## 5. The Compressed Instruction Bug: 16-bit vs 32-bit Instructions

When testing `stvec` via `ebreak`, advancing the Program Counter unconditionally by 4 bytes caused a crash:

```text
Disassembly:
  a4:   9002         ebreak          # 16-bit compressed instruction (2 bytes)!
  a6:   00000517     auipc a0, 0x0   # Next instruction begins at 0xa6
```

If `regs->epc += 4` is executed:
- `epc` advances from `0xa4` to `0xa8` (`0xa4 + 4`).
- Address `0xa8` is halfway inside the 32-bit `auipc` instruction.
- The CPU decodes invalid bits, triggering an **Illegal Instruction Exception (`scause = 2`)** and fatal halt.

### The RISC-V Variable Instruction Length Rule
Under the RISC-V ISA, examining the lowest 2 bits of any instruction word identifies its length:
- `(insn & 0x3) == 0x3`: Standard **32-bit (4-byte)** instruction.
- `(insn & 0x3) != 0x3`: Compressed **16-bit (2-byte)** instruction.

```c
uint16_t insn = *(volatile uint16_t *)regs->epc;
if ((insn & 3) == 3) {
    regs->epc += 4; /* Standard 32-bit instruction */
} else {
    regs->epc += 2; /* Compressed 16-bit instruction */
}
```
With dynamic instruction length detection, `ebreak` returns cleanly without faulting.
