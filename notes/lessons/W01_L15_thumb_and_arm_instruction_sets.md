# W01-L15: Thumb, Arm Instruction Sets, And Important Cortex-M4 Instructions

**Status:** Source-reviewed, 2026-11-01: local Arm DUI0553B sections 1.1,
2.1.3 and 3.1; existing P1 compiler rule and disassembly; GCC Arm options.
Examples below are explanatory, not new compiler output or measured execution.
This is a concept-only note: no implementation, build or board run is needed.
Publication does not establish learner completion. Quizzes remain paused.

## Goal

Understand what an instruction set is, why Arm has `A32`, `T32` (Thumb / Thumb-2)
and `A64`, why our STM32 Cortex-M4 executes Thumb only, and what each important
Cortex-M4 assembly instruction in our bare-metal and FreeRTOS lessons does.

**Main answer:** Thumb (`T32`) is a compact way of encoding CPU instructions
using a mix of `16-bit` (`2-byte`) and `32-bit` (`4-byte`) instructions that
operate on `32-bit` registers. Your STM32F446RE's Cortex-M4 executes Thumb only.

## Know This First

| Term | Simple meaning |
| --- | --- |
| Instruction | One CPU command, such as load a number, add two registers, read memory, or branch |
| Instruction set | The complete set of commands a CPU understands, including their binary bit patterns |
| Encoding | The exact binary bit pattern stored in Flash memory for an instruction |
| Register | A fast 32-bit storage location inside the CPU core, such as `R0` through `R12`, `SP`, `LR`, or `PC` |
| Assembly | Human-readable names for machine instructions, such as `movs`, `ldr`, `mrs`, or `bx lr` |
| Compiler / assembler | Tools that translate C code and assembly text into binary machine instructions |
| Flash | Nonvolatile memory on the STM32 (`0x08000000`) that stores our compiled instructions |
| ABI | Application Binary Interface: rules that functions follow so compiled files work together (for example, passing the 1st argument in `R0`) |

---

## Unified Example: How Instructions, Encodings, And Registers Work Together on Cortex-M4

Before looking at individual instructions, trace what happens from your C source file to the Cortex-M4 CPU executing instructions from Flash:

1. **Step 1 — The Compiler Translates C into `16-Bit` and `32-Bit` Thumb (`T32`) Instructions:**
   * When we compile our C code with `-mcpu=cortex-m4 -mthumb`, `arm-none-eabi-gcc` translates each C statement into Thumb (`T32`) machine instructions and stores them in Flash (`0x08000000`).
   * Common simple commands (like putting `1` into register `R3`: `movs r3, #1`) use a compact **`16-bit` (`2-byte`)** encoding (`0x2301`) to save Flash space.
   * Operations that need larger constants or more register options use a **`32-bit` (`4-byte`)** Thumb-2 encoding (such as `add.w r2, r0, r1` or `mrs r0, control`).

2. **Step 2 — The Cortex-M4 Fetches Instructions in Thumb State (`EPSR.T = 1`):**
   * The Cortex-M4 processor has only a Thumb instruction decoder (it has no `A32` or `A64` decoder).
   * To tell the hardware that every jump target contains Thumb instructions, **bit `0` of every function address in the vector table (`g_pfnVectors[]`) and every `bx`/`blx` branch address must be `1`**, which keeps bit `24` (`T` bit) of the CPU status register (`xPSR`) set to `1` (`0x01000000UL`).

3. **Step 3 — `16-Bit` and `32-Bit` Instructions Operate on Full `32-Bit` CPU Registers and Memory:**
   * Even when an instruction is only `16 bits` (`2 bytes`) long in Flash, it operates on full **`32-bit` registers (`R0..R12`, `SP`, `LR`, `PC`)** and full `32-bit` memory addresses (`0x40023830` for `RCC->AHB1ENR`, `0x20000000` for SRAM).

4. **Step 4 — Special Instructions Control CPU Registers, Interrupts, Barriers, and Context Switches:**
   * Ordinary C code compiles to data/memory instructions (`movs`, `add`, `ldr`, `str`, `push`, `pop`, `bx lr`).
   * For startup, interrupt control, memory protection (`MPU`), and RTOS context switching (`SVC` and `PendSV`), we use small inline assembly instructions: `mrs`/`msr` (read/write special CPU registers), `cpsid i`/`cpsie i` (disable/enable interrupts), `dsb`/`isb` (memory/pipeline barriers), `svc #imm` (system call), and `stmdb`/`ldmia` (save/restore registers `R4..R11` on a task stack).

