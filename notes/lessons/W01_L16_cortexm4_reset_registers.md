# W01-L16: Arm Cortex-M4 Important Core Registers, Reset State, And System Register Blocks

**Status:** Source-reviewed, 2026-11-01: local Arm DUI0553B, STM32F446
RM0390 Rev 6, and the existing STM32F446/Cortex-M4 CMSIS headers.
Examples are explanatory, not measured register readings. Concept-only:
no project edits, builds or hardware runs. Existing startup lesson unchanged.
Publication is not learner completion; quizzes remain paused.

Previous: [Thumb, Arm instruction sets, and important Cortex-M4 instructions](W01_L15_thumb_and_arm_instruction_sets.md).

## Goal And Know This First

Understand every important Arm Cortex-M4 core register and STM32 startup register
block in sequential order before writing the bare-metal startup, interrupt, `MPU`,
and `PendSV` code in [W01-L17](W01_L17_b1_exti_interrupt_handoff_and_debounce.md)
through [W01-L19](W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md).

- **Register:** a small 32-bit hardware storage location used to hold data, settings,
  or status.
- **Bit field:** one or more bits inside a register with one defined meaning.
- **Read/write:** software may read and change the field, subject to its rules.
- **Read-only status:** hardware reports the value; software reads it.
- **Exception:** an event that diverts execution to a handler. Interrupts and
  faults are types of exception.
- **Thread mode:** normal program execution, including bare-metal `main()`.
  It does not require an RTOS thread to exist.
- **Handler mode:** execution of an interrupt or other exception handler.
- **Privileged:** allowed to perform protected system-control operations.
- **Unprivileged:** restricted by the processor's access rules.
- **Mask:** a control that temporarily prevents selected exceptions from
  activating.
- **Reserved bits:** not application settings; preserve the documented values.

---

## Unified Example: How Cortex-M4 Core Registers And System Blocks Work Together From Reset

When power stabilizes on the STM32F446RE and the reset pin (`NRST`) releases, the Cortex-M4 core registers and memory-mapped system blocks work together in **5 simple steps**:

1. **Step 1 — Hardware Loads `MSP` and `PC` From the Reset Vector Table:**
   * Before running any C code, the Cortex-M4 hardware reads two 32-bit words from address `0x00000000` and `0x00000004` (which point to Main Flash `0x08000000` and `0x08000004` when `BOOT0 = 0`):
     * Word `0` (`0x08000000`) loads the **Main Stack Pointer (`MSP`, `R13`)** with `0x20020000` (top of `128 KB` SRAM).
     * Word `1` (`0x08000004`) loads the **Program Counter (`PC`, `R15`)** with the address of `Reset_Handler`, and its bit `0 = 1` sets bit `24` (`T` bit) of the **Program Status Register (`xPSR`)** so the CPU runs in Thumb state.

2. **Step 2 — CPU Special Registers Set the Reset Execution State:**
   * Inside the CPU core:
     * **`CONTROL = 0x00`:** Thread mode starts as **Privileged (`nPRIV = 0`)** using **`MSP` (`SPSEL = 0`)** with no active floating-point context (`FPCA = 0`).
     * **`IPSR = 0`:** Reports that the CPU is running normal **Thread mode** (`0`), not an interrupt handler.
     * **`PRIMASK = 0`, `BASEPRI = 0`, `FAULTMASK = 0`:** None of the three CPU interrupt-mask registers is blocking exceptions at reset (though peripheral interrupts still stay off until enabled in `NVIC` and the peripheral).

3. **Step 3 — Memory-Mapped System Control Block (`SCB` at `0xE000ED00`) Locates Handlers:**
   * Unlike `CONTROL` or `PRIMASK` (which have no memory address and use `mrs`/`msr`), the **System Control Block (`SCB`)** sits at memory address `0xE000ED00` and is accessed using C struct pointers (`SCB->VTOR`, `SCB->AIRCR`, `SCB->SHP`).
   * At reset, **`SCB->VTOR` (`0xE000ED08`)** is `0x00000000`. During startup, we write `SCB->VTOR = 0x08000000` so the CPU looks up every interrupt handler directly in our Flash vector table.

4. **Step 4 — Memory-Mapped Clock (`RCC` at `0x40023800`) and Flash (`FLASH` at `0x40023C00`) Blocks Set CPU Speed:**
   * At reset, **`RCC->CR`** runs the CPU from the internal **`16 MHz` `HSI` oscillator** (`HSION = 1`, `HSIRDY = 1`) with all PLLs turned off (`PLLON = 0`), and **`FLASH->ACR`** has **`0` Flash wait states (`LATENCY = 0`)**.
   * To raise the CPU clock to `84 MHz`, software first sets `FLASH->ACR` to **`2` wait states (`LATENCY = 2`)** so Flash reads keep up with `84 MHz`, enables the Main PLL in `RCC->CR`, waits for `PLLRDY == 1`, switches `RCC->CFGR` (`SW = 2`), and waits for hardware confirmation (`SWS == 2`).

