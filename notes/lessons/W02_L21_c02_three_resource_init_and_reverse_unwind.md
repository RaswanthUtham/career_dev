# W02-L21: Three-Resource Initialization And Reverse-Order Unwind (`C02`) — Linux Kernel (`goto`), FreeRTOS, And AUTOSAR / MISRA-C

> **Scope & verification status:** Source-reviewed and repaired 2026-11-17. C02 teaches P1 resource dependencies and reverse unwind. Linux-style and automotive-style code are bounded comparisons, not additional curricula or compliance claims. The standalone host harness has been inspected, not compiled or run; the learner selected source review only.

Previous: [BSP — Board Support Package or Bootstrap Processor](W01_L20_bsp_board_support_and_bootstrap_processor.md).

---

## The Core Problem: Why Acquire In Forward Order (`R1 -> R2 -> R3`) And Unwind In Reverse Order (`R3 -> R2 -> R1`)?

When you initialize a real peripheral driver (such as our `P1` sensor/fault acquisition service on `STM32F446RE`), you cannot turn everything on in a random order. A driver acquires **3 dependent resources in strict forward order (`R1 -> R2 -> R3`)**:

1. **Resource 1 (`R1` — Hardware Clock & GPIO Pins):**
   * Enable the peripheral clock in `RCC` and configure the GPIO pins.
   * **Why 1st:** If the peripheral clock is off, writes to the peripheral's registers are ignored by hardware.
2. **Resource 2 (`R2` — Memory Buffer / Queue / DMA Channel):**
   * Allocate or claim the receive buffer / FreeRTOS queue (`rx_queue`) that the interrupt handler will write into.
   * **Why 2nd:** The buffer/queue **must exist in RAM before any interrupt is allowed to fire**!
3. **Resource 3 (`R3` — Interrupt / Periodic Trigger):**
   * Enable the hardware interrupt in `EXTI` and `NVIC` (and start the worker task/timer).
   * **Why 3rd (Last):** The interrupt is the **producer**. You only open the interrupt gate after `R1` (hardware) and `R2` (buffer/queue) are 100% ready.

---

### What Happens If `R3` Fails During `init()`, or When `deinit()` Shuts the Driver Down?

Suppose `init()` acquires `R1` (Clock/GPIO) and `R2` (Queue/Buffer), and then **`R3` (Interrupt/Task setup) fails**:
* You must undo **only what was already acquired (`R2`, then `R1`)** so the system does not leak memory or leave hardware clocks/pins active.
* You must **NOT** release `R3` if its acquire function failed without leaving any owned resource behind.

**Contract used by all examples:** Each acquire helper is failure-atomic: it
either succeeds completely or cleans up its own partial work before returning
failure. Otherwise the caller needs explicit partial-ownership information.
Pass a valid, initialized, exclusively managed context; do not initialize an
already-live instance. Release helpers return only after their users have
stopped. The simple `deinit()` functions require successful initialization and
one shutdown; only the ownership-mask variant skips already-released resources.

And what if all 3 succeed (`R1`, `R2`, `R3`), and later `deinit()` shuts the driver down?
* You **MUST** release them in **strict reverse order (`R3 -> R2 -> R1` — Last-In, First-Out `LIFO`)**:
  1. **First, stop `R3` (Interrupt/Trigger and its users):** Prevent new work and establish quiescence: all DMA, workers and callbacks have stopped accessing the resources.
  2. **Second, free `R2` (Buffer/Queue):** Once no user can access it, free/delete the queue or DMA buffer. An interrupt mask alone does not stop DMA or deferred work.
  3. **Third, disable `R1` (Clock/GPIO):** Finally, gate off the peripheral clock and reset the pins.

* **Otherwise (What Fails If You Unwind in Forward Order `R1 -> R2 -> R3`?):**
  * **Crash 1 (Clock Gated While ISR Active):** If you disable `R1` (Peripheral Clock) while `R3` (Interrupt) is still enabled, an interrupt firing a microsecond later tries to read/clear peripheral registers whose clock is OFF -> **bus hang or infinite ISR loop**!
  * **Crash 2 (Use-After-Free in ISR):** If you free `R2` (Buffer/Queue) while `R3` (Interrupt) is still enabled, an interrupt firing a microsecond later writes into **freed memory (`HardFault` / RAM corruption)**!

