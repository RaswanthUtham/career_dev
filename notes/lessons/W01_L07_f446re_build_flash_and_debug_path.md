# W01-L07: From C Source To Running Firmware On The F446RE (The Silicon Journey)

> **Source-reviewed:** [startup_stm32f446retx.s](../../../p1_f446re_baseline/Core/Startup/startup_stm32f446retx.s),
> [STM32F446RETX_FLASH.ld](../../../p1_f446re_baseline/STM32F446RETX_FLASH.ld),
> and [system_stm32f4xx.c](../../../p1_f446re_baseline/Core/Src/system_stm32f4xx.c),
> 2026-10-27.

Previous: [W01-L06: MCU Driver Contracts And Buffer Ownership](W01_L06_mcu_driver_contracts_and_buffer_ownership.md).

## 0. Know This First

How does a `.c` file typed on your Windows laptop turn into voltage signals and
running instructions inside the **STM32F446RE** microcontroller?

Let's follow two global C variables and `main()` on their journey from your
editor all the way to the first line of `main()`:

```c
uint32_t service_count = 7; /* Initialized global (.data) */
uint32_t fault_count;       /* Zero-initialized global (.bss) */
```

| Term | Role in our story |
| --- | --- |
| **Host (`Windows Laptop`)** | Runs STM32CubeIDE, the `arm-none-eabi-gcc` cross-compiler, and the GDB debugger. |
| **Onboard `ST-LINK` Probe** | The debug translator chip on the top section of the Nucleo board. Bridges USB from your laptop to the STM32's **SWD** pins. |
| **Target (`STM32F446RE`)** | The Arm Cortex-M4 microcontroller with **512 KB Flash** (`0x08000000`) and **128 KB SRAM** (`0x20000000`). |
| **Flash (`0x08000000`)** | Non-volatile memory that keeps your code (`Reset_Handler`, `main`), constants (`.rodata`), and initial variable values (`7`) even when power is unplugged. |
| **SRAM (`0x20000000`)** | Fast read/write working memory for live variables (`.data`, `.bss`) and the CPU stack (`sp`). Starts with random bytes at power-on until `Reset_Handler` initializes it. |
| **Vector Table (`.isr_vector`)** | An array of 32-bit addresses placed at the very start of Flash (`0x08000000`). Word 0 holds the initial Stack Pointer (`_estack`), and Word 1 holds the address of `Reset_Handler`. |

---

## 1. Goal

Follow the 4-scene movie of how C code is **cross-compiled and linked** on
Windows, **flashed over USB/SWD** into `0x08000000`, **booted by Cortex-M4
hardware** via the vector table, and **prepared by `Reset_Handler`** so
`service_count == 7` and `fault_count == 0` when `main()` begins.

---

## 2. The 4-Scene Movie: From `main.c` on Windows to `main()` on the STM32

```text
SCENE 1: The Build Factory on Windows (arm-none-eabi-gcc + Linker)
  |  main.c + startup_stm32f446retx.s
  |    -- [Preprocess -> Compile -> Assemble] --> .o object files
  |    -- [Linker + STM32F446RETX_FLASH.ld]   --> p1_f446re_baseline.elf + .map
  v
SCENE 2: Flashing Over the USB-to-SWD Bridge
  |  Windows GDB Server reads the loadable Flash segments from .elf
  |    -- [USB cable] --> Onboard ST-LINK probe
  |    -- [SWD wires] --> Programs STM32F446RE Flash starting at 0x08000000
  v
SCENE 3: Hardware Reset Inside the Cortex-M4 Core
  |  1. CPU reads Word 0 of Flash (0x08000000) -> loads Stack Pointer `sp = _estack` (0x20020000)
  |  2. CPU reads Word 1 of Flash (0x08000004) -> jumps Program Counter `PC` to `Reset_Handler`
  v
SCENE 4: `Reset_Handler` Prepares SRAM Before Calling `main()`
     1. Calls `SystemInit()` -> enables the Cortex-M4 Floating-Point Unit (FPU)
     2. Copies `.data` initial values (like `service_count = 7`) from Flash (`_sidata`) to SRAM (`_sdata.._edata`)
     3. Writes `0` across `.bss` in SRAM (`_sbss.._ebss`, setting `fault_count = 0`)
     4. Calls `__libc_init_array()` and branches to `main()`!
```

---

## 3. Before And After of Each Scene

### Scene 1: Linking With `STM32F446RETX_FLASH.ld`
Why does `.data` (`service_count = 7`) appear in **both** Flash and SRAM?
Because SRAM loses its contents when power is unplugged! So the linker stores the initial value `7` in **Flash** (`_sidata`, the Load Memory Address / LMA) and assigns the variable's live working address in **SRAM** (`_sdata`, the Virtual/Runtime Memory Address / VMA):