```text
C Source Code (.c)
  |  Compiled with: arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb
  v
Flash Memory (0x08000000): Mixed 16-bit (2 B) & 32-bit (4 B) Thumb-2 Instructions
  |  Vector table pointers have bit 0 = 1 -> keeps CPU in Thumb state (xPSR bit 24 = 1)
  v
Cortex-M4 Instruction Decoder (T32 / Thumb-2)
  |
  +--> Data & Memory:       movs, add.w, ldr, str, push, pop
  +--> CPU Special Regs:    mrs (read CONTROL/PSP/IPSR), msr (write CONTROL/PSP/BASEPRI)
  +--> Interrupts/Barriers: cpsid i, cpsie i, dsb 0xF, isb 0xF, nop
  +--> RTOS & Exceptions:   svc #imm, tst lr, #4, ite eq, stmdb r0!, {r4-r11}, ldmia r0!, {r4-r11}, bx lr
```

---

## Scene 1: Why Thumb (`T32` / `Thumb-2`) Exists And Why Bit `0` Must Be `1`

### 1. Why Thumb Combines `16-Bit` (`2-Byte`) and `32-Bit` (`4-Byte`) Instructions

1. The original Arm instruction set (**`A32`**) used a fixed `32 bits` (`4 bytes`) for every instruction.
2. Many everyday operations are simple: load a small number into a register, add two registers, compare, or branch. Using `4 bytes` for every simple instruction takes more Flash memory.
3. **Thumb (`T32`)** introduced compact `16-bit` (`2-byte`) encodings for common operations so firmware fits in less Flash memory.
4. Because a `16-bit` instruction has fewer bits to specify registers and constants, early `16-bit`-only Thumb had more restrictions than `A32`.
5. **Thumb-2** extended Thumb by adding `32-bit` (`4-byte`) instructions alongside the `16-bit` instructions. The compiler automatically picks a `2-byte` instruction when the operation fits, and a `4-byte` Thumb-2 instruction when a wider constant or register field is needed.
* **Otherwise:** Without `16-bit` compact instructions, firmware occupies more Flash; without `32-bit` Thumb-2 instructions, the CPU would need multiple short instructions to do one wide operation.

Illustrative storage arithmetic:

```text
BEFORE: 100 instructions, 4 bytes each -> 400 bytes
AFTER:  100 instructions, 2 bytes each -> 200 bytes
```

- This is a size example, not a promise that any C program becomes half-size.
- The real result depends on the instruction mix, compiler and constants.
- Smaller code can reduce instruction-fetch traffic and help performance or
  energy use. It does not guarantee fewer clock cycles.

### A tiny encoding example

Suppose an invented 8-bit instruction uses the top 4 bits for its operation
and the bottom 4 bits for a number:

```text
operation 0001 = "load number"
number    0101 = 5

encode: load 5 -> 0001 0101
decode: 0001 0101 -> operation 0001, number 0101 -> load 5
```

The 4-bit number field fits only 0 through 15. Loading 100 needs a different
encoding or more instructions. Real Arm encodings have more detailed layouts,
but this illustrates why compact instructions have restrictions.

---

### 2. Why Vector Pointers Need Bit `0 = 1` and Stacked `xPSR` Needs Bit `24 = 1`

Your STM32F446RE's Cortex-M4 executes **Thumb instructions only**. Whenever the CPU loads a function address from the vector table (`g_pfnVectors[]`) or jumps via a register (`bx` / `blx`), hardware checks **bit `0`** of that address:
* **Bit `0 = 1`** tells the CPU "execute at the even address `(addr & ~1UL)` in **Thumb state** (`xPSR` bit `24` `EPSR.T = 1`)." (When you put a C function name like `Reset_Handler` into `g_pfnVectors[]` with `-mthumb`, the compiler and linker set bit `0 = 1` automatically.)
* Similarly, when we build a fake initial stack frame for a task in `W01-L19` (or FreeRTOS), we must set **bit `24` (`T` bit) of the stacked `xPSR` word to `1` (`0x01000000UL`)**.
* **Otherwise:** If a vector pointer has bit `0 == 0` or a stacked `xPSR` has bit `24 == 0`, the Cortex-M4 faults immediately with a **`UsageFault` (`INVSTATE`)** on the first instruction fetch!