```text
FORWARD ACQUISITION ORDER (init):
  [R1: Clock & GPIO] ---> [R2: Buffer / Queue] ---> [R3: Enable Interrupt (Last!)]

REVERSE UNWIND ORDER (on init failure OR deinit shutdown):
  [R3: Stop + quiesce all users] ---> [R2: Free Buffer / Queue] ---> [R1: Disable Clock & GPIO]

  If R1 fails: Undo nothing (return error immediately)
  If R2 fails: Undo R1 only
  If R3 fails: Undo R2 first, then undo R1
```

Below are three ways to express this contract: **Linux-style C (`goto`)**,
**FreeRTOS (`P1`)**, and **a project rule that avoids `goto` in MISRA-oriented C**.

---

## Method 1 — How It Is Done In The Linux Kernel (Reverse-Order `goto` Ladder)

### How the Linux Kernel `goto` Ladder Works (Step-by-Step)

In the Linux kernel (`Documentation/process/coding-style.rst`, Chapter 7), driver `probe()` and `init()` functions use **named `goto` labels stacked in reverse order** at the bottom of the function:

1. **Acquire `R1` (Clock & GPIO):**
   * If `acquire_r1()` fails, nothing has been acquired yet -> **`return -EIO;` directly** (no `goto` needed).
2. **Acquire `R2` (Buffer / DMA):**
   * If `acquire_r2()` fails, `R1` is already active and must be undone -> **`goto err_release_r1;`**.
3. **Acquire `R3` (IRQ):**
   * If `acquire_r3()` fails, both `R2` and `R1` are active -> **`goto err_release_r2;`**.
4. **How the Reverse Labels Fall Through at the Bottom:**
   * `err_release_r2:` releases `R2`, and then **falls straight through** to `err_release_r1:`, which releases `R1` and returns the negative error code (`ret`)!
   * **Rule for Label Names:** Name each label after **what that label undoes** (`err_release_r2:`, `err_release_r1:`), never after what failed (`err_r3_failed:`), so anyone reading the `goto` sees immediately which cleanup step runs first.
   * A shared `cleanup` label is also valid when it checks reliable ownership flags or stages. A reverse-label ladder makes the successfully acquired subset explicit without those extra checks.

```text
Linux Kernel Reverse `goto` Ladder Flow:

  1. acquire_r1() fails? -------------------------> return ret; (Nothing to undo)
  2. acquire_r2() fails? ------------+
  3. acquire_r3() fails? ---+        |
  4. All 3 succeeded!       |        |
     return 0;              |        |
                            v        |
     err_release_r2:  <-----+        |
       release_r2();                 |
       /* falls through! */          |
     err_release_r1:  <--------------+
       release_r1();
       return ret;
```

### Method 1 Code: Linux Kernel `probe()` / `init()` Reverse `goto` Pattern

```c
#include <stdbool.h>
#include <stdint.h>

#define EIO     5
#define ENOMEM  12
#define EBUSY   16

typedef struct {
    bool     r1_clk_gpio_owned;
    uint8_t *r2_dma_buf;
    bool     r3_irq_enabled;
} linux_drv_ctx_t;

/* Simulated hardware/resource primitives: */
int      hw_clk_gpio_enable(linux_drv_ctx_t *ctx);
void     hw_clk_gpio_disable(linux_drv_ctx_t *ctx);
uint8_t *dma_buf_alloc(void);
void     dma_buf_free(linux_drv_ctx_t *ctx, uint8_t *buf);
int      irq_line_request(linux_drv_ctx_t *ctx);
void     irq_line_free(linux_drv_ctx_t *ctx);

/* Linux kernel style init: returns 0 on success, negative -ERRNO on failure: */
int linux_style_driver_init(linux_drv_ctx_t *ctx)
{
    int ret;

    /* 1. Acquire Resource 1 (Clock & GPIO):
     *    If R1 fails, nothing was acquired yet -> return immediately: */
    ret = hw_clk_gpio_enable(ctx);
    if (ret < 0) {
        return ret;
    }

    /* 2. Acquire Resource 2 (DMA Buffer):
     *    If R2 fails, only R1 is owned -> jump to err_disable_clk: */
    ctx->r2_dma_buf = dma_buf_alloc();
    if (ctx->r2_dma_buf == (uint8_t *)0) {
        ret = -ENOMEM;
        goto err_disable_clk;
    }

    /* 3. Acquire Resource 3 (Enable IRQ — LAST, after R1 and R2 are ready!):
     *    If R3 fails, R2 and R1 are owned -> jump to err_free_buf: */
    ret = irq_line_request(ctx);
    if (ret < 0) {
        goto err_free_buf;
    }

    return 0; /* All 3 resources acquired! */

/* Reverse-order unwind ladder (LIFO: R2 first, then R1): */
err_free_buf:
    dma_buf_free(ctx, ctx->r2_dma_buf);
    ctx->r2_dma_buf = (uint8_t *)0;

err_disable_clk:
    hw_clk_gpio_disable(ctx);
    return ret;
}

/* Normal shutdown (remove/deinit) unwinds in the exact same reverse order (R3 -> R2 -> R1): */
void linux_style_driver_deinit(linux_drv_ctx_t *ctx)
{
    irq_line_free(ctx);                 /* 1. R3 first: stop IRQ before freeing buffer! */
    dma_buf_free(ctx, ctx->r2_dma_buf); /* 2. R2 second: free DMA buffer                */
    ctx->r2_dma_buf = (uint8_t *)0;
    hw_clk_gpio_disable(ctx);           /* 3. R1 last: gate off peripheral clock        */
}
```

