# W03-L22: FreeRTOS From The Ground Up — Task States, TCB & Lists, Priorities, `vTaskDelayUntil`, And ISR Task Notifications (`P1`)

> **Scope & verification status:** Source-reviewed and repaired 2026-11-17 against the FreeRTOS V10.3.1 task/CM4F port source bundled with STM32CubeF4 V1.28.3. This is a proposed P1 integration, not an installed or running RTOS application. No compilation, execution or timing measurement was performed. Times below are illustrative ticks at a configured 1 kHz.

Previous: [Three-resource initialization and reverse-order unwind (`C02`)](W02_L21_c02_three_resource_init_and_reverse_unwind.md).

---

## The Core Idea: What FreeRTOS Adds On Top Of Our `W01-L19` `PendSV` Switcher

In [W01-L19](W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md), we studied a 2-task context-switcher example using `SVC_Handler` and `PendSV_Handler`. However, it had one big limitation:
* Both `Task 0` (Button) and `Task 1` (LED) ran inside `for (;;)` loops that **never slept**.
* Even when the button was not pressed and the LED did not need to toggle for another `499 ms`, `PendSV_Handler` kept switching between `Task 0` and `Task 1` thousands of times per second (**burning 100% CPU**).

**FreeRTOS uses the same Cortex-M exception/stack-switching mechanisms**, with additional port logic such as per-task `EXC_RETURN` and conditional FP saves. It adds **four kernel mechanisms** in C (`tasks.c`) so Blocked tasks consume no execution time. The Idle task and interrupts still execute; low-power sleep needs separate idle/tickless configuration.

1. **Four Task States (`Running`, `Ready`, `Blocked`, `Suspended`) and Separate Kernel Lists:**
   * `PendSV_Handler` (`vTaskSwitchContext()`) **only** picks tasks that are in the **`Ready` list**.
   * When a task is waiting for time (`500 ms`) or waiting for an interrupt (`B1` button press), FreeRTOS **removes that task from the `Ready` list** and puts it into the **`Blocked` state (`0% CPU`)**!
2. **Task Priority Lists (`pxReadyTasksLists[0 .. configMAX_PRIORITIES - 1]`):**
   * Instead of alternating `0 -> 1 -> 0 -> 1`, FreeRTOS always runs the **highest-priority `Ready` task**.
   * If both the Button Task and the LED Task are `Blocked` (asleep), FreeRTOS runs the built-in **`Idle Task` (`Priority 0`)**.
3. **Sorted Wakeup Lists for `vTaskDelayUntil()` (`pxDelayedTaskList` & `pxOverflowDelayedTaskList`):**
   * When the LED task calls `vTaskDelayUntil(&xLastWakeTime, 500)`, FreeRTOS calculates its scheduled release tick (`xLastWakeTime + 500`). If that tick is still in the future, it moves the task into a delayed list and switches away; a late call does not block.
   * When `SysTick_Handler` (`xTaskIncrementTick()`) reaches that tick, it moves the LED task back to the **`Ready` List**!
