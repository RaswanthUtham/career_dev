# W04-L24: FreeRTOS Queues, Semaphores, Mutexes, Critical Sections And Priority Inversion (`P1`)

> **Status:** Source/API-reviewed, 2026-11-17 (rev 2). Concept and worked-trace
> lesson for single-core `STM32F446RE` (`84 MHz` `SYSCLK`, `1 kHz` `SysTick`)
> with FreeRTOS V10.3.1 (`CM4F` port in CubeF4 V1.28.3). C snippets and the
> unified end-of-lesson module illustrate exact FreeRTOS contracts; no target
> build, flash, or hardware timing run was performed here.

Prerequisites: [C02 reverse unwind](W02_L21_c02_three_resource_init_and_reverse_unwind.md),
[FreeRTOS tasks, delays & notifications](W03_L22_freertos_tasks_states_delays_and_notifications.md), and
[C03 payload ownership](W03_L23_c03_producer_consumer_payload_ownership.md).
Roadmap: [Week 4 synchronization](../../plans/five_month_development_roadmap.md#5-weekly-schedule).

---

## Know This First

| Term | Simple meaning |
| --- | --- |
| Task (`Thread` mode, `PSP`) | An independent C loop with its own stack and priority (`0` = lowest, higher number = more urgent) |
| `ISR` (`Handler` mode, `MSP`) | Hardware interrupt routine; has **no** FreeRTOS `TCB` and **can never sleep/block** |
| **Queue** (`xQueueCreate`) | Fixed-size FIFO ring buffer that **copies actual data bytes** (`struct`s/integers) between `ISR`/Tasks |
| **Binary Semaphore** (`xSemaphoreCreateBinary`) | A `1`-slot **event flag** (`0` = Empty, `1` = Pending signal; `0` data bytes). Producer *gives*, Consumer *takes* |
| **Counting Semaphore** (`xSemaphoreCreateCounting`) | An integer **token counter (`0 .. Max`)** tracking either *pending events* or *free pool slots* |
| **Mutex** (`xSemaphoreCreateMutex`) | A **task-owned lock ("1 key on a hook")** protecting a shared resource. Same task **must** *take* then *give*; includes **Priority Inheritance** |
| **Critical Section** (`taskENTER_CRITICAL`) | Masks Cortex-M4 interrupts via `BASEPRI` for a few CPU instructions to copy/update shared variables safely against `ISR`s |
| **Base vs. Effective Priority** | **Base:** priority assigned at `xTaskCreate`; **Effective:** temporary boosted priority while holding a contended Mutex |
| **Priority Inversion** | High-priority `Task H` is stuck waiting for Low-priority `Task L` while unrelated Medium-priority `Task M` preempts `L`! |
| **Priority Inheritance** | Kernel temporarily boosts Mutex owner `Task L` up to `Task H`'s priority so `L` finishes fast and hands over the Mutex |

**Opposite Priority Numbers Reminder ([W03-L22](W03_L22_freertos_tasks_states_delays_and_notifications.md)):**
* **FreeRTOS Task Priority:** Higher number = **HIGHER** urgency (`H = 3` beats `M = 2` beats `L = 1` beats `Idle = 0`; requires `configMAX_PRIORITIES >= 4`).
* **Cortex-M4 `NVIC` Interrupt Priority:** Lower number = **HIGHER** urgency (`0` is highest; `5..15` are allowed to call FreeRTOS `...FromISR()` APIs when `PRIGROUP = 3` and `configMAX_SYSCALL_INTERRUPT_PRIORITY = 0x50`).

---

## Continuous Simple Example Used Across This Entire Lesson (`P1` Node)

Imagine our `STM32F446RE` `P1` board running **1 interrupt** and **3 tasks**:

```text
  [Button / Fault Pin PC13]
             |
             v
  EXTI15_10_IRQHandler (HW IRQ Priority 5)
     |-- (1) Increments shared 32-bit counter: g_irq_pulse_count++
     +-- (2) Sends 8-byte FaultRecord {seq, code} -----> [ g_fault_queue (2 slots x 8 B) ]
                                                                     |
                                                                     v
  Task H: FaultService  (Priority 3 - HIGH)   <-- wakes & receives FaultRecord copy
     |
     +---+--> Locks [ g_cfg_mutex ] --> Reads/Updates shared g_cfg {max_temp_c, log_en}
         |
  Task L: StatusLogger  (Priority 1 - LOW)    --> Locks [ g_cfg_mutex ] to read/update g_cfg

  Task M: MathWorker    (Priority 2 - MEDIUM) --> Pure CPU math; NEVER touches g_cfg_mutex!
```

Look at the **four distinct synchronization jobs** in this single diagram:
1. **Need to pass an 8-byte `FaultRecord` from `ISR` to `Task H`?** $\rightarrow$ **Queue** (`g_fault_queue`).
2. **Need only to wake a task ("an event happened!") with zero data bytes?** $\rightarrow$ **Binary Semaphore** (or `W03-L22` **Task Notification**).
3. **Need `Task H` and `Task L` to take turns updating multi-field `g_cfg` (or sharing `SPI1`/`I2C1`) without tearing data?** $\rightarrow$ **Mutex** (`g_cfg_mutex`).
4. **Need `Task L` to read `g_irq_pulse_count` (or a multi-word `ISR` counter) without racing `IRQ 40`?** $\rightarrow$ **Critical Section** (`taskENTER_CRITICAL()`).

---

## Scene 1: How Queue, Semaphores, And Mutex Actually Differ Under The Hood

In FreeRTOS, Queues, Binary Semaphores, Counting Semaphores, and Mutexes are **all built on the same internal kernel struct (`Queue_t`)**—they differ only in **item size**, **starting count**, **ownership**, and **priority inheritance**:

```text
1. QUEUE: xQueueCreate(2, 8)               --> 2 slots x 8 bytes. Starts EMPTY (count = 0).
   [ Slot 0: 8B | Slot 1: 8B ]                 Copies data bytes in/out. Any ISR/Task can send/receive.

2. BINARY SEMAPHORE: xSemaphoreCreateBinary() -> 1 slot x 0 bytes! Starts EMPTY (count = 0).
   [ Token: 0 or 1 ]                           Event signal: Producer GIVES (0->1), Consumer TAKES (1->0).
                                               NO owner, NO priority inheritance!

3. COUNTING SEMAPHORE: xSemaphoreCreateCounting(3, 3) -> Max=3, 0 bytes. Count = 0..3.
   [ Count: 0 .. 3 ]                           Counts events (start 0) OR free C03 pool slots (start 3).

4. MUTEX: xSemaphoreCreateMutex()          --> 1 slot x 0 bytes! Starts AVAILABLE (count = 1 = Unlocked!).
   [ Key: 1, Owner: NULL, Inherit: YES ]       Task TAKES key (1->0, Owner = Task), SAME Task GIVES key
                                               back (0->1, Owner = NULL). ISR FORBIDDEN!
```

### Quick 5-Second Selection Table

| Question You Ask | Pick This Primitive | Why |
| --- | --- | --- |
| *"Must I pass small structs/bytes in FIFO order?"* | **Queue** (`xQueueCreate`) | Safely copies each item by value; wakes waiting consumer |
| *"Must an `ISR` wake **one specific** known task?"* | **Task Notification** (`vTaskNotifyGiveFromISR`, [L22](W03_L22_freertos_tasks_states_delays_and_notifications.md)) | Fastest, `0` extra RAM (lives inside `TCB`) |
| *"Must an `ISR` signal an event through a decoupled handle?"* | **Binary Semaphore** (`xSemaphoreCreateBinary`) | Clean event token (`0` or `1`); coalesces rapid duplicates |
| *"Must I count `N` rapid events or `N` free `C03` pool slots?"* | **Counting Semaphore** (`xSemaphoreCreateCounting`) | Tracks exact count `0..N` without dropping up to `N` |
| *"Must two **Tasks** share `g_cfg`, `SPI1`, `I2C1`, or `UART2`?"* | **Mutex** (`xSemaphoreCreateMutex`, `configUSE_MUTEXES = 1`) | Tracks owner task + **Priority Inheritance** (`ISR` never allowed!) |
| *"Must a Task read/update a few words shared with an **`ISR`**?"* | **Critical Section** (`taskENTER_CRITICAL` / `EXIT`) | Masks `BASEPRI` (`5..15`) for `< 1 us`; zero blocking |

---

## Scene 2: Queue — Copying `FaultRecord` Bytes From `ISR` To `Task H`

Suppose `EXTI15_10_IRQHandler` detects fault `0xE1` at sequence `17` and needs `Task H` to log `{ .seq = 17, .code = 0xE1 }` (`8` bytes).

1. **Create `g_fault_queue = xQueueCreate(2U, sizeof(FaultRecord))` in `main()` BEFORE enabling `NVIC` IRQ `40`:**
   * **Otherwise:** If `IRQ 40` fires while `g_fault_queue == NULL`, `xQueueSendFromISR()` hits an assertion/fault!
2. **Initialize all fields of `FaultRecord` in the `ISR`, call `xQueueSendFromISR(g_fault_queue, &rec, &woken)`, and handle `!= pdPASS`:**
   * `xQueueSendFromISR()` **copies all `8` bytes** of `rec` into the queue's internal RAM before returning, so `rec` can safely live on the `ISR`'s stack!
   * **Otherwise:** When all `2` slots are already full, `xQueueSendFromISR()` returns `errQUEUE_FULL` (`!= pdPASS`). Counting `g_queue_drops++` makes overload visible instead of silently losing faults.
3. **Call `portYIELD_FROM_ISR(woken)` at the end of the `ISR`, and check `xQueueReceive(...) == pdPASS` in `Task H`:**
   * **Otherwise:** Skipping `portYIELD_FROM_ISR(woken)` leaves `Task H` waiting until the next `1 ms` `SysTick` instead of switching immediately via `PendSV`; skipping the `== pdPASS` check processes `rx` garbage when a wait times out!

```text
1. BEFORE ISR Fires                2. AFTER xQueueSendFromISR()        3. AFTER Task H xQueueReceive()
   g_fault_queue: [ empty | empty ]   g_fault_queue: [ {17,0xE1} | - ]    g_fault_queue: [ empty | empty ]
   Task H: Blocked (waiting)      ->  Task H: Ready (woken = pdTRUE!) ->  Task H local rx = {17, 0xE1}

4. OVERLOAD CASE (Queue Already Has 2 Items [{17,0xE1}, {18,0xE2}] When {19,0xE3} Arrives):
   xQueueSendFromISR() returns errQUEUE_FULL -> {17,0xE1},{18,0xE2} preserved; {19,0xE3} dropped (g_queue_drops++)
```

```c
typedef struct {
    uint32_t seq;
    uint32_t code;
} FaultRecord;

static QueueHandle_t     g_fault_queue;
static volatile uint32_t g_queue_drops;

/* Producer: runs in Handler mode (MSP) at NVIC priority 5 */
void fault_isr_send_example(uint32_t seq_num, uint32_t err_code)
{
    FaultRecord rec = { .seq = seq_num, .code = err_code };
    BaseType_t  woken = pdFALSE;

    if (xQueueSendFromISR(g_fault_queue, &rec, &woken) != pdPASS) {
        g_queue_drops++;                    /* Drop-newest policy: count overflow */
    }
    portYIELD_FROM_ISR(woken);              /* Pends PendSV if Task H outranks current task */
}

/* Consumer: runs inside Task H (Thread mode, PSP) */
void fault_task_receive_example(void)
{
    FaultRecord rx = { 0U, 0U };

    /* Block Task H up to 20 ms (20 ticks at 1 kHz) waiting for 1 FaultRecord: */
    if (xQueueReceive(g_fault_queue, &rx, pdMS_TO_TICKS(20U)) == pdPASS) {
        /* Safe private copy in Task H's stack: rx.seq = 17, rx.code = 0xE1 */
        (void)rx;
    }
}
```

**Where:**
* `QueueHandle_t`: Opaque pointer to the FreeRTOS `Queue_t` control block and its `2 * 8 = 16`-byte storage array.
* `pdPASS` / `errQUEUE_FULL`: FreeRTOS return codes (`pdPASS == 1` when item copied; `errQUEUE_FULL == 0` when `0` free slots remain).
* `woken` (`BaseType_t`): Output flag set to `pdTRUE` by `xQueueSendFromISR()` if unblocking `Task H` woke a task with higher priority than the interrupted task.
* `pdMS_TO_TICKS(20U)`: Converts `20` milliseconds into RTOS ticks (`20` ticks when `configTICK_RATE_HZ = 1000`).

---

## Scene 3: Binary Semaphore — One Event Token (`ISR` Gives $\rightarrow$ `Task` Takes)

When an `ISR` only needs to say *"New DMA block is ready!"* (and the bytes already sit in a `C03` static buffer), you don't need a Queue to copy bytes—you can signal with a **Binary Semaphore** (`xSemaphoreCreateBinary()`).

* **Crucial Mental Model (Signaling $\neq$ Locking!):**
  * A Binary Semaphore **starts at `0` (Empty)**.
  * The **Consumer Task** calls `xSemaphoreTake(g_bin_sem, timeout)` and **sleeps (`Blocked`)** until the token becomes `1`. Notice: **the task does NOT give the semaphore back!**
  * The **Producer `ISR`** calls `xSemaphoreGiveFromISR(g_bin_sem, &woken)` to drop `1` token in (`0 -> 1`).
  * **Coalescing Rule:** Because max count is `1`, if the `ISR` fires **twice** before the task wakes up, the 1st `Give` sets `0 -> 1` (`pdTRUE`) and the 2nd `Give` sees `1` and returns `pdFALSE` (`count` stays `1`). Both events merge (*coalesce*) into **one** wakeup!

```text
Start: [ count = 0 ] --> Task calls Take() --> Task Blocked
  --> ISR #1 calls GiveFromISR() --> [ count = 1 ], Task Ready (pdTRUE)
  --> ISR #2 calls GiveFromISR() --> [ count = 1 ], returns pdFALSE (coalesced!)
  --> Task runs & finishes Take() -> [ count = 0 ]
```

```c
static SemaphoreHandle_t g_dma_done_sem;

/* Producer (ISR): drops 1 event token into the semaphore */
void dma_isr_signal_example(void)
{
    BaseType_t woken = pdFALSE;
    (void)xSemaphoreGiveFromISR(g_dma_done_sem, &woken); /* 0 -> 1 */
    portYIELD_FROM_ISR(woken);
}

/* Consumer (Task): waits for the token and consumes it (1 -> 0); NEVER gives it back! */
bool task_wait_dma_signal_example(void)
{
    if (xSemaphoreTake(g_dma_done_sem, pdMS_TO_TICKS(10U)) == pdTRUE) {
        return true;                        /* Token consumed (1 -> 0); inspect DMA buffer */
    }
    return false;                           /* Timed out after 10 ticks */
}
```

**Where:**
* `SemaphoreHandle_t`: Opaque handle to a `Queue_t` configured with `uxItemSize = 0`.
* `pdTRUE` / `pdFALSE`: Boolean status (`1` = token given/taken; `0` = semaphore already full on `Give`, or timed out on `Take`).

---

## Scene 4: Mutex — One Key Protecting A Shared Resource Between Tasks

Suppose `Task L` and `Task H` both access a shared configuration struct `g_cfg`:
```c
typedef struct {
    uint32_t max_temp_c;
    bool     logging_enabled;
} P1Config;
```
* **Why Sharing Without a Lock Fails (Torn Update):** Suppose old `g_cfg` is `{ 80, true }`, and `Task L` wants to switch to `{ 100, false }`. `Task L` writes `g_cfg.max_temp_c = 100`, gets preempted by `Task H` *right before* writing `logging_enabled = false`, and `Task H` acts on **`{ 100, true }`**—a combination that was never valid!
* **How a Mutex (`xSemaphoreCreateMutex()`) Fixes It:**
  1. Requires `configUSE_MUTEXES = 1` in `FreeRTOSConfig.h`. Starts **Unlocked (`count = 1`, `Owner = NULL`)**.
  2. Before reading **or** writing `g_cfg`, a task calls `xSemaphoreTake(g_cfg_mutex, timeout)`. If it returns `pdTRUE`, `count` becomes `0` and **`pxMutexHolder = CurrentTask`**.
  3. After updating/reading both fields, **that exact same task** calls `xSemaphoreGive(g_cfg_mutex)` (`Owner -> NULL`, `count -> 1`).
  4. **If `xSemaphoreTake()` times out (`!= pdTRUE`), DO NOT touch `g_cfg`!** A timeout only means *your wait expired*—the other task still owns the mutex! *(And when a mutex protects `SPI1`/`I2C1` DMA, hold the mutex until the wire/DMA transfer finishes or is quiesced—never release the mutex right after starting DMA!)*

```text
BEFORE Task L Takes            INSIDE Locked Section               AFTER Task L Gives
  Mutex: count=1, Owner=NULL     Mutex: count=0, Owner=Task L        Mutex: count=1, Owner=NULL
  g_cfg: {80, true}          ->  Task L writes {100, false} safely -> Consistent {100, false} visible!
```

```c
static SemaphoreHandle_t g_cfg_mutex;
static P1Config          g_cfg = { .max_temp_c = 80U, .logging_enabled = true };

/* Task-only helper (NEVER call from an ISR!): */
bool p1_cfg_update_safe(uint32_t new_max_temp_c, bool new_log_en)
{
    /* 1. Acquire ownership of the mutex key (wait up to 5 ms): */
    if (xSemaphoreTake(g_cfg_mutex, pdMS_TO_TICKS(5U)) != pdTRUE) {
        return false;                       /* Did NOT get key -> leave g_cfg untouched! */
    }

    /* 2. Update both fields atomically as one logical unit: */
    g_cfg.max_temp_c      = new_max_temp_c;
    g_cfg.logging_enabled = new_log_en;

    /* 3. Same owner task returns the key: */
    (void)xSemaphoreGive(g_cfg_mutex);
    return true;
}
```

**Where:**
* `g_cfg_mutex` (`SemaphoreHandle_t`): Mutex handle created with `xSemaphoreCreateMutex()` (`configUSE_MUTEXES = 1`), starting unlocked (`count = 1`, `pxMutexHolder = NULL`).
* `pdMS_TO_TICKS(5U)`: Maximum wait time (`5` ticks at `1 kHz` = `5 ms`) to obtain the mutex before returning `pdFALSE`.

---

## Scene 5: Priority Inversion & How Mutex Priority Inheritance Fixes It

Here is the classic interview and real-world Mars Pathfinder bug: **"Why can `Task M` (`Priority 2`) freeze `Task H` (`Priority 3`), even though `Task M` NEVER touches the lock?"**

Let's trace concrete numbers (`1 unit = 1 ms` of pure CPU work):
* **`Task L` (`Priority 1`):** Grabs the lock at `t = 0 ms` and needs **`3 ms`** of CPU work inside the lock (`1 ms` done before `t = 1`, **`2 ms` left**).
* **`Task H` (`Priority 3`):** Wakes up at `t = 1 ms`, wants the **same lock**, and needs **`1 ms`** inside the lock.
* **`Task M` (`Priority 2`):** Also wakes up at `t = 1 ms` to do **`4 ms`** of unrelated math (**never uses the lock**).

---

### Case A — WRONG: Locking With A Binary Semaphore (NO Priority Inheritance!)

1. **`t = 0..1 ms`:** Only `Task L (1)` is Ready. `L` takes the Binary Semaphore and completes `1 ms` of its `3 ms` locked work (`2 ms` remaining).
2. **`t = 1 ms`:** `Task H (3)` and `Task M (2)` both become Ready! `H (3)` preempts `L (1)` immediately, tries `xSemaphoreTake()`, sees `count == 0`, and enters **`Blocked`** state waiting for `L` to finish.
3. **`t = 1..5 ms` (THE INVERSION BUG!):** Who is the highest Ready task now? `H` is `Blocked`, `L` has priority **`1`**, and `M` has priority **`2`**. So **`Task M (2)` preempts `Task L (1)` for `4 full milliseconds`!** While `M` crunches math, `L` cannot run to release the lock, so **`Task H (3)` is stuck waiting behind `Task M (2)`!** *(If `M` ran in an infinite busy loop, `H` would starve forever!)*
4. **`t = 5..7 ms`:** `M` finally finishes; `L (1)` resumes for its remaining `2 ms` and releases the lock at `t = 7 ms`.
5. **`t = 7..8 ms`:** `H (3)` finally gets the lock and finishes at **`t = 8 ms`** (**waited `6 ms`**!).

```text
CASE A — BINARY SEMAPHORE AS LOCK (No Inheritance -> H waits 6 ms!):

Time (ms):   0       1                       5           7       8
CPU Runs:    | L (1) |         M (2)         |   L (1)   | H (3) |
Lock Owner:  | <---------------- Task L ---------------> |Task H |
Task H (3):  .       [======= BLOCKED WAITING 6 ms ======] Runs!
                     ^-- Unrelated M(2) steals CPU from L(1), delaying H(3)!
```

---

### Case B — RIGHT: Locking With A FreeRTOS Mutex (Priority Inheritance!)

Now replace the Binary Semaphore with `g_cfg_mutex = xSemaphoreCreateMutex()`:

1. **`t = 0..1 ms`:** `Task L` (`Base = 1, Effective = 1`) takes `g_cfg_mutex` (`Owner = L`) and completes `1 ms` of locked work (`2 ms` left).
2. **`t = 1 ms` (`xTaskPriorityInherit()` Fires!):** `Task H (3)` tries `xSemaphoreTake(g_cfg_mutex)`. FreeRTOS sees `Owner == Task L` whose priority (`1`) is lower than `H` (`3`). Before blocking `H`, FreeRTOS **temporarily raises `Task L`'s Effective Priority from `1` $\rightarrow$ `3`**!
3. **`t = 1..3 ms`:** Who is the highest Ready task now? `Task L` has **Effective Priority `3`**, which beats `Task M` (`2`)! **`L (boosted to 3)` runs immediately**, finishes its remaining `2 ms` of locked work, and calls `xSemaphoreGive(g_cfg_mutex)` at `t = 3 ms`.
4. **`t = 3..4 ms` (`xTaskPriorityDisinherit()` Fires!):** Inside `xSemaphoreGive()`, FreeRTOS drops `L` back to its **Base Priority `1`** and unblocks `Task H (3)`. `H (3)` preempts `L (1)`, does its `1 ms` work, and finishes at **`t = 4 ms`** (**waited only `2 ms` instead of `6 ms`!**).
5. **`t = 4..8 ms`:** `Task M (2)` runs its `4 ms` math cleanly *after* urgent `Task H` is already done!

```text
CASE B — FREERTOS MUTEX (Priority Inheritance -> H waits only 2 ms!):

Time (ms):   0       1           3       4                       8
CPU Runs:    | L (1) | L (3 !!)  | H (3) |         M (2)         |
Lock Owner:  | <---- Task L ---> |Task H |         none          |
Task H (3):  .       [Blocked 2ms] Runs!
                     ^-- L inherits H's priority 3 -> beats M(2) -> frees lock fast!
```

| Metric | Case A: Binary Semaphore | Case B: Mutex (Inheritance) |
| --- | --- | --- |
| `Task L` Effective Priority while `H` waits | Stays **`1`** (beaten by `M`) | Boosted **`1 -> 3`** (beats `M`!) |
| Time `Task H (3)` waits for lock | **`6 ms`** *(unbounded if `M` loops!)* | **`2 ms`** *(bounded by `L`'s lock section)* |
| `Task H (3)` completion time | `t = 8 ms` | **`t = 4 ms`** |

* **Three Practical Rules To Remember For Interviews:**
  1. **Inheritance speeds up CPU scheduling, NOT `L`'s own locked code:** `H` still waits for `L`'s `2 ms` critical work—so **never** put `vTaskDelay()` or slow polling inside a mutex!
  2. **Never spin-poll `xSemaphoreTake(mutex, 0)` in `Task H`:** If `H` loops without blocking, `H` never sleeps, `L` never gets the CPU (and never inherits priority!), causing an instant **deadlock**!
  3. **FreeRTOS Simplified Inheritance (`tasks.c:xTaskPriorityDisinherit`):** If `Task L` holds *two* mutexes at once, FreeRTOS restores `L`'s base priority only when `L` releases **all** held mutexes—so hold **one short mutex at a time** in a fixed lock order.

---

## Scene 6: Critical Sections (`taskENTER_CRITICAL`) & `BASEPRI` (`Task <-> ISR`)

What if `Task L` wants a consistent snapshot of two 32-bit counters updated inside `EXTI15_10_IRQHandler` (`g_irq_pulse_count` and `g_last_irq_tick`)?
* **Why NOT a Mutex?** Because `EXTI15_10_IRQHandler` is an **`ISR`**—an `ISR` can never call `xSemaphoreTake()`!
* **Why NOT `vTaskSuspendAll()`?** Because `vTaskSuspendAll()` only pauses the task scheduler—**hardware `ISR`s still fire** right in the middle of your read!
* **Right Tool — Short Critical Section (`taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()`):**
  * On `STM32F446` (`CM4F` port, `4` priority bits, `PRIGROUP = 3`, `configMAX_SYSCALL_INTERRUPT_PRIORITY = 0x50`), `taskENTER_CRITICAL()` writes **`BASEPRI = 0x50`** (`5 << 4`) and increments a nesting counter.
  * **Hardware priorities `5..15`** (including our `IRQ 40` at priority `5`, `PendSV` at `15`, and `SysTick` at `15`) are **masked** for those 2 lines of C!
  * **Hardware priorities `0..4`** (ultra-fast motor/safety hardware trips that **never** call FreeRTOS APIs) remain **unmasked**!

```text
BEFORE taskENTER_CRITICAL()      INSIDE Critical Section           AFTER Outer taskEXIT_CRITICAL()
  BASEPRI = 0x00 (All IRQs on)     BASEPRI = 0x50 (Prio 5..15 off)   BASEPRI = 0x00 (Prio 5..15 on)
  IRQ 40 (prio 5) can preempt  ->  Copy pulse_count & tick safely -> Pending IRQ 40 runs immediately!
```

```c
static volatile uint32_t g_irq_pulse_count;
static volatile uint32_t g_last_irq_tick;

bool p1_read_isr_counters_snapshot(uint32_t *out_pulses, uint32_t *out_tick)
{
    if (out_pulses != NULL) { *out_pulses = 0U; }
    if (out_tick   != NULL) { *out_tick   = 0U; }
    if ((out_pulses == NULL) || (out_tick == NULL)) {
        return false;
    }
    taskENTER_CRITICAL();                   /* BASEPRI -> 0x50 (masks NVIC prio 5..15) */
    *out_pulses = g_irq_pulse_count;        /* Fast 2-word snapshot (< 10 CPU cycles!) */
    *out_tick   = g_last_irq_tick;
    taskEXIT_CRITICAL();                    /* Nesting == 0 -> BASEPRI -> 0x00         */
    return true;
}
```

**Where:**
* `BASEPRI`: Arm Cortex-M4 *Base Priority Mask Register* (`0x00` = no masking; `0x50` = blocks exceptions whose shifted hardware priority byte is numerically `>= 0x50`, i.e., logical priorities `5..15`).
* `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()`: Nesting-safe task-context macros (for an `ISR` at priority `5..15` that needs a critical section, use `UBaseType_t saved = taskENTER_CRITICAL_FROM_ISR(); ... taskEXIT_CRITICAL_FROM_ISR(saved);`). **Never** call blocking FreeRTOS APIs (`vTaskDelay`, `xQueueReceive`, `xSemaphoreTake`) inside a critical section!

---

## Scene 7: Complete Unified `P1` Synchronization Example (`p1_sync_demo.c`)

Here is our single continuous `P1` scenario in one clean file showing creation + reverse unwind (`C02`), `ISR -> Queue`, `Task H <-> Task L` Mutex with Priority Inheritance, and `Task <-> ISR` Critical Section:

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "stm32f446xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

typedef struct {
    uint32_t seq;
    uint32_t code;
} FaultRecord;

typedef struct {
    uint32_t max_temp_c;
    bool     logging_enabled;
} P1Config;

static QueueHandle_t     g_fault_queue;
static SemaphoreHandle_t g_cfg_mutex;
static P1Config          g_cfg = { .max_temp_c = 80U, .logging_enabled = true };

static volatile uint32_t g_irq_pulse_count;
static volatile uint32_t g_last_irq_tick;
static volatile uint32_t g_queue_drops;

/* 1. Hardware ISR (PC13 / EXTI15_10 at NVIC Priority 5 >= 5): */
void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & (1UL << 13)) != 0U) {
        EXTI->PR = (1UL << 13);             /* W1C: write 1 (NOT |=) to clear EXTI13! */

        g_irq_pulse_count++;
        g_last_irq_tick = xTaskGetTickCountFromISR();

        FaultRecord rec   = { .seq = g_irq_pulse_count, .code = 0xE1U };
        BaseType_t  woken = pdFALSE;
        if (xQueueSendFromISR(g_fault_queue, &rec, &woken) != pdPASS) {
            g_queue_drops++;
        }
        portYIELD_FROM_ISR(woken);          /* Wakes Task H immediately via PendSV */
    }
}