---

## Method 2 — How It Is Done In FreeRTOS (`P1` On STM32 Cortex-M4)

### What Changes in FreeRTOS on an MCU?

In a FreeRTOS driver on `STM32F446RE` (`P1`), the 3 resources are typically:
1. **`R1` (STM32 Hardware Clock & Pins):** Enable `RCC` clock and configure `GPIO` / `EXTI` routing (with the `NVIC` interrupt **still masked**!).
2. **`R2` (FreeRTOS Queue / Ring Buffer):** Create the queue (`xQueueCreate`) that holds sensor/fault frames between the ISR and the task.
3. **`R3` (FreeRTOS Worker Task + `NVIC` Interrupt Enable):** Create the worker task (`xTaskCreate`) **and only after both the queue and task exist, unmask the hardware interrupt in `NVIC` (`NVIC_EnableIRQ`)**!

### Two Critical Staff-Level FreeRTOS Rules

1. **Never enable the `NVIC` interrupt before `xQueueCreate()` and `xTaskCreate()` succeed:**
   * If you enable `NVIC_EnableIRQ(EXTI15_10_IRQn)` in Step 1, and an interrupt fires *before* `xQueueCreate()` runs in Step 2, the ISR calls `xQueueSendFromISR(NULL, ...)` and **crashes immediately (`HardFault` / `configASSERT`)**!
2. **During `deinit()`, stop new work and quiesce every user BEFORE calling `vTaskDelete()` or `vQueueDelete()`:**
   * Masking the hardware interrupt is one step. `vTaskDelete()` does not release application allocations, mutexes or DMA ownership. A worker must acknowledge shutdown and park without holding these resources before another task deletes it.
   * Calling `vQueueDelete(ctx->queue)` while an ISR, worker or callback can still use it is a use-after-free.

```text
FreeRTOS Driver Init & Unwind Order on STM32 Cortex-M4:

  Init Order:
    R1: Enable RCC Clock + GPIO (keep NVIC IRQ masked!)
     |
    R2: ctx->queue = xQueueCreate(8, sizeof(uint32_t))
     |
    R3: xTaskCreate(..., &ctx->task) -> if OK, NVIC_EnableIRQ(IRQ40) LAST!
        (If xTaskCreate fails -> vQueueDelete(R2) -> Disable Clock/GPIO(R1))

  DeInit Order (Reverse LIFO):
    1. Mask IRQ -> bounded stop/quiesce -> delete parked worker          [Stop R3 first!]
       On quiesce failure: report failure; retain queue, task and hardware.
    2. vQueueDelete(ctx->queue)                                             [Free R2 second!]
    3. Disable RCC Clock + Reset GPIO                                       [Stop R1 last!]
```

### Method 2 Code: FreeRTOS Driver Init and Reverse Unwind

This is interface pseudocode; a real build uses `FreeRTOS.h`, `task.h` and
`queue.h`, not the representative declarations below. Run init from task
context with the scheduler running and a zero-initialized, non-live context.
`xTaskCreate()` may immediately run the worker: its first action must be to wait
on the already-created queue, without using the not-yet-published task handle.
The caller serializes init/deinit. Deinit runs in a separate control task,
never in the worker being deleted or in an ISR.

`p1_stop_and_quiesce()` is a required driver-specific helper: stop triggers,
complete/cancel DMA, drain or explicitly discard owned work, and obtain a
bounded worker acknowledgment that it is parked and holds no resources.
On failure, return `false` and keep resources allocated for recovery. C04 will
develop that shutdown protocol; this lesson shows where it belongs.