4. **Built-In Task Notifications (`ulTaskNotifyTake` + `vTaskNotifyGiveFromISR`):**
   * Every FreeRTOS task has a built-in 32-bit notification counter (`ulNotifiedValue`) inside its Task Control Block (`TCB`).
   * If its counter is zero, the Button task waits in `ulTaskNotifyTake()`. An ISR notification then makes it **Ready**. `portYIELD_FROM_ISR()` requests a switch when appropriate; the task runs when interrupt masks, active interrupts and scheduling permit. A notification received before the wait remains pending, so the next take can return without blocking.

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
| **`ListItem_t xStateListItem`** | Linked-list node for the task's **current state** | Placed in one state list: ready, delayed/overflow-delayed, or suspended list. The suspended list also stores indefinite event waits; list membership alone does not distinguish an explicitly Suspended task from a Blocked one. |
| **`ListItem_t xEventListItem`** | Linked-list node for **event waiting** | When a task blocks on a Queue or Mutex with a timeout, `xStateListItem` sits in `pxDelayedTaskList` (for the timeout) while `xEventListItem` sits in the Queue's waiting list! |
| **`UBaseType_t uxPriority`** | Task priority (`0` = lowest, `configMAX_PRIORITIES - 1` = highest) | Tells the scheduler which `pxReadyTasksLists[uxPriority]` bucket this task belongs to. |
| **`StackType_t *pxStack`** | Lowest address of the task's stack array (`&stack[0]`) | Used by `configCHECK_FOR_STACK_OVERFLOW` to detect if `pxTopOfStack` grew below `pxStack`. |
| **`uint32_t ulNotifiedValue`** & **`uint8_t ucNotifyState`** | Built-in 32-bit notification counter and state (`NotWaiting`, `Waiting`, `Received`) | Allows an ISR (`vTaskNotifyGiveFromISR`) to wake this task directly without allocating a separate Queue or Semaphore! |

---

### 2. How a Task Moves Between the 4 FreeRTOS States