```c
#include <stdint.h>

void Reset_Handler(void);

/* 1. In a Cortex-M4 vector table, taking the address of a C function compiled
 *    with `-mthumb` automatically sets bit 0 = 1 (Thumb state marker), while
 *    the instruction itself resides at the even Flash address (addr & ~1UL): */
__attribute__((section(".isr_vector"), used))
const uintptr_t g_reset_preview[2] = {
    0x20020000UL,                  /* Word 0 (0x08000000): Initial MSP */
    (uintptr_t)Reset_Handler       /* Word 1 (0x08000004): Reset_Handler | 1 (bit 0 = 1 -> EPSR.T = 1) */
};

/* 2. When forging an initial task stack frame (as in W01-L19 / FreeRTOS),
 *    stacked xPSR (sp[15]) MUST have bit 24 (EPSR.T) = 1 (0x01000000UL): */
static inline uint32_t make_initial_xpsr_thumb(void)
{
    return (1UL << 24);            /* 0x01000000UL: EPSR.T = 1 (otherwise UsageFault INVSTATE) */
}
```

---

## Scene 2: How Many Main Arm Instruction Sets Exist? (`A32`, `T32`, `A64`)

```text
Main Arm instruction-set names
          |
          +-> A32: original Arm (fixed 32-bit instructions, 32-bit registers)
          +-> T32: Thumb, including Thumb-2 (16-bit or 32-bit instructions, 32-bit registers)
          +-> A64: instruction set for AArch64 (fixed 32-bit instructions, 64-bit registers)
```

| Main set | Instruction length | Register/data model | Why it exists / typical use |
| --- | --- | --- | --- |
| **A32**, traditionally called ARM | Fixed 32 bits (`4 bytes`) | 32-bit general-purpose registers | Original Arm instruction set; existing 32-bit software on processors that support it |
| **T32**, called Thumb | 16 or 32 bits (`2` or `4 bytes`) | 32-bit general-purpose registers | Compact code; Cortex-M firmware and supported 32-bit application/real-time processors |
| **A64** | Fixed 32 bits (`4 bytes`) | 64-bit general-purpose registers, also supporting 32-bit operations | 64-bit software, larger address spaces and a redesigned register/instruction model on supporting processors |

1. **Remember the three main instruction-set names (`A32`, `T32`, `A64`), and note that `Thumb-2` is part of `T32`, not a fourth execution state.**
   * `AArch32` is the 32-bit execution environment (which can provide `A32` and `T32` on Cortex-A/R); `AArch64` uses `A64`.
   * Historical specialist facilities such as Jazelle and ThumbEE are not choices for `P1`/`P2`.
   * DSP, floating-point (`FPU`), Neon, and SVE are optional instruction extensions added onto a core, not replacements for `A32`/`T32`/`A64`.
2. **Keep *instruction width* (bytes in Flash) separate from *register width* (bits in a CPU register):**
   * **Otherwise:** Confusing the two makes people think a `16-bit` Thumb instruction can only hold 16-bit numbers, or that `A64` instructions are 64 bits long. A `16-bit` (`2-byte`) Thumb instruction operates on full **`32-bit` registers**, and an `A64` instruction is `32 bits` (`4 bytes`) long and operates on **`64-bit` registers**.

```text
Instruction width = bytes occupied by the command in Flash
Register width    = size of the CPU register on which it operates

16-bit Thumb (T32) instruction -> operates on a 32-bit register (R0..R12)
32-bit A64 instruction         -> operates on a 64-bit register (X0..X30)
```

T32 does not mean every instruction is 32 bits.
A64 does not mean every instruction is 64 bits.

```c
#include <stdint.h>

/* Enabling GPIOA clock (RCC->AHB1ENR bit 0 at 0x40023830):
 * Even when the compiler emits 16-bit (2-byte) Thumb instructions such as
 * `movs r3, #1` or `orrs r2, r3`, those instructions operate on full 32-bit
 * Cortex-M4 registers and full 32-bit addresses: */