```c
#include <stdbool.h>
#include <stdint.h>

/* Representative FreeRTOS types & return codes for our P1 driver contract: */
typedef void    *QueueHandle_t;
typedef void    *TaskHandle_t;
typedef int32_t  BaseType_t;
#define pdPASS   1
#define pdFAIL   0

typedef struct {
    bool          hw_clk_enabled;       /* R1: STM32 RCC + GPIO state */
    QueueHandle_t evt_queue;            /* R2: FreeRTOS Queue handle  */
    TaskHandle_t  worker_task;          /* R3: FreeRTOS Task + NVIC   */
    bool          irq_enabled;
} freertos_drv_ctx_t;

bool          stm32_hw_acquire(freertos_drv_ctx_t *ctx);
void          stm32_hw_release(freertos_drv_ctx_t *ctx);
QueueHandle_t xQueueCreate(uint32_t length, uint32_t item_size);
void          vQueueDelete(QueueHandle_t q);
BaseType_t    xTaskCreate(void (*fn)(void *), const char *name,
                          uint16_t stack_words, void *arg,
                          uint32_t prio, TaskHandle_t *out_handle);
void          vTaskDelete(TaskHandle_t t);
void          stm32_irq_enable(freertos_drv_ctx_t *ctx);
void          stm32_irq_disable_sync(freertos_drv_ctx_t *ctx);
bool          p1_stop_and_quiesce(freertos_drv_ctx_t *ctx);
void          p1_worker_task(void *arg);

bool freertos_driver_init(freertos_drv_ctx_t *ctx)
{
    /* 1. R1: Enable STM32 peripheral clock & pins (NVIC IRQ stays disabled!): */
    if (!stm32_hw_acquire(ctx)) {
        return false;                   /* R1 failed: nothing to unwind */
    }

    /* 2. R2: Create FreeRTOS event queue (8 items of uint32_t): */
    ctx->evt_queue = xQueueCreate(8U, sizeof(uint32_t));
    if (ctx->evt_queue == (QueueHandle_t)0) {
        stm32_hw_release(ctx);          /* Unwind R1 */
        return false;
    }

    /* 3. R3: Create worker task; only enable NVIC IRQ after task creation succeeds: */
    if (xTaskCreate(p1_worker_task, "P1Svc", 256U, ctx, 2U, &ctx->worker_task) != pdPASS) {
        vQueueDelete(ctx->evt_queue);   /* Reverse unwind: R2 first */
        ctx->evt_queue = (QueueHandle_t)0;
        stm32_hw_release(ctx);          /* Reverse unwind: R1 second */
        return false;
    }

    /* Both Queue (R2) and Task (R3) are ready -> safe to unmask NVIC interrupt now: */
    stm32_irq_enable(ctx);
    return true;
}

bool freertos_driver_deinit(freertos_drv_ctx_t *ctx)
{
    /* 1. Stop R3: masking alone is insufficient; every owner must be quiescent. */
    stm32_irq_disable_sync(ctx);
    if (!p1_stop_and_quiesce(ctx)) {
        return false;                  /* Caller records failure; ownership retained. */
    }
    if (ctx->worker_task != (TaskHandle_t)0) {
        vTaskDelete(ctx->worker_task);
        ctx->worker_task = (TaskHandle_t)0;
    }

    /* 2. Free R2 second: delete queue now that neither ISR nor task can touch it: */
    if (ctx->evt_queue != (QueueHandle_t)0) {
        vQueueDelete(ctx->evt_queue);
        ctx->evt_queue = (QueueHandle_t)0;
    }

    /* 3. Release R1 last: gate off STM32 peripheral clock & pins: */
    stm32_hw_release(ctx);
    return true;
}
```

---

## Method 3 — How It Is Done When Project Rules Avoid `goto` (MISRA-Oriented C)

### Why Do Some Automotive Projects Avoid `goto`?

In automotive firmware (AURIX / AUTOSAR Classic OS / ISO 26262 with **MISRA C:2012**):
* **MISRA C:2012 Rule 15.1 is Advisory:** it recommends avoiding `goto`.
* **MISRA C:2012 Rule 15.5** (Advisory, often enforced by company guidelines) also prefers a **single point of exit (`return`)** at the end of a function so static analysis tools (Polyspace, LDRA, QA-C) can verify all control-flow paths easily.
* The project's adopted rules and compliance process determine enforcement.
  AUTOSAR or ISO 26262 alone does not make every `goto` forbidden. Avoiding
  `goto` and using one return do not establish whole-program MISRA compliance.

