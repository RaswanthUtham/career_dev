# W01-L12: Blinking `LD2` (`PA5`) And Seeing How `BSRR` And `HAL_Delay()` Work

> **Source-reviewed:** [Core/Src/main.c](../../../p1_f446re_baseline/Core/Src/main.c),
> [Core/Inc/main.h](../../../p1_f446re_baseline/Core/Inc/main.h),
> [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c),
> and [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c),
> 2026-10-27.

Previous: [W01-L11: Watching The STM32 Wake Up Step By Step](W01_L11_trace_initialization_clock_gpio_uart.md).

## 0. Know This First

In Lesson 11, `MX_GPIO_Init()` turned on the `GPIOA` clock and configured `PA5`
(`LD2`, the green LED on your NUCLEO-F446RE) as a push-pull output.

Now we add **4 lines of C inside `while (1)`** to blink `LD2` every `500 ms` and
trace what happens down at the hardware register level (`GPIOA->BSRR`,
`GPIOA->ODR`, and `uwTick`).

| Term | What it does in our blink story |
| --- | --- |
| **`LD2` (`PA5`)** | Onboard green user LED wired to **Port A, Pin 5** (`GPIOA`, `GPIO_PIN_5`). Driving `PA5` HIGH (`1`) turns `LD2` ON; driving `PA5` LOW (`0`) turns `LD2` OFF. |
| **Pin Mask (`GPIO_PIN_5`)** | A 16-bit bitmask where bit `5` is `1` (`1U << 5 = 0x0020`). Each bit position `0..15` corresponds to pin `PA0..PA15`. |
| **`GPIOA->ODR` (Output Data Register)** | 32-bit hardware register (`uint32_t ODR`) whose lower 16 bits (`15..0`) hold the current output-latch state (`0` or `1`) of pins `PA0..PA15`. |
| **`GPIOA->BSRR` (Bit Set/Reset Register)** | 32-bit write-only command register: writing `1` to bits `0..15` **sets** the matching `ODR` bit to `1`; writing `1` to bits `16..31` **resets (clears)** the matching `ODR` bit to `0`. |
| **`HAL_Delay(500U)`** | Busy-wait loop in Thread mode that reads `uwTick` until `501` millisecond ticks have elapsed. |

---

## 1. Goal

Add the 4-line `LD2` blink loop to `Core/Src/main.c`, trace how `HAL_GPIO_WritePin()`
writes `0x00000020` and `0x00200000` into `GPIOA->BSRR` to flip `ODR` bit 5
atomically, trace how `HAL_Delay(500U)` counts `uwTick` ticks, and see why real
cyclic drivers use non-blocking state timers instead of `HAL_Delay()`.

---

## 2. The 4-Scene Blink Movie

```text
SCENE 1: Turn LD2 ON (`HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET)`)
  |  Writes `0x00000020` (bit 5) to `GPIOA->BSRR`
  |  -> Hardware atomically sets `GPIOA->ODR` bit 5 = 1 -> Pin PA5 goes HIGH (3.3 V) -> LD2 turns ON!
  v
SCENE 2: Hold ON for 500 ms (`HAL_Delay(500U)`)
  |  Main loop (Thread mode) snapshots `tickstart = uwTick` (e.g., 100) and waits for `uwTick - 100 >= 501`
  |  Meanwhile, every 1 ms, `SysTick_Handler` (Handler mode) increments `uwTick` (101, 102, ..., 601)
  v
SCENE 3: Turn LD2 OFF (`HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET)`)
  |  Writes `0x00200000` (`0x0020 << 16`, bit 21) to `GPIOA->BSRR`
  |  -> Hardware atomically clears `GPIOA->ODR` bit 5 = 0 -> Pin PA5 goes LOW (0 V) -> LD2 turns OFF!
  v
SCENE 4: Hold OFF for 500 ms (`HAL_Delay(500U)`) & Repeat `while (1)`
     Main loop waits another 501 `uwTick` increments (601 -> 1102), then jumps back to Scene 1!
```

---

## 3. Before And After: Pin Masks, `BSRR`, and `HAL_Delay()`

### A. Pin Number (`5`) vs. Pin Mask (`1U << 5 = 0x0020`)
Start with a tiny **4-pin port** (`Pins 0..3`):
* To select **Pin 2**, we set bit position `2` to `1`:
  `binary 0100 = (1U << 2) = 4`.
* On our 16-pin Port A (`PA0..PA15`), **Pin 5 (`PA5`)** is bit position `5`:
  ```text
  GPIO_PIN_5 = (1U << 5) = 32 decimal = 0x0020 (binary 0000 0000 0010 0000)
  ```
*(Note: Passing the raw number `5` (`binary 0101`) would select `PA0` and `PA2` instead of `PA5`!)*