static inline void enable_gpioa_clock_32bit_reg(void)
{
    volatile uint32_t *const rcc_ahb1enr = (volatile uint32_t *)0x40023830UL;
    *rcc_ahb1enr |= (1UL << 0); /* 32-bit read-modify-write at 0x40023830 */
}
```

---

## Scene 3: Important Cortex-M4 Assembly Instructions (Sequential Guide)

Before writing bare-metal startup (`W01-L17`), `MPU`/`SVC` handlers (`W01-L18`), and `PendSV` task switching (`W01-L19`), here are the exact Cortex-M4 Thumb instructions we use and what each one does:

### 1. Moving Constants, Adding, and Reading/Writing Memory (`movs`, `add.w`, `ldr`, `str`)

* **`movs r0, #5`:** Puts the constant `5` into 32-bit register `r0` and updates condition flags (`S` suffix). Uses a `16-bit` (`2-byte`) Thumb encoding.
* **`add.w r2, r0, r1`:** Adds `r0 + r1` and stores the result in `r2`. The `.w` suffix requests a wide `32-bit` (`4-byte`) Thumb-2 encoding while staying in Thumb state (`EPSR.T = 1`).
* **`ldr r0, [r1]` and `str r0, [r1]`:** `ldr` (Load Register) reads a 32-bit word from the memory address in `r1` into `r0`; `str` (Store Register) writes the 32-bit word in `r0` to the memory address in `r1`. (Cortex-M4 is a **load/store architecture**: to change a hardware register in memory, the CPU loads it into a register with `ldr`, modifies it, and stores it back with `str`.)

```text
Flash instruction (16-bit or 32-bit T32) -> decode command -> read 32-bit inputs -> update 32-bit register

BEFORE                         AFTER
r0 = unknown                   r0 = 5     (after `movs r0, #5`,   16-bit T32)
r1 = unknown          ->       r1 = 7     (after `movs r1, #7`,   16-bit T32)
r2 = unknown                   r2 = 12    (after `add.w r2,r0,r1`, 32-bit T32)
instruction state = Thumb      instruction state = Thumb
```

| Step | Where | Goal/state change | Data in -> out | Context |
| --- | --- | --- | --- | --- |
| 1 | Illustrative assembly: `MOVS` | Set `r0` | Constant 5 -> `r0 = 5` | Thumb |
| 2 | Illustrative assembly: `MOVS` | Set `r1` | Constant 7 -> `r1 = 7` | Thumb |
| 3 | Illustrative assembly: `ADD.W` | Add into `r2` | 5 and 7 -> `r2 = 12` | Thumb |

```asm
movs r0, #5          /* r0 becomes 5; a 16-bit (2-byte) T32 encoding is available. */
movs r1, #7          /* r1 becomes 7; a 16-bit (2-byte) T32 encoding is available. */
add.w r2, r0, r1     /* r2 becomes 5 + 7 = 12; uses a 32-bit (4-byte) T32 encoding. */
```

```c
#include <stdint.h>

static inline uint32_t thumb2_add_demo(void)
{
    uint32_t r0_val, r1_val, r2_val;
    __asm volatile (
        "movs  %0, #5        \n" /* 16-bit T32 -> r0_val = 5  */
        "movs  %1, #7        \n" /* 16-bit T32 -> r1_val = 7  */
        "add.w %2, %0, %1    \n" /* 32-bit T32 -> r2_val = 12 */
        : "=&l" (r0_val), "=&l" (r1_val), "=r" (r2_val)
        :
        : "cc"
    );
    return r2_val;
}
```

---

### 2. Reading and Writing Special CPU Registers (`mrs` and `msr`)

Special CPU registers (`CONTROL`, `IPSR`, `MSP`, `PSP`, `PRIMASK`, `BASEPRI`, `FAULTMASK`) live inside the CPU core and do not have a memory address (`ldr` and `str` cannot access them).
* **`mrs r0, <special_reg>` (Move to Register from Special register):** Copies a special CPU register into a general-purpose register (`r0`).
* **`msr <special_reg>, r0` (Move to Special register from Register):** Copies a general-purpose register (`r0`) into a writable special CPU register.
* **Otherwise:** Trying to access `CONTROL`, `PSP`, or `BASEPRI` without `mrs`/`msr` is impossible in standard C.

```c
/* Read PSP using `mrs`, and write PSP using `msr`: */
static inline uint32_t __get_PSP(void)
{
    uint32_t result;
    __asm volatile ("mrs %0, psp" : "=r" (result));
    return result;
}