### How Do We Write Clean Reverse Unwind Without `goto` (And Without Deep Nested `if` Pyramids)?

If you ban `goto` and write 4 or 5 nested `if` blocks (`if (r1) { if (r2) { if (r3) { ... } else { undo_r2(); } } else { undo_r1(); } }`), the code turns into an unreadable pyramid and duplicates the teardown calls in two places (`Init` and `DeInit`).

At Staff/Principal level in AUTOSAR Complex Device Drivers (`CDD`), we solve this cleanly using **two techniques together**:
1. **Sequential Guarded Acquisition (`status == E_OK`):**
   * Each step `k` only runs `if (status == E_OK)` and records its ownership bit in `ctx->owned_mask` (or increments `ctx->owned_stage`) **only when that step succeeds**.
2. **One Shared Reverse-Order Unwind Function (`Cdd_UnwindOwnedResources(ctx)`):**
   * Checks the ownership flags in **strict reverse order (`R3 -> R2 -> R1`)** and releases **only** the resources whose ownership bit is set!
   * If `Cdd_Init()` fails at any step (`status != E_OK`), it calls `Cdd_UnwindOwnedResources(ctx)` at the single exit point.
   * And when `Cdd_DeInit()` is called later to shut down the driver, `Cdd_DeInit()` calls **the exact same `Cdd_UnwindOwnedResources(ctx)` function**!

* **Why this pattern is superior for MISRA / AUTOSAR:**
  * **Zero `goto` statements** (passes MISRA Rule 15.1).
  * **Single `return status;` exit** (passes MISRA Rule 15.5).
  * **Zero duplicated teardown code** between `Cdd_Init()` failure and `Cdd_DeInit()`.
  * **Reduced nesting depth** (no deeply nested `if/else` pyramid). Cyclomatic complexity counts control-flow decisions; flatter indentation does not remove those decisions.

```text
AUTOSAR / MISRA-C `goto`-Free Pattern:

  Cdd_Init(ctx):
    status = E_OK;
    Step 1: Acquire R1 -> if OK, set bit CDD_OWN_R1 (0x01); else status = E_NOT_OK
    Step 2: if (status == E_OK) Acquire R2 -> if OK, set bit CDD_OWN_R2 (0x02); else status = E_NOT_OK
    Step 3: if (status == E_OK) Acquire R3 -> if OK, set bit CDD_OWN_R3 (0x04); else status = E_NOT_OK

    if (status != E_OK) {
        Cdd_UnwindOwnedResources(ctx);  <-- Checks bit R3, then R2, then R1 in REVERSE order!
    }
    return status;                      <-- Single point of exit!
```

### Method 3 Code: AUTOSAR / MISRA-C `goto`-Free Reverse Unwind