5. **Step 5 — General-Purpose Registers (`R0..R12`) and `LR` (`R14`) Run C Functions and Interrupts:**
   * As `main()` and interrupt handlers run, `R0..R3` pass function arguments and return values, `R4..R11` hold saved local variables across calls, `SP` (`MSP` or `PSP`) points to the current stack in RAM, and `LR` (`R14`) holds either the function return address or the special `EXC_RETURN` code (`0xFFFFFFF9` / `0xFFFFFFFD`) during an interrupt.

```text
+---------------------------------------------------------------------------------------+
| 1. INSIDE THE CORTEX-M4 CPU CORE (No Memory Address -> Accessed via Assembly)         |
|                                                                                       |
|    General & Core Bank (R0..R15, xPSR):                                               |
|      R0..R3   = Function arguments / return value / scratch registers                 |
|      R4..R11  = Callee-saved registers (must be preserved across function calls/tasks)|
|      R12      = Intra-procedure scratch register                                      |
|      R13 (SP) = Stack Pointer -> Banked as MSP (Kernel/ISRs) and PSP (App/Tasks)      |
|      R14 (LR) = Link Register -> Return address in C, or EXC_RETURN in Handler mode   |
|      R15 (PC) = Program Counter -> Address of current instruction in Flash            |
|      xPSR     = Status Register -> APSR (Flags) + IPSR (IRQ #) + EPSR (Bit 24 T = 1)  |
|                                                                                       |
|    Special Control & Mask Registers (Read with `mrs`, Written with `msr` / `cpsid`):  |
|      CONTROL  = Bit 0 (nPRIV: 0=Priv, 1=Unpriv), Bit 1 (SPSEL: 0=MSP, 1=PSP)          |
|      PRIMASK  = Bit 0 (1 = masks all configurable-priority interrupts)                |
|      BASEPRI  = Bits [7:4] (masks interrupts with numerical priority >= threshold)    |
|      FAULTMASK= Bit 0 (1 = masks all exceptions including HardFault, except NMI)      |
+---------------------------------------------------------------------------------------+
                                           |
                                           | Load / Store (`ldr` / `str` via `volatile` C pointers)
                                           v
+---------------------------------------------------------------------------------------+
| 2. MEMORY-MAPPED REGISTER BLOCKS (Have Fixed Memory Addresses -> `STRUCT->REG`)       |
|                                                                                       |
|    Arm Core Private Peripheral Bus (0xE000E000 .. 0xE00FFFFF, Privileged Only):       |
|      SysTick (0xE000E010) -> CTRL, LOAD, VAL (24-bit 1 ms down-counter)               |
|      NVIC    (0xE000E100) -> ISER, ICER, ICPR, IP (External IRQ enable & priority)    |
|      SCB     (0xE000ED00) -> VTOR (vector base), AIRCR, SHP, SHCSR, CFSR, MMFAR       |
|      MPU     (0xE000ED90) -> TYPE, CTRL, RNR, RBAR, RASR (Memory protection regions)  |
|                                                                                       |
|    STM32F446 Peripheral Bus (0x40000000+):                                            |
|      RCC     (0x40023800) -> CR, PLLCFGR, CFGR, AHB1ENR, APB1ENR, APB2ENR (Clocks)    |
|      FLASH   (0x40023C00) -> ACR (LATENCY bits [3:0] Flash wait states)               |
+---------------------------------------------------------------------------------------+
```

---

## Scene 1: The 16 CPU Core Registers (`R0..R15`, `xPSR`) vs. Memory-Mapped Register Blocks

### 1. The 16 General-Purpose and Core Registers (`R0..R15` and `xPSR`)

Inside the Cortex-M4 processor ([DUI0553B](../../docs/stm32/um-cortexm4/DUI0553.pdf) Sec 2.1.3), every instruction operates on 16 core registers (`R0` through `R15`) plus the Program Status Register (`xPSR`):

| Register | Name | What It Does in Our Firmware |
| --- | --- | --- |
| **`R0..R3`** | Argument / Scratch Registers | Pass the first 4 arguments into a C function and return the result in `R0`. Any function or ISR may overwrite them, so hardware automatically pushes `R0..R3` onto the stack when an interrupt occurs. |
| **`R4..R11`** | Callee-Saved Variables | Hold local variables across function calls. A C function (and our `PendSV_Handler` task switcher in `W01-L19`) **must save and restore `R4..R11`** before changing them. |
| **`R12`** | Scratch Register (`IP`) | Temporary scratch register used by compiler veneers/branches; hardware automatically pushes `R12` on interrupt entry. |
| **`R13` (`SP`)** | Stack Pointer (`MSP` or `PSP`) | Points to the current top of the stack in SRAM. Hardware provides two physical stack pointers behind `R13`: **`MSP`** (used at reset and in all interrupt handlers) and **`PSP`** (used by application tasks when `CONTROL` bit `1 == 1`). |
| **`R14` (`LR`)** | Link Register | When `bl func` calls a function, hardware saves the return address in `LR`. When an interrupt enters Handler mode, hardware puts a special return code called **`EXC_RETURN`** (`0xFFFFFFF9` or `0xFFFFFFFD`) into `LR`. |
| **`R15` (`PC`)** | Program Counter | Holds the Flash address of the current instruction being executed. |
| **`xPSR`** | Combined Program Status Register | Combines three status views in one 32-bit word: **`APSR`** (bits `[31:27]`: `N, Z, C, V, Q` arithmetic condition flags), **`EPSR`** (bit `24` `T = 1` for Thumb state), and **`IPSR`** (bits `[8:0]`: active exception number). |