1. **`Ready`:** The task is able to run, and its `xStateListItem` is inside `pxReadyTasksLists[uxPriority]`, waiting only because a higher- or equal-priority task is currently using the CPU.
2. **`Running`:** The task currently owns the CPU (`pxCurrentTCB` points to this task's `TCB`, and the CPU's `PSP` points to this task's stack). On our single-core Cortex-M4, **at most one task is `Running` at any instant**.
3. **`Blocked` (`0% CPU`):** A waiting API could not complete immediately and has a nonzero wait. Its state node leaves the ready list for a delayed list or, for an indefinite wait, the suspended list. Queue/semaphore waits also use the separate event node; notifications use their TCB state. A pending notification, available queue item, zero timeout or expired periodic release need not block.
4. **`Suspended` (`0% CPU`):** Explicitly paused by `vTaskSuspend()`; resumed with `vTaskResume()` or the ISR-safe `xTaskResumeFromISR()`. Resume is not a queued event and is not a replacement for notifications.

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
   * With `configUSE_PORT_OPTIMISED_TASK_SELECTION = 1`, the Cortex-M4 port uses a ready-priority bitmap and a **`clz` (Count Leading Zeros)** based calculation: `31 - __CLZ(uxTopReadyPriority)`. Finding a bitmap position is only part of selection; scheduling and context switching take additional instructions.

2. **Hardware NVIC Interrupt Priorities (`NVIC_SetPriority` on STM32F446):**
   * **Smaller number = Higher urgency!**
   * `Hardware Priority 0` is the **highest** urgency; `Hardware Priority 15` (`0xF0`) is the **lowest** urgency.
   * An enabled, pending ISR can preempt Thread mode regardless of the task's `uxPriority`, when `PRIMASK`/`BASEPRI` and exception-priority rules permit it.

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
| HW Priority 0 .. 4  (0x00..0x40): Not masked by this BASEPRI threshold (0x50)    |
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

Use the following as a configuration excerpt, not a complete configuration
file. For this example also select preemption, `configMAX_PRIORITIES >= 3`,
`configUSE_16_BIT_TICKS = 0`, `configTICK_RATE_HZ = 1000`,
`configCPU_CLOCK_HZ = SystemCoreClock`, task notifications, dynamic allocation,
and `INCLUDE_vTaskDelayUntil`, `INCLUDE_vTaskDelete`, `INCLUDE_vTaskSuspend`
all enabled. `INCLUDE_vTaskSuspend = 1` makes `portMAX_DELAY` an indefinite
wait; otherwise it is a finite maximum tick count. Enable a working
`configASSERT` for interrupt-priority and configuration checks.

```c
/* Key settings in FreeRTOSConfig.h for STM32F446RE (4 priority bits -> PRIGROUP = 3): */
#define configPRIO_BITS                               4U
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY       15U  /* PendSV & SysTick at HW Prio 15 */
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5U   /* ISRs calling FromISR: 5..15!   */

#define configKERNEL_INTERRUPT_PRIORITY \
    (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8U - configPRIO_BITS))      /* 0xF0 */
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8U - configPRIO_BITS)) /* 0x50 */

/* Startup excerpt: the receiver and scheduler must be ready before IRQ delivery. */
static void b1_exti_irq_priority_init(void)
{
    /* Set PRIGROUP = 3 (all 4 bits are preemption priority; required by FreeRTOS CM4F): */
    NVIC_SetPriorityGrouping(3U);

    /* Set IRQ 40 to logical priority 5 (>= configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY): */
    NVIC_SetPriority(EXTI15_10_IRQn, 5U); /* Writes (5U << 4) = 0x50 into NVIC->IP[40] */
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    /* Enable IRQ 40 only after its receiver/startup contract is established. */
}
```

---

## Step 3 — Periodic Work Without Drift (`vTaskDelayUntil` vs. `vTaskDelay`)

### Why Do Periodic Tasks Use `vTaskDelayUntil()` Instead of `vTaskDelay()`?

Suppose our LED task (`vLedPeriodicTask`) wants releases every `500` or `150`
ticks. For this illustrative 1 kHz trace, work/preemption consumes `3` ticks,
and no further scheduling delay occurs when the task becomes Ready:

* **If you call `vTaskDelay(500)` (Relative Delay):**
  * `vTaskDelay(500)` tells FreeRTOS: *"Wake me `500` ticks **from the moment I call `vTaskDelay()`** (`wake_time = xTickCount + 500`)."*
  * Because `3 ms` already passed before `vTaskDelay(500)` was called, the task wakes at `503 ms`, then `1006 ms`, then `1509 ms`—drifting later on every single cycle!
* **If you call `vTaskDelayUntil(&xLastWakeTime, period_ticks)` (Scheduled Periodic Release):**
  * Before the `for (;;)` loop starts, you initialize `xLastWakeTime = xTaskGetTickCount();` once (for example, `0`).
  * Inside the loop, `vTaskDelayUntil(&xLastWakeTime, 500)` calculates the next wakeup from the **previous scheduled wakeup time**:
    $$\text{xTimeToWake} = \text{xLastWakeTime} + \text{period\_ticks}$$
    and updates `xLastWakeTime = xTimeToWake` (`0 -> 500 -> 1000 -> 1500`).
  * Even if the task body took `3 ms` (so `xTickCount` is currently `3`), `xTimeToWake` is still `0 + 500 = 500`!

### What `vTaskDelayUntil()` Does Inside FreeRTOS (`tasks.c`)

Steps 2-5 describe a future release. The API always advances the saved schedule,
but if the release is already due it does not put the task in a delayed list.

1. Adds `xTimeIncrement` (`500`) to `*pxPreviousWakeTime` (`0 + 500 = 500`).
2. Removes `vLedPeriodicTask`'s `xStateListItem` from `pxReadyTasksLists[1]`.
3. Sets `xStateListItem.xItemValue = 500` and inserts it into `pxDelayedTaskList` in sorted order (or into `pxOverflowDelayedTaskList` if `xTimeToWake` wrapped past `0xFFFFFFFFUL`).
4. Sets `SCB->ICSR = SCB_ICSR_PENDSVSET_Msk` so `PendSV_Handler` switches the CPU to the next Ready task (`vLedPeriodicTask` is now **`Blocked` using `0% CPU`**!).
5. When the tick reaches `500`, `xTaskIncrementTick()` moves the task to Ready. In this example it outranks Idle, so the port requests a switch. Actual execution can be later because of ISRs, masks or higher-priority tasks.

**Late-work example:** `last = 1000`, `period = 150`, `now = 1200` gives
`next = 1150`. The call updates `last` to `1150` and returns without blocking.
Repeated late calls can produce catch-up iterations. Choose catch-up versus
skipping/rebasing explicitly for a real cyclic service. Require
`pdMS_TO_TICKS(period_ms) > 0`; conversion truncates and a sub-tick period can
be zero. A changed blink period affects the next delay calculation, not an
already-blocked delay.

```text
vTaskDelay(500) [Drifts by work time 3 ms each cycle]:
  0 ms --(3ms work)--> calls vTaskDelay(500) at 3 ms ---------> wakes at 503 ms -> 1006 ms!

vTaskDelayUntil(&xLastWakeTime, 500) [No work-time drift in scheduled releases]:
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

        /* 3. Block if the next scheduled release is future; otherwise continue. */
        TickType_t period_ticks = pdMS_TO_TICKS(g_blink_period_ms);
        vTaskDelayUntil(&xLastWakeTime, period_ticks);
    }
}
```

---

## Step 4 — Sleeping On An Interrupt (`ulTaskNotifyTake` + `vTaskNotifyGiveFromISR`)

### How Both Sides of a FreeRTOS Task Notification Work Together

The following trace assumes a zero notification counter and a task currently
waiting in `ulTaskNotifyTake()`. A give increments the counter even when the
task is Ready, Running or waiting for something else; it only unblocks a
notification wait. `pdTRUE` clears the accumulated count on a successful take,
coalescing several edges. A finite timeout returns zero; this example uses the
indefinite-wait configuration above.

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
     * PendSV switches to the highest-priority Ready task when exception priorities and masks permit it. In this trace, that is `vButtonServiceTask`.

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
3. `EXTI15_10_IRQHandler` exits -> when eligible, `PendSV_Handler` (HW Prio 15) runs:
   -> Switches PSP to `vButtonServiceTask` (Task Prio 2)!
```

### Step 4 Code: `vButtonServiceTask()` and `EXTI15_10_IRQHandler()`

**Behavior contract:** This is a notification/cooldown demonstration. It accepts
at most one serviced edge per `200` ticks, suppresses edges during the first
`200` ticks after boot, and coalesces pending notifications. A held-low button
at boot produces no new falling edge. Release bounce can also produce a falling
edge, so this is not the stable-press/stable-release algorithm in
[L14](W01_L14_b1_button_polling_and_nonblocking_debounce.md). The timestamp is
the task's service time, not the physical edge time. Production button handling
must carry over the earlier debounce/rearm contract.

`g_blink_period_ms` is one naturally aligned 32-bit scalar, with one writer and
one reader on this single-core Cortex-M4/GCC target. Each access is one
word load/store; the LED task can see either period during a change. `volatile`
keeps these accesses observable; it does not protect a multi-field invariant or
make this a portable multi-threaded C synchronization pattern. Use queues or
properly scoped locking for the shared records introduced in L23/L24.

```c
static volatile uint32_t g_accepted_press_count = 0U;
static volatile uint32_t g_b1_irq_count         = 0U;

/* Consumer Task (Task Priority 2): blocks when the notification count is zero. */
static void vButtonServiceTask(void *pvParameters)
{
    (void)pvParameters;
    TickType_t xLastPressTick = 0U;

    for (;;) {
        /* 1. Consume pending notifications, or wait if the count is zero.
         *    pdTRUE clears the accumulated count on a successful take. */
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

        /* 2. Increment its notification count; unblock a notification wait. */
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        if (xButtonTaskHandle != NULL) {
            vTaskNotifyGiveFromISR(xButtonTaskHandle, &xHigherPriorityTaskWoken);
        }

        /* 3. If vButtonServiceTask has higher priority than the interrupted task,
         *    request PendSV; it runs when exception priorities and masks permit: */
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}
```

---

## Complete Application-Side Listing (`Core/Src/freertos_p1_app.c`)

Here is the complete FreeRTOS application file combining our CMSIS `84 MHz` clock + GPIO + `EXTI13` setup (with **3-resource initialization order from `W02-L21`**: Hardware Clock/GPIO first -> Tasks created second -> `NVIC` interrupt enabled last!) with `vButtonServiceTask` and `vLedPeriodicTask`:

**Build integration contract:** Use the vendor STM32F446 startup, linker script,
CMSIS headers and `system_stm32f4xx.c` (the sole definition of `SystemCoreClock`).
Enter this `main()` from reset on HSI, not after another routine already switched
to PLL. Include the V10.3.1 kernel, `portable/GCC/ARM_CM4F` port and exactly one
compatible heap implementation (`heap_4.c` for this dynamic-allocation example).
Provision heap and task-stack sizes from build/runtime evidence; `256` is
`StackType_t` words (`1024 bytes` on this port), not a measured sufficient budget.
Map `vPortSVCHandler` to `SVC_Handler`, `xPortPendSVHandler` to `PendSV_Handler`,
and `xPortSysTickHandler` to `SysTick_Handler` in the build configuration. Keep
one definition of each vector handler, one `main()` and one tick owner; do not
link the earlier custom switcher or HAL SysTick handler alongside them.
Resolve optional hooks and timer-service requirements against the selected
configuration before a build.

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

/* SystemCoreClock is supplied by the vendor system_stm32f4xx.c. */

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

## Corrections

- 2026-11-17: Approved source-only repair: pinned the reference/configuration,
  clarified task-state/list/notification semantics, scheduled versus actual
  execution and overruns, labelled cooldown limits, removed unsupported timing
  claims, and specified application integration and symbol ownership.
- Source anchors: CubeF4 V1.28.3 FreeRTOS `include/task.h` identifies V10.3.1;
  `tasks.c:vTaskDelayUntil` contains the future-release guard;
  `portable/GCC/ARM_CM4F/port.c:vPortSVCHandler` restores PSP and EXC_RETURN.
  The local `queue.c` and `portmacro.h` reads were sandbox-blocked during this
  review; their complete implementations were not inspected.

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
   * `vLedPeriodicTask` turns `LD2` OFF and calls `vTaskDelayUntil(&xLastWakeTime, 500)` (next release = `500 + 500 = 1000`), going back to **`Blocked` (`0% CPU`)**. Execution duration remains unmeasured.

4. **First Button Press at `t = 1000 ms`:**
   * You press `B1` (`PC13` falls). `EXTI15_10_IRQHandler` (HW Prio 5) runs on `MSP`:
     * Clears `EXTI->PR = B1_MASK`.
     * Calls `vTaskNotifyGiveFromISR(xButtonTaskHandle, &xHigherPriorityTaskWoken)` -> moves `vButtonServiceTask` (Prio 2) from **`Blocked` to `Ready`**, and sets `xHigherPriorityTaskWoken = pdTRUE`.
     * Calls `portYIELD_FROM_ISR(pdTRUE)` -> sets `SCB->ICSR = PENDSVSET`.
   * When eligible, `PendSV_Handler` (HW Prio 15) switches to **`vButtonServiceTask` (Prio 2)**.
   * `vButtonServiceTask` wakes from `ulTaskNotifyTake()`, checks `(1000 - 0) >= 200` (**TRUE**), sets `xLastPressTick = 1000`, increments `g_accepted_press_count` (`1`), changes `g_blink_period_ms` to **`150U`**, loops around, and goes right back to **`Blocked` (`0% CPU`)** inside `ulTaskNotifyTake()`!

5. **Second Button Press (or Bounce) Within `200 ms` at `t = 1050 ms`:**
   * `EXTI15_10_IRQHandler` notifies `vButtonServiceTask` (Prio 2) -> `PendSV_Handler` runs `vButtonServiceTask`.
   * `vButtonServiceTask` checks `(1050 - 1000) >= 200`, or `50 >= 200` (**FALSE** -> ignored!), then waits again in `ulTaskNotifyTake()`.

```text
Illustrative timeline: the button is serviced before LED runs at tick 1000.
If LED runs first, it schedules one more 500-tick interval instead.
No other interrupts or higher-priority work delay these example executions.

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