```c
#include <stdbool.h>
#include <stdint.h>

typedef uint8_t Std_ReturnType;
#define E_OK     ((Std_ReturnType)0x00U)
#define E_NOT_OK ((Std_ReturnType)0x01U)

/* Ownership bitmask tracking which resources were actually acquired: */
#define CDD_OWN_R1_CLK_PORT ((uint8_t)(1U << 0)) /* Bit 0: Clock & Port pins */
#define CDD_OWN_R2_BUFFER   ((uint8_t)(1U << 1)) /* Bit 1: Ring buffer / DMA */
#define CDD_OWN_R3_ISR_CAT2 ((uint8_t)(1U << 2)) /* Bit 2: OS Category-2 ISR */

typedef struct {
    uint8_t owned_mask;                 /* Bitmask of currently owned resources */
} Cdd_DriverCtxType;

Std_ReturnType Cdd_AcquireR1_ClkPort(Cdd_DriverCtxType *ctx);
void           Cdd_ReleaseR1_ClkPort(Cdd_DriverCtxType *ctx);
Std_ReturnType Cdd_AcquireR2_Buffer(Cdd_DriverCtxType *ctx);
void           Cdd_ReleaseR2_Buffer(Cdd_DriverCtxType *ctx);
Std_ReturnType Cdd_AcquireR3_EnableIsr(Cdd_DriverCtxType *ctx);
void           Cdd_ReleaseR3_DisableIsr(Cdd_DriverCtxType *ctx);

/* Shared Reverse-Order Unwind Helper (used by BOTH Cdd_Init failure and Cdd_DeInit): */
static void Cdd_UnwindOwnedResources(Cdd_DriverCtxType *ctx)
{
    /* 1. Reverse Step 1 (R3 first): Disable Category-2 ISR if owned: */
    if ((ctx->owned_mask & CDD_OWN_R3_ISR_CAT2) != 0U) {
        Cdd_ReleaseR3_DisableIsr(ctx);
        ctx->owned_mask &= (uint8_t)(~CDD_OWN_R3_ISR_CAT2);
    }

    /* 2. Reverse Step 2 (R2 second): Release DMA/ring buffer if owned: */
    if ((ctx->owned_mask & CDD_OWN_R2_BUFFER) != 0U) {
        Cdd_ReleaseR2_Buffer(ctx);
        ctx->owned_mask &= (uint8_t)(~CDD_OWN_R2_BUFFER);
    }

    /* 3. Reverse Step 3 (R1 last): Disable clock & port pins if owned: */
    if ((ctx->owned_mask & CDD_OWN_R1_CLK_PORT) != 0U) {
        Cdd_ReleaseR1_ClkPort(ctx);
        ctx->owned_mask &= (uint8_t)(~CDD_OWN_R1_CLK_PORT);
    }
}

/* Init illustrating Rule 15.1/15.5 recommendations, not full MISRA compliance: */
Std_ReturnType Cdd_Init(Cdd_DriverCtxType *ctx)
{
    Std_ReturnType status = E_OK;

    if (ctx == (Cdd_DriverCtxType *)0) {
        status = E_NOT_OK;
    } else {
        ctx->owned_mask = 0U;

        /* Step 1: Acquire Resource 1 (Clock & Port): */
        status = Cdd_AcquireR1_ClkPort(ctx);
        if (status == E_OK) {
            ctx->owned_mask |= CDD_OWN_R1_CLK_PORT;
        }

        /* Step 2: Acquire Resource 2 (Buffer / DMA) only if Step 1 succeeded: */
        if (status == E_OK) {
            status = Cdd_AcquireR2_Buffer(ctx);
            if (status == E_OK) {
                ctx->owned_mask |= CDD_OWN_R2_BUFFER;
            }
        }

        /* Step 3: Acquire Resource 3 (Enable ISR) only if Steps 1 & 2 succeeded: */
        if (status == E_OK) {
            status = Cdd_AcquireR3_EnableIsr(ctx);
            if (status == E_OK) {
                ctx->owned_mask |= CDD_OWN_R3_ISR_CAT2;
            }
        }

        /* If any step failed, unwind ONLY the acquired resources in reverse order: */
        if (status != E_OK) {
            Cdd_UnwindOwnedResources(ctx);
        }
    }

    return status; /* Single point of exit (MISRA C:2012 Rule 15.5) */
}

void Cdd_DeInit(Cdd_DriverCtxType *ctx)
{
    if (ctx != (Cdd_DriverCtxType *)0) {
        Cdd_UnwindOwnedResources(ctx);
    }
}
```

---

## Complete Runnable C02 Verification Code (Tests All 4 Failure/Success Cases)

This standalone hosted C11 file checks both unwind patterns across four cases.
Suggested learner command for a host GCC toolchain:
`gcc -std=c11 -Wall -Wextra -Werror c02_unwind_test.c -o c02_unwind_test.exe`,
then `.\c02_unwind_test.exe` in PowerShell. A target run additionally needs
startup and output support. Expected: eight `PASS` lines and exit status `0`;
any failed check returns `1`. These are sequential resource models, not
DMA/worker-shutdown tests.