static inline void __set_PSP(uint32_t topOfProcStack)
{
    __asm volatile ("msr psp, %0" : : "r" (topOfProcStack) : "memory");
}
```

---

### 3. Masking and Unmasking Interrupts (`cpsid i` and `cpsie i`) and Barriers (`dsb`, `isb`, `nop`)

* **`cpsid i` (Change Processor State — Interrupt Disable):** Sets `PRIMASK = 1`, which prevents all configurable-priority interrupts (`SysTick`, `EXTI15_10`, `PendSV`, etc.) from activating (`NMI` and `HardFault` remain active). Ignored if the CPU is in Unprivileged mode (`CONTROL.nPRIV == 1`).
* **`cpsie i` (Change Processor State — Interrupt Enable):** Clears `PRIMASK = 0` so enabled interrupts can activate again.
* **`dsb 0xF` (Data Synchronization Barrier):** Forces the CPU to wait until all memory and register writes before `dsb` have completed before executing any instruction after `dsb` (used after changing clock/peripheral/NVIC/MPU registers).
* **`isb 0xF` (Instruction Synchronization Barrier):** Flushes the CPU instruction pipeline so all instructions after `isb` are fetched fresh using the new `CONTROL`, `MPU`, `CPACR` (FPU), or clock settings.
* **`nop` (No Operation):** Does nothing for 1 instruction step (used in idle/fault loops so the debugger can halt cleanly).

```c
static inline void __disable_irq(void) { __asm volatile ("cpsid i" : : : "memory"); }
static inline void __enable_irq(void)  { __asm volatile ("cpsie i" : : : "memory"); }
static inline void __DSB(void)         { __asm volatile ("dsb 0xF" : : : "memory"); }
static inline void __ISB(void)         { __asm volatile ("isb 0xF" : : : "memory"); }
static inline void __NOP(void)         { __asm volatile ("nop"); }
```

---

### 4. Saving and Restoring Task Registers on a Stack (`push`/`pop`, `stmdb r0!, {r4-r11}`, `ldmia r0!, {r4-r11}`)

* **`push {r3, lr}` and `pop {r3, lr}`:** Pushes or pops registers using the **currently active stack pointer** (`MSP` when inside an interrupt handler).
* **`stmdb r0!, {r4-r11}` (Store Multiple Decrement Before, with write-back `!`):** Used inside `PendSV_Handler` (where the active stack pointer is `MSP`, but `r0` holds the outgoing task's `PSP`). It subtracts `32 bytes` (`8 * 4`) from `r0`, stores `r4` through `r11` into `r0[0..7]` (lowest register `r4` at lowest address `r0[0]`), and updates `r0` to the new stack address (`!`).
* **`ldmia r0!, {r4-r11}` (Load Multiple Increment After, with write-back `!`):** Loads `r4` through `r11` from `r0[0..7]` on the incoming task's `PSP` and adds `32 bytes` to `r0` (`!`) so `r0` points to the 8-word hardware frame above `r11`.

```text
Outgoing Task PSP in R0 (Before `stmdb`)          Outgoing Task PSP in R0 (After `stmdb r0!, {r4-r11}`)
+--------------------------------------+          +--------------------------------------+
| [R0..R3, R12, LR, PC, xPSR] (8 words)|          | [R0..R3, R12, LR, PC, xPSR] (8 words)|
+--------------------------------------+ <-- R0   +--------------------------------------+
| (Free stack space below)             |          | [R4, R5, R6, R7, R8, R9, R10, R11]   |
+--------------------------------------+          +--------------------------------------+ <-- R0 (Minus 32 B)
```

```c
/* Excerpt from PendSV_Handler (W01-L19): save outgoing R4..R11 to PSP, restore incoming R4..R11: */
static inline void pendsv_save_restore_example(void)
{
    __asm volatile (
        "mrs   r0, psp           \n" /* 1. R0 = outgoing task's PSP                   */
        "stmdb r0!, {r4-r11}     \n" /* 2. Push R4..R11 onto outgoing PSP (R0 -= 32)  */
        /* ... C scheduler saves R0 to outgoing TCB and loads incoming TCB's PSP ...  */
        "ldmia r0!, {r4-r11}     \n" /* 3. Pop R4..R11 from incoming PSP (R0 += 32)   */
        "msr   psp, r0           \n" /* 4. Update PSP to incoming task's HW frame     */
    );
}
```

---

### 5. System Calls, Bit Testing, and Exception Return (`svc #imm`, `tst`, `ite eq`, `bx lr`)

