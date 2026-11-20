# W01-L09: Inside Your First Debug Build And Memory Map (`STM32F446RETX_FLASH.ld`)

> **Source-reviewed:** [STM32F446RETX_FLASH.ld](../../../p1_f446re_baseline/STM32F446RETX_FLASH.ld),
> [Core/Src/main.c](../../../p1_f446re_baseline/Core/Src/main.c),
> [Debug/p1_f446re_baseline.map](../../../p1_f446re_baseline/Debug/p1_f446re_baseline.map),
> and [Debug/Core/Src/subdir.mk](../../../p1_f446re_baseline/Debug/Core/Src/subdir.mk),
> 2026-10-27.

Previous: [W01-L08: Creating Our Clean Windows CubeIDE Project](W01_L08_windows_cubeide_project_baseline.md).

## 0. Know This First

When you click **Build Project** on `p1_f446re_baseline` (`Debug` configuration),
STM32CubeIDE runs `arm-none-eabi-gcc` and the linker to turn your C and assembly
files into `Debug/p1_f446re_baseline.elf` and `Debug/p1_f446re_baseline.map`.

| Term | What it does in our build |
| --- | --- |
| **Debug Configuration** | Compiles with `-O0` (disables optimization so source lines and variables follow your C code closely during debugging) and `-g3` (full debug symbols, including `#define` macros). |
| **Hard-Float ABI (`-mfloat-abi=hard`)** | Tells the compiler to use the Cortex-M4 hardware FPU (`fpv4-sp-d16`) instructions and pass floating-point arguments in `s0..s15` FPU registers. |
| **Memory Region (`FLASH` / `RAM`)** | Hardware address ranges declared in `STM32F446RETX_FLASH.ld`: `FLASH` starts at `0x08000000` (`512 KB`) and `RAM` starts at `0x20000000` (`128 KB`). |
| **`_estack` (`0x20020000`)** | Symbol set to `ORIGIN(RAM) + LENGTH(RAM)`. Because the Cortex-M4 stack is **full-descending** (decrements `sp` before storing), `_estack` starts at the top boundary `0x20020000` and grows downward into RAM (`0x2001FFFC`, `0x2001FFF8`, ...). |
| **`size` Summary (`text`, `data`, `bss`)** | Reports how many bytes your compiled firmware uses in Flash and SRAM (`text = 8408 B`, `data = 12 B`, `bss = 1644 B`). |

---

## 1. Goal

Trace the 3-scene build movie: how compiler flags compile `main.c`, how
`STM32F446RETX_FLASH.ld` maps `FLASH` and `RAM` addresses in
`p1_f446re_baseline.map`, and how the 4 setup calls inside `main.c` prepare the
chip (including the `84 MHz` PLL math).

---

## 2. The 3-Scene Build Story

```text
SCENE 1: Compiling Each Source File (.c / .s -> .o)
  |  arm-none-eabi-gcc compiles Core/Src/main.c and Drivers/.../*.c with:
  |    -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard
  |    -DSTM32F446xx -DUSE_HAL_DRIVER -O0 -g3
  v
SCENE 2: Linking With STM32F446RETX_FLASH.ld (.o -> .elf + .map)
  |  Linker places code & constants into FLASH (0x08000000..0x0807FFFF)
  |  and runtime variables/stack into RAM (0x20000000..0x2001FFFF)
  v
SCENE 3: Reporting Final Section Sizes (arm-none-eabi-size)
     text = 8408 bytes  (code + vector table + rodata in FLASH)
     data =   12 bytes  (initialized globals in RAM, loaded from FLASH)
     bss  = 1644 bytes  (zero-initialized globals + _user_heap_stack in RAM)
```

### Compiler & Linker Flags Breakdown

| Flag in `subdir.mk` / makefile | What it tells `arm-none-eabi-gcc` |
| --- | --- |
| `-mcpu=cortex-m4 -mthumb` | Emit Thumb-2 instructions for the Arm Cortex-M4 core |
| `-mfpu=fpv4-sp-d16 -mfloat-abi=hard` | Use the Cortex-M4 single-precision hardware FPU and FPU register calling convention |
| `-DSTM32F446xx -DUSE_HAL_DRIVER` | Define preprocessor macros so `stm32f4xx.h` selects `stm32f446xx.h` and includes the HAL headers |
| `-O0 -g3` | Disable compiler optimization (`-O0`) for predictable source-level debugging and embed maximum debug/macro symbol tables (`-g3`) |
| `-T"../STM32F446RETX_FLASH.ld"` | Use our 512 KB Flash / 128 KB RAM linker script |
| `-Wl,-Map="p1_f446re_baseline.map"` | Pass `-Map` to the linker (`ld`) to generate the memory map report |