```c
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

/* ============================================================================
 * FAULT-INJECTION HARNESS TRACKING ACQUISITION & RELEASE ORDER
 * ============================================================================ */
#define RES_R1_CLK_GPIO (1U << 0) /* 0x01: Resource 1 (Clock & GPIO) */
#define RES_R2_BUFFER   (1U << 1) /* 0x02: Resource 2 (DMA / Queue)  */
#define RES_R3_IRQ      (1U << 2) /* 0x04: Resource 3 (Interrupt)    */

typedef struct {
    uint8_t  fail_at_step;       /* 0 = none, 1 = fail R1, 2 = fail R2, 3 = fail R3 */
    uint8_t  live_hw_mask;       /* Bitmask of currently active hardware resources  */
    uint8_t  owned_mask;         /* Bitmask recorded by AUTOSAR/MISRA driver        */
    uint8_t  acquire_log[3];     /* Records order of acquired resources (1, 2, 3)   */
    uint8_t  acquire_count;
    uint8_t  release_log[3];     /* Records order of released resources (3, 2, 1)   */
    uint8_t  release_count;
    bool     order_violation;    /* Set true if R2/R1 released while R3 still live! */
    bool     unowned_release;    /* Set true if code releases an unacquired resource*/
} c02_harness_t;

static void harness_reset(c02_harness_t *h, uint8_t fail_step)
{
    *h = (c02_harness_t){ .fail_at_step = fail_step };
}

static bool acquire_r1(c02_harness_t *h)
{
    if (h->fail_at_step == 1U) {
        return false;
    }
    h->live_hw_mask |= RES_R1_CLK_GPIO;
    h->acquire_log[h->acquire_count++] = 1U;
    return true;
}

static void release_r1(c02_harness_t *h)
{
    /* Violation if R1 is released when not owned, or while R2 or R3 is still live! */
    if ((h->live_hw_mask & RES_R1_CLK_GPIO) == 0U) {
        h->unowned_release = true;
    }
    if ((h->live_hw_mask & (RES_R2_BUFFER | RES_R3_IRQ)) != 0U) {
        h->order_violation = true;
    }
    h->live_hw_mask &= (uint8_t)(~RES_R1_CLK_GPIO);
    h->release_log[h->release_count++] = 1U;
}

static bool acquire_r2(c02_harness_t *h)
{
    if (h->fail_at_step == 2U) {
        return false;
    }
    h->live_hw_mask |= RES_R2_BUFFER;
    h->acquire_log[h->acquire_count++] = 2U;
    return true;
}

static void release_r2(c02_harness_t *h)
{
    /* Violation if R2 is released when not owned, or while R3 (IRQ) is still live! */
    if ((h->live_hw_mask & RES_R2_BUFFER) == 0U) {
        h->unowned_release = true;
    }
    if ((h->live_hw_mask & RES_R3_IRQ) != 0U) {
        h->order_violation = true;
    }
    h->live_hw_mask &= (uint8_t)(~RES_R2_BUFFER);
    h->release_log[h->release_count++] = 2U;
}

static bool acquire_r3(c02_harness_t *h)
{
    if (h->fail_at_step == 3U) {
        return false;
    }
    h->live_hw_mask |= RES_R3_IRQ;
    h->acquire_log[h->acquire_count++] = 3U;
    return true;
}

static void release_r3(c02_harness_t *h)
{
    if ((h->live_hw_mask & RES_R3_IRQ) == 0U) {
        h->unowned_release = true;
    }
    h->live_hw_mask &= (uint8_t)(~RES_R3_IRQ);
    h->release_log[h->release_count++] = 3U;
}

/* ============================================================================
 * 1. LINUX KERNEL REVERSE `goto` IMPLEMENTATION
 * ============================================================================ */
bool linux_goto_init(c02_harness_t *h)
{
    if (!acquire_r1(h)) {
        return false;              /* R1 failed: nothing to undo */
    }
    if (!acquire_r2(h)) {
        goto err_release_r1;       /* R2 failed: undo R1 only    */
    }
    if (!acquire_r3(h)) {
        goto err_release_r2;       /* R3 failed: undo R2 then R1 */
    }
    return true;

err_release_r2:
    release_r2(h);
err_release_r1:
    release_r1(h);
    return false;
}

void linux_goto_deinit(c02_harness_t *h)
{
    release_r3(h);                 /* Reverse LIFO: 3 -> 2 -> 1  */
    release_r2(h);
    release_r1(h);
}

/* ============================================================================
 * 2. AUTOSAR / MISRA-C `goto`-FREE IMPLEMENTATION
 * ============================================================================ */
static void misra_unwind_owned(c02_harness_t *h)
{
    if ((h->owned_mask & RES_R3_IRQ) != 0U) {
        release_r3(h);
        h->owned_mask &= (uint8_t)(~RES_R3_IRQ);
    }
    if ((h->owned_mask & RES_R2_BUFFER) != 0U) {
        release_r2(h);
        h->owned_mask &= (uint8_t)(~RES_R2_BUFFER);
    }
    if ((h->owned_mask & RES_R1_CLK_GPIO) != 0U) {
        release_r1(h);
        h->owned_mask &= (uint8_t)(~RES_R1_CLK_GPIO);
    }
}

bool misra_no_goto_init(c02_harness_t *h)
{
    bool ok = true;
    h->owned_mask = 0U;

    if (acquire_r1(h)) {
        h->owned_mask |= RES_R1_CLK_GPIO;
    } else {
        ok = false;
    }

    if (ok) {
        if (acquire_r2(h)) {
            h->owned_mask |= RES_R2_BUFFER;
        } else {
            ok = false;
        }
    }

    if (ok) {
        if (acquire_r3(h)) {
            h->owned_mask |= RES_R3_IRQ;
        } else {
            ok = false;
        }
    }

    if (!ok) {
        misra_unwind_owned(h);
    }
    return ok;
}

int main(void)
{
    c02_harness_t h;
    bool all_pass = true;

    /* Both implementations: success + deinit, then fail R1, R2 and R3. */
    for (unsigned variant = 0U; variant < 2U; ++variant) {
        for (uint8_t step = 0U; step <= 3U; ++step) {
            harness_reset(&h, step);
            bool ok = (variant == 0U) ? linux_goto_init(&h)
                                      : misra_no_goto_init(&h);
            uint8_t expected = (step == 0U) ? 3U : (uint8_t)(step - 1U);
            bool pass = (ok == (step == 0U));
            if (ok) {
                pass = pass && (h.live_hw_mask == 7U);
                if (variant == 0U) {
                    linux_goto_deinit(&h);
                } else {
                    misra_unwind_owned(&h);
                }
            }
            pass = pass && (h.live_hw_mask == 0U) && (h.owned_mask == 0U)
                        && (h.acquire_count == expected)
                        && (h.release_count == expected)
                        && !h.order_violation && !h.unowned_release;
            for (uint8_t i = 0U; i < expected; ++i) {
                pass = pass && (h.acquire_log[i] == (uint8_t)(i + 1U))
                            && (h.release_log[i] == (uint8_t)(expected - i));
            }
            printf("%s step=%u: %s\n", variant == 0U ? "goto" : "guarded",
                   (unsigned)step, pass ? "PASS" : "FAIL");
            all_pass = all_pass && pass;
        }
    }
    return all_pass ? 0 : 1;
}
```