---

### 2. CPU Special Registers (`MRS`/`MSR`) vs. Memory-Mapped Register Blocks (`->`)

1. **CPU special registers (`CONTROL`, `IPSR`, `MSP`, `PSP`, `PRIMASK`, `BASEPRI`, `FAULTMASK`) live inside the CPU core and have no memory address.**
   * Read them with the assembly instruction `mrs` and write them with `msr` (or `cpsid` / `cpsie`).
   * **Otherwise:** You cannot take the address of `CONTROL` or `PRIMASK` with a C pointer.
2. **Memory-mapped register blocks (`SCB` at `0xE000ED00`, `RCC` at `0x40023800`, `FLASH` at `0x40023C00`) sit at fixed addresses in the 4 GB memory space.**
   * Access them in C using `volatile uint32_t` struct pointers (`SCB->VTOR`, `RCC->CR`, `FLASH->ACR`).
   * **Otherwise:** If you omit `volatile` on a hardware struct member, the C compiler may optimize away register writes or cache a status register in a CPU register forever.

| Name | Kind | What It Answers |
| --- | --- | --- |
| `CONTROL` | CPU special register (`mrs`/`msr`) | What privilege (`nPRIV`) and stack (`SPSEL`) does Thread mode use? |
| `MSP` / `PSP` | CPU banked `R13` (`mrs`/`msr`) | Where is the current top of the Main Stack or Process Stack in RAM? |
| `IPSR` | CPU special register (`mrs`) | Which exception handler is executing right now (`0` = Thread mode)? |
| `PRIMASK` | CPU special register (`mrs`/`msr`/`cpsid`) | Are all configurable-priority exceptions masked (`1`) or unmasked (`0`)? |
| `BASEPRI` | CPU special register (`mrs`/`msr`) | Which priority threshold (`bits [7:4]`) is masking less-urgent interrupts? |
| `FAULTMASK` | CPU special register (`mrs`/`msr`) | Is the stronger fault mask (blocking even `HardFault`) active? |
| `SCB->VTOR` | Memory-mapped (`0xE000ED08`) | Where in Flash or SRAM is the interrupt vector table located? |
| `RCC->CR` | Memory-mapped (`0x40023800`) | Which oscillators (`HSI`/`HSE`) and PLLs are enabled and ready? |
| `FLASH->ACR` | Memory-mapped (`0x40023C00`) | How many Flash wait states (`LATENCY`) are configured for the CPU clock? |

```c
#include <stdint.h>

/* 1. CPU Special Register (no memory address -> must use inline assembly `mrs`): */
static inline uint32_t read_cpu_control(void)
{
    uint32_t val;
    __asm volatile ("mrs %0, control" : "=r" (val));
    return val;
}

/* 2. Memory-Mapped Register Block (fixed hardware address -> `volatile` C struct pointer): */
typedef struct {
    volatile uint32_t CPUID; /* 0xE000ED00 */
    volatile uint32_t ICSR;  /* 0xE000ED04 */
    volatile uint32_t VTOR;  /* 0xE000ED08: Vector Table Offset Register */
} SCB_Preview_Type;

#define SCB_PREVIEW ((SCB_Preview_Type *)0xE000ED00UL)

static inline uint32_t read_scb_vtor(void)
{
    return SCB_PREVIEW->VTOR; /* Reads 32-bit word from address 0xE000ED08 */
}
```

---

## Scene 2: `CONTROL` Chooses Privilege And Stack (`MSP` vs. `PSP`)

### 1. Why `CONTROL` Exists and What Its 3 Bits Do

1. Startup code needs full permission (**Privileged mode**) to configure clocks, interrupts, and memory protection.
2. Once the system is running, an OS or safety supervisor can drop application code to **Unprivileged mode** (`bit 0 = 1`) and give it a separate stack pointer (**`PSP`**, `bit 1 = 1`) so application bugs cannot corrupt the kernel/interrupt stack (`MSP`).
3. **Otherwise:** If `CONTROL` is written with `SPSEL = 1` (`bit 1 = 1`) before loading a valid RAM address into `PSP`, the next stack push in Thread mode faults immediately!