```text
BEFORE Linking (.o files):               AFTER Linking (p1_f446re_baseline.elf):
+------------------------------------+   +--------------------------------------------+
| Unplaced sections:                 |   | FLASH (0x08000000, kept when power is off):|
|   .isr_vector (vector table)       |   |   [ .isr_vector ] (_estack, Reset_Handler) |
|   .text       (functions)          |   |   [ .text       ] (Reset_Handler, main...) |
|   .data       (service_count = 7)  |   |   [ _sidata     ] (stored initial value 7) |
|   .bss        (fault_count)        |   |                                            |
|                                    |   | SRAM (0x20000000, runtime working memory): |
|                                    |   |   [ .data (_sdata.._edata) ] service_count |
|                                    |   |   [ .bss  (_sbss.._ebss)   ] fault_count   |
+------------------------------------+   +--------------------------------------------+
```

### What the 3 Build Output Files Do

| File | What it contains and how we use it |
| --- | --- |
| **`.elf` (`p1_f446re_baseline.elf`)** | Contains the loadable Flash image **plus** symbol names (`main`, `SystemCoreClock`) and DWARF source-line debug info used by GDB. |
| **`.map` (`p1_f446re_baseline.map`)** | Text report written by the linker showing the exact Flash/SRAM address and size of every section and global symbol. |
| **`.bin` / `.hex`** | **`.bin`:** raw memory bytes; **`.hex` (Intel HEX):** ASCII hex records with target addresses, record types, and checksums. Used by standalone production programmers. |

### Scene 3 & 4: Hardware Reset -> `Reset_Handler` -> `main()`
Look at how `service_count` and `fault_count` in SRAM change from power-on to `main()`:

```text
AT POWER-ON (Before Reset_Handler):      ENTERING main() (After Reset_Handler):
+------------------------------------+   +--------------------------------------------+
| CPU PC          = Reset_Handler    |   | CPU PC          = main()                   |
| CPU sp          = 0x20020000       |   | CPU sp          = 0x20020000               |
| Flash `_sidata` = 7                |   | Flash `_sidata` = 7                        |
| SRAM `.data`    = random bytes     |   | SRAM `.data`    = 7 (copied from _sidata!) |
| SRAM `.bss`     = random bytes     |   | SRAM `.bss`     = 0 (zeroed by loop!)      |
+------------------------------------+   +--------------------------------------------+
```

---

## 4. How the Startup Assembly Actually Does This (`startup_stm32f446retx.s`)