* **`svc #1` (Supervisor Call):** Triggers Exception `#11` (`SVC_Handler`), switching the CPU from Unprivileged Thread mode into Privileged Handler mode on `MSP`. The 8-bit number (`#1`) is encoded inside the 2-byte instruction (`0xDF01`) so `SVC_Handler` can read `return_pc[-2]` to see which service was requested.
* **`tst lr, #4` + `ite eq` + `mrseq r0, msp` + `mrsne r0, psp`:**
  * `tst lr, #4` performs a bitwise AND `(LR & 4)` to test bit `2` of `EXC_RETURN` in `LR` and sets the Zero flag (`Z = 1` if bit `2 == 0`; `Z = 0` if bit `2 == 1`).
  * `ite eq` (If-Then-Else on Equal/Zero) makes the next instruction (`mrseq r0, msp`) run if bit `2 == 0` (frame is on `MSP`) and the following instruction (`mrsne r0, psp`) run if bit `2 == 1` (frame is on `PSP`).
* **`bx lr` (Branch and Exchange via `LR`):**
  * In a normal C function, `LR` holds the caller's return address (with bit `0 = 1`), so `bx lr` returns to the caller.
  * Inside an interrupt or exception handler, `LR` holds the special `EXC_RETURN` value (`0xFFFFFFFD` for Thread mode on `PSP`, or `0xFFFFFFF9` for Thread mode on `MSP`). Executing `bx lr` with `0xFFFFFFFD` tells the CPU hardware to pop `{R0, R1, R2, R3, R12, LR, Return_PC, xPSR}` from `PSP` and resume Thread mode on `PSP`!

```c
/* Trigger Supervisor Call #1 from Thread mode: */
static inline void request_svc_1(void)
{
    __asm volatile ("svc #1" : : : "memory");
}
```

---

## Scene 4: When To Select Which Instruction Set And Compiler Flags

```text
Exact CPU -> supported instructions -> software/ABI requirements -> compiler target
```

| Target / situation | Selection |
| --- | --- |
| **P1: STM32F446RE, Cortex-M4** | **Thumb only**, with the Cortex-M4 instruction capabilities |
| **P2: STM32F767ZI, Cortex-M7** | **Thumb only**, with the Cortex-M7 instruction capabilities |
| A 32-bit processor supporting both A32 and Thumb | Follow platform/library requirements. Thumb is commonly useful for compact code; compare measured size and timing if either is permitted |
| A 64-bit application environment on a supporting processor | A64, with the matching 64-bit toolchain and ABI |
| Maintaining existing firmware | Preserve its target and ABI unless there is a specific, tested reason to change |

1. **Pass both `-mcpu=cortex-m4` (for `P1` on `F446RE`, or `-mcpu=cortex-m7` for `P2` on `F767ZI`) and `-mthumb` to `arm-none-eabi-gcc`.**
   * **Otherwise:** A compiler flag cannot add an instruction set (`A32` or `A64`) that Cortex-M hardware does not implement; and passing only `-mthumb` without `-mcpu=cortex-m4` targets an older baseline (like Cortex-M0) that lacks Cortex-M4 Thumb-2 and FPU instructions.
2. **Keep the CPU and float-ABI flags identical across all compiled `.c` files and linked libraries.**
   * **Otherwise:** Changing an RTOS (FreeRTOS, Zephyr) or removing HAL does not change the CPU instruction set, and mixing incompatible ABI flags across files causes linker errors or stack corruption.

### The Existing `P1` Build Selection And Disassembly