| Bit | Name | Value `0` (Reset Default) | Value `1` |
| --- | --- | --- | --- |
| `0` | `nPRIV` | Thread mode is **Privileged** | Thread mode is **Unprivileged** |
| `1` | `SPSEL` | Thread mode uses **`MSP`** | Thread mode uses **`PSP`** |
| `2` | `FPCA` | No active floating-point context | Floating-point (`FPU`) context is active |

- Bits `31:3` are reserved.
- `FPCA` tells exception hardware whether floating-point registers are active in the current context; it is not the FPU enable switch (`SCB->CPACR` enables the FPU).
- **Handler mode (all ISRs) always runs Privileged and always uses `MSP`.** (Writes to `CONTROL.SPSEL` while in Handler mode are ignored; returning from an exception with `EXC_RETURN = 0xFFFFFFFD` selects `PSP` for Thread mode.)
- Unprivileged software cannot re-privilege itself by writing `CONTROL`; only taking an exception (`svc #imm` or an interrupt) enters Privileged Handler mode.

### 2. Decode A Small Example

Bit `0` contributes `1`; bit `1` contributes `2`:

```text
binary 0011 = 2 + 1 = 3 = hexadecimal 0x03

bit 1 = 1 -> Thread mode uses PSP
bit 0 = 1 -> Thread mode is Unprivileged
bit 2 = 0 -> no active floating-point context

RESET STATE                         POSSIBLE LATER THREAD STATE
CONTROL = 0x00             ->       CONTROL = 0x03
Privileged, MSP                     Unprivileged, PSP
```

| `CONTROL` (with `FPCA = 0`) | Thread-mode meaning |
| --- | --- |
| `0x00` | Privileged, `MSP` |
| `0x01` | Unprivileged, `MSP` |
| `0x02` | Privileged, `PSP` |
| `0x03` | Unprivileged, `PSP` |

When `FPCA` is `1`, the corresponding values are `0x04`, `0x05`, `0x06`, `0x07`.

### 3. `MSP` And `PSP` Hold Memory Addresses in SRAM

- **MSP (Main Stack Pointer):** At reset, hardware loads `MSP` from word `0` of the boot vector table (`0x20020000`). On Cortex-M4's descending stack, each 4-byte push subtracts `4` from the stack pointer and writes to that RAM address.
- **PSP (Process Stack Pointer):** Uninitialized at reset; software must load `PSP` with a valid 8-byte-aligned SRAM address before selecting `SPSEL = 1`.

```c
#include <stdbool.h>
#include <stdint.h>

static inline uint32_t __get_CONTROL(void)
{
    uint32_t result;
    __asm volatile ("mrs %0, control" : "=r" (result));
    return result;
}

static inline uint32_t __get_MSP(void)
{
    uint32_t result;
    __asm volatile ("mrs %0, msp" : "=r" (result));
    return result;
}

/* Verify that startup is running in Privileged Thread mode on MSP (CONTROL bits [1:0] == 0): */
static inline bool is_privileged_on_msp(void)
{
    return (__get_CONTROL() & 0x3UL) == 0UL;
}
```

---

## Scene 3: `IPSR` Reports What Is Running (`Thread Mode` vs. `Handler Mode`)

1. **`IPSR` (Interrupt Program Status Register)** is a read-only status register inside `xPSR` (`bits [8:0]`).
2. When normal C code (`Reset_Handler()`, `main()`, or an RTOS task) is running in **Thread mode**, `IPSR` bits `[8:0]` read as **`0`**.
3. When an exception or interrupt handler is running in **Handler mode**, `IPSR` bits `[8:0]` report that handler's **exception number** (`15` for `SysTick`, `16 + IRQ` for peripheral interrupts).
4. **Otherwise:** Software cannot trigger an interrupt by writing a number to `IPSR`, and the exception number in `IPSR` is the vector slot index—not the interrupt's priority!

| `IPSR` value | Meaning |
| --- | --- |
| `0` | Thread mode (normal code, including startup after reset) |
| `2` | `NMI`: non-maskable interrupt |
| `3` | `HardFault` |
| `4` | `MemManage` fault (`MPU` violation) |
| `5` | `BusFault` |
| `6` | `UsageFault` |
| `11` | `SVCall` (`svc #imm` system-call handler) |
| `12` | `DebugMonitor` |
| `14` | `PendSV` (context-switch handler) |
| `15` | `SysTick` (`1 ms` timer handler) |
| `16 + IRQ number` | STM32 peripheral interrupt handler (for `EXTI15_10` `IRQ 40`: `16 + 40 = 56`) |

```text
Before acceptance       During EXTI15_10 handler       After return to Thread
IPSR = 0         ->     IPSR = 16 + 40 = 56      ->    IPSR = 0
```