### B. Why Hardware Provides `BSRR` Instead of Modifying `ODR` Directly
Suppose `main()` wanted to change `PA5` by reading `GPIOA->ODR`, modifying bit 5,
and writing `GPIOA->ODR` back (`GPIOA->ODR |= (1U << 5);`), and **right between
the read and write**, an interrupt fired and changed `PA3`! When `main()` wrote
its old `ODR` value back, it would erase the interrupt's change to `PA3`!

**How `GPIOA->BSRR` solves this in a single instruction:**
* Bits `15..0` of `BSRR` are the **Set** bits: writing `1` sets that pin's `ODR` bit to `1`.
* Bits `31..16` of `BSRR` are the **Reset** bits: writing `1` to bit `pin + 16` clears that pin's `ODR` bit to `0`.
* Writing `0` to any `BSRR` bit leaves that pin untouched!

```text
1. HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET):
   Writes `0x00000020` to `GPIOA->BSRR` (Set bit 5):
   BEFORE (ODR low 8 bits):  [ 0 0 0 0 1 0 0 0 ]  (PA5 = 0, PA3 = 1)
   AFTER  (ODR low 8 bits):  [ 0 0 1 0 1 0 0 0 ]  (PA5 = 1 ON! PA3 stays 1!)

2. HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET):
   Writes `0x00200000` (`0x0020 << 16`, Reset bit 21) to `GPIOA->BSRR`:
   BEFORE (ODR low 8 bits):  [ 0 0 1 0 1 0 0 0 ]  (PA5 = 1, PA3 = 1)
   AFTER  (ODR low 8 bits):  [ 0 0 0 0 1 0 0 0 ]  (PA5 = 0 OFF! PA3 stays 1!)
```

---

## 4. Target Code To Add And Annotated HAL Implementation

### Step A: Your 4-Line Addition in `Core/Src/main.c`

