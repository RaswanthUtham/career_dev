# W01-L10: Freezing Time At The Front Door Of `main()` (First ST-LINK Debug Session)

> **Source-reviewed:** [Debug/p1_f446re_baseline.elf](../../../p1_f446re_baseline/Debug/p1_f446re_baseline.elf),
> [Debug/p1_f446re_baseline.map](../../../p1_f446re_baseline/Debug/p1_f446re_baseline.map),
> [Core/Startup/startup_stm32f446retx.s](../../../p1_f446re_baseline/Core/Startup/startup_stm32f446retx.s),
> and [Core/Src/system_stm32f4xx.c](../../../p1_f446re_baseline/Core/Src/system_stm32f4xx.c),
> 2026-10-27.

Previous: [W01-L09: Inside Your First Debug Build And Memory Map](W01_L09_first_debug_build_and_memory_map.md).

## 0. Know This First

Now that `Debug/p1_f446re_baseline.elf` is built, we connect the
**NUCLEO-F446RE** board over USB and launch a **Debug Session** that flashes the
chip, runs `Reset_Handler`, and freezes the CPU at the start of `main()` (the
`main` function symbol starts at `0x080004f4` with its stack-frame prologue, and
GDB sets the `break main` source breakpoint at `0x080004f8` on the `HAL_Init();`
call).

| Term | What it does in our debug session |
| --- | --- |
| **GDB (`arm-none-eabi-gdb`)** | Host debugger running on Windows that reads symbol names and source lines from `p1_f446re_baseline.elf`. |
| **ST-LINK GDB Server** | Host bridge program that translates GDB commands into USB packets for the onboard ST-LINK probe. |
| **SWD (`Serial Wire Debug`)** | 2-wire hardware debug bus (`SWDIO` on `PA13`, `SWCLK` on `PA14`) connecting the onboard ST-LINK to the STM32F446RE core. |
| **Hardware Breakpoint** | A comparator inside the Cortex-M4 **Flash Patch and Breakpoint (`FPB`)** unit that halts the CPU the instant the Program Counter (`PC`) fetches the target instruction address (`main`). |
| **`SystemCoreClock`** | Global `uint32_t` in `.data` (`0x20000000`) holding the current core clock frequency in Hz (`16000000` at `main()` entry before `SystemClock_Config()` runs). |
| **`uwTick`** | Global `volatile uint32_t` in `.bss` (`0x20000070`) incremented every `1 ms` by `SysTick_Handler` (`0` at `main()` entry). |

---

## 1. Goal

Follow the 3-scene debug story from clicking **Debug** in CubeIDE to halting at
`main()`, and explain the exact values sitting in SRAM (`SystemCoreClock`,
`uwTick`, and `huart2.Init.BaudRate`) before `HAL_Init()` executes.

---

## 2. The 3-Scene Debug Story

```text
SCENE 1: Flashing Over USB -> ST-LINK -> SWD
  |  CubeIDE launches GDB + ST-LINK GDB Server on Windows
  |  -> Programs loadable Flash sections (8408 B text + 12 B data init) into 0x08000000
  |  -> Sets a hardware breakpoint for `main` (`0x080004f8`, right after the 4-byte prologue at `0x080004f4`) and issues a hardware reset
  v
SCENE 2: Hardware Reset & `Reset_Handler` Execute on the STM32F446RE
  |  1. Cortex-M4 loads `SP = 0x20020000` (`_estack`) and `PC = Reset_Handler`
  |  2. `Reset_Handler` calls `SystemInit()` (enables FPU)
  |  3. `Reset_Handler` copies `.data` from Flash to SRAM (`SystemCoreClock = 16000000`)
  |  4. `Reset_Handler` zeroes `.bss` in SRAM (`uwTick = 0`, `huart2 = {0}`)
  |  5. `Reset_Handler` executes `bl main`!
  v
SCENE 3: CPU Freezes at the Front Door of `main()` (Before `HAL_Init()`)
     The Cortex-M4 FPB unit matches `PC == main` and halts the CPU in Thread Mode
     with the cursor sitting on `HAL_Init();`!
```

---

## 3. Before And After: What Is Sitting in SRAM When Stopped at `main()`?

Look at the three variables in the **Expressions** view while the CPU is halted
on the `HAL_Init();` line at the start of `main()`:

| Expression in Debugger | SRAM Address (from `.map`) | Section | Value at `main()` Entry | Why It Has This Value Right Now |
| --- | --- | --- | ---: | --- |
| `SystemCoreClock` | `0x20000000` | `.data` | **`16000000`** | Initialized to `16000000` (`16 MHz` HSI) in `system_stm32f4xx.c:139` and copied from Flash (`_sidata`) to SRAM by `Reset_Handler`. |
| `uwTick` | `0x20000070` | `.bss` | **`0`** | Zeroed by `Reset_Handler`'s `FillZerobss` loop; `HAL_Init()` has not started `SysTick` yet. |
| `huart2.Init.BaudRate` | `0x2000002c` (`huart2` at `0x20000028`) | `.bss` | **`0`** | Zeroed by `Reset_Handler`'s `FillZerobss` loop; `MX_USART2_UART_Init()` has not written `115200` yet. |