```c
#include <stdbool.h>
#include <stdint.h>

static inline uint32_t __get_IPSR(void)
{
    uint32_t result;
    __asm volatile ("mrs %0, ipsr" : "=r" (result));
    return result;
}

/* Returns true when called inside any ISR/exception handler (IPSR bits [8:0] != 0): */
static inline bool in_handler_mode(void)
{
    return (__get_IPSR() & 0x1FFUL) != 0UL;
}
```

---

## Scene 4: The Three Interrupt Mask Registers (`PRIMASK`, `BASEPRI`, `FAULTMASK`)

### 1. `PRIMASK`: Broad Mask for All Configurable-Priority Exceptions

* **What & Why:** Setting `PRIMASK` bit `0` to `1` (`cpsid i`) prevents all configurable-priority exceptions (`SysTick`, `PendSV`, and all peripheral IRQs `0..N`) from activating during a short critical section. (`NMI` and `HardFault` are never blocked by `PRIMASK`.)
* **Otherwise:** If you do not save `PRIMASK` before `cpsid i` and restore the saved value at the end, calling `cpsie i` inside a nested critical section re-enables interrupts too early! (Also note: masking delays activation of a pending interrupt until unmasked; it does not clear the hardware pending flag.)

| `PRIMASK` bit `0` | Meaning |
| --- | --- |
| `0` | No `PRIMASK` blocking (reset default) |
| `1` | Prevent activation of all configurable-priority exceptions |

```text
BEFORE                  DURING SHORT PROTECTED WORK        AFTER
PRIMASK = 0      ->      PRIMASK = 1                 ->     restore saved 0
```

---

### 2. `BASEPRI`: Priority-Threshold Mask (Used by FreeRTOS Critical Sections)

* **What & Why:** `BASEPRI` blocks interrupts whose numerical priority is **`>=` a threshold** while allowing more-urgent interrupts (smaller priority numbers `<` threshold) to keep running.
* **How STM32F446 Encodes Priority in Bits `[7:4]`:**
  1. Smaller priority numbers mean **higher urgency** (`Priority 0` is highest urgency; `Priority 15` is lowest urgency).
  2. STM32F446 implements `4` priority bits (`0..15`) stored in the **upper 4 bits `[7:4]`** of the 8-bit priority byte (`logical_priority << 4`).
  3. **Otherwise:** If you write `5` (`0x05`) instead of `5U << 4` (`0x50`) to `BASEPRI`, hardware ignores the unimplemented low 4 bits (`[3:0]`), so `BASEPRI` reads back as `0x00` and masks **nothing**!

```text
logical priority 5 -> binary 0101 -> shifted into bits [7:4] = 0101 0000 = 0x50

When BASEPRI = 0x50 (assuming all 4 bits are preemption priority, PRIGROUP = 3):
  priority 2  -> ALLOWED (2 < 5, more urgent)
  priority 5  -> BLOCKED (5 >= 5)
  priority 10 -> BLOCKED (10 >= 5)
```

| `BASEPRI` value | Effect (with 4 preemption-priority bits) |
| --- | --- |
| `0x00` | No `BASEPRI` masking (reset default) |
| `0x10` | Mask logical priorities `1` through `15` (only priority `0` runs) |
| `0x20` | Mask logical priorities `2` through `15` |
| `0x50` | Mask logical priorities `5` through `15` (priorities `0..4` still run) |
| `0xF0` | Mask logical priority `15` |

- Priority `0` cannot be masked with `BASEPRI` because writing `0x00` means "disable `BASEPRI` masking."

---

### 3. `FAULTMASK` and What Three Zeroes Mean at Reset

* **`FAULTMASK` bit `0`:** `0` = no effect (reset default); `1` = blocks all exceptions including `HardFault` (leaving only `NMI`). Used only in specialized fault handlers; hardware automatically clears `FAULTMASK = 0` on exception return (except from `NMI`).
* **At Reset (`PRIMASK = 0`, `BASEPRI = 0`, `FAULTMASK = 0`):** None of the three CPU masks is blocking exceptions, but individual peripheral interrupts still stay disabled until enabled in `EXTI` and `NVIC`. The masks combine (`BASEPRI = 0` does not cancel `PRIMASK = 1`).

```c
#include <stdint.h>

static inline uint32_t __get_PRIMASK(void)
{
    uint32_t result;
    __asm volatile ("mrs %0, primask" : "=r" (result));
    return result;
}

static inline void __set_PRIMASK(uint32_t priMask)
{
    __asm volatile ("msr primask, %0" : : "r" (priMask) : "memory");
}

static inline void __set_BASEPRI(uint32_t basePri)
{
    __asm volatile ("msr basepri, %0" : : "r" (basePri) : "memory");
}

/* 1. Save/restore PRIMASK around a short shared-state snapshot: */
static inline uint32_t read_atomic_u32(volatile const uint32_t *ptr)
{
    uint32_t saved = __get_PRIMASK();
    __asm volatile ("cpsid i" : : : "memory"); /* PRIMASK = 1 */
    uint32_t val = *ptr;
    __set_PRIMASK(saved);                      /* Restore previous PRIMASK */
    return val;
}

/* 2. Mask logical priorities 5..15 via BASEPRI (leaves priorities 0..4 active): */
static inline void mask_from_priority_5(void)
{
    __set_BASEPRI(5U << 4);                    /* 0x50 in upper 4 bits [7:4] */
}
```