/* 2. Task H (Priority 3 - HIGH): Receives FaultRecord & updates g_cfg under Mutex */
static void Task_High_FaultService(void *arg)
{
    (void)arg;
    FaultRecord rx = { 0U, 0U };

    for (;;) {
        if (xQueueReceive(g_fault_queue, &rx, portMAX_DELAY) == pdPASS) {
            /* If Task L holds g_cfg_mutex right now, L inherits Priority 3! */
            if (xSemaphoreTake(g_cfg_mutex, pdMS_TO_TICKS(10U)) == pdTRUE) {
                if (rx.code == 0xE1U) {
                    g_cfg.max_temp_c      = 70U; /* Tighten limit on fault */
                    g_cfg.logging_enabled = true;
                }
                (void)xSemaphoreGive(g_cfg_mutex);
            }
        }
    }
}

/* 3. Task M (Priority 2 - MEDIUM): Pure CPU work; cannot starve Task H thanks to Mutex! */
static void Task_Med_MathWorker(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    for (;;) {
        for (volatile uint32_t i = 0U; i < 5000U; ++i) { /* Simulated math burst */ }
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10U));
    }
}

/* 4. Task L (Priority 1 - LOW): Reads ISR snapshot (Critical Section) + g_cfg (Mutex) */
static void Task_Low_StatusLogger(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t pulses = 0U;
        uint32_t tick   = 0U;

        /* Short critical section for ISR-shared counters: */
        taskENTER_CRITICAL();
        pulses = g_irq_pulse_count;
        tick   = g_last_irq_tick;
        taskEXIT_CRITICAL();

        /* Mutex lock for task-shared g_cfg: */
        if (xSemaphoreTake(g_cfg_mutex, pdMS_TO_TICKS(10U)) == pdTRUE) {
            P1Config local_cfg = g_cfg;
            (void)xSemaphoreGive(g_cfg_mutex);
            (void)local_cfg;
            (void)pulses;
            (void)tick;
        }
        vTaskDelay(pdMS_TO_TICKS(50U));
    }
}

