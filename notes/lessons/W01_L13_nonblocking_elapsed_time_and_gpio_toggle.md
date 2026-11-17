# W01-L13: Non-Blocking Elapsed-Time Scheduling And How `HAL_GPIO_TogglePin()` Works

> **Source-reviewed:** [Core/Src/main.c](../../../p1_f446re_baseline/Core/Src/main.c),
> [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c#L436),
> and [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal.c#L323),
> 2026-10-27.

Previous: [W01-L12: Blinking `LD2` (`PA5`) And Seeing How `BSRR` And `HAL_Delay()` Work](W01_L12_ld2_gpio_output_and_blocking_delay.md).

## 0. Know This First

In Lesson 12, `LD2` (`PA5`) blinked using `HAL_Delay(500U)`. At our nominal
`84 MHz` clock (`84,000,000` cycles/second × `0.5 s` = `42,000,000` nominal
cycles per half-second wait), the Cortex-M4 core stayed inside `HAL_Delay()`
unable to run any other main-loop work during each wait.

In this lesson, we replace `HAL_Delay(500U)` with a **non-blocking `500 ms` LED
check** so `while (1)` returns immediately on each pass—leaving room in the main
loop for the future **P1 sensor service** (which will have its own schedule and
execution budget).

| Term | What it does in our story |
| --- | --- |
| **Non-Blocking Elapsed-Time Check** | Compares `(now_ms - last_toggle_ms) >= 500U` on each pass of `while (1)`. When `< 500 ms` have elapsed, the `if` body is skipped by a short check so the next job in `while (1)` can run right away. |
| **`HAL_GetTick()`** | Returns the current 32-bit millisecond count stored in `uwTick` (`0, 1, 2, ...`). |
| **Unsigned Counter Rollover** | When `uint32_t uwTick` reaches `0xFFFFFFFF` (`4,294,967,295 ms` ≈ `49.7 days`), the next tick wraps it to `0x00000000`. Unsigned subtraction `now_ms - last_toggle_ms` computes the exact elapsed ticks across that rollover provided fewer than `2^32` ticks have elapsed since `last_toggle_ms`. |
| **`HAL_GPIO_TogglePin()`** | Reads `GPIOA->ODR` and uses a single masked `GPIOA->BSRR` write to flip `PA5` from `0 -> 1` or `1 -> 0` while leaving all other 15 Port A pins untouched. |

---

## 1. Goal

Replace blocking `HAL_Delay(500U)` calls in `Core/Src/main.c` with a
**non-blocking elapsed-time scheduler** (`(now_ms - last_toggle_ms) >= 500U`)
and `HAL_GPIO_TogglePin()`, trace what happens when an 8-bit timer variable
overflows (`250 + 10` addition bug vs. `259`-tick full-wrap bug) and how to
avoid both bugs, and trace the bitwise math inside `HAL_GPIO_TogglePin()` bit by
bit.

---

## 2. The 3-Scene Story: Freeing the Main Loop

```text
SCENE 1: Snapshot the Starting Timestamp Before `while (1)`
  |  `uint32_t last_toggle_ms = HAL_GetTick();` (say `100 ms`)
  v
SCENE 2: Fast Non-Blocking Check on Every Pass of `while (1)`
  |  Read `now_ms = HAL_GetTick();`
  |  - At `now_ms = 101 ms`: `101 - 100 = 1 < 500` -> False! Short check skips LED toggle
  |    and immediately continues to the rest of `while (1)`!
  |  - At `now_ms = 599 ms`: `599 - 100 = 499 < 500` -> False! Skip and keep running other jobs.
  v
SCENE 3: 500 ms Reached (`now_ms = 600 ms`) -> Toggle `LD2` and Advance Timestamp!
     - At `now_ms = 600 ms`: `600 - 100 = 500 >= 500` -> True!
     - Advance `last_toggle_ms = now_ms;` (`600 ms`)
     - Call `HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);` -> flips `PA5` in one masked `BSRR` write!
```

### `Before -> After`: Blocking Delay vs. Non-Blocking Elapsed-Time Loop

```text
BEFORE (Lesson 12 Blocking `HAL_Delay(500U)`):
  while (1) {
    SET LD2;
    HAL_Delay(500U);   /* CPU trapped here for 500 ms! Other main-loop work blocked! */
    RESET LD2;
    HAL_Delay(500U);   /* CPU trapped here for 500 ms! Other main-loop work blocked! */
  }

AFTER (Lesson 13 Non-Blocking Elapsed-Time Check):
  uint32_t last_toggle_ms = HAL_GetTick();
  while (1) {
    uint32_t now_ms = HAL_GetTick();
    if ((now_ms - last_toggle_ms) >= 500U) {
      last_toggle_ms = now_ms;
      HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin); /* One masked BSRR write every 500 ms */
    }
    /* Leaves room here for future P1 service checks (with their own schedule & budget) */
  }
```

---

## 3. Two Deep Technical Gems Inside Those 6 Lines of C

### Gem 1: What Happens When a Timer Variable Overflows (`250 + 10`) And How To Avoid Timer Bugs

Suppose we have an **8-bit unsigned timer counter (`uint8_t`, range `0..255`, wraps modulo `256`)**,
our current timestamp is **`curr_time = 250`**, and we want to wait for an
**`interval = 10` ticks**.

There are **two classic timer-overflow bugs** you must know how to explain and prevent:

#### Bug 1 — The Addition Overflow Bug (`curr_time + interval`, i.e., `250 + 10`)
What happens if you calculate a future deadline by **adding** `curr_time + interval`?
```c
/* BROKEN PATTERN: Adding curr_time + interval into an 8-bit variable */
uint8_t curr_time = 250U;
uint8_t interval  = 10U;
uint8_t deadline  = (uint8_t)(curr_time + interval); /* 250 + 10 = 260 -> wraps mod 256 to 4! */
```
Look at what goes wrong on the very next tick (`now = 251`):
* Mathematical deadline was `260`, which wrapped past `255` (`260 - 256`) to **`deadline = 4`**.
* At `now = 251` (only **1 tick** later!), the check `if (now >= deadline)` tests `if (251 >= 4)` -> **TRUE immediately!**
* Instead of waiting `10` ticks, your timer fires `9` ticks too early!
* *(And even if `curr_time + interval` didn't wrap—say `240 + 10 = 250`—if a busy loop checked it late after `now` wrapped past `255` to `0`, `if (0 >= 250)` would become **FALSE**, missing the deadline completely!)*

```text
BROKEN Addition Pattern (`deadline = curr_time + interval`):
  curr_time = 250, interval = 10
  deadline  = (uint8_t)(250 + 10) = 260 mod 256 = 4!
  Next tick (now = 251): is 251 >= 4? YES! (Bug: fires after 1 tick instead of 10!)
```

#### How We Fix Bug 1: Unsigned Subtraction (`(now - start) >= interval`)
Instead of adding `curr_time + interval`, **keep `start = 250` and subtract `now - start`**!
*(Note: In C, `uint8_t - uint8_t` promotes to signed `int`, so for an 8-bit counter we cast `(uint8_t)(now - start)`; for `uint32_t now_ms - last_toggle_ms`, both operands are already 32-bit `uint32_t` and subtract modulo `2^32` directly!)*

| Tick Step | Total Elapsed Ticks | 8-Bit Counter `now` (`0..255`) | Unsigned Subtraction `(uint8_t)(now - 250)` | Check `>= 10` | Result |
| --- | ---: | ---: | --- | --- | --- |
| Start | `0` | `250` | `250 - 250 = 0` | `0 >= 10` (`false`) | Keep running |
| +1 tick | `1` | `251` | `251 - 250 = 1` | `1 >= 10` (`false`) | Does NOT fire early! |
| +5 ticks | `5` | `255` | `255 - 250 = 5` | `5 >= 10` (`false`) | Keep running |
| +6 ticks (wrapped!) | `6` | **`0`** | `(0 - 250) mod 256 = 256 - 250 = 6` | `6 >= 10` (`false`) | Rollover handled cleanly! |
| +9 ticks | `9` | `3` | `(3 - 250) mod 256 = 259 - 250 = 9` | `9 >= 10` (`false`) | Keep running |
| **+10 ticks** | **`10`** | **`4`** | **`(4 - 250) mod 256 = 260 - 250 = 10`** | **`10 >= 10` (`true`!)** | **Fires at exact 10th tick!** |

#### Bug 2 — The "Full Lap Around the Clock" Bug (When `elapsed >= 256` in 8-Bit)

**First, picture an ordinary 12-hour wall clock (with no AM/PM):**
* You put a cake in the oven at **1:00**, and you want to wait **10 hours** (until **11:00**).
* What if you fall asleep and wake up **15 hours later** (`1 full 12-hour lap + 3 hours`)?
* After 12 hours, the clock is back at **1:00**. Plus 3 more hours = **4:00**!
* When you look at the clock (`4:00`) and subtract your start time (`1:00`), you get **`4:00 - 1:00 = 3 hours`** (`< 10 hours`)!
* Because the clock repeats every 12 hours, **it cannot tell the difference between `3 hours` and `12 + 3 = 15 hours`!**

**Now see the exact same "Full Lap" bug on our 8-bit counter (`0..255`, where 1 full lap = `256` ticks):**
* Our counter starts at **`start = 250`**, and we want to wait **`interval = 10` ticks**:

```text
What 1 Full Lap (256 ticks) Does to Any Starting Number:
  Start at 250  ---[ +256 ticks (1 full lap around 0..255) ]---> Back at 250!
  Start at 250  ---[ +259 ticks (1 full lap + 3 more ticks) ]--> Lands on 250 + 3 = 253!
```

Now compare **3 ticks elapsed** versus **259 ticks elapsed (`256 + 3`)**:

| Case | Actual Real Time Elapsed | Where Counter `now` Lands | What `(uint8_t)(now - 250)` Calculates | Does `>= 10` Trigger? |
| --- | --- | --- | --- | --- |
| **Case A (Normal early check)** | **`3` ticks** | `250 + 3 = 253` | `253 - 250 = 3` | `3 >= 10` is **`false`** (Correct: only 3 ticks passed) |
| **Case B (CPU stalled for 1 full lap!)** | **`259` ticks** (`256 + 3`) | `250` + 1 full lap (`256`) + `3` = **`253`** | `253 - 250 = 3` | `3 >= 10` is **`false`** (**BUG!** 259 ticks passed, but counter looks like only 3 ticks passed!) |

**Why did Case B fail?**
Because the CPU waited so long (`259` ticks) without updating `start` that the 8-bit counter completed **an entire 256-tick lap** past `250` and landed on `253` again!

#### Why `100 - (MAX - 400)` Gives `+500` (And the 16-Bit Promotion Trap!)

Suppose the counter wraps at `65,536` (`2^16`, where `UINT16_MAX = 65,535`):
* Let `last_toggle = 65,536 - 400 = 65,136` (`400` ticks before rollover).
* After `500` ticks elapse:
  * The counter steps `400` ticks to reach `0` (`65,136 + 400 = 65,536 -> 0`).
  * Then it steps `100` more ticks to reach **`now = 100`**.
* Now, what happens when we subtract `now - last_toggle` (`100 - 65,136`)?
  * In ordinary math, `100 - 65,136 = -65,036`.
  * In **16-bit unsigned modular math** (modulo `65,536`), a negative result wraps by adding `65,536`:
    ```text
    (100 - 65,136) + 65,536 = -65,036 + 65,536 = +500!
    ```
  * **Why it works:** The **subtraction underflow (`+ 65,536`) is the exact mirror image of the counter's rollover (`- 65,536`)**—the two wraps cancel each other out and recover the exact elapsed distance (`400 + 100 = 500`)!

```text
Counter Number Circle (Modulo 65,536):
   ... ---> 65,136 (last_toggle) ---[+400 ticks]---> 0 (wrap!) ---[+100 ticks]---> 100 (now)

   Modular distance = (100 - 65,136) mod 65,536
                    = 100 - (65,536 - 400) + 65,536
                    = 100 + 400 = 500 ticks!
```

> **Watch Out: When IS It a Bug in C? (16-Bit `uint16_t` vs. 32-Bit `uint32_t`)**
> * **With 32-bit `uint32_t` (like `uwTick` on STM32):**
>   Both `now_ms` and `last_toggle_ms` are `uint32_t`, and the counter wraps at `2^32`. In C, `uint32_t - uint32_t` is computed directly in `uint32_t` (modulo `2^32`), so `(100U - (0xFFFFFFFFU - 399U))` evaluates directly to **`500U`**. No bug!
> * **With a 16-bit `uint16_t` counter on a 32-bit CPU (`sizeof(int) == 4`):**
>   If `now_16` and `last_16` are `uint16_t` and you write:
>   ```c
>   /* BUG if now_16 and last_16 are uint16_t without a (uint16_t) cast! */
>   if ((now_16 - last_16) >= 500U)
>   ```
>   1. C promotes both `uint16_t` variables to **32-bit signed `int`**, so `100 - 65136` becomes **`-65036`** (signed `int`, **not** wrapped at `65,536`!).
>   2. Worse, because `500U` is `unsigned int` (`uint32_t`), C converts `-65036` to 32-bit `uint32_t` (`4,294,902,260`)! That means as soon as `now_16` wraps to `0` (after only `400` ticks, **100 ticks too early!**), `0 - 65136 = -65136` converts to `4,294,902,160 >= 500U` (**TRUE early!**).
>   3. **Fix for 16-bit timers:** Always cast the subtraction back to the counter's exact width:
>      ```c
>      if ((uint16_t)(now_16 - last_16) >= 500U) /* (uint16_t)(-65036) = 500! */
>      ```

#### How Do We Actually Solve "Full-Lap" Wrap (Instead of Just Making the Counter Bigger)?

Making the counter 32-bit (`49.7 days`) or 64-bit (`584 million years`) only makes the clock circle larger—it does not change the fact that **polling a free-running counter (`now - start`) has no memory of how many full laps passed while you weren't looking**.

To **retain an expired timer state** across a full counter lap (while the timer
or tick ISR continues operating), embedded systems use **three architectural
designs**:

| Solution | How It Works (Even With an 8-Bit `uint8_t`!) | Why It Retains Expiry Across a Full Lap |
| --- | --- | --- |
| **1. Saturating Countdown Timer (ISR Decrements to `0` and Stops)** | Instead of `main()` polling `now - start`, `main()` sets `countdown = 10` (with safe atomic/critical-section access when rearming). Every `1 ms`, `SysTick_Handler` checks `if (countdown > 0) countdown--;`. When `countdown` hits `0`, it **stops at `0` (saturates)**! | As long as the tick ISR keeps running, whether `main()` checks after `10` ticks or `259` ticks, `countdown` stays at `0` without wrapping past `0`. |
| **2. Sticky Hardware / Software Event Flag (`expired = 1`)** | When the timer reaches the target (`10` ticks), hardware (like STM32 `TIMx->SR` `UIF` bit) or the ISR latches a **sticky flag** (`timer_expired = 1`). | Even if the counter keeps spinning for more laps, the sticky flag **stays `1`** until `main()` clears it (remembering that *at least one* expiry occurred). |
| **3. RTOS Task Delay / Timer Queue (FreeRTOS `vTaskDelayUntil`)** | The RTOS kernel keeps an ordered list of wakeup ticks inside the tick ISR and moves the task to the **Ready** list on the tick its delay expires (running it when scheduled by priority). | Expiry is checked every tick by the kernel tick handler, and once moved to the **Ready** list, the task stays Ready until scheduled. |

```text
Why Solution 1 (Saturating 8-Bit Countdown) Never Wraps Even After 259 Ticks:

  main() sets:  uint8_t countdown = 10;
  Tick 1..10:   SysTick ISR does `if (countdown > 0) countdown--;`  (10 -> 9 -> ... -> 0)
  Tick 11..259: `countdown` is already 0, so ISR leaves it at 0! (Sticky at 0!)
  Tick 259:     main() finally checks `if (countdown == 0)` -> TRUE! Reloads `countdown = 10`!
```

#### Deep Dive: Why Doesn't FreeRTOS Use a Hardware Sticky Flag, and How Does `vTaskDelayUntil()` Solve Both Bugs Step by Step?

**1. Why doesn't FreeRTOS use a hardware timer's sticky flag for every task?**
* **Hardware scarcity (1 timer vs. 50 tasks):** An STM32F446RE has only ~14 hardware timers, and most are needed for physical I/O (PWM, ADC triggers, input capture). Meanwhile, your firmware may have **20+ software tasks and timeouts** (`1 ms`, `10 ms`, `50 ms`, `500 ms`), all sharing **one** `SysTick` hardware timer.
* **A sticky register bit cannot sleep or context-switch a CPU thread:** A hardware flag (`TIMx->SR` `UIF`) only latches a `1` in a register. To let Task A sleep for `500 ms` while Task B uses the CPU—and then automatically wake Task A—the OS needs a **sorted task wakeup list** checked by the `1 ms` `SysTick` interrupt.

**2. How FreeRTOS Moves Your Task to Ready at `4` (Illustrative 8-Bit Model, Assuming a `1 ms` Tick):**

*(Note: Real Cortex-M4F FreeRTOS ports use 16-bit or 32-bit `TickType_t` at the configured `configTICK_RATE_HZ`; we use an illustrative 8-bit `0..255` counter at `1 ms` per tick here so the rollover math stays tiny.)*

Forget the long C names for a moment and picture **two alarm lists on FreeRTOS's desk**:
* **List 1 ("This Lap's Alarms"):** FreeRTOS checks **only this list** on every tick (`250, 251, ..., 255`).
* **List 2 ("Next Lap's Alarms"):** A separate holding list for alarms that wrapped past `255 -> 0`. FreeRTOS **does not check List 2 at all** until the clock rolls over to `0`!

Now watch what happens **millisecond by millisecond** from `250` to `4` when `xLastWakeTime` was initialized to `250` (`curr_time = 250`) and your task calls `vTaskDelayUntil(&xLastWakeTime, 10)`:

#### At `curr_time = 250` (`xLastWakeTime = 250`): Your Task Calls `vTaskDelayUntil(&xLastWakeTime, 10)`
1. FreeRTOS adds the increment to the previous wake time: `wake_time = xLastWakeTime + 10 = (uint8_t)(250 + 10) = 4` (wrapped past `255`), and updates `xLastWakeTime = 4`.
2. Since `curr_time == 250` and `wake_time (4) < curr_time (250)`, FreeRTOS knows `wake_time` **rolled over past `255`** into the next lap!
3. Because it rolled over, FreeRTOS puts your task (`wake @ 4`) into **List 2 ("Next Lap's Alarms")**—**NOT** List 1!

```text
At ms = 250:
  List 1 (This Lap — checked every ms):   [ empty ]
  List 2 (Next Lap — ignored for now):    [ Your Task: wake at 4 ]
```

#### Now Watch Every Single Millisecond Tick (`251` to `4`)

| Millisecond Tick | Clock `curr_time` | List 1 ("This Lap" — Checked by ISR) | List 2 ("Next Lap" — Ignored) | What FreeRTOS Does on This Tick |
| --- | ---: | --- | --- | --- |
| **+1 ms** | `251` | `[ empty ]` | `[ Your Task: wake @ 4 ]` | List 1 is empty -> Your task stays asleep! *(Why didn't `251 >= 4` wake it? Because List 2 is NOT checked!)* |
| **+2 ms** | `252` | `[ empty ]` | `[ Your Task: wake @ 4 ]` | List 1 is empty -> Your task stays asleep |
| **+3 ms** | `253` | `[ empty ]` | `[ Your Task: wake @ 4 ]` | List 1 is empty -> Your task stays asleep |
| **+4 ms** | `254` | `[ empty ]` | `[ Your Task: wake @ 4 ]` | List 1 is empty -> Your task stays asleep |
| **+5 ms** | `255` | `[ empty ]` | `[ Your Task: wake @ 4 ]` | List 1 is empty -> Your task stays asleep |
| **+6 ms (ROLLOVER!)** | **`0`** | **`[ Your Task: wake @ 4 ]`** *(Swapped from List 2!)* | `[ empty ]` | **Clock rolled over `255 -> 0`!** FreeRTOS moves List 2 into List 1! Then checks: is `0 >= 4`? **No** -> stay asleep |
| **+7 ms** | `1` | `[ Your Task: wake @ 4 ]` | `[ empty ]` | Checks List 1: is `1 >= 4`? **No** -> stay asleep |
| **+8 ms** | `2` | `[ Your Task: wake @ 4 ]` | `[ empty ]` | Checks List 1: is `2 >= 4`? **No** -> stay asleep |
| **+9 ms** | `3` | `[ Your Task: wake @ 4 ]` | `[ empty ]` | Checks List 1: is `3 >= 4`? **No** -> stay asleep |
| **+10 ms (READY!)** | **`4`** | `[ empty ]` *(Task moved to Ready!)* | `[ empty ]` | Checks List 1: **is `4 >= 4`? YES!** The tick ISR moves your task to the **Ready List** (and it runs on the CPU as soon as scheduled by priority)! |

#### The 3 Golden Rules When Using Polled `(now - start) >= interval`
1. **Always subtract (`(now - start) >= interval`), never add (`now >= start + interval`):** Subtraction computes the true modulo-`2^N` elapsed ticks across a boundary rollover without premature wrap.
2. **Total elapsed time since `start` (`last_toggle_ms`) must stay strictly less than one full counter cycle (`< 2^N` ticks):**
   * An 8-bit `1 ms` counter wraps every `256 ms`—too small for a `500 ms` delay!
   * STM32's 32-bit `uint32_t uwTick` wraps every `2^32 ms` = `4,294,967,296 ms` (**`49.7 days`**). As long as fewer than `2^32` ticks elapse since `last_toggle_ms` (and `interval` is well below `2^32`, e.g., `< 2^31` ticks), `now_ms - last_toggle_ms` is 100% unambiguous.
3. **Check the timer often enough to detect expiry and advance `last_toggle_ms` before `2^N` ticks elapse since `last_toggle_ms`:** Once `(now_ms - last_toggle_ms) >= 500U` triggers and updates `last_toggle_ms = now_ms`, the reference timestamp is refreshed for the next cycle.

---

### Gem 2: How `HAL_GPIO_TogglePin()` Flips `PA5` Using One `BSRR` Write

Open [Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c:436](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c#L436):

```c
/* Inside stm32f4xx_hal_gpio.c (line 118 and lines 436-448):
 * #define GPIO_NUMBER  16U */
void HAL_GPIO_TogglePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin)
{
  uint32_t odr;

  /* 1. READ CURRENT 32-BIT OUTPUT DATA REGISTER (lower 16 bits = PA0..PA15): */
  odr = GPIOx->ODR;

  /* 2. COMPUTE BSRR COMMAND TO FLIP SELECTED PIN(S) ONLY:
   *    - Left half  `((odr & GPIO_Pin) << 16U)`: if the pin was 1 (HIGH), puts 1
   *      in the upper 16 Reset bits of BSRR to clear it to 0!
   *    - Right half `(~odr & GPIO_Pin)`:         if the pin was 0 (LOW), puts 1
   *      in the lower 16 Set bits of BSRR to set it to 1! */
  GPIOx->BSRR = ((odr & GPIO_Pin) << GPIO_NUMBER) | (~odr & GPIO_Pin);
}
```

#### Let's Trace That Expression With Concrete Numbers (`GPIO_Pin = 0x0020` for `PA5`)

Suppose `PA3` (`0x0008`) is also `1` in `GPIOA->ODR`, so we can see that `PA3` is
never disturbed when we toggle `PA5` (`0x0020`):

* **Case 1 — `LD2` (`PA5`) is currently OFF (`0`), and `PA3` is `1` (`odr = 0x00000008`):**
  1. `odr & GPIO_Pin` = `0x00000008 & 0x00000020` = **`0x00000000`** (because bit 5 of `odr` is `0`).
  2. `((odr & GPIO_Pin) << 16U)` = `0x00000000 << 16U` = **`0x00000000`** (Reset half of `BSRR` is all `0`).
  3. `~odr` flips all 32 bits (`0xFFFFFFF7`), so bit 5 of `~odr` is now `1`.
  4. `~odr & GPIO_Pin` = `0xFFFFFFF7 & 0x00000020` = **`0x00000020`** (masks out every bit except bit 5!).
  5. `GPIOA->BSRR` receives `0x00000000 | 0x00000020` = **`0x00000020`** -> **Sets `PA5` to `1` (`LD2` turns ON)** while `PA3` stays `1`!

* **Case 2 — `LD2` (`PA5`) is currently ON (`1`), and `PA3` is `1` (`odr = 0x00000028`):**
  1. `odr & GPIO_Pin` = `0x00000028 & 0x00000020` = **`0x00000020`** (because bit 5 of `odr` is `1`).
  2. `((odr & GPIO_Pin) << 16U)` = `0x00000020 << 16U` = **`0x00200000`** (Reset bit 21 in upper half of `BSRR` is `1`!).
  3. `~odr` has bit 5 equal to `0` (`0xFFFFFFD7`), so `~odr & GPIO_Pin` = **`0x00000000`** (Set half of `BSRR` is `0`).
  4. `GPIOA->BSRR` receives `0x00200000 | 0x00000000` = **`0x00200000`** -> **Resets `PA5` to `0` (`LD2` turns OFF)** while `PA3` stays `1`!

```text
Summary of `((odr & 0x0020) << 16U) | (~odr & 0x0020)`:
+------------------+-----------------------+-------------------+-------------------+------------------+
| Starting PA5 ODR | `(odr & 0x20) << 16U` | `~odr & 0x0020`   | Written to `BSRR` | New PA5 ODR      |
|                  | (Upper Reset Half)    | (Lower Set Half)  |                   |                  |
+------------------+-----------------------+-------------------+-------------------+------------------+
| `0` (LD2 OFF)    | `0x00000000`          | `0x00000020`      | **`0x00000020`**  | **`1` (LD2 ON)** |
| `1` (LD2 ON)     | `0x00200000`          | `0x00000000`      | **`0x00200000`**  | **`0` (LD2 OFF)**|
+------------------+-----------------------+-------------------+-------------------+------------------+
```

---

## 4. Target Code To Update In `Core/Src/main.c`

Source link: [Core/Src/main.c:91](../../../p1_f446re_baseline/Core/Src/main.c#L91)

```c
  /* USER CODE BEGIN 2 */
  /* Snapshot the starting millisecond tick right before entering while (1): */
  uint32_t last_toggle_ms = HAL_GetTick();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t now_ms = HAL_GetTick();

    /* Non-blocking 500 ms check: returns immediately when < 500 ms elapsed */
    if ((now_ms - last_toggle_ms) >= 500U)
    {
      last_toggle_ms = now_ms;
      HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    }
  }
  /* USER CODE END 3 */
```

### Complex Syntax Breakdown

* **`~odr` (Bitwise NOT):** Flips every `0` bit to `1` and every `1` bit to `0` in the 32-bit `uint32_t odr` value.
* **`& GPIO_Pin` (Bitwise AND mask):** Because `GPIO_Pin` is `uint16_t` (`0x0020`) and `odr` is `uint32_t`, C converts `GPIO_Pin` to `uint32_t` (`0x00000020`) and zeroes out all 31 other bits, keeping only bit 5.
* **`<< GPIO_NUMBER` (`<< 16U`, Left Shift):** Moves bit `5` up by `16` positions to bit `21` (`0x00200000`), which is the `PA5` **Reset** bit in the upper half of `GPIOA->BSRR`.
* **`|` (Bitwise OR):** Combines the upper 16 Reset bits and lower 16 Set bits into one 32-bit command word written to `GPIOx->BSRR`.

---

## 5. Control Flow

### (a) One-Line Call Chain

```text
main() in while (1) -> HAL_GetTick() reads uwTick
  -> if (now_ms - last_toggle_ms >= 500U)
       -> HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5) -> reads GPIOA->ODR -> writes GPIOA->BSRR
  -> continues immediately to next statement in while (1)
```

### (b) Step Table (Tracing `last_toggle_ms = 100` through `now_ms = 600`)

| # | `now_ms` (`uwTick`) | `now_ms - last_toggle_ms` | Condition `>= 500U` | Action Taken | `PA5` (`LD2`) State |
| --- | ---: | ---: | --- | --- | --- |
| 1 | `100` | `100 - 100 = 0` | `false` | Skips `if` body immediately | `0` (OFF) |
| 2 | `350` | `350 - 100 = 250` | `false` | Skips `if` body immediately | `0` (OFF) |
| 3 | `600` | `600 - 100 = 500` | **`true`** | Sets `last_toggle_ms = 600`; writes `0x00000020` to `BSRR` | **`1` (ON)** |
| 4 | `601` | `601 - 600 = 1` | `false` | Skips `if` body immediately | `1` (ON) |
| 5 | `1100` | `1100 - 600 = 500` | **`true`** | Sets `last_toggle_ms = 1100`; writes `0x00200000` to `BSRR` | **`0` (OFF)** |

### (c) Timeline: Thread Mode vs. Handler Mode

```text
Thread Mode (`main` in `while (1)`)                 Handler Mode (`SysTick_Handler` ISR)
-----------------------------------                 ------------------------------------
now_ms = 599: (599 - 100) < 500 -> skip `if`
  (leaves room for future P1 service checks)
                                                    <--- [1 ms SysTick fires]
                                                         HAL_IncTick() -> uwTick = 600
now_ms = 600: (600 - 100) >= 500 -> TRUE!
  last_toggle_ms = 600
  HAL_GPIO_TogglePin() writes BSRR (one masked write)
  (immediately continues to rest of while (1))
```

---

## 6. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
In Lesson 12, `HAL_Delay(500U)` was like a chef staring at the kitchen clock for
half a second without stirring any other pot. In Lesson 13, the chef writes down
the time (`last_toggle_ms = 100`) on a sticky note. On each walk past the stove,
the chef glances at the clock (`now_ms - last_toggle_ms >= 500U`): if `500 ms`
haven't passed yet, the chef keeps working on other dishes right away! The
moment `500 ms` have elapsed (`now_ms = 600`), the chef updates the sticky note
to `600` and calls `HAL_GPIO_TogglePin()`, which checks whether switch #5 in
`ODR` is currently `0` or `1` and sends the opposite command (`0x00000020` or
`0x00200000`) to `BSRR`.

---

### Interview Q1: Why is `if ((now_ms - last_ms) >= period_ms)` rollover-safe for `uint32_t` ticks, whereas `if (now_ms >= last_ms + period_ms)` fails near rollover?
* **Simple answer:** Because in unsigned arithmetic, adding `last_ms + period_ms` wraps around **before** `now_ms` wraps, whereas subtracting `now_ms - last_ms` computes the true modulo-`2^N` elapsed ticks across the boundary (provided fewer than `2^N` total ticks have elapsed since `last_ms`).
* **Concrete 8-bit example (`250 + 10`):** With an 8-bit counter (`0..255`), `uint8_t deadline = 250 + 10;` overflows to `4`, so at `now = 251` (`1` tick later) `251 >= 4` is already true! Subtracting `(uint8_t)(now - 250)` yields `1` at `now = 251`, `6` at `now = 0`, and `10` at `now = 4`.

### Interview Q2: What is the difference between updating `last_toggle_ms = now_ms;` versus `last_toggle_ms += 500U;` when a periodic deadline triggers?
* **Simple answer:**
  * **`last_toggle_ms = now_ms` (Relative / minimum-gap scheduling):** Records the sampled check timestamp (`now_ms`). If another job delayed this check by `3 ms` so `now_ms` was sampled at tick `603`, the next toggle **becomes due at tick `603 + 500 = 1103`** and occurs when the loop next executes the check at or after tick `1103` (avoiding a burst of back-to-back catch-up toggles).
  * **`last_toggle_ms += 500U` (Phase-locked / non-drifting cadence):** Advances the target due time in fixed steps (`100, 600, 1100, 1600`), keeping long-term phase locked to the tick counter, but if the loop stalls for `2000 ms`, it will trigger `4` times back-to-back to catch up unless bounded by an overrun check.

### Interview Q3: Is `HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5)` safe if an ISR modifies `PA6`, and what if an ISR also modifies `PA5`?
* **Simple answer:**
  * **Other pins on the same port (`PA6`):** **Safe.** Because `((odr & GPIO_Pin) << 16U) | (~odr & GPIO_Pin)` masks both halves with `& GPIO_PIN_5` (`0x0020`), the `BSRR` write only ever puts a `1` in bit 5 or bit 21—all other 15 pins get `0` in `BSRR` and are untouched even if an ISR changed `PA6` after `odr = GPIOx->ODR` was read.
  * **The same pin (`PA5`):** **Requires single ownership or a critical section.** If both Thread mode and an ISR toggle `PA5` concurrently, an ISR firing between `odr = GPIOx->ODR` and `GPIOx->BSRR = ...` will have its `PA5` change overwritten when Thread mode finishes its `BSRR` write.

## 7. Corrections

- 2026-10-27: Initial publication in Story Mode, audited against the 10-point Pre-Publication Self-Audit.
- 2026-10-27 (revision 2): Applied review fixes (`R1`–`R4`) and expanded Gem 1 with the full 8-bit `250 + 10` addition-overflow trace, the `259`-tick vs. `3`-tick full-counter-period wrap ambiguity counterexample, and the 3 rules to prevent timer overflow bugs.
- 2026-10-27 (revision 3): Applied review fixes (`R1`–`R3`) to the FreeRTOS two-list rollover trace: labelled the 8-bit `1 ms` model as illustrative, initialized `xLastWakeTime = 250` for `vTaskDelayUntil(&xLastWakeTime, 10)`, clarified that expiry moves the task to the **Ready** list to run when scheduled by priority, and noted ISR-progress/rearming/coalescing conditions in the 3-solution table.
