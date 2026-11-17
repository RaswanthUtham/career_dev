# W01-L11: Watching The STM32 Wake Up Step By Step (`HAL_Init` -> `84 MHz` Clock -> `GPIO` -> `UART` -> `SysTick` ISR)

> **Source-reviewed:** [Core/Src/main.c](../../../p1_f446re_baseline/Core/Src/main.c),
> [Core/Src/stm32f4xx_hal_msp.c](../../../p1_f446re_baseline/Core/Src/stm32f4xx_hal_msp.c),
> [Core/Src/stm32f4xx_it.c](../../../p1_f446re_baseline/Core/Src/stm32f4xx_it.c),
> and [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c),
> 2026-10-27.

Previous: [W01-L10: Freezing Time At The Front Door Of `main()`](W01_L10_first_stlink_debug_stop_at_main.md).

## 0. Know This First

In Lesson 10, we froze the STM32F446RE right at the first line of `main()`.
Now we step through the four initialization calls inside `main()` and watch
each subsystem wake up in order:

```text
main()
  |-- Scene 1: HAL_Init()             -> Flash caches + 1 ms SysTick @ 16 MHz + HAL_MspInit()
  |-- Scene 2: SystemClock_Config()   -> PLL locks at 84 MHz + SysTick reloaded for 84 MHz!
  |-- Scene 3: MX_GPIO_Init()         -> Port clocks on + PA5 (LD2) output + PC13 (B1) input
  |-- Scene 4: MX_USART2_UART_Init()  -> PA2/PA3 muxed to AF7 + USART2 ready at 115200 baud
  `-- Scene 5: while (1)              -> Thread mode loops while SysTick_Handler ticks every 1 ms!