Source link: [Core/Startup/startup_stm32f446retx.s:62](../../../p1_f446re_baseline/Core/Startup/startup_stm32f446retx.s#L62)

```armasm
/* 1. HARDWARE VECTOR TABLE AT 0x08000000 (lines 124-127):
 *    At reset, Cortex-M4 hardware automatically reads these first two 32-bit words: */
g_pfnVectors:
  .word  _estack         /* Word 0 (0x08000000): Hardware loads this into SP (0x20020000) */
  .word  Reset_Handler   /* Word 1 (0x08000004): Hardware jumps PC to Reset_Handler */

/* 2. RESET_HANDLER PREPARES THE C ENVIRONMENT (lines 62-102): */
Reset_Handler:
  ldr   sp, =_estack     /* Ensure stack pointer sp = 0x20020000 (top of 128 KB SRAM) */
  bl    SystemInit       /* Enable Cortex-M4 FPU coprocessor (CP10/CP11) */

  /* 3. COPY .data INITIAL VALUES FROM FLASH (_sidata) TO SRAM (_sdata.._edata):
   *    Copies 4 bytes at a time (e.g., copying `7` into SRAM `service_count`). */
  ldr r0, =_sdata        /* r0 = destination start in SRAM (0x20000000) */
  ldr r1, =_edata        /* r1 = destination end in SRAM */
  ldr r2, =_sidata       /* r2 = source initializers in Flash */
  movs r3, #0            /* r3 = byte offset (0, 4, 8, ...) */
  b LoopCopyDataInit
CopyDataInit:
  ldr r4, [r2, r3]       /* Read 32-bit word from Flash (_sidata + r3) */
  str r4, [r0, r3]       /* Write 32-bit word into SRAM (_sdata + r3) */
  adds r3, r3, #4
LoopCopyDataInit:
  adds r4, r0, r3
  cmp r4, r1
  bcc CopyDataInit       /* Keep copying while (_sdata + r3) < _edata */

  /* 4. ZERO-FILL .bss IN SRAM (_sbss.._ebss):
   *    Writes 0x00000000 across .bss so uninitialized globals start at 0. */
  ldr r2, =_sbss         /* r2 = start of .bss in SRAM */
  ldr r4, =_ebss         /* r4 = end of .bss in SRAM */
  movs r3, #0            /* r3 = 0 */
  b LoopFillZerobss
FillZerobss:
  str  r3, [r2]          /* Write 0 into SRAM */
  adds r2, r2, #4
LoopFillZerobss:
  cmp r2, r4
  bcc FillZerobss        /* Keep zeroing while r2 < _ebss */

  /* 5. RUN C RUNTIME INIT AND ENTER main(): */
  bl __libc_init_array
  bl main                /* Jump into our C application in Core/Src/main.c! */
```

---

## 5. Control Flow

### (a) One-Line Chain

```text
main.c + startup_stm32f446retx.s -> [arm-none-eabi-gcc & ld] -> p1_f446re_baseline.elf
  -> [ST-LINK over USB/SWD] -> Flash @ 0x08000000
  -> [Hardware Reset] -> g_pfnVectors (loads SP, jumps to Reset_Handler)
  -> SystemInit() -> copy .data (_sidata -> _sdata) -> zero .bss (_sbss.._ebss) -> main()
```

### (b) Step Table

| # | Where | Actor | What happens (`Before -> After`) |
| --- | --- | --- | --- |
| 1 | Windows Host | `arm-none-eabi-gcc` & Linker | Compiles `.c`/`.s` into `.o` and links `.elf` + `.map` using `STM32F446RETX_FLASH.ld` |
| 2 | USB -> ST-LINK -> SWD | Programmer / GDB Server | Writes loadable `.isr_vector`, `.text`, `.rodata`, and `.data` LMA image into Flash (`0x08000000`) |
| 3 | `0x08000000` (`.isr_vector`) | Cortex-M4 Hardware | Loads `sp = _estack` (`0x20020000`) and jumps `PC` to `Reset_Handler` |
| 4 | `system_stm32f4xx.c : SystemInit` | Target CPU (`Reset_Handler`) | Sets `SCB->CPACR` bits to enable the Cortex-M4 hardware FPU |
| 5 | `startup_stm32f446retx.s : CopyDataInit` | Target CPU (`Reset_Handler`) | Copies `.data` initializers from Flash (`_sidata`) into SRAM (`_sdata.._edata`) |
| 6 | `startup_stm32f446retx.s : FillZerobss` | Target CPU (`Reset_Handler`) | Writes `0` across `.bss` in SRAM (`_sbss.._ebss`) |
| 7 | `Core/Src/main.c : main` | Target CPU (Thread mode) | Begins executing `main()` with all C globals properly initialized |

---

## 6. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
Your STM32 chip has two kinds of memory: **Flash** (like a printed book that
remembers everything when the power is off) and **SRAM** (like a whiteboard that
wipes into random scribbles every time power turns off). On your laptop, the
compiler and linker pack your code and initial variable values (`7`) into the
Flash book. When the STM32 powers on, hardware reads the first two lines of the
Flash book to set up the stack pointer and jump to `Reset_Handler`.
`Reset_Handler` is the janitor who wakes up before `main()`: it copies the
starting numbers (`7`) from the Flash book onto the SRAM whiteboard (`.data`),
wipes the `.bss` part of the whiteboard clean to `0`, and then opens the door
to `main()`!

---

### Interview Q1: Why doesn't a Cortex-M microcontroller jump straight to `main()` at power-on reset?
* **Simple answer:** Because at power-on, SRAM contains random uninitialized bytes and the C runtime environment (`sp`, `.data`, `.bss`, FPU) is not set up yet.
* **What happens first:** Cortex-M hardware loads the initial stack pointer (`_estack`) from `0x08000000` and loads the `Reset_Handler` entry address from the reset-vector slot at `0x08000004` (jumping to `Reset_Handler` at `0x08000888`), which enables the FPU (`SystemInit`), copies `.data` initializers from Flash to SRAM, zeroes `.bss` in SRAM, and then calls `main()`.

### Interview Q2: Why does `.data` have two addresses in the linker script (`>RAM AT> FLASH`), while `.bss` only lives in `>RAM`?
* **Simple answer:**
  * **`.data` (`uint32_t service_count = 7;`):** Needs a **RAM** address (VMA) so C code can modify it at runtime, and a **Flash** address (LMA, `_sidata`) to store the initial value `7` while power is off.
  * **`.bss` (`uint32_t fault_count;`):** Starts at `0` for every variable. Instead of wasting Flash space storing thousands of zero bytes, the linker only records `_sbss` and `_ebss` in RAM, and `Reset_Handler` loops from `_sbss` to `_ebss` writing `0`.

### Interview Q3: Why do we give the debugger the `.elf` file instead of a raw `.bin` file?
* **Simple answer:** Both contain the machine code for Flash, but the **`.elf` file also carries the symbol table (function/variable names) and DWARF debug mapping** that allow GDB to map raw hex addresses like `0x080004f4` back to `main()` in `Core/Src/main.c`.

## 7. Corrections

- 2026-10-26: Initial publication.
- 2026-10-27 (revision 2): Rewrote in 4-scene story mode using the actual `startup_stm32f446retx.s` and `STM32F446RETX_FLASH.ld` code from `p1_f446re_baseline`.