---

## 3. Inside `STM32F446RETX_FLASH.ld`: Concrete Address Math

Source link: [STM32F446RETX_FLASH.ld:36](../../../p1_f446re_baseline/STM32F446RETX_FLASH.ld#L36)

```ld
/* 1. TOP OF STACK AT THE END OF 128 KB RAM:
 *    0x20000000 + (128 * 1024) = 0x20000000 + 0x20000 = 0x20020000 */
_estack = ORIGIN(RAM) + LENGTH(RAM);

/* 2. MINIMUM HEAP (512 B) AND STACK (1024 B) LINKER CHECK: */
_Min_Heap_Size = 0x200;
_Min_Stack_Size = 0x400;

/* 3. HARDWARE MEMORY MAP OF STM32F446RE: */
MEMORY
{
  RAM    (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
}
```

### How Byte Ranges Work (Small Example -> Real STM32 Memory Map)
* **Small example:** A `16-byte` region starting at address `100` occupies bytes `100` through `100 + 16 - 1 = 115`. Address `116` is one byte past the end.
* **Real STM32F446RE regions (`1 K = 1024 = 0x400` bytes):**

| Memory Region | Start Address | Size | Last Valid Byte (`Start + Size - 1`) | What lives here in `p1_f446re_baseline.map` |
| --- | --- | --- | --- | --- |
| **FLASH** | `0x08000000` | `512 KB` (`0x80000` B) | `0x0807FFFF` | `.isr_vector`, `.text` (`main` at `0x080004f4`), `.rodata`, and `_sidata` initializers |
| **RAM** | `0x20000000` | `128 KB` (`0x20000` B) | `0x2001FFFF` | `.data` (`SystemCoreClock` at `0x20000000`), `.bss` (`huart2` at `0x20000028`, `uwTick` at `0x20000070`), and `._user_heap_stack` |

```text
FLASH (0x08000000 .. 0x0807FFFF)              SRAM (0x20000000 .. 0x2001FFFF)
+-----------------------------------+         +-----------------------------------+
| 0x08000000: .isr_vector (0x1c4 B) |         | 0x20000000: .data                 |
| 0x080001c4: .text (Reset_Handler, |         |   (SystemCoreClock = 16000000)    |
|             main @ 0x080004f4...) |         | 0x2000000c: .bss                  |
|             .rodata               |         |   (huart2 @ 0x20000028,           |
|             _sidata (12 B init) --|--copy-->|    uwTick @ 0x20000070)           |
| ... unused Flash ...              |         |             ._user_heap_stack     |
|                                   |         |                 ^                 |
|                                   |         |                 | (grows down)    |
+-----------------------------------+         +-----------------------------------+
                                              0x20020000: _estack (initial SP)
```

---

## 4. What `Core/Src/main.c` Sets Up (And How the `84 MHz` PLL Math Works)

Source link: [Core/Src/main.c:65](../../../p1_f446re_baseline/Core/Src/main.c#L65)

```c
int main(void)
{
  /* 1. Initialize Flash caches, priority grouping, and 1 ms SysTick timer: */
  HAL_Init();

  /* 2. Switch CPU clock from 16 MHz HSI to 84 MHz PLL (math below): */
  SystemClock_Config();

  /* 3. Enable GPIO port clocks; set PA5 (LD2) as output & PC13 (B1) as input: */
  MX_GPIO_Init();

  /* 4. Configure USART2 (PA2=TX, PA3=RX) at 115200 baud, 8-N-1: */
  MX_USART2_UART_Init();

  /* 5. Main loop (ready for our application code in Lesson 12): */
  while (1)
  {
  }
}
```

### How `SystemClock_Config()` Turns `16 MHz` Into `84 MHz`
Inside [SystemClock_Config](../../../p1_f446re_baseline/Core/Src/main.c#L110), the
chip starts on its built-in **High-Speed Internal (`HSI`)** oscillator at
**`16 MHz`** and feeds it through the **Phase-Locked Loop (`PLL`)**:

```text
               HSI = 16 MHz
                    |
                    v
         +---------------------+
         | Divide by PLLM = 16 |  ---> 16 MHz / 16 = 1 MHz (PLL VCO input)
         +---------------------+
                    |
                    v
         +---------------------+
         | Multiply by PLLN=336|  ---> 1 MHz * 336 = 336 MHz (PLL VCO output)
         +---------------------+
                    |
                    v
         +---------------------+
         | Divide by PLLP = 4  |  ---> 336 MHz / 4 = 84 MHz (SYSCLK / HCLK!)
         +---------------------+
```

---

## 5. Control Flow of the Build

| # | Stage | Input -> Output |
| --- | --- | --- |
| 1 | `arm-none-eabi-gcc -c` | Compiles `Core/Src/*.c` and `Drivers/STM32F4xx_HAL_Driver/Src/*.c` into `Debug/**/*.o` |
| 2 | `arm-none-eabi-gcc -x assembler-with-cpp` | Assembles `Core/Startup/startup_stm32f446retx.s` into `Debug/Core/Startup/startup_stm32f446retx.o` |
| 3 | `arm-none-eabi-gcc -T"../STM32F446RETX_FLASH.ld"` | Links all `.o` files into `Debug/p1_f446re_baseline.elf` and writes `Debug/p1_f446re_baseline.map` |
| 4 | `arm-none-eabi-size` | Prints `text = 8408`, `data = 12`, `bss = 1644` (`dec = 10064` bytes) |

---

## 6. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
Building the project is like printing a blueprint and packing a toolbox for the
STM32. The compiler translates each C file into Cortex-M4 machine instructions
(`.o`), and the linker uses `STM32F446RETX_FLASH.ld` to assign every instruction
a permanent home in the `512 KB` Flash (`0x08000000`) and every variable a
working home in the `128 KB` SRAM (`0x20000000`), with the stack pointer starting
at the ceiling (`_estack = 0x20020000`). Inside `main.c`, the PLL acts like a
gearbox on a bicycle: it divides the `16 MHz` internal clock by `16` (`1 MHz`),
multiplies it by `336` (`336 MHz`), and divides by `4` to spin the CPU at
**`84 MHz`**!

---

### Interview Q1: In the linker script, why is `_estack` set to `0x20020000` when the last byte of 128 KB RAM is `0x2001FFFF`?
* **Simple answer:** Because the Arm Cortex-M4 stack is **Full-Descending**.
* **How it works:** When the CPU pushes a 4-byte register onto the stack (`push {r0}`), hardware **decrements `SP` by 4 first** (`0x20020000 - 4 = 0x2001FFFC`) and then writes the 4 bytes at `0x2001FFFC..0x2001FFFF`. Setting `_estack = 0x20020000` uses the very last 4 bytes of RAM cleanly without wasting a single byte.

### Interview Q2: What do `text`, `data`, and `bss` mean in the output of `arm-none-eabi-size`?
* **Simple answer:**
  * **`text` (`8408` bytes):** Code (`.text`), vector table (`.isr_vector`), and read-only constants (`.rodata`) stored in Flash.
  * **`data` (`12` bytes):** Non-zero initialized global/static variables (`SystemCoreClock = 16000000`, `uwTickFreq = 1`, `uwTickPrio`) that occupy **both** Flash (for their initial values) and SRAM (for runtime reads/writes).
  * **`bss` (`1644` bytes):** Zero-initialized globals (`huart2`, `uwTick`) plus the linker script's reserved `._user_heap_stack` (`0x200 + 0x400 = 1536` bytes) in SRAM.

### Interview Q3: Why must `-mfloat-abi=hard` match across every compiled `.o` file and library in a Cortex-M4F build?
* **Simple answer:** Because `-mfloat-abi=hard` and `-mfloat-abi=softfp`/`soft` use **different CPU registers to pass `float` parameters**.
* **What happens on a mismatch:** Code compiled with `hard` puts a `float` argument in FPU register `s0`, while a library compiled with `soft` looks for that same argument in general-purpose register `r0`—causing silent math corruption or a linker ABI error.

## 7. Corrections

- 2026-10-27: Initial publication.
- 2026-10-27 (revision 2): Rewrote in 3-scene story mode with the actual `p1_f446re_baseline.elf` sizes (`text = 8408`, `data = 12`, `bss = 1644`) and visual Flash/SRAM + PLL diagrams.