```

| Term | What it does in this story |
| --- | --- |
| **`SysTick` Timer** | A 24-bit countdown timer inside the Cortex-M4 core. Counts down from `SysTick->LOAD` to `0`, fires `SysTick_Handler` (`uwTick += 1`), and automatically reloads `LOAD`. |
| **`SysTick->LOAD` (`N - 1`)** | Because counting from `LOAD` down to `0` takes `LOAD + 1` clock ticks, a period of `N` clock cycles uses `LOAD = N - 1` (e.g., `10` ticks -> `LOAD = 9`, counting `9..0`). |
| **`HAL_MspInit()` / `HAL_UART_MspInit()` (`MSP`)** | **MCU Support Package** callbacks in `stm32f4xx_hal_msp.c`: `HAL_MspInit()` enables global system clocks (`SYSCFG`, `PWR`) and NVIC grouping during `HAL_Init()`, while `HAL_UART_MspInit()` enables the `USART2` clock and muxes pins `PA2`/`PA3` during `HAL_UART_Init()`. |
| **Thread Mode vs. Handler Mode** | **Thread mode** runs `main()`. When the `1 ms` `SysTick` interrupt fires, hardware switches the CPU to **Handler mode** to run `SysTick_Handler()`, then returns to Thread mode. |

---

## 1. Goal

Follow the 5 scenes of `main()` initialization, see the **Before -> After**
state of `SystemCoreClock`, `SysTick->LOAD`, `GPIOA`, `huart2`, and `uwTick` at
each breakpoint (`A` through `E`), and understand why `SysTick->LOAD` updates
when the clock switches from `16 MHz` to `84 MHz`.

---

## 2. The 5-Scene Initialization Movie (With Before & After Boxes)

We place breakpoints on the 5 lines in `main.c`:
* **Checkpoint A:** `HAL_Init();` (before `HAL_Init` runs)
* **Checkpoint B:** `SystemClock_Config();` (after `HAL_Init` finishes)
* **Checkpoint C:** `MX_GPIO_Init();` (after `SystemClock_Config` finishes)
* **Checkpoint D:** `MX_USART2_UART_Init();` (after `MX_GPIO_Init` finishes)
* **Checkpoint E:** `while (1)` (after `MX_USART2_UART_Init` finishes)

---

### Scene 1: `HAL_Init()` (Checkpoint A -> Checkpoint B)
* **Files:** [stm32f4xx_hal.c:157](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c#L157) and [stm32f4xx_hal_msp.c:62](../../../p1_f446re_baseline/Core/Src/stm32f4xx_hal_msp.c#L62)
* **What happens:**
  1. Enables the Flash Instruction Cache, Data Cache, and Prefetch buffer.
  2. Calls `HAL_InitTick()` to set up a **`1 ms` (`1000 Hz`) `SysTick` interrupt** using the starting `16 MHz` clock:
     * `N = 16,000,000 Hz / 1000 Hz = 16,000` clock pulses per millisecond.
     * `SysTick->LOAD = 16,000 - 1 = 15,999`.
  3. Calls `HAL_MspInit()`, which turns on the `SYSCFG` and `PWR` peripheral clocks and sets `NVIC_PRIORITYGROUP_0`.

```text
BEFORE HAL_Init() (Checkpoint A):        AFTER HAL_Init() (Checkpoint B):
+------------------------------------+   +--------------------------------------------+
| SystemCoreClock = 16,000,000 (16M) |   | SystemCoreClock = 16,000,000 (16 MHz)      |
| Flash Caches    = OFF              |   | Flash Caches    = I-Cache, D-Cache,Prefetch|
| SysTick->LOAD   = 0 (off)          |   | SysTick->LOAD   = 15,999 (1 ms @ 16 MHz!)  |
| SYSCFG / PWR clk= OFF              |   | SYSCFG / PWR clk= ON (via HAL_MspInit)     |
+------------------------------------+   +--------------------------------------------+
```

---

### Scene 2: `SystemClock_Config()` (Checkpoint B -> Checkpoint C)
* **Files:** [Core/Src/main.c:110](../../../p1_f446re_baseline/Core/Src/main.c#L110) and [stm32f4xx_hal_rcc.c](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_rcc.c)
* **Why `SysTick->LOAD` must be updated when the clock speeds up:**
  * At `84 MHz`, the CPU ticks `5.25x` faster than at `16 MHz`. If `SysTick->LOAD` stayed at `15,999`, the tick interrupt would fire every `16,000 / 84,000,000 = 0.19 ms` instead of `1.00 ms`!
  * Therefore, `HAL_RCC_ClockConfig()` updates `SystemCoreClock = 84000000` and calls `HAL_InitTick()` again to set:
    * `N = 84,000,000 Hz / 1000 Hz = 84,000` pulses per millisecond.
    * `SysTick->LOAD = 84,000 - 1 = 83,999`.

```text
BEFORE SystemClock_Config() (Chkpt B):   AFTER SystemClock_Config() (Checkpoint C):
+------------------------------------+   +--------------------------------------------+
| SYSCLK source   = HSI (16 MHz)     |   | SYSCLK source   = PLL (84 MHz!)            |
| SystemCoreClock = 16,000,000       |   | SystemCoreClock = 84,000,000               |
| SysTick->LOAD   = 15,999           |   | SysTick->LOAD   = 83,999 (1 ms @ 84 MHz!)  |
| Flash Latency   = 0 wait states    |   | Flash Latency   = 2 wait states (for 84MHz)|
+------------------------------------+   +--------------------------------------------+
```

---

### Scene 3: `MX_GPIO_Init()` (Checkpoint C -> Checkpoint D)
* **File:** [Core/Src/main.c:186](../../../p1_f446re_baseline/Core/Src/main.c#L186)
* **What happens:**
  1. Enables the hardware clocks for `GPIOC`, `GPIOH`, `GPIOA`, and `GPIOB`.
  2. Clears the `PA5` (`LD2`) output latch to `0` (`GPIO_PIN_RESET`) *before* switching `PA5` to output mode so the LED starts cleanly OFF without a brief glitch.
  3. Configures `PC13` (`B1` blue user button) for falling-edge external interrupt detection (`GPIO_MODE_IT_FALLING`).
  4. Configures `PA5` (`LD2` green LED) as a Push-Pull Output (`GPIO_MODE_OUTPUT_PP`).

```text
BEFORE MX_GPIO_Init() (Checkpoint C):    AFTER MX_GPIO_Init() (Checkpoint D):
+------------------------------------+   +--------------------------------------------+
| GPIOA / GPIOC Clocks = OFF         |   | GPIOA / GPIOC Clocks = ON                  |
| PA5 (LD2) Mode       = Reset/Input |   | PA5 (LD2) Mode = Output Push-Pull (LOW/OFF)|
| PC13 (B1) Mode       = Reset/Input |   | PC13 (B1) Mode = Falling-Edge Interrupt In |
+------------------------------------+   +--------------------------------------------+
```

---

### Scene 4: `MX_USART2_UART_Init()` (Checkpoint D -> Checkpoint E)
* **Files:** [Core/Src/main.c:152](../../../p1_f446re_baseline/Core/Src/main.c#L152) and [Core/Src/stm32f4xx_hal_msp.c:89](../../../p1_f446re_baseline/Core/Src/stm32f4xx_hal_msp.c#L89)
* **What happens:**
  1. Fills the RAM handle `huart2.Init` (`BaudRate = 115200`, `8` data bits, `1` stop bit, no parity, `TX_RX` mode).
  2. Calls `HAL_UART_Init(&huart2)`, which invokes `HAL_UART_MspInit(&huart2)` to turn on the `USART2` clock and connect pins `PA2` (`TX`) and `PA3` (`RX`) to Alternate Function 7 (`GPIO_AF7_USART2`).
  3. Writes the baud rate and control registers into the `USART2` peripheral and sets `huart2.gState = HAL_UART_STATE_READY`.

```text
BEFORE MX_USART2_UART_Init() (Chkpt D):  AFTER MX_USART2_UART_Init() (Checkpoint E):
+------------------------------------+   +--------------------------------------------+
| huart2.Init.BaudRate = 0           |   | huart2.Init.BaudRate = 115200              |
| PA2 / PA3 Pins       = Unconfigured|   | PA2 / PA3 Pins       = AF7 (USART2 TX/RX)  |
| USART2 Clock & Regs  = OFF         |   | USART2 Clock & Regs  = Configured & Enabled|
| huart2.gState        = RESET (0)   |   | huart2.gState        = HAL_UART_STATE_READY|
+------------------------------------+   +--------------------------------------------+
```

---

### Scene 5: How `uwTick` Advances Inside the Empty `while (1)` Loop

Source links: [Core/Src/stm32f4xx_it.c:184](../../../p1_f446re_baseline/Core/Src/stm32f4xx_it.c#L184) and [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c:310](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c#L310)

```c
/* In Core/Src/stm32f4xx_it.c (line 184):
 * Hardware calls this ISR every 1 ms (every 84,000 clock cycles at 84 MHz): */