---

## Scene 5: `SCB` (`0xE000ED00`) And `SCB->VTOR` (`0xE000ED08`) Locate The Handlers

1. **`SCB` (System Control Block)** is an Arm Cortex-M4 register block at address `0xE000ED00` containing registers that control the vector table (`VTOR`), pending exceptions (`ICSR`), priority grouping (`AIRCR`), core exception priorities (`SHP[0..11]`), fault enables/status (`SHCSR`, `CFSR`, `MMFAR`), and the FPU (`CPACR`).
2. **`SCB->VTOR` (Vector Table Offset Register at `0xE000ED08`)** holds the base address of the active vector table:
   * When Exception `#N` occurs, hardware reads the handler pointer from memory address **`SCB->VTOR + (N * 4)`**.
   * For `EXTI15_10` (`IRQ 40`), the exception number is `N = 16 + 40 = 56`, so its vector slot sits at **`SCB->VTOR + (56 * 4) = SCB->VTOR + 0xE0`** (`0x080000E0`).
   * **Otherwise:** Writing `SCB->VTOR` does not copy or create a table—a valid, naturally aligned vector table must already exist at that address! And remember: `0x080000E0` is the **slot address in the table** (which holds a pointer), not the code address of `EXTI15_10_IRQHandler`.

| Example `SCB->VTOR` value | Meaning |
| --- | --- |
| `0x00000000` | Vector table accessed through the STM32 boot-alias address (reset default) |
| `0x08000000` | Vector table at the start of Main Flash (`g_pfnVectors`) |
| `0x20000000` | Vector table relocated to SRAM (if firmware copied an aligned table there) |

```text
Exception #56 (IRQ 40) -> Slot Address = SCB->VTOR (0x08000000) + (56 * 4) = 0x080000E0
                       -> CPU reads 32-bit word at 0x080000E0 -> jumps to EXTI15_10_IRQHandler
```

```c
#include <stdint.h>

typedef void (*isr_fn_t)(void);
extern const isr_fn_t g_pfnVectors[113];

static inline void set_vector_table_base(void)
{
    volatile uint32_t *const scb_vtor = (volatile uint32_t *)0xE000ED08UL;
    *scb_vtor = (uint32_t)(uintptr_t)g_pfnVectors; /* Point VTOR to 0x08000000UL */
    __asm volatile ("dsb 0xF" : : : "memory");
}
```

---

## Scene 6: `RCC` (`0x40023800`) Starts And Selects The Clocks

### 1. `RCC->CR` (`0x40023800`): Enabling Oscillators/PLLs vs. Waiting for Ready Status

1. **`RCC` (Reset and Clock Control)** at `0x40023800` is the STM32 register block that controls oscillators, the Main PLL, bus clock dividers (`CFGR`), and peripheral clock gates (`AHB1ENR`, `APB1ENR`, `APB2ENR`).
2. In **`RCC->CR` (Clock Control Register, offset `+0x00`)**, each clock source has two separate bits:
   * An **`ON` control bit** (`HSION` bit `0`, `PLLON` bit `24`) that software sets to `1` to request turning the oscillator or PLL on.
   * A read-only **`RDY` status bit** (`HSIRDY` bit `1`, `PLLRDY` bit `25`) that hardware sets to `1` only after the oscillator or PLL has stabilized and locked.
3. **Otherwise:** Software cannot make a clock ready by writing `1` to an `RDY` bit; and switching the CPU to a PLL before `PLLRDY == 1` feeds the CPU an unlocked or dead clock! (At reset, `RCC->CR` is `0x0000XX83`: the `16 MHz` internal oscillator `HSI` is ON and Ready, and all three PLLs are OFF.)

| `RCC->CR` bit | Field | `0` | `1` | Role |
| --- | --- | --- | --- | --- |
| `0` | `HSION` | HSI disabled | HSI enabled (`1` at reset) | Read/write enable control |
| `1` | `HSIRDY` | HSI not ready | HSI ready (`1` at reset) | Read-only hardware status |
| `24` | `PLLON` | Main PLL disabled (`0` at reset) | Main PLL enabled | Read/write enable control |
| `25` | `PLLRDY` | Main PLL not locked (`0` at reset) | Main PLL locked/ready | Read-only hardware status |
| `26` | `PLLI2SON` | PLLI2S disabled | PLLI2S enabled | Read/write enable control |
| `27` | `PLLI2SRDY` | PLLI2S not locked | PLLI2S locked/ready | Read-only hardware status |
| `28` | `PLLSAION` | PLLSAI disabled | PLLSAI enabled | Read/write enable control |
| `29` | `PLLSAIRDY` | PLLSAI not locked | PLLSAI locked/ready | Read-only hardware status |