---

## Complete Scenario Walkthrough: What Happens When Resource 3 Fails (`fail_at_step = 3`)

Trace both implementations when **`R1` and `R2` succeed, and `R3` fails (`fail_at_step = 3`)**:

1. **Acquire `R1` (Clock & GPIO):**
   * `acquire_r1()` succeeds -> `live_hw_mask` becomes `0x01` (`RES_R1_CLK_GPIO`), `acquire_log[0] = 1`.
   * In the MISRA version, `owned_mask` becomes `0x01`.
2. **Acquire `R2` (Buffer / Queue):**
   * `acquire_r2()` succeeds -> `live_hw_mask` becomes `0x03` (`RES_R1_CLK_GPIO | RES_R2_BUFFER`), `acquire_log[1] = 2`.
   * In the MISRA version, `owned_mask` becomes `0x03`.
3. **Attempt `R3` (Interrupt Enable) — FAILS!**
   * `acquire_r3()` returns `false`. `live_hw_mask` is still `0x03` (`R3` bit is `0`).
4. **Reverse Unwind (`R2` first, then `R1`):**
   * **In Linux `goto`:** Jumps to `err_release_r2:` -> calls `release_r2()` (`release_log[0] = 2`, `live_hw_mask` becomes `0x01`) -> falls through to `err_release_r1:` -> calls `release_r1()` (`release_log[1] = 1`, `live_hw_mask` becomes `0x00`) -> returns `false`.
   * **In AUTOSAR / MISRA:** Calls `misra_unwind_owned()`:
     1. Checks `RES_R3_IRQ` (`0x04`): bit is **`0`** in `owned_mask` (`0x03`), so `release_r3()` is **skipped**!
     2. Checks `RES_R2_BUFFER` (`0x02`): bit is **`1`**, so it calls `release_r2()` (`release_log[0] = 2`) and clears bit `1`.
     3. Checks `RES_R1_CLK_GPIO` (`0x01`): bit is **`1`**, so it calls `release_r1()` (`release_log[1] = 1`) and clears bit `0`.
   * **Final State:** `live_hw_mask == 0x00` (zero leaked resources), `release_log = {2, 1}` (exact reverse order), `order_violation == false`, `unowned_release == false`!

## Corrections

- 2026-11-17: Approved source-only repair: failure-atomic acquire and quiescent
  teardown contracts, bounded worker-stop failure, advisory MISRA rules versus
  project compliance, nesting versus cyclomatic complexity, and checks for both
  host unwind variants with failing exit status. Expected harness output is
  specified; compilation and execution remain for the learner.