void SysTick_Handler(void)
{
  HAL_IncTick(); /* Adds uwTickFreq (1) to the global uwTick counter! */
}
```

```text
Thread Mode (`main.c` in `while (1)`)         Handler Mode (`SysTick_Handler` ISR)
-------------------------------------         ------------------------------------
spinning in empty `while (1)` (`uwTick = 12`)
  |
  | <--- [SysTick counter hits 0 after 84,000 clocks (1 ms)]
  |      Hardware stacks registers & jumps to SysTick_Handler ---> HAL_IncTick()
  |                                                                `uwTick` becomes 13!
  | <--- [Exception return restores registers] <------------------ returns from ISR
  v
resumes `while (1)` (`uwTick = 13`)
```

---

## 3. Summary Table Across All 5 Checkpoints

| Checkpoint | Stopped Line in `main.c` | `SystemCoreClock` | `SysTick->LOAD` | `huart2.Init.BaudRate` | `huart2.gState` |
| --- | --- | ---: | ---: | ---: | --- |
| **A** | `HAL_Init();` | `16000000` | `0` | `0` | `HAL_UART_STATE_RESET` |
| **B** | `SystemClock_Config();` | `16000000` | `15999` | `0` | `HAL_UART_STATE_RESET` |
| **C** | `MX_GPIO_Init();` | `84000000` | `83999` | `0` | `HAL_UART_STATE_RESET` |
| **D** | `MX_USART2_UART_Init();` | `84000000` | `83999` | `0` | `HAL_UART_STATE_RESET` |
| **E** | `while (1)` | `84000000` | `83999` | `115200` | `HAL_UART_STATE_READY` |

---

## 4. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
Stepping through `main()` is like watching a pilot run the pre-flight checklist:
1. **`HAL_Init()`** turns on the Flash caches and sets the `SysTick` metronome to click every `16,000` beats (`1 ms` at `16 MHz`).
2. **`SystemClock_Config()`** fires up the `84 MHz` PLL jet engine—and updates the `SysTick` metronome to count `84,000` beats per click (`LOAD = 83,999`) so it still clicks once every `1 ms`!
3. **`MX_GPIO_Init()`** switches on power to the GPIO ports and sets up the green LED pin (`PA5`) and blue button pin (`PC13`).
4. **`MX_USART2_UART_Init()`** connects pins `PA2` and `PA3` to the `USART2` serial block at `115200` baud.
5. In **`while (1)`**, even though `main()` is doing nothing, the `SysTick` interrupt briefly steps in every `1 ms` in Handler mode to add `1` to `uwTick`!

---

### Interview Q1: Why is `SysTick->LOAD` set to `83,999` instead of `84,000` for a `1 ms` tick at `84 MHz`?
* **Simple answer:** Because the SysTick down-counter counts **inclusively through `0`** (`83999, 83998, ..., 1, 0`) before reloading on the next clock edge.
* **Formula:** Counting from `LOAD` down to `0` takes `LOAD + 1` clock cycles, so for `N = 84,000` cycles per millisecond, `LOAD = N - 1 = 83,999`.

### Interview Q2: Why does `HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET)` appear *before* `HAL_GPIO_Init()` inside `MX_GPIO_Init()`?
* **Simple answer:** To prevent a **startup output glitch** on the pin.
* **How it works:** Writing `GPIO_PIN_RESET` pre-loads `0` into the port's Output Data Register (`ODR`) while the pin is still in high-impedance input mode. When `HAL_GPIO_Init()` then enables the push-pull output driver in `MODER`, the pin immediately drives a clean `0` (LOW) instead of briefly driving whatever state was in `ODR`.

### Interview Q3: What is the difference between `MX_USART2_UART_Init()` in `main.c` and `HAL_UART_MspInit()` in `stm32f4xx_hal_msp.c`?
* **Simple answer:**
  * **`MX_USART2_UART_Init()` (Protocol layer):** Configures the UART protocol parameters (`115200` baud, `8-N-1`) and calls `HAL_UART_Init(&huart2)`.
  * **`HAL_UART_MspInit()` (Board/MCU wiring layer):** Called automatically by `HAL_UART_Init()` to enable the `USART2` and `GPIOA` peripheral clocks and mux physical pins `PA2` and `PA3` to Alternate Function `AF7`.

## 5. Corrections

- 2026-10-27: Initial publication.
- 2026-10-27 (revision 2): Rewrote in 5-scene story mode with Before/After boxes for every initialization function.