/* 5. Startup Init with C02 Reverse-Order Unwind Before Enabling IRQ 40: */
bool p1_sync_init(void)
{
    TaskHandle_t h_high = NULL;
    TaskHandle_t h_med  = NULL;
    TaskHandle_t h_low  = NULL;

    g_fault_queue = xQueueCreate(2U, sizeof(FaultRecord));
    if (g_fault_queue == NULL) {
        return false;
    }

    g_cfg_mutex = xSemaphoreCreateMutex();
    if (g_cfg_mutex == NULL) {
        vQueueDelete(g_fault_queue);
        g_fault_queue = NULL;
        return false;
    }

    /* Stack depth 256U = 256 32-bit words (1024 bytes) on Cortex-M4! */
    if (xTaskCreate(Task_High_FaultService, "FaultH", 256U, NULL, 3U, &h_high) != pdPASS) {
        goto err_mutex;
    }
    if (xTaskCreate(Task_Med_MathWorker,    "MathM",  256U, NULL, 2U, &h_med)  != pdPASS) {
        goto err_task_high;
    }
    if (xTaskCreate(Task_Low_StatusLogger,  "LogL",   256U, NULL, 1U, &h_low)  != pdPASS) {
        goto err_task_med;
    }

    /* Enable hardware producer LAST, only after queue, mutex, and all tasks exist! */
    NVIC_SetPriority(EXTI15_10_IRQn, 5U);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
    return true;

err_task_med:
    vTaskDelete(h_med);
err_task_high:
    vTaskDelete(h_high);
err_mutex:
    vSemaphoreDelete(g_cfg_mutex);
    g_cfg_mutex = NULL;
    vQueueDelete(g_fault_queue);
    g_fault_queue = NULL;
    return false;
}
```

**Where (`p1_sync_demo.c` Notations):**
* `EXTI->PR = (1UL << 13)`: Write-`1`-to-Clear (`W1C`) pending register for pin `PC13` (using `=` clears **only** line `13`).
* `xTaskGetTickCountFromISR()`: ISR-safe read of the FreeRTOS `1 kHz` tick counter.
* `portMAX_DELAY`: Blocks `Task_High_FaultService` indefinitely (`Blocked` state, `0%` CPU) until `g_fault_queue` receives an item (when `INCLUDE_vTaskSuspend == 1`).
* `256U` (`usStackDepth` in `xTaskCreate`): Stack size measured in **`StackType_t` words (`256 * 4 = 1024 bytes` on 32-bit Cortex-M4)**, not bytes!

---

## Control Flow In Three Parts

**(a) One-Line Chain:**
`EXTI15_10_IRQHandler (Handler/MSP) -> xQueueSendFromISR() + portYIELD_FROM_ISR() -> [PendSV] -> Task_High_FaultService (Thread/PSP) -> xQueueReceive() -> xSemaphoreTake(g_cfg_mutex) -> [xTaskPriorityInherit boosts Task L: 1 -> 3 if held] -> xSemaphoreGive(g_cfg_mutex)`

**(b) Step Table:**

| # | Where | Goal / State Change | Concrete Data In $\rightarrow$ Out | Context / Mode |
| --- | --- | --- | --- | --- |
| 1 | `EXTI15_10_IRQHandler` | Clear `EXTI->PR`, copy `FaultRecord` to queue | `{seq=1, code=0xE1}` -> `queue[0]` | Handler (`MSP`, prio `5`) |
| 2 | `PendSV_Handler` | Switch from interrupted task to `Task H (3)` | `Task H`: `Ready -> Running` | Handler (`MSP`, prio `15`) |
| 3 | `Task_High_FaultService` | Pop `FaultRecord` into local stack `rx` | `queue[0]` -> `rx = {1, 0xE1}` | Thread (`PSP`, prio `3`) |
| 4 | `xSemaphoreTake(g_cfg_mutex)` | Acquire `g_cfg` key (or boost owner `L`: `1 -> 3`) | `L` finishes -> `Owner = Task H` | Thread (`PSP`, prio `3`) |
| 5 | `Task_Low_StatusLogger` | Snapshot `g_irq_pulse_count` under `BASEPRI=0x50` | `BASEPRI: 0x00 -> 0x50 -> 0x00` | Thread (`PSP`, prio `1`) |

**(c) Privilege & Scheduling Timeline (When `Task L` Holds `g_cfg_mutex` And `IRQ 40` Fires):**

```text
EXTI15_10 ISR (MSP)          FreeRTOS Kernel / PendSV         Tasks (Thread / PSP)
-------------------          ------------------------         --------------------
                                                              Task L (prio 1) takes g_cfg_mutex
