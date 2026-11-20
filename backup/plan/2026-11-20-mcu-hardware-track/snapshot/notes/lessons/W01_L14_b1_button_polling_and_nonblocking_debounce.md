# W01-L14: Reading `B1` (`PC13`) With Non-Blocking Debounce To Change The `LD2` Blink Speed

> **Source-reviewed:** [Core/Src/main.c](../../../p1_f446re_baseline/Core/Src/main.c#L186),
> [Core/Inc/main.h](../../../p1_f446re_baseline/Core/Inc/main.h#L61),
> [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c#L377),
> and [Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f446xx.h](../../../p1_f446re_baseline/Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f446xx.h#L460),
> 2026-10-27.

Previous: [W01-L13: Non-Blocking Elapsed-Time Scheduling And How `HAL_GPIO_TogglePin()` Works](W01_L13_nonblocking_elapsed_time_and_gpio_toggle.md).

## 0. Know This First

In Lesson 13, we freed `while (1)` so `LD2` (`PA5`) blinks every `500 ms` without
blocking the CPU. Now we use that free CPU time to add a second concurrent job
inside `while (1)`: **polling the blue user button `B1` (`PC13`) so each clean
button press switches `LD2` between slow blink (`500 ms`) and fast blink (`150 ms`)!**

Why can't we just write `if (button_is_pressed) change_speed();`? Because a
physical push-button has **two real-world behaviors** our code must handle:
1. **Holding vs. Pressing (Level vs. Edge):** Your finger holds the button down
   for `~100 ms` to `300 ms`. During `100 ms`, a non-blocking `while (1)` loop
   runs **hundreds of thousands of times**! We want **1 speed change per press**
   (the *transition* from released to pressed), not 100,000 changes while held.
2. **Mechanical Contact Bounce:** When metal switch contacts slam together, they
   physically bounce open and closed for several milliseconds (`1 -> 0 -> 1 -> 0`)
   before settling at `0`. Without a **debounce filter**, one finger press looks
   like 5 separate presses!

| Term | What it does in our story |
| --- | --- |
| **`B1` (`PC13`)** | Onboard blue user push-button wired to **Port C, Pin 13** (`GPIOC`, `GPIO_PIN_13 = 1U << 13 = 0x2000`). |
| **Active-Low Input** | On the NUCLEO-F446RE board, `PC13` is pulled HIGH (`1`, `GPIO_PIN_SET`) when `B1` is **released**, and pulled LOW (`0`, `GPIO_PIN_RESET`) when `B1` is **pressed**. |
| **`GPIOC->IDR` (Input Data Register)** | 32-bit hardware register (`uint32_t IDR`) whose lower 16 bits (`15..0`) report the live digital voltage level (`0` or `1`) sampled on pins `PC0..PC15`. |
| **`HAL_GPIO_ReadPin()`** | Reads `GPIOx->IDR`, masks it with `& GPIO_Pin` (`0x2000`), and returns `GPIO_PIN_SET` (`1`) or `GPIO_PIN_RESET` (`0`). |
| **Non-Blocking Debounce (`20 ms`)** | Requires every sampled reading on `PC13` during a chosen `20 ms` window (`(now_ms - debounce_start_ms) >= 20U`) to observe the same level before accepting it as a new stable button state (requires frequent polling, and a physical press must stay settled for at least `20 ms + 1 polling interval`). |
| **Edge Detection** | Triggers the speed change **only once** when the debounced `stable_pressed` state transitions from `false` (released) to `true` (pressed). |

---

## 1. Goal

Trace how `HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13)` reads bit 13 of `GPIOC->IDR`,
build a **non-blocking `20 ms` debounce + edge-detection state machine** in
`Core/Src/main.c`, and run both the button task and the `LD2` blink task
together in `while (1)` without blocking each other.

---

## 2. The 4-Scene Story: From Finger Press on `B1` to Speed Change on `LD2`

```text
SCENE 1: Button Released (`PC13` is HIGH = 1)
  |  `GPIOC->IDR` bit 13 is `1` (`GPIO_PIN_SET`) -> `raw_pressed = false`
  |  `stable_pressed = false`; `LD2` blinks every `blink_period_ms = 500 ms`
  v
SCENE 2: Finger Presses `B1` — Metal Contacts Bounce (`t = 200..204 ms`)
  |  `PC13` bounces `1 -> 0 -> 1 -> 0`!
  |  Every time `raw_pressed != candidate_pressed`, our filter resets `debounce_start_ms = now_ms`.
  |  Meanwhile, `while (1)` never blocks—the `LD2` timer keeps running on schedule!
  v
SCENE 3: Contacts Settle LOW for 20 Continuous Milliseconds (`t = 204..224 ms`)
  |  At `t = 224 ms`: `(now_ms - debounce_start_ms) == (224 - 204) == 20 ms >= 20U`!
  |  `stable_pressed` transitions `false -> true` (1 clean press edge detected!)
  |  `blink_period_ms` switches once from `500 ms` to `150 ms` (fast blink)!
  v
SCENE 4: Finger Holds `B1` Down (`t = 225..400 ms`), Then Releases (`t = 400..420 ms`)
     - While held (`t = 225..400 ms`): `candidate_pressed` (true) == `stable_pressed` (true) -> 0 extra triggers!
     - When released and stable HIGH for 20 ms (`t = 420 ms`): `stable_pressed` resets `true -> false`,
       ready for the next press!
```

---

## 3. `Before -> After` Boxes: `GPIOC->IDR`, Bounce Filtering, and Edge Detection

### A. Bit Mask for `PC13` (`1U << 13 = 0x2000`) and `GPIOC->IDR`
* **Tiny 4-bit example:** To read **Pin 1** out of 4 pins (`bits 3..0`), we mask with `1U << 1 = 0b0010 = 2`. If `IDR` is `0b1010`, then `IDR & 0b0010` is non-zero (`Pin 1` is `1`); if `IDR` is `0b1000`, then `IDR & 0b0010` is `0` (`Pin 1` is `0`).
* **Real 16-pin Port C (`PC13`):**
  `B1_Pin` is `GPIO_PIN_13 = (1U << 13) = 8192 = 0x2000`:

```text
WHEN B1 IS RELEASED (PC13 = 3.3 V HIGH):     WHEN B1 IS PRESSED (PC13 = 0 V LOW):
+----------------------------------------+   +----------------------------------------+
| GPIOC->IDR bit 13 = 1                  |   | GPIOC->IDR bit 13 = 0                  |
| (GPIOC->IDR & 0x2000U) != 0U           |   | (GPIOC->IDR & 0x2000U) == 0U           |
| HAL_GPIO_ReadPin() -> GPIO_PIN_SET (1) |   | HAL_GPIO_ReadPin() -> GPIO_PIN_RESET(0)|
| raw_pressed        -> false            |   | raw_pressed        -> true             |
+----------------------------------------+   +----------------------------------------+
```

*(Note on `MX_GPIO_Init()` in `Core/Src/main.c`: CubeMX configured `B1_Pin` with `GPIO_MODE_IT_FALLING`. In STM32 hardware, `GPIO_MODE_IT_FALLING` leaves the `GPIOC->MODER` bits for `PC13` set to `00` (Input mode), so `GPIOC->IDR` bit 13 continuously reflects the live pin voltage for polling!)*

### B. How the Non-Blocking `20 ms` Debounce Filter Rejects Bounce Without `HAL_Delay()`

```text
Pin PC13 Voltage During 1 Finger Press:
  HIGH (Released) ----+   +-+   +
                      |   | |   |
  LOW  (Pressed)      +---+ +---+----------------------------------------
  Time (ms):         200 202 204                  ...                 224
                                  (-- No different level sampled for 20 ms! --)

How Our 3 State Variables React on Each Millisecond Sample:
  - t = 200 ms: raw_pressed flips 0 -> 1! Sets candidate = 1, debounce_start = 200
  - t = 202 ms: bounce! raw_pressed flips 1 -> 0! Sets candidate = 0, debounce_start = 202
  - t = 204 ms: settles! raw_pressed flips 0 -> 1! Sets candidate = 1, debounce_start = 204
  - t = 205..223 ms: every sample reads 1 (== candidate), elapsed < 20 ms -> wait
  - t = 224 ms: (224 - 204) == 20 ms >= 20U!
                stable_pressed flips false -> true!
                blink_period_ms toggles 500 ms -> 150 ms ONCE!
```

---

## 4. Annotated Source Code

### Unit 1: How `HAL_GPIO_ReadPin()` Reads `GPIOx->IDR` (`stm32f4xx_hal_gpio.c`)

Source link: [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c:377](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c#L377)

```c
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
  GPIO_PinState bitstatus;

  /* 1. MASK THE 32-BIT INPUT DATA REGISTER (IDR) WITH GPIO_Pin (0x2000 for PC13):
   *    - GPIOx->IDR holds the live input states of pins 0..15 in bits 0..15.
   *    - `& GPIO_Pin` zeroes every bit except bit 13 (0x2000).
   *    - If bit 13 is 1 (B1 released), `(GPIOx->IDR & 0x2000) != 0` is TRUE -> GPIO_PIN_SET.
   *    - If bit 13 is 0 (B1 pressed),  `(GPIOx->IDR & 0x2000) != 0` is FALSE -> GPIO_PIN_RESET. */
  if ((GPIOx->IDR & GPIO_Pin) != (uint32_t)GPIO_PIN_RESET)
  {
    bitstatus = GPIO_PIN_SET;
  }
  else
  {
    bitstatus = GPIO_PIN_RESET;
  }
  return bitstatus;
}
```

### Unit 2: Target Update in `Core/Src/main.c` (Non-Blocking Button + Non-Blocking LED)

Source link: [Core/Src/main.c:26](../../../p1_f446re_baseline/Core/Src/main.c#L26) and [Core/Src/main.c:91](../../../p1_f446re_baseline/Core/Src/main.c#L91)

Add `#include <stdbool.h>` inside `/* USER CODE BEGIN Includes */`, initialize
the state variables inside `/* USER CODE BEGIN 2 */`, and place the two
non-blocking jobs inside `/* USER CODE BEGIN 3 */`:

```c
/* In Core/Src/main.c — inside USER CODE BEGIN Includes (line 26): */
#include <stdbool.h>

/* In Core/Src/main.c — inside USER CODE BEGIN 2 (line 91, right before while (1)): */
  uint32_t last_toggle_ms    = HAL_GetTick();
  uint32_t blink_period_ms   = 500U;  /* Starts at 500 ms slow blink */
  uint32_t debounce_start_ms = last_toggle_ms;
  bool     candidate_pressed = false; /* Latest raw reading being timed */
  bool     stable_pressed    = false; /* Debounced button state */

/* In Core/Src/main.c — inside USER CODE BEGIN 3 (line 99, inside while (1)): */
    uint32_t now_ms = HAL_GetTick();

    /* JOB 1: NON-BLOCKING B1 BUTTON DEBOUNCE & PRESS-EDGE DETECTION
     * Active-low B1 on PC13 reads GPIO_PIN_RESET (0) when pressed: */
    bool raw_pressed = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET);

    if (raw_pressed != candidate_pressed)
    {
      /* Input level just changed (press start, release start, or bounce):
       * record the new candidate level and restart the 20 ms stability timer. */
      candidate_pressed = raw_pressed;
      debounce_start_ms = now_ms;
    }
    else if ((now_ms - debounce_start_ms) >= 20U)
    {
      /* Every sampled check over >= 20 ms observed `candidate_pressed`!
       * Check if this is a NEW transition of the debounced state: */
      if (stable_pressed != candidate_pressed)
      {
        stable_pressed = candidate_pressed;

        /* Trigger ONLY on the released -> pressed edge (stable_pressed == true): */
        if (stable_pressed)
        {
          blink_period_ms = (blink_period_ms == 500U) ? 150U : 500U;
        }
      }
    }

    /* JOB 2: NON-BLOCKING LD2 BLINK SCHEDULER (uses current blink_period_ms) */
    if ((now_ms - last_toggle_ms) >= blink_period_ms)
    {
      last_toggle_ms = now_ms;
      HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    }
```

### Complex Syntax Breakdown

* **`bool raw_pressed = (... == GPIO_PIN_RESET);`:** Evaluates the comparison `HAL_GPIO_ReadPin(...) == GPIO_PIN_RESET` first, producing `true` (`1`) when `PC13` is LOW (pressed) and `false` (`0`) when `PC13` is HIGH (released).
* **`(blink_period_ms == 500U) ? 150U : 500U;` (Ternary Conditional Operator `cond ? a : b`):** If `blink_period_ms == 500U` is true, evaluates to `150U`; otherwise evaluates to `500U`. Equivalent to an `if / else` assignment in one concise line.
* **`GPIOx->IDR & GPIO_Pin`:** Bitwise AND between the 32-bit `IDR` register and the 16-bit mask `0x2000` (promoted to 32-bit `0x00002000`), isolating bit 13.

---

## 5. Control Flow

### (a) One-Line Call Chain

```text
while (1) pass -> HAL_GetTick()
  -> Job 1: HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) reads GPIOC->IDR bit 13
       -> 20 ms debounce filter -> on false->true press edge, flips blink_period_ms (500 <-> 150)
  -> Job 2: if ((now_ms - last_toggle_ms) >= blink_period_ms) -> HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5)
```

### (b) Step Table (Tracing One Bouncing Press From `t = 200 ms` to `t = 225 ms`)

| # | `now_ms` | `PC13` (`IDR[13]`) | `raw_pressed` | `candidate_pressed` | `debounce_start_ms` | `stable_pressed` | `blink_period_ms` | What Happened |
| --- | ---: | :---: | :---: | :---: | ---: | :---: | ---: | --- |
| 1 | `199` | `1` (HIGH) | `false` | `false` | `0` | `false` | `500` | Button idle (released) |
| 2 | `200` | `0` (LOW) | `true` | `false -> true` | `200` | `false` | `500` | First contact! Timer starts at `200` |
| 3 | `202` | `1` (Bounce) | `false` | `true -> false` | `202` | `false` | `500` | Contact bounced open; timer resets to `202` |
| 4 | `204` | `0` (Settles) | `true` | `false -> true` | `204` | `false` | `500` | Contact settles closed; timer resets to `204` |
| 5 | `215` | `0` (LOW) | `true` | `true` | `204` | `false` | `500` | Steady for `11 ms < 20 ms` (still debouncing) |
| 6 | **`224`** | `0` (LOW) | `true` | `true` | `204` | **`false -> true`** | **`500 -> 150`** | **Steady for `20 ms`! Press edge fires once!** |
| 7 | `225` | `0` (Held) | `true` | `true` | `204` | `true` | `150` | `stable == candidate` -> holding does not re-trigger |

### (c) Timeline: Two Jobs Sharing `while (1)` Without Blocking

```text
Time (ms)   Job 1: B1 Button Filter (PC13)              Job 2: LD2 Blink Scheduler (PA5)
---------   ------------------------------              --------------------------------
200..204    Detects bounce, resets debounce_start=204   Checks (now - last < 500) -> continues
205..223    Counts steady LOW time (1..19 ms)           Checks (now - last < 500) -> continues
224         20 ms reached! Flips period 500 -> 150 ms   Checks (now - last >= 150) -> toggles LD2!
225..373    Button still held (ignored by edge check)   Waits for next 150 ms mark (due @ 374 ms)
```

---

## 6. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
When you press a physical push-button, its metal contacts bounce like a dropped
basketball for a few milliseconds before resting flat, and your finger stays on
the button for hundreds of thousands of CPU loop passes. If our code reacted
instantly to every LOW reading on `PC13`, one press would flip the LED speed
dozens of times! Instead, our code uses a **20-millisecond stopwatch**
(`now_ms - debounce_start_ms >= 20U`): every bounce resets the stopwatch, and
only when the button stays steadily pressed for a full `20 ms` do we update
`stable_pressed` from `false` to `true` and change the LED speed **once**—all
while the LED keeps blinking on schedule!

---

### Interview Q1: What is the difference between `GPIOx->ODR` and `GPIOx->IDR` on an STM32?
* **Simple answer:**
  * **`GPIOx->ODR` (Output Data Register):** Stores the digital output latch states (`0` or `1`) that software commands the port's output drivers to drive (modified atomically via `GPIOx->BSRR`).
  * **`GPIOx->IDR` (Input Data Register):** A read-only 32-bit register whose lower 16 bits (`15..0`) report the actual physical voltage levels (`0` or `1`) sampled by the Schmitt triggers on the port's pins every AHB1 clock cycle.

### Interview Q2: Why must button handling combine **both** debouncing and edge detection instead of using only one of them?
* **Simple answer:** Because they solve two different physical problems:
  1. **Without debouncing (edge detection only):** Mechanical contact bounce (`1 -> 0 -> 1 -> 0`) during the first `2–5 ms` of a press creates multiple fake `1 -> 0` edges from a single finger press.
  2. **Without edge detection (debouncing only):** Even a clean debounced level stays `true` for the entire `200 ms` your finger holds the button down, which would re-trigger the action on every pass of `while (1)`.

### Interview Q3: When should you use polled GPIO debouncing (like Lesson 14) versus an EXTI interrupt (our next lesson)?
* **Simple answer:**
  * **Polled non-blocking debounce:** Ideal when a cyclic task already polls frequently (e.g., every `1 ms`) and the physical button press lasts significantly longer than the debounce window plus the polling interval (e.g., human finger presses of `50–200 ms` against a `20 ms` multi-sample debounce algorithm), because the debounce state machine rejects contact bounce without risking an interrupt storm.
  * **EXTI interrupt handoff:** Needed when the MCU sleeps in a low-power mode waiting for a wakeup edge, or when capturing a brief hardware pulse that is shorter than the main loop's polling interval (provided noisy bouncy lines are either hardware-filtered or masked/debounced by a timer after the first edge).

## 7. Corrections

- 2026-10-27: Initial publication in Story Mode, audited against the 10-point Pre-Publication Self-Audit.
- 2026-10-27 (revision 2): Applied review fixes (`R4`, `R5`): clarified sampled-observation debounce semantics and minimum pulse duration (`>= debounce_window + polling_interval`), and moved the `t = 204..224 ms` steady segment onto the `LOW (Pressed)` line in the Section 3B ASCII waveform.
