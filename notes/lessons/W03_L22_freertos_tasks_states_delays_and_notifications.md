# W03-L22: FreeRTOS From The Ground Up — Task States, TCB & Lists, Priorities, `vTaskDelayUntil`, And ISR Task Notifications (`P1`)

> **Scope & verification status:** `Source-reviewed FreeRTOS architecture & P1 vertical-slice lesson (2026-11-04): covers FreeRTOS tasks.c / ARM_CM4F port.c internals, TCB & list mechanics, Cortex-M4 BASEPRI / configMAX_SYSCALL_INTERRUPT_PRIORITY rules, vTaskDelayUntil, and ulTaskNotifyTake / vTaskNotifyGiveFromISR on NUCLEO-F446RE.`

Previous: [Three-resource initialization and reverse-order unwind (`C02`)](W02_L21_c02_three_resource_init_and_reverse_unwind.md).

---

## The Core Idea: What FreeRTOS Adds On Top Of Our `W01-L19` `PendSV` Switcher

In [W01-L19](W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md), we built a working 2-task context switcher using `SVC_Handler` and `PendSV_Handler`. However, it had one big limitation:
* Both `Task 0` (Button) and `Task 1` (LED) ran inside `for (;;)` loops that **never slept**.
* Even when the button was not pressed and the LED did not need to toggle for another `499 ms`, `PendSV_Handler` kept switching between `Task 0` and `Task 1` thousands of times per second (**burning 100% CPU**).

**FreeRTOS uses the exact same `SVC_Handler` and `PendSV_Handler` assembly instructions you learned in `W01-L19`**, and adds **four kernel mechanisms** in C (`tasks.c`) so waiting tasks use **0% CPU**:

1. **Four Task States (`Running`, `Ready`, `Blocked`, `Suspended`) and Separate Kernel Lists:**
   * `PendSV_Handler` (`vTaskSwitchContext()`) **only** picks tasks that are in the **`Ready` list**.
   * When a task is waiting for time (`500 ms`) or waiting for an interrupt (`B1` button press), FreeRTOS **removes that task from the `Ready` list** and puts it into the **`Blocked` state (`0% CPU`)**!
2. **Task Priority Lists (`pxReadyTasksLists[0 .. configMAX_PRIORITIES - 1]`):**
   * Instead of alternating `0 -> 1 -> 0 -> 1`, FreeRTOS always runs the **highest-priority `Ready` task**.
   * If both the Button Task and the LED Task are `Blocked` (asleep), FreeRTOS runs the built-in **`Idle Task` (`Priority 0`)**.
3. **Sorted Wakeup Lists for `vTaskDelayUntil()` (`pxDelayedTaskList` & `pxOverflowDelayedTaskList`):**
   * When the LED task calls `vTaskDelayUntil(&xLastWakeTime, 500)`, FreeRTOS calculates its exact wakeup tick (`xLastWakeTime + 500`), moves the task into the **`Blocked` Delayed List**, and switches away.
   * When `SysTick_Handler` (`xTaskIncrementTick()`) reaches that tick, it moves the LED task back to the **`Ready` List**!
4. **Built-In Task Notifications (`ulTaskNotifyTake` + `vTaskNotifyGiveFromISR`):**
   * Every FreeRTOS task has a built-in 32-bit notification counter (`ulNotifiedValue`) inside its Task Control Block (`TCB`).
   * The Button task calls `ulTaskNotifyTake()` and sleeps in the **`Blocked` state (`0% CPU`)** until `EXTI15_10_IRQHandler` calls `vTaskNotifyGiveFromISR()`, which moves the Button task to **`Ready`** and sets `PENDSVSET` (`portYIELD_FROM_ISR`) so the Button task runs immediately!

```text
What Changes From W01-L19 (Busy Polling) to W03-L22 (FreeRTOS States):

  W01-L19:  Task 0 (Ready) <---> Task 1 (Ready)   [Both spin in for(;;), 100% CPU]

  FreeRTOS:
    - Button Task (Task Prio 2, High): BLOCKED in `ulTaskNotifyTake()` (0% CPU)
                                         -> Wakes to READY only when EXTI15_10 fires!
    - LED Task    (Task Prio 1, Med):  BLOCKED in `vTaskDelayUntil()`  (0% CPU)
                                         -> Wakes to READY only every 500 ms / 150 ms!
    - Idle Task   (Task Prio 0, Low):  READY / RUNNING whenever both tasks are Blocked!
```

---

## Step 1 — Inside The FreeRTOS Task Control Block (`tskTCB`) And The 4 Task States

### 1. What Is Inside a FreeRTOS `TCB_t` (`tskTCB` in `tasks.c`)?

In `W01-L19`, our `tcb_t` had only one pointer: `uint32_t *psp;`.
In FreeRTOS (`tasks.c`), `tskTCB` starts with that **exact same `pxTopOfStack` pointer at offset `+0x00`** (so `PendSV_Handler` assembly can load/store it directly), and adds list nodes, priority, and notification fields:

| `tskTCB` Field (`tasks.c`) | What It Stores | Why FreeRTOS Needs It |
| --- | --- | --- |
| **`volatile StackType_t *pxTopOfStack`** (`+0x00`) | Saved `PSP` address (points to `R4` on the task's stack when suspended) | **Must be the 1st field in `tskTCB`** so `xPortPendSVHandler` assembly can read/write `[r2]` (`pxCurrentTCB->pxTopOfStack`) with zero offset. |
| **`ListItem_t xStateListItem`** | Linked-list node for the task's **current state** | Placed in **one** list at a time: `pxReadyTasksLists[prio]` (when `Ready`/`Running`), `pxDelayedTaskList` (when `Blocked` on time), or `xSuspendedTaskList` (when `Suspended`). |
| **`ListItem_t xEventListItem`** | Linked-list node for **event waiting** | When a task blocks on a Queue or Mutex with a timeout, `xStateListItem` sits in `pxDelayedTaskList` (for the timeout) while `xEventListItem` sits in the Queue's waiting list! |
| **`UBaseType_t uxPriority`** | Task priority (`0` = lowest, `configMAX_PRIORITIES - 1` = highest) | Tells the scheduler which `pxReadyTasksLists[uxPriority]` bucket this task belongs to. |
| **`StackType_t *pxStack`** | Lowest address of the task's stack array (`&stack[0]`) | Used by `configCHECK_FOR_STACK_OVERFLOW` to detect if `pxTopOfStack` grew below `pxStack`. |
| **`uint32_t ulNotifiedValue`** & **`uint8_t ucNotifyState`** | Built-in 32-bit notification counter and state (`NotWaiting`, `Waiting`, `Received`) | Allows an ISR (`vTaskNotifyGiveFromISR`) to wake this task directly without allocating a separate Queue or Semaphore! |

---

### 2. How a Task Moves Between the 4 FreeRTOS States

1. **`Ready`:** The task is able to run, and its `xStateListItem` is inside `pxReadyTasksLists[uxPriority]`, waiting only because a higher- or equal-priority task is currently using the CPU.
2. **`Running`:** The task currently owns the CPU (`pxCurrentTCB` points to this task's `TCB`, and the CPU's `PSP` points to this task's stack). On our single-core Cortex-M4, **at most one task is `Running` at any instant**.
3. **`Blocked` (`0% CPU`):** The task called a waiting API (`vTaskDelay`, `vTaskDelayUntil`, `ulTaskNotifyTake`, `xQueueReceive`). FreeRTOS **removes** its `xStateListItem` from `pxReadyTasksLists` and places it into `pxDelayedTaskList` (or an event list). The scheduler **never schedules a `Blocked` task** until the event or timeout moves it back to `Ready`!
4. **`Suspended` (`0% CPU`):** Paused indefinitely by `vTaskSuspend()` (no timeout); only `vTaskResume()` moves it back to `Ready`.

```text
                       vTaskDelayUntil() /
                     ulTaskNotifyTake() waits
              +-----------------------------------+
              |                                   |
              v                                   |
        +-----------+   Timeout expires OR  +-----------+   Highest-priority   +-----------+
        |  BLOCKED  |  xTaskNotifyFromISR() |   READY   |  Ready task selected |  RUNNING  |
        | (0% CPU)  | --------------------> |  (In list | -------------------> | (Owns CPU |
        |           |                       |  for prio)| <------------------- |  & PSP!)  |
        +-----------+                       +-----------+   Preempted by a     +-----------+
                                             |         ^    higher-prio task
                              vTaskSuspend() |         | vTaskResume()
                                             v         |
                                            +-----------+
                                            | SUSPENDED |
                                            +-----------+
```

### Step 1 Code: Creating Static or Dynamic Tasks in FreeRTOS (`xTaskCreate`)

```c
#include "FreeRTOS.h"
#include "task.h"

/* Global handle to the Button Task's TCB so EXTI15_10_IRQHandler can notify it: */
static TaskHandle_t xButtonTaskHandle = NULL;
static TaskHandle_t xLedTaskHandle    = NULL;

/* In FreeRTOS task priorities: HIGHER number = HIGHER urgency!
 *   - Idle Task (created automatically by vTaskStartScheduler): Priority 0 (Lowest)
 *   - vLedPeriodicTask:                                         Priority 1 (Medium)
 *   - vButtonServiceTask:                                       Priority 2 (High)   */
#define PRIO_LED_TASK      (tskIDLE_PRIORITY + 1U) /* Priority 1 */
#define PRIO_BUTTON_TASK   (tskIDLE_PRIORITY + 2U) /* Priority 2 */
```

---

## Step 2 — FreeRTOS Task Priorities vs. Cortex-M4 Hardware NVIC Priorities (`configMAX_SYSCALL_INTERRUPT_PRIORITY`)

### The #1 Cortex-M4 FreeRTOS Rule: Why Priority Numbers Go in Opposite Directions!

You must keep **Software Task Priorities** and **Hardware NVIC Interrupt Priorities** completely separate:

1. **Software Task Priorities (`uxPriority` in FreeRTOS `xTaskCreate`):**
   * **Higher number = Higher urgency!**
   * `Priority 0` (`tskIDLE_PRIORITY`) is the **lowest** urgency; `Priority 2` (`PRIO_BUTTON_TASK`) preempts `Priority 1` (`PRIO_LED_TASK`).
   * How does FreeRTOS find the highest-priority `Ready` task in 1 CPU cycle? On Cortex-M4 (`configUSE_PORT_OPTIMISED_TASK_SELECTION = 1`), FreeRTOS sets bit `P` in a 32-bit bitmap `uxTopReadyPriority` whenever a task at priority `P` is `Ready`, and uses the single-cycle hardware instruction **`clz` (Count Leading Zeros)**: `31 - __CLZ(uxTopReadyPriority)`!

2. **Hardware NVIC Interrupt Priorities (`NVIC_SetPriority` on STM32F446):**
   * **Smaller number = Higher urgency!**
   * `Hardware Priority 0` is the **highest** urgency; `Hardware Priority 15` (`0xF0`) is the **lowest** urgency.
   * Note that **every hardware ISR (`Priority 0..15`) preempts every Thread-mode task**, regardless of the task's `uxPriority`!

3. **How `configMAX_SYSCALL_INTERRUPT_PRIORITY` and `BASEPRI` Work:**
   * When FreeRTOS updates its internal task lists (`pxReadyTasksLists` or `pxDelayedTaskList`) inside `taskENTER_CRITICAL()` or inside a `...FromISR()` function, it **does not disable all interrupts with `PRIMASK`**.
   * Instead, FreeRTOS writes **`configMAX_SYSCALL_INTERRUPT_PRIORITY` (`5U << 4 = 0x50`) into the CPU `BASEPRI` register**!
   * Setting `BASEPRI = 0x50` masks hardware priorities **`5` through `15`** (`EXTI15_10` at `5`, `SysTick` at `15`, `PendSV` at `15`), while leaving ultra-urgent hardware interrupts at priorities **`0` through `4`** completely unmasked!
   * **CRITICAL RULE:** Because `BASEPRI = 0x50` only blocks hardware priorities `5..15`:
     * **Any ISR that calls a FreeRTOS `...FromISR()` function (such as `EXTI15_10_IRQHandler` calling `vTaskNotifyGiveFromISR()`) MUST have a hardware priority between `5` and `15` (numerically `>= 5`)!**
     * **Otherwise:** If you leave `EXTI15_10` at its reset hardware priority `0` (or `1..4`) and call `vTaskNotifyGiveFromISR()`, `EXTI15_10` can interrupt FreeRTOS in the middle of `taskENTER_CRITICAL()` (because `BASEPRI = 0x50` cannot block Priority `0..4`!) and corrupt the kernel linked lists! (In `FreeRTOSConfig.h`, `configASSERT` checks the ISR's hardware priority and halts immediately if it is `< 5`.)

```text
CORTEX-M4 HARDWARE NVIC PRIORITIES (Smaller Number = Higher Urgency):
+-----------------------------------------------------------------------------------+
| HW Priority 0 .. 4  (0x00..0x40): Zero-Jitter Fast ISRs (NEVER masked by BASEPRI) |
|   -> FORBIDDEN to call any FreeRTOS `...FromISR()` API!                           |
+===================================================================================+ <-- configMAX_SYSCALL_INTERRUPT_PRIORITY = 5 (0x50)
| HW Priority 5 .. 14 (0x50..0xE0): FreeRTOS-Safe Peripheral ISRs                   |
|   -> ALLOWED to call `vTaskNotifyGiveFromISR()`, `xQueueSendFromISR()`, etc.      |
|   -> Our `EXTI15_10_IRQHandler` (IRQ 40) is set to HW Priority 5 (0x50)!          |
+-----------------------------------------------------------------------------------+
| HW Priority 15      (0xF0, Lowest): `SysTick_Handler` and `PendSV_Handler`        |
+-----------------------------------------------------------------------------------+
                                         |
                                         v (All ISRs run in Handler Mode on MSP)
=====================================================================================
FREERTOS THREAD-MODE TASKS ON PSP (Larger Number = Higher Urgency!):
  Task Priority 2 (High):   `vButtonServiceTask` (Wakes on B1 notify, preempts LED)
  Task Priority 1 (Medium): `vLedPeriodicTask`   (Wakes every 500 ms / 150 ms)
  Task Priority 0 (Lowest): `prvIdleTask`        (Runs when both tasks are Blocked)
```

### Step 2 Code: `FreeRTOSConfig.h` Priority Settings and `NVIC` Setup

```c
/* Key settings in FreeRTOSConfig.h for STM32F446RE (4 priority bits -> PRIGROUP = 3): */
#define configPRIO_BITS                               4U
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY       15U  /* PendSV & SysTick at HW Prio 15 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5U   /* ISRs calling FromISR: 5..15!   */

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))      /* 0xF0 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS)) /* 0x50 */

/* Setting EXTI15_10 (IRQ 40) to Hardware Priority 5 (0x50) before enabling it: */
static void b1_exti_irq_priority_init(void)
{
    /* Set PRIGROUP = 3 (all 4 bits are preemption priority; required by FreeRTOS CM4F): */
    NVIC_SetPriorityGrouping(3U);

    /* Set IRQ 40 to logical priority 5 (>= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY): */
    NVIC_SetPriority(EXTI15_10_IRQn, 5U); /* Writes (5U << 4) = 0x50 into NVIC->IP[40] */
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}
```

---

## Step 3 — Periodic Work Without Drift (`vTaskDelayUntil` vs. `vTaskDelay`)

### Why Do Periodic Tasks Use `vTaskDelayUntil()` Instead of `vTaskDelay()`?

Suppose our LED task (`vLedPeriodicTask`) wants to run at an exact period (`500 ms` or `150 ms`), and suppose the code inside the loop (or a higher-priority task that preempted it) takes `3 ms` to finish before the delay call is reached:

* **If you call `vTaskDelay(500)` (Relative Delay):**
  * `vTaskDelay(500)` tells FreeRTOS: *"Wake me `500` ticks **from the moment I call `vTaskDelay()`** (`wake_time = xTickCount + 500`)."*
  * Because `3 ms` already passed before `vTaskDelay(500)` was called, the task wakes at `503 ms`, then `1006 ms`, then `1509 ms`—drifting later on every single cycle!
* **If you call `vTaskDelayUntil(&xLastWakeTime, period_ticks)` (Absolute Deadline Periodic Delay):**
  * Before the `for (;;)` loop starts, you initialize `xLastWakeTime = xTaskGetTickCount();` once (for example, `0`).
  * Inside the loop, `vTaskDelayUntil(&xLastWakeTime, 500)` calculates the next wakeup from the **previous scheduled wakeup time**:
    $$\text{xTimeToWake} = \text{xLastWakeTime} + \text{period\_ticks}$$
    and updates `xLastWakeTime = xTimeToWake` (`0 -> 500 -> 1000 -> 1500`).
  * Even if the task body took `3 ms` (so `xTickCount` is currently `3`), `xTimeToWake` is still `0 + 500 = 500`!

### What `vTaskDelayUntil()` Does Inside FreeRTOS (`tasks.c`)

1. Adds `xTimeIncrement` (`500`) to `*pxPreviousWakeTime` (`0 + 500 = 500`).
2. Removes `vLedPeriodicTask`'s `xStateListItem` from `pxReadyTasksLists[1]`.
3. Sets `xStateListItem.xItemValue = 500` and inserts it into `pxDelayedTaskList` in sorted order (or into `pxOverflowDelayedTaskList` if `xTimeToWake` wrapped past `0xFFFFFFFFUL`).
4. Sets `SCB->ICSR = SCB_ICSR_PENDSVSET_Msk` so `PendSV_Handler` switches the CPU to the next Ready task (`vLedPeriodicTask` is now **`Blocked` using `0% CPU`**!).
5. When `SysTick_Handler` (`xTaskIncrementTick()`) increments `xTickCount` to `500`, it sees `500 >= xNextTaskUnblockTime (500)`, moves `vLedPeriodicTask` from `pxDelayedTaskList` back to `pxReadyTasksLists[1]`, and pends `PendSV`!

```text
vTaskDelay(500) [Drifts by work time 3 ms each cycle]:
  0 ms --(3ms work)--> calls vTaskDelay(500) at 3 ms ---------> wakes at 503 ms -> 1006 ms!

vTaskDelayUntil(&xLastWakeTime, 500) [Zero cumulative drift]:
  0 ms --(3ms work)--> calls vTaskDelayUntil(&last=0, 500) ---> wakes at 500 ms -> 1000 ms!
```

### Step 3 Code: `vLedPeriodicTask` Using `vTaskDelayUntil()`

```c
static volatile uint32_t g_blink_period_ms = 500U;

static void vLedPeriodicTask(void *pvParameters)
{
    (void)pvParameters;

    /* 1. Initialize xLastWakeTime ONCE before entering the loop: */
    TickType_t xLastWakeTime = xTaskGetTickCount();
    bool       led_on        = false;

    for (;;) {
        /* 2. Toggle LD2 (PA5) atomically via GPIOA->BSRR: */
        led_on      = !led_on;
        GPIOA->BSRR = led_on ? (1UL << 5) : (1UL << (5U + 16U));

        /* 3. Sleep in BLOCKED state (0% CPU) until (xLastWakeTime + period_ticks): */
        TickType_t period_ticks = pdMS_TO_TICKS(g_blink_period_ms);
        vTaskDelayUntil(&xLastWakeTime, period_ticks);
    }
}
```

---

## Step 4 — Sleeping On An Interrupt (`ulTaskNotifyTake` + `vTaskNotifyGiveFromISR`)

### How Both Sides of a FreeRTOS Task Notification Work Together

Instead of `vButtonServiceTask` spinning in a loop checking `b1_irq_pending`, we use **both sides** of a FreeRTOS Direct-to-Task Notification:

1. **Consumer Task Side (`vButtonServiceTask`, Task Priority `2`):**
   * Calls **`ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`**:
     * First argument `pdTRUE`: clears the task's `ulNotifiedValue` back to `0` when woken (acts like a binary semaphore).
     * Second argument `portMAX_DELAY`: tells FreeRTOS to keep `vButtonServiceTask` in the **`Blocked` state (`0% CPU`)** until `EXTI15_10_IRQHandler` sends a notification.
2. **Producer ISR Side (`EXTI15_10_IRQHandler`, Hardware Priority `5`):**
   * Clears the STM32 peripheral latch: **`EXTI->PR = (1UL << 13);`**.
   * Initializes `BaseType_t xHigherPriorityTaskWoken = pdFALSE;`.
   * Calls **`vTaskNotifyGiveFromISR(xButtonTaskHandle, &xHigherPriorityTaskWoken)`**:
     * Increments `xButtonTaskHandle->ulNotifiedValue` (`0 -> 1`).
     * Moves `vButtonServiceTask` from **`Blocked` to `Ready`** (`pxReadyTasksLists[2]`).
     * Because `vButtonServiceTask` (`Task Priority 2`) has a **higher priority** than whatever task was running (`vLedPeriodicTask` at `Priority 1` or `Idle` at `Priority 0`), FreeRTOS sets **`xHigherPriorityTaskWoken = pdTRUE`**!
   * Calls **`portYIELD_FROM_ISR(xHigherPriorityTaskWoken)`**:
     * When `xHigherPriorityTaskWoken == pdTRUE`, this macro writes **`SCB->ICSR = SCB_ICSR_PENDSVSET_Msk`** (`bit 28`)!
     * The instant `EXTI15_10_IRQHandler` finishes, `PendSV_Handler` runs and switches the CPU **immediately** to `vButtonServiceTask`!

```text
1. Button Task (Task Prio 2) calls `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`:
   -> `ulNotifiedValue` is 0 -> Task moves from READY to BLOCKED (0% CPU)!
                                         |
2. User presses Button B1 (PC13) --------+
   -> `EXTI15_10_IRQHandler` (HW Prio 5) runs on MSP:
        a. Clears `EXTI->PR = B1_MASK`
        b. Calls `vTaskNotifyGiveFromISR(xButtonTaskHandle, &xHigherPriorityTaskWoken)`
           - Increments `ulNotifiedValue` (0 -> 1)
           - Moves Button Task from BLOCKED -> READY (`pxReadyTasksLists[2]`)
           - Sets `xHigherPriorityTaskWoken = pdTRUE` (because Prio 2 > Prio 0/1)
        c. Calls `portYIELD_FROM_ISR(xHigherPriorityTaskWoken)`
           - Sets `SCB->ICSR = PENDSVSET`!
                                         |
3. `EXTI15_10_IRQHandler` exits -> `PendSV_Handler` (HW Prio 15) runs immediately:
   -> Switches PSP to `vButtonServiceTask` (Task Prio 2)!
```

### Step 4 Code: `vButtonServiceTask()` and `EXTI15_10_IRQHandler()`

```c
static volatile uint32_t g_accepted_press_count = 0U;
static volatile uint32_t g_b1_irq_count         = 0U;

/* Consumer Task (Task Priority 2 — Highest): Sleeps at 0% CPU until B1 interrupt arrives: */
static void vButtonServiceTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastPressTick = 0U;

    for (;;) {
        /* 1. Wait in BLOCKED state (0% CPU) until EXTI15_10_IRQHandler notifies us:
         *    pdTRUE clears ulNotifiedValue back to 0 on wakeup. */
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* 2. Woken by interrupt! Apply 200 ms cooldown check: */
        TickType_t now_tick = xTaskGetTickCount();
        if ((now_tick - xLastPressTick) >= pdMS_TO_TICKS(200U)) {
            xLastPressTick = now_tick;
            ++g_accepted_press_count;

            uint32_t cur = g_blink_period_ms;
            g_blink_period_ms = (cur == 500U) ? 150U : 500U;
        }
    }
}

/* Producer ISR (Hardware Priority 5): Wakes vButtonServiceTask and requests PendSV yield: */
void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & (1UL << 13)) != 0U) {
        /* 1. Clear STM32 EXTI13 peripheral pending latch (Write-1-to-Clear): */
        EXTI->PR = (1UL << 13);
        ++g_b1_irq_count;

        /* 2. Notify vButtonServiceTask (moves it from Blocked -> Ready): */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xButtonTaskHandle != NULL) {
            vTaskNotifyGiveFromISR(xButtonTaskHandle, &xHigherPriorityTaskWoken);
        }

        /* 3. If vButtonServiceTask has higher priority than the interrupted task,
         *    set SCB->ICSR = PENDSVSET so PendSV switches to it immediately on exit: */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
```

---

## Complete FreeRTOS Application Code (`Core/Src/freertos_p1_app.c`)

Here is the complete FreeRTOS application file combining our CMSIS `84 MHz` clock + GPIO + `EXTI13` setup (with **3-resource initialization order from `W02-L21`**: Hardware Clock/GPIO first -> Tasks created second -> `NVIC` interrupt enabled last!) with `vButtonServiceTask` and `vLedPeriodicTask`:

```c
#include <stdbool.h>
#include <stdint.h>
#include "stm32f446xx.h"
#include "FreeRTOS.h"
#include "task.h"

#define LED_MASK    (1UL << 5)    /* PA5  (LD2 Green LED) */
#define B1_MASK     (1UL << 13)   /* PC13 (B1 Pushbutton) */
#define IRQ40_MASK  (1UL << 8)    /* EXTI15_10_IRQn (40)  */
#define POLL_LIMIT  1000000U

#define PRIO_LED_TASK     (tskIDLE_PRIORITY + 1U) /* Task Priority 1 */
#define PRIO_BUTTON_TASK  (tskIDLE_PRIORITY + 2U) /* Task Priority 2 */

uint32_t SystemCoreClock = 16000000U;

static TaskHandle_t      xButtonTaskHandle      = NULL;
static TaskHandle_t      xLedTaskHandle         = NULL;
static volatile uint32_t g_blink_period_ms      = 500U;
static volatile uint32_t g_accepted_press_count = 0U;
static volatile uint32_t g_b1_irq_count         = 0U;
static volatile uint32_t g_fault_code           = 0U;

static _Noreturn void fault_stop(uint32_t code)
{
    __disable_irq();
    g_fault_code = code;
    for (;;) {
        __NOP();
    }
}

static void wait_field(volatile const uint32_t *reg,
                       uint32_t mask, uint32_t expected, uint32_t code)
{
    for (uint32_t n = 0U; n < POLL_LIMIT; ++n) {
        if ((*reg & mask) == expected) {
            return;
        }
    }
    fault_stop(code);
}

/* 1. Configure 84 MHz PLL from 16 MHz HSI (2 Flash wait states): */
static void clock_init_84mhz(void)
{
    RCC->CR |= 1UL;
    wait_field(&RCC->CR, 1UL << 1, 1UL << 1, 2U);
    RCC->CR &= ~(1UL << 24);
    wait_field(&RCC->CR, 1UL << 25, 0U, 3U);

    RCC->APB1ENR |= 1UL << 28;
    (void)RCC->APB1ENR;
    __DSB();
    PWR->CR = (PWR->CR & ~(3UL << 14)) | (1UL << 14);

    FLASH->ACR = (FLASH->ACR & ~15UL) | 2UL;
    wait_field(&FLASH->ACR, 15UL, 2UL, 4U);

    RCC->CFGR = (RCC->CFGR & ~((15UL << 4) | (7UL << 10) | (7UL << 13))) | (4UL << 10);

    const uint32_t pll_mask  = 0x7F437FFFUL;
    const uint32_t pll_value = 16UL | (336UL << 6) | (1UL << 16) | (7UL << 24) | (2UL << 28);
    RCC->PLLCFGR = (RCC->PLLCFGR & ~pll_mask) | pll_value;
    RCC->CR |= 1UL << 24;
    wait_field(&RCC->CR, 1UL << 25, 1UL << 25, 5U);
    wait_field(&PWR->CSR, 1UL << 14, 1UL << 14, 6U);

    RCC->CFGR = (RCC->CFGR & ~3UL) | 2UL;
    wait_field(&RCC->CFGR, 3UL << 2, 2UL << 2, 7U);
    __DSB();
    __ISB();
    SystemCoreClock = 84000000U;
}

/* 2. Resource 1 (Hardware Clocks, PA5 Output, PC13 EXTI13 Routing — NVIC still masked!): */
static void hw_gpio_and_exti_route_init(void)
{
    NVIC->ICER[1] = IRQ40_MASK;
    EXTI->IMR    &= ~B1_MASK;

    RCC->AHB1ENR |= (1UL << 0) | (1UL << 2);
    (void)RCC->AHB1ENR;
    RCC->APB2ENR |= 1UL << 14;
    (void)RCC->APB2ENR;
    __DSB();

    GPIOA->BSRR    = LED_MASK << 16;
    GPIOA->OTYPER  &= ~LED_MASK;
    GPIOA->OSPEEDR &= ~(3UL << 10);
    GPIOA->PUPDR   &= ~(3UL << 10);
    GPIOA->MODER   = (GPIOA->MODER & ~(3UL << 10)) | (1UL << 10);

    GPIOC->MODER   &= ~(3UL << 26);
    GPIOC->PUPDR   = (GPIOC->PUPDR & ~(3UL << 26)) | (1UL << 26);

    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~(15UL << 4)) | (2UL << 4);
    EXTI->EMR  &= ~B1_MASK;
    EXTI->RTSR &= ~B1_MASK;
    EXTI->FTSR |= B1_MASK;                  /* Falling edge on PC13 (button press) */
}

/* 3. Task 1 (Task Priority 1): Periodic LD2 blink using vTaskDelayUntil (0% CPU while waiting): */
static void vLedPeriodicTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    bool       led_on        = false;

    for (;;) {
        led_on      = !led_on;
        GPIOA->BSRR = led_on ? LED_MASK : (LED_MASK << 16);

        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(g_blink_period_ms));
    }
}

/* 4. Task 2 (Task Priority 2 — Highest): Sleeps in ulTaskNotifyTake until B1 ISR notifies it: */
static void vButtonServiceTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastPressTick = 0U;

    for (;;) {
        (void)ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        TickType_t now_tick = xTaskGetTickCount();
        if ((now_tick - xLastPressTick) >= pdMS_TO_TICKS(200U)) {
            xLastPressTick = now_tick;
            ++g_accepted_press_count;

            uint32_t cur = g_blink_period_ms;
            g_blink_period_ms = (cur == 500U) ? 150U : 500U;
        }
    }
}

/* 5. EXTI15_10 ISR (Hardware Priority 5 >= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY): */
void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & B1_MASK) != 0U) {
        EXTI->PR = B1_MASK;                 /* Clear STM32 EXTI13 latch (W1C) */
        ++g_b1_irq_count;

        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xButtonTaskHandle != NULL) {
            vTaskNotifyGiveFromISR(xButtonTaskHandle, &xHigherPriorityTaskWoken);
        }
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

int main(void)
{
    __disable_irq();
    clock_init_84mhz();

    /* Step A (R1): Initialize GPIO and EXTI routing (keep NVIC IRQ 40 masked): */
    hw_gpio_and_exti_route_init();

    /* Step B (R2): Create both FreeRTOS tasks BEFORE enabling the NVIC interrupt: */
    if (xTaskCreate(vLedPeriodicTask, "LED", 256U, NULL,
                    PRIO_LED_TASK, &xLedTaskHandle) != pdPASS) {
        fault_stop(0x10U);
    }
    if (xTaskCreate(vButtonServiceTask, "BTN", 256U, NULL,
                    PRIO_BUTTON_TASK, &xButtonTaskHandle) != pdPASS) {
        vTaskDelete(xLedTaskHandle);        /* Reverse unwind R2a if R2b fails */
        fault_stop(0x11U);
    }

    /* Step C (R3 — LAST!): Set PRIGROUP = 3, set IRQ 40 to HW Priority 5, and unmask: */
    NVIC_SetPriorityGrouping(3U);
    NVIC_SetPriority(EXTI15_10_IRQn, 5U);   /* 0x50: safe for FromISR() APIs   */
    EXTI->PR      = B1_MASK;
    NVIC->ICPR[1] = IRQ40_MASK;
    EXTI->IMR    |= B1_MASK;
    NVIC->ISER[1] = IRQ40_MASK;

    /* Start the FreeRTOS scheduler (creates Idle Task, configures SysTick/PendSV, runs svc #0): */
    vTaskStartScheduler();

    fault_stop(0xFEU);
}
```

---

## Complete Scenario Walkthrough: Task States From Boot Through Two Button Presses (`1000 ms` And `1050 ms`)

Trace the exact state (`Running`, `Ready`, `Blocked`) of all 3 tasks (`vButtonServiceTask` Prio 2, `vLedPeriodicTask` Prio 1, `prvIdleTask` Prio 0) from boot:

1. **At `t = 0 ms` (`vTaskStartScheduler()` runs `svc #0`):**
   * Both `vButtonServiceTask` (Prio 2) and `vLedPeriodicTask` (Prio 1) are `Ready`.
   * Because Prio 2 > Prio 1 > Prio 0, **`vButtonServiceTask` (Prio 2) runs first**!
   * `vButtonServiceTask` immediately calls `ulTaskNotifyTake(pdTRUE, portMAX_DELAY)`:
     * Because `ulNotifiedValue == 0`, `vButtonServiceTask` enters **`Blocked` (`0% CPU`)** and `PendSV` switches to the next highest-priority Ready task: **`vLedPeriodicTask` (Prio 1)**!
   * `vLedPeriodicTask` (Prio 1) turns `LD2` ON (`GPIOA->BSRR`) and calls `vTaskDelayUntil(&xLastWakeTime, 500)`:
     * FreeRTOS sets its wakeup tick to `0 + 500 = 500`, puts `vLedPeriodicTask` into **`Blocked` (`0% CPU`)** in `pxDelayedTaskList`, and `PendSV` switches to **`prvIdleTask` (Prio 0)**!

2. **From `t = 1 ms` to `t = 499 ms`:**
   * `vButtonServiceTask` is **`Blocked`** (waiting for interrupt).
   * `vLedPeriodicTask` is **`Blocked`** (waiting for tick `500`).
   * Only `prvIdleTask` (Prio 0) is `Running`!

3. **At `t = 500 ms` (`SysTick_Handler` fires):**
   * `xTaskIncrementTick()` increments `xTickCount` to `500`, sees `vLedPeriodicTask`'s wakeup time (`500`) has arrived, moves `vLedPeriodicTask` from **`Blocked` to `Ready`** (`pxReadyTasksLists[1]`), and sets `PENDSVSET`.
   * `PendSV_Handler` preempts `prvIdleTask` (Prio 0) and runs **`vLedPeriodicTask` (Prio 1)**.
   * `vLedPeriodicTask` turns `LD2` OFF and calls `vTaskDelayUntil(&xLastWakeTime, 500)` (next wake = `500 + 500 = 1000`), going back to **`Blocked` (`0% CPU`)** in a few microseconds!

4. **First Button Press at `t = 1000 ms`:**
   * You press `B1` (`PC13` falls). `EXTI15_10_IRQHandler` (HW Prio 5) runs on `MSP`:
     * Clears `EXTI->PR = B1_MASK`.
     * Calls `vTaskNotifyGiveFromISR(xButtonTaskHandle, &xHigherPriorityTaskWoken)` -> moves `vButtonServiceTask` (Prio 2) from **`Blocked` to `Ready`**, and sets `xHigherPriorityTaskWoken = pdTRUE`.
     * Calls `portYIELD_FROM_ISR(pdTRUE)` -> sets `SCB->ICSR = PENDSVSET`.
   * `EXTI15_10_IRQHandler` exits into `PendSV_Handler` (HW Prio 15), which immediately switches to **`vButtonServiceTask` (Prio 2)**!
   * `vButtonServiceTask` wakes from `ulTaskNotifyTake()`, checks `(1000 - 0) >= 200` (**TRUE**), sets `xLastPressTick = 1000`, increments `g_accepted_press_count` (`1`), changes `g_blink_period_ms` to **`150U`**, loops around, and goes right back to **`Blocked` (`0% CPU`)** inside `ulTaskNotifyTake()`!

5. **Second Button Press (or Bounce) Within `200 ms` at `t = 1050 ms`:**
   * `EXTI15_10_IRQHandler` notifies `vButtonServiceTask` (Prio 2) -> `PendSV_Handler` runs `vButtonServiceTask`.
   * `vButtonServiceTask` checks `(1050 - 1000) = 50 < 200` (**FALSE** -> ignored!) and goes right back to **`Blocked` (`0% CPU`)** inside `ulTaskNotifyTake()`.

```text
Task State Timeline (t = 0 ms to 1050 ms):

Time (ms) | vButtonServiceTask (Prio 2) | vLedPeriodicTask (Prio 1) | prvIdleTask (Prio 0)
----------+-----------------------------+---------------------------+---------------------
t = 0     | RUNNING -> calls NotifyTake | READY -> RUNNING (LD2 ON) | READY -> RUNNING
          |         -> BLOCKED          |       -> DelayUntil(500)  |
          |                             |       -> BLOCKED          |
t = 1..499| BLOCKED (0% CPU)            | BLOCKED (0% CPU)          | RUNNING
t = 500   | BLOCKED                     | Woken by SysTick ->       | Preempted ->
          |                             | RUNNING (LD2 OFF) ->      | RUNNING again
          |                             | BLOCKED until 1000        |
t = 1000  | Woken by EXTI15_10 ISR!     | Woken at 1000 -> runs     | Preempted ->
(Press 1) | RUNNING: 1000-0 >= 200 OK!  | after Prio 2 blocks ->    | RUNNING again
          | Sets period = 150 -> BLOCKED| DelayUntil(1150)->BLOCKED |
t = 1050  | Woken by EXTI15_10 ISR!     | BLOCKED                   | Preempted ->
(Bounce)  | RUNNING: 1050-1000=50 < 200 |                           | RUNNING again
          | Ignored! -> BLOCKED         |                           |
```