```text
AT RESET (Before Reset_Handler):         STOPPED AT main() (Before HAL_Init):
+------------------------------------+   +--------------------------------------------+
| SRAM 0x20000000 (SystemCoreClock): |   | SRAM 0x20000000 (SystemCoreClock):         |
|   [ random power-on bytes ]        |   |   16000000 (copied from Flash _sidata!)    |
| SRAM 0x20000028 (huart2):          |   | SRAM 0x20000028 (huart2):                  |
|   [ random power-on bytes ]        |   |   all 0 (BaudRate = 0, cleared by .bss!)   |
| SRAM 0x20000070 (uwTick):          |   | SRAM 0x20000070 (uwTick):                  |
|   [ random power-on bytes ]        |   |   0 (cleared by .bss!)                     |
+------------------------------------+   +--------------------------------------------+
```

---

## 4. How the Code Sets Up These Starting Values

Source links: [Core/Src/system_stm32f4xx.c:139](../../../p1_f446re_baseline/Core/Src/system_stm32f4xx.c#L139) and [Core/Src/main.c:44](../../../p1_f446re_baseline/Core/Src/main.c#L44)

```c
/* 1. In Core/Src/system_stm32f4xx.c (line 139):
 *    Placed in `.data` because it has a non-zero initializer (16,000,000 Hz).
 *    Reset_Handler copies 16000000 from Flash into 0x20000000 before main()! */
uint32_t SystemCoreClock = 16000000;

/* 2. In Core/Src/main.c (line 44):
 *    Unannotated global struct -> placed in `.bss` at 0x20000028.
 *    Reset_Handler zeroes every field (including huart2.Init.BaudRate = 0) before main()! */
UART_HandleTypeDef huart2;

/* 3. In Core/Src/main.c (line 65):
 *    Our hardware breakpoint stops the CPU here BEFORE HAL_Init() runs: */
int main(void)
{
  HAL_Init();            /* <--- Debugger cursor stops here first! */
  SystemClock_Config();  /* Will later update SystemCoreClock to 84000000 */
  MX_GPIO_Init();
  MX_USART2_UART_Init(); /* Will later set huart2.Init.BaudRate to 115200 */
  while (1) {}
}
```

---

## 5. Control Flow: Host to Target Breakpoint

```text
CubeIDE Debug Launch -> ST-LINK GDB Server -> USB -> Onboard ST-LINK -> SWD
  -> Flash programmed @ 0x08000000 -> Hardware Reset -> Reset_Handler (.data & .bss init)
  -> Hardware Breakpoint hits at main() -> GDB reads SRAM (16000000, 0, 0)
```

| # | Actor | Action | Result |
| --- | --- | --- | --- |
| 1 | CubeIDE / GDB | Load `Debug/p1_f446re_baseline.elf` | Host loads symbols; ST-LINK flashes `0x08000000` |
| 2 | GDB Startup Tab | Set breakpoint at `main` and Resume | Hardware FPB breakpoint armed at `main` (`0x080004f8`, first C line of `main` @ `0x080004f4`) |
| 3 | STM32 `Reset_Handler` | Run `SystemInit`, copy `.data`, zero `.bss` | `SystemCoreClock = 16000000`, `uwTick = 0`, `huart2 = {0}` |
| 4 | STM32 Cortex-M4 | Execute `bl main` | Halts at first line of `main()` (`HAL_Init();`) |
| 5 | Expressions View | Read `0x20000000`, `0x20000070`, `0x2000002c` | Displays `16000000`, `0`, `0` |

---

## 6. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
Setting a breakpoint at `main()` is like putting a stop sign right at the front
door of your office building (`main()`), while letting the morning janitor
(`Reset_Handler`) finish unlocking the building and setting up the whiteboards
first. When the CPU halts at `main()`, the janitor has already copied
`SystemCoreClock = 16000000` from Flash into SRAM and wiped `uwTick` and
`huart2` to `0`—and now the CPU is paused waiting for your signal to run the
setup functions inside `main()`!

---

### Interview Q1: What is the difference between setting a breakpoint at `main` (and resetting) versus manually setting `PC = main` in the debugger?
* **Simple answer:**
  * **Breakpoint at `main` + Reset:** Lets the CPU execute the normal hardware reset sequence (`Loads SP = _estack` -> runs `Reset_Handler` -> enables FPU in `SystemInit` -> copies `.data` -> zeroes `.bss`) before halting cleanly at `main()`.
  * **Manually forcing `PC = main`:** Skips `Reset_Handler` completely, leaving `.data` and `.bss` uninitialized and the stack pointer (`SP`) in whatever state it had before!

### Interview Q2: Why is `SystemCoreClock` equal to `16,000,000` when the debugger stops at the start of `main()`, even though we configured an `84 MHz` clock in CubeMX?
* **Simple answer:** Because at hardware reset, the STM32F446RE always boots on its internal **`16 MHz` HSI oscillator**.
* **How it changes:** `Reset_Handler` copies the initial value `uint32_t SystemCoreClock = 16000000;` into SRAM before `main()` starts. The chip only switches to the `84 MHz` PLL when `SystemClock_Config()` executes *inside* `main()`.

### Interview Q3: How does GDB know that `SystemCoreClock` lives at `0x20000000` when the STM32 chip itself only stores raw bytes in Flash and SRAM?
* **Simple answer:** GDB reads the **symbol table and DWARF debug sections** stored inside `p1_f446re_baseline.elf` on the Windows PC, finds that the symbol `SystemCoreClock` is mapped to `0x20000000` (`4` bytes), and asks the ST-LINK probe to read `4` bytes from address `0x20000000` over SWD.

## 7. Corrections

- 2026-10-27: Initial publication.
- 2026-10-27 (revision 2): Rewrote in 3-scene story mode with Before/After SRAM diagrams and annotated code.