Source link: [Core/Src/main.c:96](../../../p1_f446re_baseline/Core/Src/main.c#L96)

```c
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);   /* PA5 HIGH -> LD2 ON */
    HAL_Delay(500U);                                           /* Wait ~500 ms */
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET); /* PA5 LOW  -> LD2 OFF */
    HAL_Delay(500U);                                           /* Wait ~500 ms */
  }
  /* USER CODE END 3 */
```

### Step B: What `HAL_GPIO_WritePin()` Does Inside `stm32f4xx_hal_gpio.c`

Source link: [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c:419](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c#L419)

```c
void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState)
{
  if (PinState != GPIO_PIN_RESET)
  {
    /* SET PATH (PinState == GPIO_PIN_SET):
     * Writes 0x0020 into the lower 16 bits of GPIOA->BSRR -> sets ODR bit 5 = 1. */
    GPIOx->BSRR = GPIO_Pin;
  }
  else
  {
    /* RESET PATH (PinState == GPIO_PIN_RESET):
     * Shifts 0x0020 left by 16 bits (0x00200000) into the upper 16 bits of
     * GPIOA->BSRR -> clears ODR bit 5 = 0. */
    GPIOx->BSRR = (uint32_t)GPIO_Pin << 16U;
  }
}
```

### Step C: What `HAL_Delay(500U)` Does Inside `stm32f4xx_hal.c`

Source link: [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c:390](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c#L390)

```c
__weak void HAL_Delay(uint32_t Delay)
{
  /* 1. SNAPSHOT STARTING TICK (e.g., tickstart = 100): */
  uint32_t tickstart = HAL_GetTick();
  uint32_t wait = Delay; /* wait = 500 */

  /* 2. ADD 1 EXTRA TICK (uwTickFreq = 1) TO GUARANTEE AT LEAST 500 MS:
   *    Because we might call HAL_Delay() 0.99 ms into the current tick,
   *    adding 1 sets wait = 501 ticks. */
  if (wait < HAL_MAX_DELAY)
  {
    wait += (uint32_t)(uwTickFreq); /* wait = 501 */
  }

  /* 3. BUSY-WAIT IN THREAD MODE UNTIL SysTick_Handler ADVANCES uwTick BY 501:
   *    - At uwTick = 600: (600 - 100) = 500 < 501 -> keep looping
   *    - At uwTick = 601: (601 - 100) = 501 < 501 is FALSE -> return! */
  while ((HAL_GetTick() - tickstart) < wait)
  {
  }
}
```

---

## 5. Control Flow And Why Cyclic Services Replace `HAL_Delay()`

### (a) Step Table (One Full 1-Second Blink Cycle Starting at `uwTick = 100`)

| # | Where | Action | Register / Variable State | Mode |
| --- | --- | --- | --- | --- |
| 1 | `main.c` -> `HAL_GPIO_WritePin` | Set `PA5` HIGH | `GPIOA->BSRR = 0x00000020` -> `ODR[5] = 1` (`LD2` ON) | Thread mode |
| 2 | `stm32f4xx_hal.c : HAL_Delay(500)` | Poll `uwTick - 100 < 501` | `tickstart = 100`, `wait = 501` | Thread mode |
| 3 | `stm32f4xx_it.c : SysTick_Handler` | Fires 501 times (`1 ms` each) | `uwTick` advances `100 -> 601` | Handler mode (ISR) |
| 4 | `main.c` -> `HAL_GPIO_WritePin` | Reset `PA5` LOW | `GPIOA->BSRR = 0x00200000` -> `ODR[5] = 0` (`LD2` OFF) | Thread mode |
| 5 | `stm32f4xx_hal.c : HAL_Delay(500)` | Poll `uwTick - 601 < 501` | `uwTick` advances `601 -> 1102`, then loop repeats | Thread + Handler |

### (b) Why Our P1 Driver Will Use Non-Blocking Checks Instead of `HAL_Delay()`
Look at what happens to the CPU during `HAL_Delay(500U)`:

```text
Blocking `HAL_Delay(500U)` in main():
  [Set LD2 ON] -> [CPU spins for 500,000 us checking uwTick...] -> [Finally checks sensor!]
                  (No other main-loop job can run for half a second!)

Non-Blocking Cyclic Check (what we build next):
  Every 1 ms: if ((now - last_toggle_ms) >= 500U) { toggle LD2; last_toggle_ms = now; }
              service_sensor_state_machine(); /* Runs every single millisecond! */
```

---

## 6. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
To blink `LD2` on pin `PA5`, we use `GPIO_PIN_5` (`0x0020`), which is a 16-switch
mask with only switch #5 flipped on. Instead of reading all 16 switches in `ODR`
and writing them back, we drop a command into `GPIOA->BSRR`: writing `0x00000020`
into the bottom half flips switch #5 ON, and writing `0x00200000` into the top
half flips switch #5 OFF without touching any other pin! Between flips,
`HAL_Delay(500U)` watches the `uwTick` clock on the wall until the `SysTick`
interrupt clicks `501` times—great for a first hardware test, and it shows us why
real drivers check the clock without standing still for `500 ms`!

---

### Interview Q1: Why do we pass `GPIO_PIN_5` (`0x0020`) instead of `5` to `HAL_GPIO_WritePin()`?
* **Simple answer:** Because the GPIO output registers (`BSRR` and the lower 16 bits of `ODR`, as well as input reads on `IDR`) use a **16-bit pin mask** where bit `N` selects pin `N` (allowing one register write to set or clear multiple output bits simultaneously).
* **What `5` would do:** `5` in binary is `0000 0000 0000 0101` (`bit 0` + `bit 2`), which selects **`ODR` bits 0 and 2** (`PA0` and `PA2`'s output latch bits) instead of **bit 5 (`PA5`)** (`1U << 5 = 0x0020`).

### Interview Q2: Why does `HAL_GPIO_WritePin()` write to `GPIOx->BSRR` instead of using `GPIOx->ODR |= pin` and `GPIOx->ODR &= ~pin`?
* **Simple answer:** Writing to **`BSRR` is an atomic single-instruction hardware command**, whereas `GPIOx->ODR |= pin` is a 3-step **Read-Modify-Write** sequence (`LDR`, `ORR`, `STR`).
* **The bug `BSRR` prevents:** If an ISR modifies another pin on the same GPIO port (`GPIOA`) after Thread mode reads `ODR` but before Thread mode writes `ODR` back, Thread mode's `STR` overwrites and erases the ISR's pin change. `BSRR` modifies only the bits with a `1` written to them in a single bus write.

### Interview Q3: Why does `HAL_Delay(500U)` add `uwTickFreq` (`1`) to `wait` (`wait = 501`), and what happens if you call `HAL_Delay()` inside a high-priority ISR?
* **Simple answer:**
  1. **Why `+ 1`:** `HAL_Delay()` can be called right before a `SysTick` edge (e.g., `0.01 ms` before `uwTick` increments). Waiting for `501` tick edges guarantees the delay is **at least `500.0 ms`** rather than `499.01 ms`.
  2. **Why never call `HAL_Delay()` in a high-priority ISR:** `HAL_Delay()` waits for `SysTick_Handler` to increment `uwTick`. If called inside an ISR whose priority is higher than or equal to `SysTick`, `SysTick_Handler` is blocked from pre-empting it, so `uwTick` never changes and `HAL_Delay()` deadlocks forever!

## 7. Corrections

- 2026-10-27: Initial publication.
- 2026-10-27 (revision 2): Rewrote in 4-scene story mode with visual `BSRR` / `ODR` diagrams and annotated `HAL_GPIO_WritePin()` and `HAL_Delay()` code.