---

### 2. `RCC->CFGR` (`0x40023808`): Selecting `SYSCLK` (`SW` vs. `SWS`)

Enabling `PLLON = 1` in `RCC->CR` turns the PLL on, but **does not connect it to the CPU**. To select which clock feeds `SYSCLK`, we use **`RCC->CFGR`**:
* **`SW[1:0]` (bits `[1:0]`):** Software writes the requested system-clock source here.
* **`SWS[3:2]` (bits `[3:2]`):** Read-only status bits where hardware reports which clock source is currently active.
* **Otherwise:** Until `SW[1:0]` is set to `0b10` (`Main PLL P`) and `SWS[3:2]` reads `0b10`, the CPU continues running on the `16 MHz` `HSI` reset clock (`0b00`).

| `2-bit` code (`SW` or `SWS`) | Clock source |
| --- | --- |
| `00` (`0`) | `HSI` (`16 MHz` internal oscillator — selected at reset) |
| `01` (`1`) | `HSE` (external crystal/clock source) |
| `10` (`2`) | Main `PLL P` output (`84 MHz` in our `P1` setup) |
| `11` (`3`) | Main `PLL R` output |

```text
Enable source (RCC->CR ON=1) -> Wait ready (RCC->CR RDY==1) -> Select (RCC->CFGR SW) -> Confirm (RCC->CFGR SWS)
```

```c
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    volatile uint32_t CR;      /* 0x00: bit 0=HSION, bit 1=HSIRDY, bit 24=PLLON, bit 25=PLLRDY */
    volatile uint32_t PLLCFGR; /* 0x04 */
    volatile uint32_t CFGR;    /* 0x08: bits [1:0]=SW, bits [3:2]=SWS */
} RCC_Preview_Type;

#define RCC_PREVIEW ((RCC_Preview_Type *)0x40023800UL)

/* Returns true when HSI is ready (CR bit 1 == 1), all 3 PLLs are OFF (CR bits 24,26,28 == 0),
 * and HSI is the active system clock (CFGR SWS bits [3:2] == 0b00): */
static inline bool is_rcc_in_hsi_reset_state(void)
{
    uint32_t cr   = RCC_PREVIEW->CR;
    uint32_t cfgr = RCC_PREVIEW->CFGR;
    bool hsi_ready = (cr & (1UL << 1)) != 0U;
    bool plls_off  = (cr & ((1UL << 24) | (1UL << 26) | (1UL << 28))) == 0U;
    bool sws_hsi   = (cfgr & (3UL << 2)) == 0U;
    return hsi_ready && plls_off && sws_hsi;
}
```

---

## Scene 7: `FLASH->ACR` (`0x40023C00`) Configures Flash Wait States (`LATENCY`)

1. Flash memory takes physical time to read a word. At the `16 MHz` reset clock, the CPU can read Flash with **`0` wait states (`FLASH->ACR` bits `[3:0]` `LATENCY = 0`)**.
2. Before raising the CPU clock to **`84 MHz`** (at `3.3 V`), we must set **`FLASH->ACR` bits `[3:0]` (`LATENCY`) to `2` (`2 Wait States`, per `RM0390` Table 5)** and read `FLASH->ACR` back to confirm `LATENCY == 2`.
3. **Otherwise:** If you switch `SYSCLK` to `84 MHz` while `FLASH->ACR` is still at `0` wait states, the CPU fetches instructions faster than the Flash memory array can output valid bits, reading corrupted instructions and crashing immediately! (And `0` wait states means no extra Flash wait cycles—it does not mean every CPU instruction finishes in 1 clock cycle.)

```text
CPU requests Flash instruction -> Flash interface waits `LATENCY` cycles -> CPU receives valid instruction
  At 16 MHz (Reset): LATENCY = 0 Wait States
  At 84 MHz (3.3 V): LATENCY = 2 Wait States (MUST be set BEFORE switching SYSCLK to 84 MHz!)
```

```c
#include <stdint.h>

typedef struct {
    volatile uint32_t ACR; /* 0x40023C00: bits [3:0] = LATENCY (0..15 wait states) */
} FLASH_Preview_Type;

#define FLASH_PREVIEW ((FLASH_Preview_Type *)0x40023C00UL)

static inline void set_flash_2_wait_states(void)
{
    /* Set LATENCY[3:0] = 2 WS (required for 60 MHz < HCLK <= 90 MHz at 2.7..3.6 V): */
    FLASH_PREVIEW->ACR = (FLASH_PREVIEW->ACR & ~0xFUL) | 2UL;
    while ((FLASH_PREVIEW->ACR & 0xFUL) != 2UL) {
        /* Wait until hardware confirms LATENCY == 2 */
    }
}
```

---

## Put The Reset State Together