PC13 edge -> IRQ 40!
xQueueSendFromISR({1,0xE1})-> Wakes Task H (prio 3)
portYIELD_FROM_ISR(pdTRUE) -> PendSV switches to Task H ----> Task H (3) receives {1, 0xE1}
                                                              Task H (3) calls Take(g_cfg_mutex)
                             Boosts Task L: prio 1 -> 3! ---> Task L (eff 3) beats M (2) & Gives!
                             Restores L -> 1, wakes H (3) --> Task H (3) updates g_cfg & Gives!
```

---

## Planned Acceptance Evidence & Source Lookup

| Verification Case | Expected Observation |
| --- | --- |
| **Queue full (`3` rapid `ISR` sends into `2`-slot queue)** | Oldest `2` records received intact; `g_queue_drops == 1` |
| **Binary semaphore given twice before `Take()`** | 1st `Give` returns `pdTRUE` (`0 -> 1`); 2nd returns `pdFALSE` (`1`); task wakes once |
| **Priority inversion trace (`L=1, M=2, H=3`)** | With Binary Semaphore: `M` preempts `L` (`H` waits `6` units); with Mutex: `L` boosts to `3` (`H` waits `2` units) |
| **Mutex timeout (`pdFALSE`)** | Caller returns `false` without reading or writing `g_cfg` |
| **Critical section (`taskENTER_CRITICAL`)** | `BASEPRI` rises `0x00 -> 0x50`, masking `IRQ 40` (`prio 5`) for `< 1 us` snapshot |

| Question | Authoritative Lookup | Section / Keyword |
| --- | --- | --- |
| Queue copy & blocking rules | [FreeRTOS Queue Guide](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/02-Queues-mutexes-and-semaphores/01-Queues) | `Blocking on Queues` |
| Binary semaphore event signaling | [FreeRTOS Binary Semaphore Guide](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/02-Queues-mutexes-and-semaphores/02-Binary-semaphores) | `Binary Semaphores` |
| Mutex ownership & simplified inheritance | [FreeRTOS Mutex Guide](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/02-Queues-mutexes-and-semaphores/04-Mutexes) & CubeF4 V1.28.3 `tasks.c` | `xTaskPriorityInherit` (`#L4014`), `xTaskPriorityDisinherit` (`#L4104`) |
| `BASEPRI` critical-section nesting | [FreeRTOS Critical Sections](https://www.freertos.org/Documentation/02-Kernel/04-API-references/04-RTOS-kernel-control/01-taskENTER_CRITICAL_taskEXIT_CRITICAL) & [W03-L22](W03_L22_freertos_tasks_states_delays_and_notifications.md) | `configMAX_SYSCALL_INTERRUPT_PRIORITY` |

---

## Feynman Recap

All FreeRTOS synchronization tools solve one question—**who is talking to whom, and are we passing data, signaling an event, or locking a shared room?** A **Queue** copies small data packets safely from an `ISR` or task into a waiting task. A **Binary Semaphore** (starting at `0`) lets an `ISR` drop one wakeup flag for a sleeping task. A **Mutex** (starting at `1` with `Owner` tracking) is a single key shared **only between tasks**: if urgent `Task H` needs the key while slow `Task L` holds it, **Priority Inheritance** temporarily lends `H`'s high priority to `L` so medium-priority `Task M` cannot jump ahead and stall `H`. And for a tiny `2`-word variable shared between a task and an `ISR`, a `1`-microsecond **`taskENTER_CRITICAL()`** region masks `BASEPRI` (`5..15`) cleanly without blocking.

---

## Spoken-English Interview Answers

### 1. What is the exact difference between a FreeRTOS Binary Semaphore and a Mutex?
* **Simple answer:** A **Binary Semaphore** is an ownerless event signal (starts at `0`, `ISR` gives, task takes), whereas a **Mutex** is a task-owned lock (starts at `1`, same task takes then gives, with **Priority Inheritance**).
* **How:** Under the hood both use `Queue_t` with `item_size = 0`, but a Mutex stores `pxMutexHolder` (`TCB` pointer) and calls `xTaskPriorityInherit()` when a higher-priority task blocks on it; an `ISR` has no `TCB`, so it can never take or give a Mutex.
* **Trade-off:** Using a Binary Semaphore as a lock disables Priority Inheritance and invites unbounded priority inversion; using a Mutex for `ISR` signaling is illegal in FreeRTOS.

### 2. Explain Priority Inversion and Priority Inheritance with `H`, `M`, and `L`.
* **Simple answer:** `Task H (3)` blocks waiting for a lock held by `Task L (1)`, and unrelated `Task M (2)` preempts `L`, starving `H`!
* **How:** With a FreeRTOS Mutex, blocking `H (3)` boosts `L`'s **Effective Priority** from `1` to `3` (`xTaskPriorityInherit`). Now `L (3)` outranks `M (2)`, finishes its short critical section immediately, and calls `xSemaphoreGive()`, which restores `L` to `1` and lets `H (3)` run right away.
* **Trade-off:** Inheritance prevents `M` from delaying `H`, though `H` still waits for `L`'s remaining lock hold time—so keep mutex sections short and never sleep (`vTaskDelay`) while holding a mutex.

### 3. Can an ISR take a Mutex if `xTicksToWait == 0`, and how do you protect data shared with an ISR instead?
* **Simple answer:** **No**—even with `0` wait, FreeRTOS mutexes manipulate task ownership (`pxCurrentTCB`) and inheritance lists, which are invalid in `Handler` mode.
* **How:** Either pass copies/handles from the `ISR` to a task via `xQueueSendFromISR()` / `vTaskNotifyGiveFromISR()`, or protect a tiny shared counter/struct snapshot with `taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()` (`BASEPRI = 0x50`).

### 4. Does a `5 ms` (`5-tick`) Mutex timeout guarantee the function finishes within `5 ms`?
* **Simple answer:** **No**—`5` ticks only caps how long `xSemaphoreTake()` waits to *acquire* the lock.
* **How:** If the timeout expires (`pdFALSE`), the caller did **not** get the lock (the owner still holds it!) and must abort without touching the shared resource; if acquired at tick `4`, the caller still has to execute the locked work and can still be preempted by higher-priority tasks/ISRs.

---

## Corrections

- 2026-11-17: Initial concept/source-trace publication.
- 2026-11-17 (revision 2): Rewrote `W04-L24` in plain-English Story Mode around one continuous `P1` example (`EXTI15_10` `ISR` + `Task H=3`, `Task M=2`, `Task L=1`), added the internal `Queue_t` comparison (`Queue` vs. `Binary Semaphore` vs. `Counting Semaphore` vs. `Mutex`), explicit `Where:` breakdowns, step-by-step `t = 0..8 ms` Priority Inversion vs. Priority Inheritance timelines, and a complete unified `p1_sync_demo.c` listing at the end. Next planned topic: `C04` stop/drain ownership.