The current generated
[compiler rule](../../../p1_f446re_baseline/Debug/Core/Src/subdir.mk#L34)
already contains:

```text
-mcpu=cortex-m4 -mthumb
```

And the saved [disassembly](../../../p1_f446re_baseline/Debug/p1_f446re_baseline.list#L62)
shows the 16-bit (`2-byte`) Thumb encoding `2301` for `movs r3, #1`:

```text
2301      movs r3, #1
```

```c
/* Compile-time guard verifying the compiler is targeting Thumb-2 on Cortex-M: */
#if !defined(__thumb__) || !defined(__thumb2__)
#error "Cortex-M4 firmware must be compiled in Thumb-2 mode (-mcpu=cortex-m4 -mthumb)!"
#endif
```

- `-mcpu=cortex-m4`: generate instructions appropriate for this CPU.
- `-mthumb`: generate Thumb code.
- These are an excerpt, not a complete build command; preserve the existing
  floating-point, ABI and other project options.
- In ordinary C development, the compiler and assembler select the individual
  short or wide encodings. You do not manually choose an encoding for each C line.
- That disassembly is existing build-artifact evidence, not a new build or target observation.

## Feynman Recap

An instruction set is the set of commands a processor understands. Thumb (`T32`)
uses compact `16-bit` (`2-byte`) commands for simple operations and `32-bit`
(`4-byte`) Thumb-2 commands for wider operations so firmware takes less Flash
memory while still operating on full `32-bit` registers. `A32` is the original
fixed 32-bit Arm instruction set, and `A64` is for 64-bit Arm processors. Our
STM32 Cortex-M4 and Cortex-M7 processors execute Thumb only, using standard load/store
instructions (`movs`, `ldr`, `str`, `push`, `pop`) for normal C code and small
special instructions (`mrs`, `msr`, `cpsid`, `dsb`, `isb`, `svc`, `stmdb`, `ldmia`,
`bx lr`) for startup, interrupts, memory protection, and RTOS context switching.

## Short Interview Answers

### Why was Thumb introduced?

- **Simple answer:** To improve code density: fit useful work into fewer Flash bytes.
- **How:** Short `16-bit` (`2-byte`) encodings for common operations, combined with `32-bit` (`4-byte`) Thumb-2 encodings where wider operands are needed.
- **Trade-off:** `16-bit` encodings have fewer bits for registers and constants; Thumb-2 adds `32-bit` encodings without switching out of Thumb state.

### Is Thumb a 16-bit processor mode?

- **Simple answer:** No. Instruction width in Flash and register width in the CPU are different.
- **How:** A `16-bit` (`2-byte`) Thumb instruction operates on full `32-bit` Cortex-M4 registers (`R0..R12`, `SP`, `LR`, `PC`).
- **Trade-off:** Compact encoding restricts which registers or immediate constants fit in a 2-byte instruction.

### Which set should we choose for Cortex-M4, and why must vector entries have bit `0 = 1`?

- **Simple answer:** Thumb (`-mcpu=cortex-m4 -mthumb`), with bit `0 = 1` on all vector/branch target addresses.
- **How:** Cortex-M4 implements Thumb only; bit `0 = 1` sets `xPSR` bit `24` (`EPSR.T = 1`) while the instruction itself sits at the even address `(addr & ~1UL)`.
- **Why:** If bit `0` of a vector entry or `bx` target is `0` (or stacked `xPSR` bit `24` is `0`), Cortex-M4 triggers a `UsageFault` (`INVSTATE`).

## Evidence And References

- [Local Arm Cortex-M4 Generic User Guide, DUI0553B](../../docs/stm32/um-cortexm4/DUI0553.pdf):
  section 1.1 describes Thumb-2 technology; section 2.1.3 states Thumb-only
  execution; section 3.1 lists supported instructions.
- [GCC Arm options](https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html):
  `-mcpu`, `-mthumb` and `-marm`.
- [Existing P1 compiler rule](../../../p1_f446re_baseline/Debug/Core/Src/subdir.mk#L34)
  and [saved disassembly](../../../p1_f446re_baseline/Debug/p1_f446re_baseline.list#L62).
- [Current project contracts](../../plans/project_portfolio.md):
  F446RE for P1 and F767ZI for P2.

## Corrections

- Initial publication, 2026-10-28.
- 2026-10-29: Updated Scenes 1–4 with step-by-step explanations and C/build code snippets.
- 2026-11-01: Renamed from `W01-L18` to `W01-L15` as the first Arm Cortex-M4 prerequisite lesson, added the Unified Example, and added the sequential guide to all important Cortex-M4 assembly instructions (`movs`, `ldr`/`str`, `mrs`/`msr`, `cpsid`/`cpsie`, `dsb`/`isb`, `push`/`pop`, `stmdb`/`ldmia`, `svc`, `tst`/`ite`, `bx lr`).