| Reset item | Plain-English meaning |
| --- | --- |
| Thread mode, Privileged | Startup runs normal code with full system-control permission |
| `CONTROL = 0x00` | Thread mode uses `MSP` (`SPSEL = 0`), Privileged (`nPRIV = 0`), no active FPU context (`FPCA = 0`) |
| Initial `MSP` (`R13`) | Loaded by hardware from word `0` of the boot vector table (`0x20020000`) |
| `IPSR = 0` | No exception handler is currently active (CPU is in Thread mode) |
| `PRIMASK = BASEPRI = FAULTMASK = 0` | None of the three CPU masks is blocking exceptions |
| `SCB->VTOR = 0x00000000` | Vector table is initially accessed through the boot-alias address |
| `HSI` ON and Ready (`16 MHz`) | Initial system clock comes from the `16 MHz` internal oscillator (`SW = 0`, `SWS = 0`) |
| All PLLs OFF | Multiplied PLL clocks (`PLLON`, `PLLI2SON`, `PLLSAION`) are disabled at reset |
| `FLASH->ACR` `LATENCY = 0` | `0` extra Flash wait cycles programmed for the `16 MHz` reset clock |

Power-reset circuitry releases reset according to its specified supply
thresholds and timing; it does not wait for the supply to equal exactly 3.3 V.
The reset values describe the initial hardware state before `Reset_Handler()` runs.

## Feynman Recap

Inside the Cortex-M4 CPU, `R0..R12` hold working numbers, `SP` (`MSP` or `PSP`)
points to the stack in RAM, `LR` holds the return address (or `EXC_RETURN`),
`PC` points to the current instruction in Flash, and `xPSR` holds flags, the
Thumb bit (`T = 1`), and `IPSR` (which tells us if an interrupt handler is
running). `CONTROL` chooses whether Thread mode is Privileged or Unprivileged
and whether it uses `MSP` or `PSP`. `PRIMASK`, `BASEPRI`, and `FAULTMASK` are
three ways to temporarily block interrupts. Outside the CPU register bank,
memory-mapped blocks sit at fixed addresses: `SCB->VTOR` (`0xE000ED08`) points
to the vector table, `RCC` (`0x40023800`) turns oscillators/PLLs on (`CR`) and
selects the system clock (`CFGR`), and `FLASH->ACR` (`0x40023C00`) adds Flash
wait states before raising the clock to `84 MHz`.

## Short Interview Answers

### How do `PRIMASK` and `BASEPRI` differ?

- **Simple answer:** `PRIMASK` broadly blocks all configurable-priority exceptions; `BASEPRI` blocks only interrupts at or below a chosen priority threshold.
- **How:** Writing `BASEPRI = (5U << 4)` (`0x50`) on STM32F446 blocks logical priorities `5..15` while leaving higher-urgency priorities `0..4` unmasked.
- **Why:** An RTOS kernel (like FreeRTOS) can protect shared kernel lists while allowing urgent hardware interrupts (`0..4`) to fire with zero delay.

### Does `CONTROL = 0` mean interrupts are disabled?

- **Simple answer:** No.
- **How:** `CONTROL = 0` selects Privileged Thread mode (`nPRIV = 0`) and `MSP` (`SPSEL = 0`); interrupt masking is controlled separately by `PRIMASK`, `BASEPRI`, `FAULTMASK`, and `NVIC`.
- **Why:** Execution privilege/stack selection and interrupt admission are separate hardware controls.

### Does enabling a PLL in `RCC->CR` make it the CPU clock?

- **Simple answer:** No.
- **How:** You must configure Flash wait states (`FLASH->ACR`), enable `PLLON = 1` in `RCC->CR`, wait for `PLLRDY == 1`, request the switch in `RCC->CFGR` (`SW = 2`), and wait for `SWS == 2`.
- **Why:** Selecting a PLL before it locks or before Flash latency is increased crashes the CPU.

## References

- [Arm DUI0553B](../../docs/stm32/um-cortexm4/DUI0553.pdf):
  sections 2.1.1-2.1.3, 2.3 and 4.3.4; core register and exception tables.
- [STM32F446 RM0390 Rev 6](../../docs/stm32/um-f446re/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf):
  sections 3.8.1 and 6.3.1; reset, boot, RCC and Flash descriptions.
- [STM32F446 device header](../../../p1_f446re_baseline/Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f446xx.h#L49):
  four priority bits; RCC and FLASH fields.
- [Cortex-M4 CMSIS header](../../../p1_f446re_baseline/Drivers/CMSIS/Include/core_cm4.h#L1557):
  SCB mapping.

## Corrections

- Initial publication, 2026-10-28.
- 2026-10-29: Updated Scenes 1–7 with step-by-step explanations and C code snippets.
- 2026-11-01: Renamed from `W01-L19` to `W01-L16` as the second Arm Cortex-M4 prerequisite lesson, added the Unified Example, and added the complete `R0..R15`/`xPSR` vs. memory-mapped register block overview in Scene 1.
