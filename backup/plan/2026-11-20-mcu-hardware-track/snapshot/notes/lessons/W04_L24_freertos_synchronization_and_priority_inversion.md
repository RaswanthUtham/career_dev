# W04-L24: FreeRTOS Queues, Semaphores, Mutexes, Critical Sections And Priority Inversion (`P1`)

> **Status:** Official-source & training-book reviewed, 2026-11-20 (rev 5).
> Grounded directly in upstream FreeRTOS Kernel source (`queue.c`, `tasks.c`),
> the official FreeRTOS training book (*Mastering the FreeRTOS Real Time Kernel*,
> Chapters 5, 7, and 8), and CubeF4 V1.28.3 (`FreeRTOS V10.3.1`, Cortex-M4F port)
> for `STM32F446RE` (`SYSCLK = 84 MHz`, `SysTick = 1 kHz`). C snippets and the
> unified end-of-lesson module illustrate exact FreeRTOS contracts; no target
> build, flash, or hardware timing run was performed here.

Prerequisites: [C02 reverse unwind](W02_L21_c02_three_resource_init_and_reverse_unwind.md),
[FreeRTOS tasks, delays & notifications](W03_L22_freertos_tasks_states_delays_and_notifications.md), and
[C03 payload ownership](W03_L23_c03_producer_consumer_payload_ownership.md).
Roadmap: [Week 4 synchronization](../../plans/five_month_development_roadmap.md#5-weekly-schedule).

---

## Know This First

| Term | Plain-English meaning (verified against official FreeRTOS `queue.c` / `tasks.c`) |
| --- | --- |
| **Task** (`Thread` mode, `PSP`) | An independent C loop with its own stack and `TCB_t` (*Task Control Block*). Can sleep (`Blocked` state, `0%` CPU) while waiting |
| **`ISR`** (`Handler` mode, `MSP`) | Hardware interrupt routine controlled by CPU hardware, **not** the RTOS scheduler. Even the lowest-priority `ISR` preempts the highest-priority Task; an `ISR` has **no `TCB_t`** and **can NEVER block/sleep** |
| **Queue-by-Copy** vs. **Queue-by-Reference** | FreeRTOS **copies item bytes (`memcpy`)** into internal queue RAM (*queue-by-copy*); to avoid copying a large `256 B` buffer, you copy its `4-byte` pointer or pool-slot ID (*queue-by-reference*) |
| **FIFO** vs. **LIFO** vs. **Mailbox** | **FIFO (`SendToBack`):** 1st in, 1st out; **LIFO (`SendToFront`):** urgent item jumps to the front; **Mailbox (`xQueueOverwrite`):** `1`-slot queue holding the latest value |
| **Non-Atomic / Torn Access** | Multi-instruction read-modify-write (`LOAD -> OR -> STORE`) or multi-field `struct` update interrupted mid-way, leaving half-old / half-new corrupted state |
| **Base vs. Effective Priority** | `uxBasePriority` = permanent priority assigned at creation; `uxPriority` = live priority used by the scheduler (temporarily raised during Mutex **Priority Inheritance**) |

**Opposite Priority Numbers & Cortex-M4 `PRIGROUP` Rule ([W03-L22](W03_L22_freertos_tasks_states_delays_and_notifications.md)):**
* **FreeRTOS Task Priority (Software):** Higher number = **MORE urgent** (`Task H = 3` > `Task M = 2` > `Task L = 1` > `Idle = 0`; set `configMAX_PRIORITIES >= 4`).
* **Cortex-M4 `NVIC` Interrupt Priority (Hardware):** Lower number = **MORE urgent** (`0` = highest urgency; `STM32F446RE` implements `4` priority bits = `0..15`).
  1. **Priority Grouping (`SCB->AIRCR.PRIGROUP <= 3`):** FreeRTOS Cortex-M4F (`vPortValidateInterruptPriority`) requires **all `4` priority bits to be preemption priority (`0` subpriority bits)**—call `NVIC_SetPriorityGrouping(3U)` in CMSIS (called `NVIC_PRIORITYGROUP_4` in STM32 HAL) before starting FreeRTOS!
  2. **`FromISR` Ceiling (`configMAX_SYSCALL_INTERRUPT_PRIORITY = 0x50`, i.e., `5 << 4`):** **Only `NVIC` priorities `5..15`** may call FreeRTOS `...FromISR()` functions! Leaving an `IRQ` at reset default **`0`** triggers `configASSERT` inside `portASSERT_IF_INTERRUPT_PRIORITY_INVALID()`. Priorities `0..4` stay unmasked for ultra-fast non-RTOS hardware trips.

---

## Unified `P1` Scenario Used Across Every Scene

Keep this one `STM32F446RE` `P1` diagram in mind—every FreeRTOS synchronization mechanism in Scenes 1–6 solves **one exact arrow** here:

```text
  [Over-Current / Button Pin PC13]
                 |
                 v
  EXTI15_10_IRQHandler (Hardware ISR, NVIC Priority 5)
     |-- Updates 2 related 32-bit stats (pulse_count, last_tick) ---(Critical Section)---> Task L
     |-- Copies 8-byte FaultRecord {seq, code} --------------------> [ g_fault_queue ] --> Task H
     +-- (Or signals 0-byte "DMA / FIFO Ready!" event) ------------> [ Binary/Count Sem ]-> Sensor Task

  Task H: FaultService (Priority 3 - HIGH) ---+
                                              +--> Share g_cfg struct via [ g_cfg_mutex ]
  Task L: StatusLogger (Priority 1 - LOW)  ---+    (or share UART2/I2C1 via a [ Gatekeeper Task ])

  Task M: MathWorker   (Priority 2 - MEDIUM) ----> Pure CPU filter math; NEVER touches g_cfg_mutex!
```

---

## Scene 1: Inside `Queue_t` In RAM — How Queues, Semaphores & Mutexes Really Work

In FreeRTOS (`queue.c`), **Queues, Binary Semaphores, Counting Semaphores, Mutexes, and Recursive Mutexes are ALL built from one C structure: `Queue_t` (`struct QueueDefinition`)**! Once you see what `Queue_t` allocates in RAM, you will never confuse them again.

---

### 1. Queue (`xQueueCreate(2, 8)`) — Exact Byte Slots & How FreeRTOS Knows *Which* Slot Holds Your Item

Suppose `FaultRecord` is `8` bytes (`uint32_t seq` + `uint32_t code`), and we call:
```c
QueueHandle_t g_fault_queue = xQueueCreate(2U, sizeof(FaultRecord)); /* uxLength = 2, uxItemSize = 8 */
```

#### A. What Does "`2` Slots Of `8` Bytes" Mean In RAM? Is It A 2D Array?
* **How to picture it in your head:** Picture a 2-row table `uint8_t slot[2][8]` (or `FaultRecord slot[2]`): **Maximum `2` faults can be stored inside the queue at any instant**—`Slot 0` holds `8` bytes, and `Slot 1` holds `8` bytes.
* **How `xQueueGenericCreate()` actually allocates it in `queue.c`:** FreeRTOS makes **one single `pvPortMalloc(sizeof(Queue_t) + (2 * 8))` call**, placing a flat **`16`-byte contiguous byte array** right after the `Queue_t` control struct:
  * `pcHead` points to byte `0` (**start of `Slot 0`**, `storage[0..7]`).
  * `pcHead + 8` is byte `8` (**start of `Slot 1`**, `storage[8..15]`).
  * `u.xQueue.pcTail` points to `pcHead + 16` (**one byte past the end**, used as the wrap-around boundary marker).

#### B. You Asked: *"If `uxMessagesWaiting == 1`, Which Slot Holds The Item—And How Does A Read Slot Become Empty?"*

First, **there is NO per-slot `empty` flag in RAM, and FreeRTOS NEVER zeroes (`memset`) a slot after reading it!**
Instead, FreeRTOS tracks everything using **`uxMessagesWaiting`** plus **three byte addresses** inside `Queue_t` (`queue.c`):

Suppose `xQueueCreate(2, 8)` places the `16`-byte storage array starting at RAM address **`0x100`**:
* **`pcHead = 0x100`** -> start of **`Slot 0`** (`bytes 0x100 .. 0x107`).
* **`0x108`** (`pcHead + 8`) -> start of **`Slot 1`** (`bytes 0x108 .. 0x10F`).
* **`u.xQueue.pcTail = 0x110`** (`pcHead + 16`) -> **wrap-around fence ONE BYTE AFTER `Slot 1`** (`0x110` is **not** a slot; whenever a cursor reaches `>= 0x110`, FreeRTOS wraps it back to `pcHead = 0x100`!).

Now look at the **exact asymmetry** between `pcWriteTo` and `u.xQueue.pcReadFrom` in official `FreeRTOS-Kernel/queue.c`:

```c
/* 1. Reset / Create (xQueueGenericReset in queue.c): */
pxQueue->u.xQueue.pcTail     = pxQueue->pcHead + (2U * 8U);         /* 0x100 + 16 = 0x110 (fence!)     */
pxQueue->uxMessagesWaiting   = 0U;
pxQueue->pcWriteTo           = pxQueue->pcHead;                     /* 0x100 (START of Slot 0)         */
pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead + ((2U - 1U) * 8U);  /* 0x108 (START of Slot 1, NOT 0x110!) */

/* 2. Write to back (prvCopyDataToQueue in queue.c): COPY FIRST, THEN +8 & WRAP! */
memcpy(pxQueue->pcWriteTo, pvItemToQueue, 8U);                      /* 1) Copy into current pcWriteTo  */
pxQueue->pcWriteTo += 8U;                                           /* 2) Advance +8                   */
if (pxQueue->pcWriteTo >= pxQueue->u.xQueue.pcTail) {               /* 3) Hit 0x110 fence?             */
    pxQueue->pcWriteTo = pxQueue->pcHead;                           /*    Wrap back to 0x100 (Slot 0)! */
}
pxQueue->uxMessagesWaiting++;                                       /* 4) Count unread items (+1)      */

/* 3. Read from front (prvCopyDataFromQueue + xQueueReceive in queue.c): +8 & WRAP FIRST, THEN COPY! */
pxQueue->u.xQueue.pcReadFrom += 8U;                                 /* 1) Advance +8 FIRST!            */
if (pxQueue->u.xQueue.pcReadFrom >= pxQueue->u.xQueue.pcTail) {     /* 2) Hit 0x110 fence?             */
    pxQueue->u.xQueue.pcReadFrom = pxQueue->pcHead;                 /*    Wrap back to 0x100 (Slot 0)! */
}
memcpy(pvBuffer, pxQueue->u.xQueue.pcReadFrom, 8U);                 /* 3) Copy out of that slot        */
pxQueue->uxMessagesWaiting--;                                       /* 4) Decrement count (-1)         */
/* Notice: Slot bytes are NEVER erased! uxMessagesWaiting == 0 is what marks the queue logically EMPTY! */
```

* **Why did `xQueueGenericReset()` start `pcReadFrom` at `0x108` (start of `Slot 1`) instead of `0x110` (`pcTail`)?**
  Because `prvCopyDataFromQueue()` **always adds `+8` BEFORE checking `>= pcTail` and copying**! Starting `pcReadFrom` at `0x108` means the very first read computes `0x108 + 8 = 0x110 >= pcTail (0x110)` -> wraps `pcReadFrom` to **`0x100` (`Slot 0`)** -> and copies `Slot 0`! *(In our earlier single-line diagram, placing two `^` arrows on one row visually pushed `pcReadFrom` toward the right edge of `Slot 1`—putting `pcWriteTo` and `pcReadFrom` on separate rows below shows their exact byte addresses!)*
* **How does `Slot 0` "become empty" in Step 2?**
  Physically in RAM (`0x100..0x107`), the old bytes `{17, 0xE1}` **stay right there** (`[STALE]`). Logically, `uxMessagesWaiting` drops `1 -> 0`:
  1. Any new `xQueueReceive()` checks `if (uxMessagesWaiting > 0)` **first**, sees `0`, and refuses to read—so stale `{17, 0xE1}` can **never** be read twice.
  2. Free space is `uxLength - uxMessagesWaiting = 2 - 0 = 2` slots, so when `pcWriteTo` later wraps back to `0x100` (in Step 4), it **legally overwrites those already-read `[STALE]` bytes in `Slot 0`**! *(Only **unread** slots are protected from overwrite!)*

Watch every byte address (`0x100`, `0x108`, `0x110`) across all 5 steps:

```text
Addresses:         pcHead = 0x100                0x108                         pcTail = 0x110 (wrap fence!)
                   v                             v                             v
                   +-----------------------------+-----------------------------+
                   | Slot 0 (bytes 0x100..0x107) | Slot 1 (bytes 0x108..0x10F) |
                   +-----------------------------+-----------------------------+

STEP 0 — Right After xQueueCreate(2, 8) (uxMessagesWaiting = 0 -> EMPTY):
                   | Slot 0: [ -- uninit -- ]    | Slot 1: [ -- uninit -- ]    |
  pcWriteTo  =0x100^ (next Send copies to 0x100)
  pcReadFrom =0x108                              ^ (starts at START of Slot 1 = 0x108, NOT 0x110!)

STEP 1 — Send Fault #1 {17, 0xE1}: memcpy(0x100), then pcWriteTo = 0x100 + 8 = 0x108 (uxMessagesWaiting = 1):
                   | Slot 0: {17, 0xE1} [UNREAD] | Slot 1: [ -- uninit -- ]    |
  pcWriteTo  =0x108                              ^ (moved to 0x108; 0x108 < 0x110 so no wrap)
  pcReadFrom =0x108                              ^ (untouched by Send -> ALSO at 0x108!)

STEP 2 — Task H Calls xQueueReceive(): pcReadFrom = 0x108 + 8 = 0x110 >= 0x110 -> WRAPS to 0x100,
         THEN memcpy(&rx, 0x100, 8), THEN uxMessagesWaiting = 1 - 1 = 0 (QUEUE LOGICALLY EMPTY!):
                   | Slot 0: {17, 0xE1} [STALE!] | Slot 1: [ -- uninit -- ]    |
  pcWriteTo  =0x108                              ^ (still 0x108 -> next Send writes Slot 1!)
  pcReadFrom =0x100^ (wrapped to 0x100 & read Slot 0! Old bytes stay in RAM, uxMessagesWaiting=0 marks empty!)

STEP 3 — Send Fault #2 {18, 0xE2}: memcpy(0x108), then pcWriteTo = 0x108 + 8 = 0x110 >= 0x110 -> WRAPS to 0x100
         (uxMessagesWaiting = 1 -> THE 1 UNREAD ITEM IS NOW IN SLOT 1, NOT SLOT 0!):
                   | Slot 0: {17, 0xE1} [STALE!] | Slot 1: {18, 0xE2} [UNREAD] |
  pcWriteTo  =0x100^ (wrapped to 0x100 -> ready to reuse STALE Slot 0 on next Send!)
  pcReadFrom =0x100^ (still 0x100 -> next Receive steps 0x100 + 8 = 0x108 and reads Slot 1!)

STEP 4 — Send Fault #3 {19, 0xE3} Before Step 3 Is Read: memcpy(0x100) overwrites STALE Slot 0,
         pcWriteTo = 0x100 + 8 = 0x108 (uxMessagesWaiting = 2 -> FULL, BOTH SLOTS UNREAD!):
                   | Slot 0: {19, 0xE3} [UNREAD] | Slot 1: {18, 0xE2} [UNREAD] |
  pcWriteTo  =0x108                              ^ (cannot write while uxMessagesWaiting == 2!)
  pcReadFrom =0x100^ (1st Receive: 0x100+8=0x108 reads {18}; 2nd Receive: 0x108+8=0x110->0x100 reads {19}!)

STEP 5 — Send Fault #4 {20, 0xE4} While Still FULL (uxMessagesWaiting == 2 == uxLength):
                   | Slot 0: {19, 0xE3} [UNREAD] | Slot 1: {18, 0xE2} [UNREAD] |
  pcWriteTo  =0x108                              ^ (UNCHANGED! SendFromISR returns errQUEUE_FULL = 0!)
  pcReadFrom =0x100^ (UNCHANGED! Zero unread bytes overwritten; Fault #4 is rejected!)
```

#### C. Why FreeRTOS Copies Bytes (*Queue-by-Copy*) Instead Of Storing Only References (*Official Book Ch 5.2.1 & 5.5*)
* **Small data (`<= 16` bytes, e.g., `FaultRecord`):** Queue the `struct` itself (`sizeof(FaultRecord)`). Because FreeRTOS `memcpy`s all `8` bytes into `Slot 0`/`Slot 1` immediately, the sender's local stack variable `FaultRecord rec` can safely disappear the instant `xQueueSend` / `xQueueSendFromISR` returns—and works even across `MPU` privilege boundaries!
* **Large data (`64..512` bytes, e.g., `DMA` frame):** Never `memcpy` `512` bytes through a queue! Keep the payload in a static `C03` pool buffer and queue a `4-byte` pointer or small `8-byte` tagged descriptor (`typedef struct { EventId_t id; void *buf; } AppMsg_t;` — *Official Book Listing 5.16*), where producer and consumer strictly obey buffer ownership!

---

### 2. How Binary Semaphores, Counting Semaphores & Mutexes Reuse That Same `Queue_t`

Look at the `union u` inside `Queue_t` in official `queue.c`:
```c
/* Simplified directly from FreeRTOS-Kernel/queue.c: */
typedef struct QueueDefinition {
    int8_t * pcHead;                        /* Storage start (or NULL == queueQUEUE_IS_MUTEX!) */
    int8_t * pcWriteTo;
    union {
        QueuePointers_t xQueue;             /* { pcTail, pcReadFrom } -> used by Queues        */
        SemaphoreData_t xSemaphore;         /* { xMutexHolder, uxRecursiveCallCount } -> Mutex */
    } u;
    List_t               xTasksWaitingToSend;    /* Priority-ordered list of blocked senders   */
    List_t               xTasksWaitingToReceive; /* Priority-ordered list of blocked receivers */
    volatile UBaseType_t uxMessagesWaiting;      /* Item count (Queue) OR Token count (Sem/Mtx)*/
    UBaseType_t          uxLength;               /* Max slots (Queue)  OR Max count (Sem/Mtx)  */
    UBaseType_t          uxItemSize;             /* Bytes per item (0 for ALL Semaphores/Mutex)*/
    /* ... lock counters cRxLock, cTxLock ... */
} Queue_t;
```

When `uxItemSize == 0`, FreeRTOS allocates **`0` bytes of `storage[]` array** (`uxLength * 0 = 0`) and never calls `memcpy`—it simply increments (`Give`) and decrements (`Take`) **`uxMessagesWaiting`**!
* **How does `queue.c` tell a Semaphore apart from a Mutex if both have `uxItemSize == 0`?**
  Look at `prvInitialiseNewQueue()` vs. `prvInitialiseMutex()` in `queue.c`:
  * For a **Binary or Counting Semaphore**, `pcHead` is set to `(int8_t *)pxNewQueue` (a non-`NULL` dummy address), so `xSemaphoreTake()` skips owner tracking and priority inheritance.
  * For a **Mutex**, `prvInitialiseMutex()` overwrites **`pcHead = NULL`** (`#define queueQUEUE_IS_MUTEX NULL`)! Whenever `xSemaphoreTake()` or `xSemaphoreGive()` sees `pcHead == NULL`, it activates `u.xSemaphore.xMutexHolder` and Priority Inheritance!
* *(Legacy API warning from `semphr.h`: always use the function **`xSemaphoreCreateBinary()`**, which starts at **`0`**, and **never** the deprecated `vSemaphoreCreateBinary()` macro, which pre-gave the semaphore to start at `1`!)*

| FreeRTOS Object | Internal `xQueueGenericCreate` Setup | Starts At (`uxMessagesWaiting`) | Tracks Owner Task? | Priority Inheritance? | Legal In `ISR`? | Primary Use Case |
| --- | --- | --- | --- | --- | --- | --- |
| **Queue** (`xQueueCreate(N, S)`) | `uxLength = N`, `uxItemSize = S` (`N*S` RAM bytes) | **`0`** (Empty) | No | No | **YES** (`FromISR`) | Pass `S`-byte copies (`ISR -> Task` or `Task -> Task`) |
| **Binary Semaphore** (`xSemaphoreCreateBinary()`) | `uxLength = 1`, `uxItemSize = 0`, `pcHead != NULL` | **`0`** (Empty doorbell) | No | No | **YES** (`FromISR`) | `ISR` signals sleeping task ("hardware event happened!") |
| **Counting Semaphore** (`xSemaphoreCreateCounting(M, I)`) | `uxLength = M`, `uxItemSize = 0`, `pcHead != NULL` | **`I`** (`0` for events; `M` for pool) | No | No | **YES** (`FromISR`) | Count rapid `ISR` bursts (`0..M`) OR free pool buffers (`M..0`) |
| **Mutex** (`xSemaphoreCreateMutex()`) | `uxLength = 1`, `uxItemSize = 0`, `pcHead = NULL` | **`1`** (**Unlocked key!**) | **YES** (`xMutexHolder`) | **YES!** | **NO!** (`semphr.h` + `configASSERT` if held) | Lock shared data/bus between **Tasks** (`Take` then `Give`) |
| **Recursive Mutex** (`xSemaphoreCreateRecursiveMutex()`) | Mutex + `uxRecursiveCallCount` (`0..N`) | **`1`** (Unlocked) | **YES** (`xMutexHolder`) | **YES!** | **NO!** | Same task locks nested helper functions without self-deadlock |
| **Task Notification** ([W03-L22](W03_L22_freertos_tasks_states_delays_and_notifications.md)) | **No `Queue_t` at all!** Uses `uint32_t` inside `TCB_t` | **`0`** | Fixed `1` receiver task | No | **YES** (`FromISR`) | Fastest (`~45%` faster, `0` extra RAM) `ISR -> 1 known Task` wakeup |

---

## Scene 2: Queue Deep-Dive — Every Timing Window, Full-Queue Rule & All Queue APIs (`Ch 5`)

### 1. All 4 Timing Scenarios On Our `2`-Slot `FaultRecord` Queue (`xQueueCreate(2, 8)`)

Let `EXTI15_10_IRQHandler` (`NVIC` Priority `5`) send `FaultRecord`s into `g_fault_queue` (`2` slots), and let **`Task H` (`Priority 3`)** read them with `xQueueReceive()`. Here is what happens in **every** possible arrival timing:

1. **Scenario A — 2nd Fault Arrives *BEFORE* `Task H` Runs:**
   * Suppose `Task H` was briefly blocked or another `ISR` was running when two over-current spikes hit `50 us` apart:
     * `Fault #1 {17, 0xE1}` copies into **`Slot 0`** (`uxMessagesWaiting = 1`).
     * `Fault #2 {18, 0xE2}` copies into **`Slot 1`** (`uxMessagesWaiting = 2`, queue is now **Full**).
   * When `ISR` exits via `portYIELD_FROM_ISR`, `Task H` runs `xQueueReceive` in its `for(;;)` loop: 1st iteration pops `Slot 0` (`{17, 0xE1}`), 2nd iteration immediately pops `Slot 1` (`{18, 0xE2}`), and 3rd iteration sees `uxMessagesWaiting == 0` and goes back to `Blocked` sleep!
2. **Scenario B — 2nd Fault Arrives *DURING* `Task H` Execution (Two Sub-Cases!):**
   * **Sub-case B1 (`IRQ 40` fires during the `~20` CPU cycles while `Task H` is inside `xQueueReceive` copying `Slot 0`):** Inside `xQueueReceive`, FreeRTOS wraps `prvCopyDataFromQueue()` in `taskENTER_CRITICAL()` (`BASEPRI = 0x50`). Hardware holds `IRQ 40` (`Priority 5`) **Pending** for `< 1 us` until `Task H` finishes copying `Slot 0` and setting `uxMessagesWaiting = 0`; the instant `taskEXIT_CRITICAL()` restores `BASEPRI = 0x00`, `EXTI15_10_IRQHandler` enters and safely writes `Fault #2` into **`Slot 1`** (`uxMessagesWaiting = 1`)!
   * **Sub-case B2 (`IRQ 40` fires while `Task H` is running C code *after* `xQueueReceive` already returned `Fault #1`):** `Slot 0` is already freed (`uxMessagesWaiting == 0`). Because even the lowest hardware `ISR` preempts the highest-priority task, `EXTI15_10_IRQHandler` pauses `Task H`, copies `Fault #2` into **`Slot 1`** (`uxMessagesWaiting: 0 -> 1`), and returns right back to `Task H`. `Task H` finishes handling `Fault #1`, loops around to `xQueueReceive()`, immediately finds `Fault #2` waiting in **`Slot 1`** without sleeping, and handles it!
3. **Scenario C — 2nd Fault Arrives *AFTER* `Task H` Finished And Went Back To Sleep:**
   * `Task H` already finished `Fault #1` and is `Blocked` inside `xQueueReceive` (`uxMessagesWaiting == 0`, `pcReadFrom = &Slot 0`, `pcWriteTo = &Slot 1`).
   * `IRQ 40` fires, copies `Fault #2` into **`Slot 1`**, moves `Task H` from `xTasksWaitingToReceive` to `Ready` (`woken = pdTRUE`), and `portYIELD_FROM_ISR(woken)` switches straight into `Task H` to read **`Slot 1`**.
4. **Scenario D — Queue Is FULL (`Slot 0` AND `Slot 1` Still Unread, `uxMessagesWaiting == 2`) And A 3rd `Fault #3` Arrives!**
   * **Does `xQueueSend` or `xQueueSendFromISR` overwrite `Slot 0` or `Slot 1`? NO — NEVER!**
   * **If sent from an `ISR` (`xQueueSendFromISR`):** An `ISR` cannot wait. FreeRTOS sees `uxMessagesWaiting == 2`, leaves **`Slot 0` and `Slot 1` completely untouched**, and immediately returns **`errQUEUE_FULL` (`0`)**! The **new 3rd item is rejected (*Drop-Newest*)**—which is why production `ISR`s check `!= pdPASS` and increment `g_queue_drops++`.
   * **If sent from a Task (`xQueueSend(q, &item, wait_ticks)`):** If `wait_ticks > 0`, FreeRTOS puts the **sending task to sleep (`Blocked`)** on `xTasksWaitingToSend` (waking the **highest-priority** waiting sender first as soon as `Task H` reads `Slot 0`!). If `wait_ticks` expires while still full (or `wait_ticks == 0`), `xQueueSend` returns **`errQUEUE_FULL`** without overwriting anything!

*(Multiple sleepers rule from Official Book Ch 5.2.3/5.2.4: if two tasks block waiting on the same queue, FreeRTOS **always unblocks the highest-priority waiting task first**; if their priorities are equal, it unblocks the task that has been waiting the longest.)*

---

### 2. Every Official Queue API Function With A Concrete Use Case & Code

#### A. Creating Queues (`xQueueCreate` vs. `xQueueCreateStatic`) & Standard FIFO (`xQueueSendToBack` / `xQueueReceive`)
* **`xQueueSend(q, &item, ticks)` is an exact macro alias for `xQueueSendToBack(q, &item, ticks)`** (normal FIFO order).
* **Use Case:** In dynamic-heap builds (`heap_4.c`), use `xQueueCreate`; in safety/MISRA/automotive builds with zero runtime heap (`configSUPPORT_STATIC_ALLOCATION = 1`), use `xQueueCreateStatic` with compile-time arrays!

```c
typedef struct {
    uint32_t seq;
    uint32_t code;
} FaultRecord;                              /* 8 bytes per item                                     */

/* Option 1 (Dynamic Heap): allocates Queue_t + 16-byte storage from FreeRTOS heap */
static QueueHandle_t g_fault_q;

/* Option 2 (Static RAM — zero malloc!): caller supplies StaticQueue_t + 16-byte array */
static StaticQueue_t g_fault_q_struct;
static uint8_t       g_fault_q_storage[2U * sizeof(FaultRecord)];

void queue_create_examples(void)
{
    g_fault_q = xQueueCreate(2U, sizeof(FaultRecord));
    /* Or static equivalent:
     * g_fault_q = xQueueCreateStatic(2U, sizeof(FaultRecord), g_fault_q_storage, &g_fault_q_struct); */
}
```

#### B. Urgent Jump-The-Line Send (`xQueueSendToFront` / `xQueueSendToFrontFromISR` — *Ch 5.3.2 & Ch 8.4*)
* **Exact Pointer Mechanics in `queue.c` (`prvCopyDataToQueue` `else` branch):**
  Instead of touching `pcWriteTo` (back of line), `xQueueSendToFront` writes into **`u.xQueue.pcReadFrom`** and moves `pcReadFrom` **backward** by `-8` bytes (wrapping `< pcHead` to `pcTail - 8`):
  ```c
  memcpy(pxQueue->u.xQueue.pcReadFrom, pvItemToQueue, 8U);            /* 1) Copy into pcReadFrom     */
  pxQueue->u.xQueue.pcReadFrom -= 8U;                                 /* 2) Step BACKWARD -8         */
  if (pxQueue->u.xQueue.pcReadFrom < pxQueue->pcHead) {               /* 3) Stepped < 0x100?         */
      pxQueue->u.xQueue.pcReadFrom = pxQueue->u.xQueue.pcTail - 8U;   /*    Wrap to 0x110 - 8 = 0x108*/
  }
  pxQueue->uxMessagesWaiting++;
  ```
* **Zero-Gap Address Trace (`0x100` / `0x108` / `0x110`):**
  Suppose `Slot 0` (`0x100`) already holds normal `{17, 0xE1}` (`STEP 1`: `uxMessagesWaiting = 1`, `pcWriteTo = 0x108`, `pcReadFrom = 0x108`). Now urgent `{18, 0xFF}` arrives via **`xQueueSendToFrontFromISR`**:
  1. `memcpy(0x108, &urgent, 8)` writes urgent `{18, 0xFF}` into **`Slot 1` (`0x108`)**.
  2. `pcReadFrom` steps backward `0x108 - 8 = 0x100` (`>= 0x100`, no wrap), and `uxMessagesWaiting` becomes `2`.
  3. Next `xQueueReceive()`: `pcReadFrom` (`0x100`) `+ 8 = 0x108` -> reads **`Slot 1` (`{18, 0xFF}` URGENT FIRST!)**; 2nd `xQueueReceive()`: `0x108 + 8 = 0x110 >= 0x110 -> 0x100` -> reads **`Slot 0` (`{17, 0xE1}` SECOND!)**!
  *(Still never overwrites unread items: if `uxMessagesWaiting == 2`, `xQueueSendToFrontFromISR` returns `errQUEUE_FULL`!)*
* **Simple Use Case:** Normal sensor logs go to the back (`xQueueSendToBack`), while an emergency **Over-Temperature Shutdown command** calls `xQueueSendToFrontFromISR()` so `Task H` executes the shutdown **before** older routine items!

```c
void emergency_estop_isr_push(uint32_t fault_seq)
{
    FaultRecord urgent = { .seq = fault_seq, .code = 0xFFU }; /* 0xFF = Emergency Stop! */
    BaseType_t  woken  = pdFALSE;

    /* Jumps to the FRONT of g_fault_q so Task H reads 0xFF before older queued items: */
    if (xQueueSendToFrontFromISR(g_fault_q, &urgent, &woken) != pdPASS) {
        /* Queue was already 2/2 full -> trigger direct hardware safe-state pin immediately! */
    }
    portYIELD_FROM_ISR(woken);
}
```

#### C. When You *DO* Want Overwrite: `1`-Slot "Mailbox" (`xQueueOverwrite` / `xQueueOverwriteFromISR` + `xQueuePeek` — *Official Book Ch 5.7*)
* **You asked: *"What if we DO want a new value to overwrite an unread old value?"***
* FreeRTOS provides **`xQueueOverwrite(xMailbox, &item)`** and **`xQueueOverwriteFromISR(xMailbox, &item, &woken)`** specifically for this—with **one strict `configASSERT` rule in `queue.c`**:
  ```c
  configASSERT( !( ( xCopyPosition == queueOVERWRITE ) && ( pxQueue->uxLength != 1 ) ) );
  ```
  * **`xQueueOverwrite` MUST ONLY be used on a queue of length `1` (`xQueueCreate(1U, sizeof(LatestTelemetry))`)!**
  * **Exact `Slot 0` trace (`pcHead = 0x100`, `pcTail = 0x108`, `pcReadFrom = 0x100`):** `xQueueOverwrite` copies the new `8` bytes into `pcReadFrom` (**`Slot 0` = `0x100`**), steps `0x100 - 8 = 0x0F8 < 0x100` -> wraps `pcReadFrom` right back to `0x108 - 8 = 0x100`, and if `uxMessagesWaiting` was already `1`, decrements `1 -> 0` before `+ 1` so **`uxMessagesWaiting` stays `1`**! It **always overwrites `Slot 0`** and **always returns `pdPASS`**.
  * Pair it with **`xQueuePeek(xMailbox, &out, ticks)`** (or `xQueuePeekFromISR`), which saves `pcOriginalReadPosition = pcReadFrom`, copies `Slot 0`, and **restores `pcReadFrom`** without decrementing `uxMessagesWaiting` (`1` stays `1`—and if `0` before the first publish, `xQueuePeek(..., 0U)` returns `errQUEUE_EMPTY`), so multiple tasks (`DisplayTask`, `LoggerTask`, `CommsTask`) can all read the latest value anytime!

```c
typedef struct {
    TickType_t timestamp_ticks;
    uint16_t   vbat_mv;
} TelemetryMailbox_t;

static QueueHandle_t g_vbat_mailbox;        /* MUST be length 1: xQueueCreate(1, sizeof(...))!      */

/* Writer (ADC Task or ISR): ALWAYS overwrites Slot 0 with newest battery reading */
void vbat_mailbox_publish_task(uint16_t latest_mv)
{
    TelemetryMailbox_t sample = {
        .timestamp_ticks = xTaskGetTickCount(),
        .vbat_mv         = latest_mv
    };
    (void)xQueueOverwrite(g_vbat_mailbox, &sample); /* Overwrites Slot 0; always returns pdPASS     */
}

/* Any Reader Task: reads Slot 0 WITHOUT removing it so other tasks can read it too! */
bool vbat_mailbox_read_latest(TelemetryMailbox_t *out)
{
    if (out != NULL) { *out = (TelemetryMailbox_t){ 0U, 0U }; }
    if (out == NULL) { return false; }
    return (xQueuePeek(g_vbat_mailbox, out, pdMS_TO_TICKS(5U)) == pdPASS);
}
```

#### D. Querying & Resetting (`uxQueueMessagesWaiting`, `uxQueueSpacesAvailable`, `xQueueReset`)
* `uxQueueMessagesWaiting(q)` (or `...FromISR`): returns unread count (`0..uxLength`) — e.g., checking queue watermark diagnostics.
* `uxQueueSpacesAvailable(q)`: returns free slot count (`uxLength - uxMessagesWaiting`).
* `xQueueReset(q)`: resets `uxMessagesWaiting = 0`, `pcWriteTo = pcHead`, and `pcReadFrom = pcHead + ((uxLength - 1U) * uxItemSize)`, unblocks one task waiting to send if any was blocked on full, and always returns `pdPASS`.
* *(Multi-source tip from Official Book Ch 5.6: if one task must wait on several sources, FreeRTOS recommends a **single queue of tagged structs** `{ EventType, Payload }` over `xQueueCreateSet()`, because a single tagged queue uses less RAM and runs faster.)*

---

## Scene 3: Deferred Interrupt Processing, `xHigherPriorityTaskWoken` & Semaphores (`Ch 7`)

### 1. Why FreeRTOS Has Separate `...FromISR` Functions & How `xHigherPriorityTaskWoken` Works (*Ch 7.2–7.3*)

Why doesn't `xQueueSendFromISR()` or `xSemaphoreGiveFromISR()` switch tasks *inside* the function the way task-level `xQueueSend()` does?
1. **Inside an `ISR`, the CPU is in hardware `Handler` mode (`MSP`):** An `ISR` might call FreeRTOS `FromISR` three times in a loop (e.g., draining 3 bytes from a UART FIFO). Switching context inside every call would waste cycles and break the rest of the `ISR`!
2. **How `woken` + `portYIELD_FROM_ISR(woken)` solves it cleanly:**
   * You initialize **`BaseType_t woken = pdFALSE;`** **once** at the top of your `ISR`.
   * Every `...FromISR(..., &woken)` call checks: *"Did I just unblock a task whose priority is strictly higher than the task this `ISR` interrupted?"* If yes, it sets **`woken = pdTRUE`** (and never resets it back to `pdFALSE`, so you can pass the **same `&woken`** to multiple `FromISR` calls!).
   * At the very end of the `ISR`, **`portYIELD_FROM_ISR(woken)`** checks `if (woken != pdFALSE) { SCB->ICSR = SCB_ICSR_PENDSVSET_Msk; }`. When the `ISR` exits (`bx lr`), hardware tail-chains straight into `PendSV_Handler` and returns **directly into the high-priority handler task** (*Deferred Interrupt Processing*, Official Book Fig 7.1)!
   * **What if you forget `portYIELD_FROM_ISR(woken)`?** `Task H` becomes `Ready`, **but does NOT run** until the next `SysTick` interrupt (up to **`1 full millisecond` of unnecessary latency** at `1 kHz`!).

---

### 2. Binary Semaphore (`xSemaphoreCreateBinary`) & The 3-Interrupt Burst Trap (*Ch 7.4 & 7.4.4*)

A Binary Semaphore is a `1`-token queue (`uxLength = 1, uxItemSize = 0`, **starts at `0`**) used to **synchronize a task with an interrupt**:
* **`ISR` *Gives* (`xSemaphoreGiveFromISR`: `0 -> 1`)** even though it never took it!
* **Handler `Task` *Takes* (`xSemaphoreTake`: `1 -> 0`)** and **never gives it back**!

#### What Happens When `1`, `2`, Or `3` Rapid Hardware Interrupts Hit? (*Official Book Fig 7.6 & Fig 7.7*)
Suppose `Task H` takes `200 us` to process one sensor burst, and hardware fires **three** interrupts at `t = 0 us`, `t = 50 us`, and `t = 100 us`:
1. **`t = 0 us` (`IRQ #1`):** `GiveFromISR` sets `0 -> 1`. `Task H` wakes, calls `xSemaphoreTake` (**`1 -> 0`**), and starts its `200 us` processing of Event #1.
2. **`t = 50 us` (`IRQ #2` arrives *during* `Task H`!):** Semaphore is currently `0`! `IRQ #2` preempts `Task H`, calls `GiveFromISR` (**`0 -> 1`**, returns `pdPASS`), and returns to `Task H`. **Event #2 is latched safely!**
3. **`t = 100 us` (`IRQ #3` arrives *still during* `Task H`'s first `200 us`!):** Semaphore is **already `1`**! `GiveFromISR` (`xQueueGiveFromISR`) sees `uxMessagesWaiting == 1`, leaves it at `1`, and returns **`errQUEUE_FULL`** (`0`, numerically equal to `pdFAIL`, whereas success is `pdPASS = 1`)! **Event #3 is LOST (coalesced into Event #2)!**

```text
Official FreeRTOS Ch 7.4.4 Pattern To Prevent Missing Hardware Events + Detect Dead Hardware:
  1. Use a FINITE timeout (not portMAX_DELAY!) so a stuck sensor that stops firing IRQs is caught!
  2. After xSemaphoreTake() succeeds, drain ALL pending items in a while() loop before sleeping again!
```

```c
static SemaphoreHandle_t g_uart_rx_sem;     /* Created via xSemaphoreCreateBinary() -> starts at 0  */

/* Hardware ISR: clears peripheral flag / pushes byte to ring, then gives binary token */
void USART2_IRQHandler(void)
{
    BaseType_t woken = pdFALSE;             /* Local stack flag: ALWAYS starts 0 on each ISR entry  */
    if ((USART2->SR & (1UL << 5)) != 0U) {  /* RXNE == 1                                            */
        ring_push_from_isr((uint8_t)USART2->DR);
        /* Sets SEMAPHORE count: 0 -> 1 (if semaphore count is already 1 from an earlier IRQ,
         * semaphore count stays 1; woken becomes pdTRUE only if a higher-prio sleeper woke up!): */
        (void)xSemaphoreGiveFromISR(g_uart_rx_sem, &woken);
    }
    portYIELD_FROM_ISR(woken);
}

/* Deferred Handler Task (Official Book Listing 7.10 pattern): */
static void Task_UartRxHandler(void *arg)
{
    (void)arg;
    uint8_t byte = 0U;

    for (;;) {
        /* Finite 500 ms timeout detects dead/disconnected sensor wire instead of hanging forever: */
        if (xSemaphoreTake(g_uart_rx_sem, pdMS_TO_TICKS(500U)) == pdPASS) {
            /* Drain ALL bytes currently in the ring before sleeping again -> 0 lost bursts! */
            while (ring_pop_from_task(&byte)) {
                parse_protocol_byte(byte);
            }
        } else {
            /* 500 ms elapsed with 0 interrupts -> check & clear UART hardware error flags! */
            uart2_check_and_recover_errors();
        }
    }
}
```

---

### 3. Counting Semaphore (`xSemaphoreCreateCounting(uxMaxCount, uxInitialCount)` — *Ch 7.5*)

Requires `configUSE_COUNTING_SEMAPHORES = 1`. Think of it as a token bucket `0 .. uxMaxCount` (`0` item bytes) with **two official patterns**:

1. **Pattern A — Counting Rapid Events (`uxInitialCount = 0`, `uxMaxCount = 10`):**
   * **Use Case:** In the 3-interrupt burst above (`t = 0, 50, 100 us`), `xSemaphoreGiveFromISR` increments `uxMessagesWaiting` **`0 -> 1 -> 2 -> 3`**! Nothing is lost up to `10` backlogged events; the handler task wakes and loops `xSemaphoreTake` `3` times (`3 -> 2 -> 1 -> 0`). *(Tip: if only **one** task waits on the `ISR`, [W03-L22](W03_L22_freertos_tasks_states_delays_and_notifications.md)'s `vTaskNotifyGiveFromISR` + `ulTaskNotifyTake(pdFALSE, ...)` does exact event counting in `0` extra RAM!)*
2. **Pattern B — Managing A Pool Of `N` Identical Resources (`uxInitialCount = 3`, `uxMaxCount = 3`):**
   * **Use Case:** You have `3` static `DMA` buffers (`C03` pool). All `3` start free (`Initial = 3`). A task must `xSemaphoreTake` a permit before claiming a buffer (`3 -> 2 -> 1 -> 0`; a 4th requester sleeps until a buffer is freed!), and calls `xSemaphoreGive` (`+1`) after releasing the buffer!

```c
/* Pattern A (Event Counter): max 10 latched pulses, starts at 0 because 0 events occurred yet */
SemaphoreHandle_t g_pulse_sem = xSemaphoreCreateCounting(10U, 0U);

/* Pattern B (Resource Pool): 3 DMA buffers, starts at 3 because all 3 buffers start FREE */
SemaphoreHandle_t g_pool_sem  = xSemaphoreCreateCounting(3U, 3U);
```

---

## Scene 4: Protecting Shared Resources — Critical Sections vs. Scheduler Lock (`Ch 8.1–8.2`)

### 1. Why Shared Variables & Registers Corrupt Without Protection (*Official Book Listing 8.1*)

Even a single line of C like ` flags |= 0x01U; ` compiles into **three separate Cortex-M instructions**:
```text
1. LDR  r1, [r0]       ; Read old flags (say 0x00) into CPU register r1
   <--- PREEMPTED HERE by Task B or an ISR that sets bit 1 (flags becomes 0x02 in RAM)!
2. ORR  r1, r1, #0x01  ; Task A resumes with stale r1 (0x00 | 0x01 = 0x01)
3. STR  r1, [r0]       ; Task A writes 0x01 -> WIPES OUT Task B's 0x02 bit!
```
The same corruption hits multi-field `struct` updates (`g_cfg`), `64-bit` timestamps (`uint64_t` takes two `32-bit` loads/stores on Cortex-M4!), and functions with `static` local state (*non-reentrant functions*).

---

### 2. Comparing The 3 Fast CPU/Scheduler Guards (`Ch 8.2`)

When an **`ISR`** shares variables with a **Task**, **you CANNOT use a Mutex** (an `ISR` cannot block or own a Mutex!). Choose from these three official guards:

| API Pair | Who Calls It? | Masks Hardware `ISR`s? | Stops Task Switches? | Official Use Case & Limits |
| --- | --- | --- | --- | --- |
| **`taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()`** | **Task only** | **YES:** sets `BASEPRI = 0x50` (masks `NVIC` prio `5..15`; prio `0..4` still run!) | **YES** (`PendSV` & `SysTick` at prio `15` are masked!) | `< 1 us` read-modify-write or multi-word snapshot shared between a **Task and an `ISR`** (`prio 5..15`). Nesting-safe via `uxCriticalNesting`. |
| **`s = taskENTER_CRITICAL_FROM_ISR()` / `taskEXIT_CRITICAL_FROM_ISR(s)`** | **`ISR` (`prio 5..15`)** | **YES:** saves previous `BASEPRI` into `s`, raises `BASEPRI` to `0x50`, then restores `s` | N/A (already in `ISR`) | Protecting a few shared bytes between **nesting `ISR`s** (e.g., `prio 6` `UART` `ISR` vs. `prio 5` `SPI` `ISR`). |
| **`vTaskSuspendAll()` / `xTaskResumeAll()`** | **Task only** | **NO! All hardware `ISR`s keep firing with zero added latency!** | **YES** (any `ISR` context-switch request is held pending until `xTaskResumeAll()`!) | Updating a **task-only** data table for `20–50 us` where masking interrupts would hurt `SPI`/`ADC` timing! Nesting-safe. |

* **Two Strict Official Rules (*Ch 8.2.1 & 8.2.3*):**
  1. **Never call FreeRTOS blocking/switching APIs** (`vTaskDelay`, `xQueueSend`, `xQueueReceive`, `xSemaphoreTake`) inside `taskENTER_CRITICAL()` or `vTaskSuspendAll()` (`queue.c` triggers `configASSERT` if `xTaskGetSchedulerState() == taskSCHEDULER_SUSPENDED` and `xTicksToWait != 0`!).
  2. **Never manually modify `BASEPRI`/`cpsid i` behind FreeRTOS's back in a task**—always use `taskENTER_CRITICAL()` so FreeRTOS's `uxCriticalNesting` counter stays in sync!

```c
static volatile uint32_t g_irq_pulse_count;
static volatile uint32_t g_last_irq_tick;

/* Example 1 — Task <-> ISR 2-word snapshot (masks NVIC prio 5..15 for ~6 CPU cycles): */
bool p1_snapshot_isr_stats(uint32_t *out_pulses, uint32_t *out_tick)
{
    if (out_pulses != NULL) { *out_pulses = 0U; }
    if (out_tick   != NULL) { *out_tick   = 0U; }
    if ((out_pulses == NULL) || (out_tick == NULL)) { return false; }

    taskENTER_CRITICAL();                   /* Outer entry: BASEPRI 0x00 -> 0x50                    */
    *out_pulses = g_irq_pulse_count;        /* Guaranteed consistent pair from the same instant!    */
    *out_tick   = g_last_irq_tick;
    taskEXIT_CRITICAL();                    /* Matching exit: BASEPRI 0x50 -> 0x00                  */
    return true;
}

/* Example 2 — ISR <-> Higher-Priority ISR shared counter (saves & restores previous BASEPRI): */
void nested_isr_counter_bump(volatile uint32_t *shared_err_cnt)
{
    UBaseType_t saved = taskENTER_CRITICAL_FROM_ISR();
    (*shared_err_cnt)++;
    taskEXIT_CRITICAL_FROM_ISR(saved);
}

/* Example 3 — Task <-> Task table update WITHOUT delaying any hardware interrupts: */
void task_only_calibration_swap(uint32_t cal_dst[8], const uint32_t cal_src[8])
{
    vTaskSuspendAll();                      /* Locks task switching; SPI/ADC/EXTI ISRs still run!   */
    for (uint32_t i = 0U; i < 8U; ++i) {
        cal_dst[i] = cal_src[i];
    }
    ( void )xTaskResumeAll();               /* Runs any context switch that became pending          */
}
```

---

## Scene 5: Mutexes, Priority Inversion, Inheritance, Deadlock & Gatekeeper Tasks (`Ch 8.3–8.4`)

### 1. Standard Mutex (`xSemaphoreCreateMutex`) — Task Ownership Contract

Requires `configUSE_MUTEXES = 1`. When created (`prvInitialiseMutex` in `queue.c`), FreeRTOS sets `uxQueueType = queueQUEUE_IS_MUTEX` (`pcHead = NULL`), `u.xSemaphore.xMutexHolder = NULL`, and calls `xQueueGenericSend` once so **`uxMessagesWaiting` starts at `1` (Unlocked — key ready on the hook!)**.

* **Take (`1 -> 0`):** Task calls `xSemaphoreTake(g_cfg_mutex, wait_ticks)`. In `queue.c`, FreeRTOS decrements `1 -> 0`, sets **`xMutexHolder = CurrentTask`**, and increments the task's `uxMutexesHeld` count.
* **Give (`0 -> 1`):** **That exact same task** calls `xSemaphoreGive(g_cfg_mutex)`, which clears `xMutexHolder = NULL`, disinherits any boosted priority, and restores `0 -> 1` (waking the highest-priority waiting task).
* **Timeout (`!= pdPASS`):** Means your task **failed to get the key**—return `false` immediately without touching the protected resource! *(And when locking a peripheral like `SPI1` with `DMA`, hold the mutex until `DMA` finishes or is quiesced—never `Give` right after starting `DMA`!)*

```c
typedef struct {
    uint32_t max_temp_c;
    bool     logging_enabled;
} P1Config;

static SemaphoreHandle_t g_cfg_mutex;
static P1Config          g_cfg = { .max_temp_c = 80U, .logging_enabled = true };

bool p1_cfg_update(uint32_t max_temp_c, bool log_en)
{
    if (xSemaphoreTake(g_cfg_mutex, pdMS_TO_TICKS(5U)) != pdPASS) {
        return false;                       /* Timed out -> we do NOT own the mutex, abort cleanly! */
    }
    g_cfg.max_temp_c      = max_temp_c;
    g_cfg.logging_enabled = log_en;
    (void)xSemaphoreGive(g_cfg_mutex);      /* Owner returns mutex (0 -> 1)                         */
    return true;
}

bool p1_cfg_read(P1Config *out_copy)
{
    if (out_copy != NULL) { *out_copy = (P1Config){ 0U, false }; }
    if (out_copy == NULL) { return false; }
    if (xSemaphoreTake(g_cfg_mutex, pdMS_TO_TICKS(5U)) != pdPASS) {
        return false;
    }
    *out_copy = g_cfg;
    (void)xSemaphoreGive(g_cfg_mutex);
    return true;
}
```

---

### 2. Unbounded Priority Inversion vs. Bounded Priority Inheritance (`t = 0 .. 8 ms` Trace)

Suppose three tasks run on our single-core `STM32F446RE` (`1 ms` = `1` unit of pure CPU work):
* **`Task L` (`Base Priority = 1`):** Grabs the lock at `t = 0 ms` and needs **`3 ms`** of CPU work inside the lock (`1 ms` completed before `t = 1 ms`, **`2 ms` remaining**).
* **`Task H` (`Base Priority = 3`):** Wakes at `t = 1 ms`, needs the **same lock** for **`1 ms`**.
* **`Task M` (`Base Priority = 2`):** Also wakes at `t = 1 ms` for **`4 ms`** of math (**never uses the lock!**).

---

#### Case A — Buggy Lock Using Binary Semaphore (NO Owner Tracking -> Unbounded Inversion!)

1. **`t = 0 .. 1 ms`:** `L (1)` takes a Binary Semaphore (`1 -> 0`) and finishes `1 ms` (`2 ms` left). Because a Binary Semaphore has **no `xMutexHolder`**, FreeRTOS has **no idea who holds it**!
2. **`t = 1 ms`:** `H (3)` and `M (2)` wake (`Ready`). `H (3)` runs first, calls `xSemaphoreTake()`, sees `0`, and enters **`Blocked`**.
3. **`t = 1 .. 5 ms` (Unbounded Priority Inversion!):** `H (3)` is `Blocked`, `M` is `Ready (2)`, `L` is `Ready (1)`. Scheduler picks **`M (2)`**! For `4 ms`, **unrelated `M (2)` starves `L (1)` of the CPU**, keeping **`H (3)` frozen**! *(If `M` looped forever, `H` would starve forever—the exact 1997 Mars Pathfinder reset bug!)*
4. **`t = 5 .. 8 ms`:** `M (2)` finishes at `5 ms`; `L (1)` finishes its `2 ms` and `Give`s at `7 ms`; `H (3)` finally finishes at **`t = 8 ms` (`6 ms` wait!)**.

```text
CASE A — BINARY SEMAPHORE AS LOCK (No Owner -> M(2) Preempts L(1) -> H(3) Waits 6 ms!):
Time (ms):   0       1                       5           7       8
CPU Runs:    | L (1) |         M (2)         |   L (1)   | H (3) |
Lock Held:   | <---------------- Task L ---------------> |Task H |
Task H (3):  .       [======= STUCK WAITING 6 ms =======] Runs!
```

#### Case B — Proper Lock Using FreeRTOS Mutex (Priority Inheritance!)

Inside every task's `TCB_t`, FreeRTOS keeps **two** priority fields when `configUSE_MUTEXES = 1`:
* `uxBasePriority`: permanent home priority (`Task L = 1`).
* `uxPriority`: **effective** scheduling priority (`Task L` starts at `1`, temporarily boosted when needed).

1. **`t = 0 .. 1 ms`:** `L` (`Base = 1, Effective = 1`) takes `g_cfg_mutex` -> **`xMutexHolder = Task L`**, `uxMutexesHeld = 1`, completes `1 ms` (`2 ms` left).
2. **`t = 1 ms` (`xTaskPriorityInherit` Runs!):** `H (3)` calls `xSemaphoreTake(g_cfg_mutex)`. FreeRTOS inspects `xMutexHolder` (`Task L`, `Effective = 1 < 3`), **immediately raises `Task L->uxPriority = 3`** (moving `L` into Ready List `[3]`), and puts `H` to sleep.
3. **`t = 1 .. 3 ms`:** Ready tasks are **`L (Effective = 3)`** and **`M (2)`**. **`L (3)` beats `M (2)`!** `L` runs its remaining `2 ms` and calls `xSemaphoreGive(g_cfg_mutex)` at `t = 3 ms`.
4. **`t = 3 .. 4 ms` (`xTaskPriorityDisinherit` Runs!):** `xSemaphoreGive()` restores **`Task L->uxPriority = uxBasePriority = 1`** and wakes `H (3)`, which finishes at **`t = 4 ms` (`2 ms` bounded wait instead of `6 ms`!)**. `M (2)` runs `t = 4 .. 8 ms`.

```text
CASE B — FREERTOS MUTEX (L Inherits Priority 3 -> Beats M(2) -> H(3) Waits Only 2 ms!):
Time (ms):   0       1           3       4                       8
CPU Runs:    | L (1) | L (eff=3!)| H (3) |         M (2)         |
Lock Held:   | <---- Task L ---> |Task H |         none          |
Task H (3):  .       [Wait 2 ms ] Runs!
```

* **Official Simplified Priority Inheritance Caveats (*Book Ch 8.3.3* & `tasks.c`):**
  FreeRTOS deliberately implements a **lightweight ("simplified") priority inheritance** model rather than tracking a per-mutex waiter list inside each task:
  1. **Holding Multiple Mutexes:** `TCB_t` tracks a **count** (`uxMutexesHeld`), not a list of every mutex's waiters. If `Task L (1)` holds `Mutex A` and `Mutex B`, gets boosted to `Priority 3` by a waiter on `Mutex B`, and releases `Mutex B` first, **`Task L` stays at `Priority 3` until it also releases `Mutex A` (`uxMutexesHeld == 0`)!**
  2. **Waiter Timeout Disinheritance (`vTaskPriorityDisinheritAfterTimeout`):** If `Task H (3)` times out while waiting on `Task L`'s mutex, FreeRTOS steps `Task L`'s priority down to the highest remaining waiter **only when `uxMutexesHeld == 1`**; if `Task L` holds `2+` mutexes, `Task L` keeps `Priority 3` until all held mutexes are released.
  3. **Mutexes Cannot Shorten Sleeping or Busy-Spinning:** Never call `vTaskDelay()` while holding a mutex (inheritance boosts CPU priority, not a clock delay!), and never spin `while (xSemaphoreTake(m, 0) != pdPASS) {}` in `Task H` (a `0` timeout never blocks, starving `Task L` of the single CPU forever!).

---

### 3. Deadlock, Recursive Mutexes, And Same-Priority Tight-Loop Starvation (`Ch 8.3.4–8.3.6`)

1. **`AB-BA` Deadlock (`Ch 8.3.4`):** `Task A` locks `Mutex X` then blocks waiting for `Mutex Y`, while `Task B` locks `Mutex Y` then blocks waiting for `Mutex X`—neither can ever proceed!
   * **Design Fix:** Enforce a **strict global lock order** (every task must acquire `Mutex X` before `Mutex Y`), prefer **holding 1 mutex at a time**, and always pass a **bounded timeout** (never `portMAX_DELAY` unless mathematically deadlock-free).
2. **Self-Deadlock & Recursive Mutexes (`Ch 8.3.5`, `configUSE_RECURSIVE_MUTEXES = 1`):**
   * If `Task A` already holds a **standard** mutex (`0`) and calls a helper function that calls `xSemaphoreTake()` on that **same** mutex, `Task A` sees `0` and **blocks waiting for itself (self-deadlock!)**.
   * **`xSemaphoreCreateRecursiveMutex()` (`xQueueTakeMutexRecursive` in `queue.c`):** Pair **`xSemaphoreTakeRecursive(rmtx, wait)`** with **`xSemaphoreGiveRecursive(rmtx)`** (**never** call plain `xSemaphoreTake`/`Give` on a recursive mutex!). When `xMutexHolder == xTaskGetCurrentTaskHandle()`, `TakeRecursive` increments **`u.xSemaphore.uxRecursiveCallCount`** (`1 -> 2 -> 3`) and returns `pdPASS` immediately; only when `GiveRecursive` unwinds `uxRecursiveCallCount` back to **`0`** (`3 -> 2 -> 1 -> 0`) does FreeRTOS release the underlying mutex! *(Prefer clean public-locks / private-unlocked helper design first so you rarely need recursive mutexes.)*
   * **Simple Use Case:** Public `flash_erase_and_log()` locks `g_flash_rmutex` (`count = 1`) and internally calls public `flash_write_record()`, which also locks `g_flash_rmutex` (`count = 2`) without self-deadlock:
   ```c
   static SemaphoreHandle_t g_flash_rmutex; /* Created via xSemaphoreCreateRecursiveMutex()         */

   bool flash_write_record(uint32_t rec_id)
   {
       if (xSemaphoreTakeRecursive(g_flash_rmutex, pdMS_TO_TICKS(20U)) != pdPASS) { return false; }
       /* ... SPI flash page write ... */ (void)rec_id;
       (void)xSemaphoreGiveRecursive(g_flash_rmutex); /* count: 2->1 (if nested) or 1->0 (unlocks!) */
       return true;
   }

   bool flash_erase_and_log(uint32_t rec_id)
   {
       if (xSemaphoreTakeRecursive(g_flash_rmutex, pdMS_TO_TICKS(20U)) != pdPASS) { return false; }
       /* ... erase sector, then call public helper on SAME task without self-deadlock: ... */
       bool ok = flash_write_record(rec_id);          /* count: 1 -> 2 -> 1                         */
       (void)xSemaphoreGiveRecursive(g_flash_rmutex); /* count: 1 -> 0 -> finally unlocks mutex!    */
       return ok;
   }
   ```
3. **Same-Priority Tight-Loop Mutex Starvation (`Ch 8.3.6`, *Book Listing 8.16*):**
   * Suppose `Task 1` and `Task 2` have the **exact same priority** and both loop tightly: `for(;;) { xSemaphoreTake(m, ...); work(); xSemaphoreGive(m); }`.
   * If `Task 1` holds `m` when a `SysTick` fires, `Task 2` runs, hits `xSemaphoreTake(m)`, and **blocks**. `Task 1` resumes, calls `xSemaphoreGive(m)` (waking `Task 2` to `Ready`, **not** preempting because equal priority does **not** preempt inside `Give`!), and **microseconds later** loops around and **re-takes `m` before the next `SysTick`**—starving `Task 2`!
   * **Official Fix (*Book Listing 8.16*):** Either add a bounded delay/event wait outside the mutex, or compare `xTaskGetTickCount()` before `Take` vs. after `Give` and call **`taskYIELD()`** if a tick boundary elapsed while holding the mutex.

---

### 4. Gatekeeper Tasks — Serialization Without Mutex Hazards (`Ch 8.4`)

What if **both Tasks AND `ISR`s** need to write formatted messages to `UART2` or an external `SPI` flash log? Remember: **`ISR`s can NEVER take a Mutex!**

The official FreeRTOS architecture pattern (*Book Section 8.4*) is a **Gatekeeper Task**:
* **One single dedicated task** (`Task_UartGatekeeper`) is the **sole owner** of the `UART2` peripheral—no other task or `ISR` ever touches `USART2->DR`!
* Any `Task` calls `xQueueSend(g_uart_q, &msg, wait)` and any `ISR` calls `xQueueSendFromISR(g_uart_q, &msg, &woken)`.
* `Task_UartGatekeeper` sleeps in `for (;;) { xQueueReceive(g_uart_q, &msg, portMAX_DELAY); uart_tx_blocking(&msg); }`.
* **Result:** **Zero mutexes, zero priority inversion, zero `AB-BA` deadlocks, and `100%` safe from both `Task` and `ISR` contexts!**

* **Simple Use Case for a Gatekeeper Task:** In `P1`, `Task H`, `Task L`, and `ADC1_IRQHandler` all push compact `LogEvent` structs into `g_log_q`, while `Task_LogGatekeeper` serializes every `UART2` / `SPI` flash write cleanly in arrival order:

```c
typedef struct { uint16_t src_id; uint16_t val; } LogEvent_t;
static QueueHandle_t g_log_q;               /* Created via xQueueCreate(8U, sizeof(LogEvent_t))     */

void log_submit_from_task(uint16_t src, uint16_t val) {
    LogEvent_t ev = { .src_id = src, .val = val };
    (void)xQueueSendToBack(g_log_q, &ev, 0U);
}
void log_submit_from_isr(uint16_t src, uint16_t val, BaseType_t *woken) {
    LogEvent_t ev = { .src_id = src, .val = val };
    (void)xQueueSendToBackFromISR(g_log_q, &ev, woken);
}
static void Task_LogGatekeeper(void *arg) { /* SOLE owner of USART2 -> 0 mutexes, 0 inversion!      */
    (void)arg; LogEvent_t rx;
    for (;;) {
        if (xQueueReceive(g_log_q, &rx, portMAX_DELAY) == pdPASS) {
            /* Only this task ever writes USART2->DR! */ (void)rx;
        }
    }
}
```

---

## Scene 6: Complete Unified `P1` Synchronization Module (`p1_sync_complete.c`)

Here is our unified `P1` module combining `C02` reverse-order startup unwind, `ISR -> Queue` (`2` slots x `8` bytes), `1`-slot Telemetry Mailbox (`xQueueOverwrite` + `xQueuePeek`), `Task <-> ISR` Critical Section, and `Task H / Task M / Task L` Mutex Priority Inheritance:

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
} FaultRecord;                              /* 8 bytes per slot                                     */

typedef struct {
    TickType_t tick;
    uint16_t   vbat_mv;
} VbatSample;

typedef struct {
    uint32_t max_temp_c;
    bool     logging_enabled;
} P1Config;

static QueueHandle_t     g_fault_queue;     /* 2 slots x 8 B = 16 B FIFO (ISR -> Task H)            */
static QueueHandle_t     g_vbat_mailbox;    /* 1 slot  x 8 B Mailbox (xQueueOverwrite / xQueuePeek) */
static SemaphoreHandle_t g_cfg_mutex;       /* Mutex with Priority Inheritance (Task H <-> Task L)  */
static P1Config          g_cfg = { .max_temp_c = 80U, .logging_enabled = true };

static volatile uint32_t g_irq_pulse_count;
static volatile uint32_t g_last_irq_tick;
static volatile uint32_t g_queue_drops;

/* 1. Hardware ISR (PC13 / EXTI15_10 at NVIC Priority 5 >= 5): */
void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & (1UL << 13)) != 0U) {
        EXTI->PR = (1UL << 13);             /* W1C (=, never |=): clear PC13 pending flag           */

        g_irq_pulse_count++;
        g_last_irq_tick = xTaskGetTickCountFromISR();

        FaultRecord rec   = { .seq = g_irq_pulse_count, .code = 0xE1U };
        BaseType_t  woken = pdFALSE;
        if (xQueueSendToBackFromISR(g_fault_queue, &rec, &woken) != pdPASS) {
            g_queue_drops++;                /* Both slots unread -> newest rejected, count drop     */
        }
        portYIELD_FROM_ISR(woken);          /* Pends PendSV so Task H (prio 3) runs immediately     */
    }
}

/* 2. Task H (Priority 3 - HIGH): Pops FaultRecord & updates g_cfg under Mutex */
static void Task_High_FaultService(void *arg)
{
    (void)arg;
    FaultRecord rx = { 0U, 0U };

    for (;;) {
        if (xQueueReceive(g_fault_queue, &rx, pdMS_TO_TICKS(500U)) == pdPASS) {
            /* If Task L holds g_cfg_mutex right now, L->uxPriority boosts 1 -> 3! */
            if (xSemaphoreTake(g_cfg_mutex, pdMS_TO_TICKS(10U)) == pdPASS) {
                if (rx.code == 0xE1U) {
                    g_cfg.max_temp_c      = 70U;
                    g_cfg.logging_enabled = true;
                }
                (void)xSemaphoreGive(g_cfg_mutex);
            }
        }
    }
}

/* 3. Task M (Priority 2 - MEDIUM): Periodic math + publishes latest Vbat to 1-slot Mailbox */
static void Task_Med_MathWorker(void *arg)
{
    (void)arg;
    TickType_t last_wake = xTaskGetTickCount();
    uint16_t   sim_mv    = 3300U;

    for (;;) {
        for (volatile uint32_t i = 0U; i < 5000U; ++i) {}
        VbatSample latest = { .tick = xTaskGetTickCount(), .vbat_mv = sim_mv };
        (void)xQueueOverwrite(g_vbat_mailbox, &latest); /* Always overwrites Slot 0 of length-1 q!  */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10U));
    }
}

/* 4. Task L (Priority 1 - LOW): Critical-section ISR snapshot + Mailbox Peek + Mutex read */
static void Task_Low_StatusLogger(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t   pulses = 0U, tick = 0U;
        VbatSample vbat   = { 0U, 0U };
        P1Config   cfg_copy;

        taskENTER_CRITICAL();               /* BASEPRI -> 0x50: atomic 2-word snapshot vs IRQ 40    */
        pulses = g_irq_pulse_count;
        tick   = g_last_irq_tick;
        taskEXIT_CRITICAL();

        if (xQueuePeek(g_vbat_mailbox, &vbat, 0U) == pdPASS) { /* Reads Slot 0 WITHOUT removing it! */
            (void)vbat;                     /* Valid only after Task M's first xQueueOverwrite()    */
        }

        if (xSemaphoreTake(g_cfg_mutex, pdMS_TO_TICKS(10U)) == pdPASS) {
            cfg_copy = g_cfg;
            (void)xSemaphoreGive(g_cfg_mutex);
            (void)cfg_copy;
        }
        (void)pulses; (void)tick;
        vTaskDelay(pdMS_TO_TICKS(50U));
    }
}

/* 5. Startup Init (call before vTaskStartScheduler(), after PC13 EXTI routing from W02-L16): */
bool p1_sync_init(void)
{
    TaskHandle_t h_high = NULL, h_med = NULL, h_low = NULL;

    g_fault_queue = xQueueCreate(2U, sizeof(FaultRecord)); /* 2 slots x 8 B = 16 B FIFO             */
    if (g_fault_queue == NULL) { return false; }

    g_vbat_mailbox = xQueueCreate(1U, sizeof(VbatSample)); /* MUST be length 1 for xQueueOverwrite! */
    if (g_vbat_mailbox == NULL) { goto err_fault_q; }

    g_cfg_mutex = xSemaphoreCreateMutex();                 /* Starts unlocked (uxMessagesWaiting=1) */
    if (g_cfg_mutex == NULL) { goto err_mailbox; }

    /* Stack 256U = 256 32-bit words (1024 bytes) on Cortex-M4! */
    if (xTaskCreate(Task_High_FaultService, "FaultH", 256U, NULL, 3U, &h_high) != pdPASS) {
        goto err_mutex;
    }
    if (xTaskCreate(Task_Med_MathWorker,    "MathM",  256U, NULL, 2U, &h_med)  != pdPASS) {
        goto err_task_high;
    }
    if (xTaskCreate(Task_Low_StatusLogger,  "LogL",   256U, NULL, 1U, &h_low)  != pdPASS) {
        goto err_task_med;
    }

    /* Cortex-M4 FreeRTOS requirement: 4 preempt bits / 0 subpriority bits (AIRCR.PRIGROUP = 3),
     * clear any boot-time latched PC13 edge, and set IRQ 40 priority >= 5 BEFORE enabling IRQ! */
    NVIC_SetPriorityGrouping(3U);
    EXTI->PR = (1UL << 13);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    NVIC_SetPriority(EXTI15_10_IRQn, 5U);
    NVIC_EnableIRQ(EXTI15_10_IRQn);         /* Enable hardware producer LAST!                       */
    return true;

err_task_med:  vTaskDelete(h_med);
err_task_high: vTaskDelete(h_high);
err_mutex:     vSemaphoreDelete(g_cfg_mutex);  g_cfg_mutex    = NULL;
err_mailbox:   vQueueDelete(g_vbat_mailbox);   g_vbat_mailbox = NULL;
err_fault_q:   vQueueDelete(g_fault_queue);    g_fault_queue  = NULL;
    return false;
}
```

---

## Control Flow In Three Parts

**(a) One-Line Chain:**
`EXTI15_10_IRQHandler (Handler/MSP) -> xQueueSendToBackFromISR() + portYIELD_FROM_ISR() -> [PendSV] -> Task_High_FaultService (Thread/PSP) -> xQueueReceive() -> xSemaphoreTake(g_cfg_mutex) -> [xTaskPriorityInherit boosts Task L: 1 -> 3 if held] -> xSemaphoreGive(g_cfg_mutex)`

**(b) Step Table:**

| # | Where | Goal / State Change | Concrete Data In -> Out | Context / Mode |
| --- | --- | --- | --- | --- |
| 1 | `EXTI15_10_IRQHandler` | Clear `EXTI->PR`, `memcpy` `8 B` to `*pcWriteTo`, advance `pcWriteTo` | `{1, 0xE1}` -> `Slot 0` (`uxMessagesWaiting: 0->1`) | Handler (`MSP`, prio `5`) |
| 2 | `PendSV_Handler` | Context-switch from interrupted task to `Task H (3)` | `Task H`: `Ready -> Running` | Handler (`MSP`, prio `15`) |
| 3 | `Task_High_FaultService` | Step `pcReadFrom` to `Slot 0`, `memcpy` `8 B` to `rx` | `Slot 0` -> `rx = {1, 0xE1}` (`uxMessagesWaiting: 1->0`) | Thread (`PSP`, prio `3`) |
| 4 | `xSemaphoreTake(g_cfg_mutex)` | Acquire `g_cfg` key (or boost `L->uxPriority`: `1 -> 3`) | `L` finishes -> `xMutexHolder = Task H` | Thread (`PSP`, prio `3`) |
| 5 | `Task_Low_StatusLogger` | Snapshot `2` `ISR` words under `BASEPRI = 0x50` | `BASEPRI: 0x00 -> 0x50 -> 0x00` | Thread (`PSP`, prio `1`) |

**(c) Privilege & Scheduling Timeline (When `Task L` Holds `g_cfg_mutex` And `IRQ 40` Fires):**

```text
EXTI15_10 ISR (MSP)          FreeRTOS Kernel / PendSV         Tasks (Thread / PSP)
-------------------          ------------------------         --------------------
                                                              Task L (prio 1) takes g_cfg_mutex
PC13 edge -> IRQ 40!
xQueueSendToBackFromISR() -> Copies to pcWriteTo, wakes H(3)
portYIELD_FROM_ISR(pdTRUE)-> PendSV switches to Task H -----> Task H (3) receives {1, 0xE1}
                                                              Task H (3) calls Take(g_cfg_mutex)
                             Boosts L->uxPriority: 1 -> 3! -> Task L (eff 3) beats M (2) & Gives!
                             Restores L -> 1, wakes H (3) --> Task H (3) updates g_cfg & Gives!
```

---

## Authoritative Official Source & Documentation Lookup

| Question | Official Source / Training Book | Exact Function / Section |
| --- | --- | --- |
| `Queue_t`, `pcHead`, `pcWriteTo`, `pcReadFrom`, `xMutexHolder` | [FreeRTOS-Kernel `queue.c`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/queue.c) | `struct QueueDefinition`, `xQueueGenericReset`, `prvCopyDataToQueue`, `prvCopyDataFromQueue` |
| Queue-by-copy, FIFO/Front, Pointers & `1`-slot Mailbox (`xQueueOverwrite`/`Peek`) | [*Mastering the FreeRTOS Real Time Kernel*, Ch 5](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book/blob/main/ch05.md) & [FreeRTOS Queues](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/02-Queues-mutexes-and-semaphores/01-Queues) | Sections `5.2.1`–`5.5` & `5.7` (`xQueueOverwrite`, `xQueuePeek`) |
| `FromISR`, `xHigherPriorityTaskWoken`, Binary 3-IRQ burst & Counting Semaphores | [*Mastering the FreeRTOS Real Time Kernel*, Ch 7](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book/blob/main/ch07.md) | Sections `7.2.4`, `7.4.4` (Listing `7.10` `while` drain), `7.5` |
| Critical sections, `vTaskSuspendAll`, Mutexes, Simplified Inheritance, Deadlock & Gatekeeper | [*Mastering the FreeRTOS Real Time Kernel*, Ch 8](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book/blob/main/ch08.md) & [FreeRTOS-Kernel `tasks.c`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/tasks.c) | Sections `8.2`–`8.4`; `xTaskPriorityInherit`, `xTaskPriorityDisinherit`, `vTaskPriorityDisinheritAfterTimeout` |

---

## Feynman Recap

Everything in FreeRTOS synchronization comes from one structure (`Queue_t`): **`xQueueCreate(2, 8)`** allocates `16` contiguous bytes (`Slot 0` + `Slot 1`) with circular `pcWriteTo` and `pcReadFrom` cursors so items stay in FIFO order without shifting bytes—and when all `2` slots are unread, normal `Send` **never overwrites** (`ISR` returns `errQUEUE_FULL`; only a **`1`-slot Mailbox** using `xQueueOverwrite` overwrites `Slot 0`). Setting `uxItemSize = 0` turns `Queue_t` into token counters: a **Binary Semaphore** (`0..1`, starts at `0`) wakes a deferred handler task (which drains all pending hardware data in a `while` loop so rapid bursts aren't lost); a **Counting Semaphore** (`0..Max`) counts events (`start 0`) or free pool buffers (`start Max`); and a **Mutex** (`starts at 1`, task-only) records **`xMutexHolder`** so **Priority Inheritance** boosts a low-priority owner (`1 -> 3`) past medium-priority tasks (`2`) whenever a high-priority task (`3`) waits—or a **Gatekeeper Task** owns a bus via a Queue so even `ISR`s can submit requests with zero locks!

---

## Spoken-English Interview Answers

### 1. In `xQueueCreate(2, 8)`, if `uxMessagesWaiting == 1`, which slot (`0` or `1`) holds the item, and does a full queue overwrite old data?
* **Simple answer:** Either `Slot 0` or `Slot 1` can hold that `1` item depending on the circular cursors `pcWriteTo` and `pcReadFrom`; and **no**, standard `xQueueSend` / `xQueueSendFromISR` **never** overwrite unread slots!
* **How:** On the 1st write, `Slot 0` holds it (`pcWriteTo` moves to `Slot 1`). After `Slot 0` is read and a 2nd item arrives, `Slot 1` holds that `1` unread item (`pcWriteTo` wraps to `Slot 0`). When `uxMessagesWaiting == 2` (full), `xQueueSendFromISR` returns `errQUEUE_FULL` leaving `Slot 0` and `Slot 1` intact, while `xQueueOverwrite` is asserted (`configASSERT`) to work **only on length-`1` mailboxes**.

### 2. What is the difference between a Binary Semaphore, Counting Semaphore, Mutex, and Gatekeeper Task?
* **Binary Semaphore (`starts 0`):** One-way `ISR -> Task` wakeup token (`0..1`); if 3 rapid IRQs hit while the task is busy, the 3rd coalesces unless the task drains all hardware items in a `while` loop.
* **Counting Semaphore:** Counts rapid events (`start 0 .. Max`) or free buffer permits (`start Max .. 0`).
* **Mutex (`starts 1`, task-only):** Tracks `xMutexHolder` (`TCB_t`) and applies **Priority Inheritance** (`ISR` forbidden!).
* **Gatekeeper Task:** One dedicated task owns a shared peripheral (`I2C1`/`UART2`) behind a Queue—zero priority inversion, zero deadlock, and `ISR`-accessible.

### 3. Explain Priority Inversion, Priority Inheritance, and FreeRTOS's "Simplified Inheritance" caveat.
* **Simple answer:** High-priority `H (3)` blocks on a lock held by `L (1)`, while unrelated `M (2)` preempts `L`, starving `H` (*unbounded priority inversion*).
* **How & Caveat:** A FreeRTOS Mutex boosts `L->uxPriority` to `3` (`xTaskPriorityInherit`) so `L` preempts `M`, finishes fast, and `Give()` restores `L` to `1`. Because FreeRTOS uses *simplified* inheritance to save RAM/cycles, if `L` holds **multiple** mutexes at once, `L` keeps its highest inherited priority until **all** held mutexes are released—so hold one short mutex at a time and never call `vTaskDelay()` inside a mutex.

### 4. Compare `taskENTER_CRITICAL()`, `taskENTER_CRITICAL_FROM_ISR()`, and `vTaskSuspendAll()`.
* **`taskENTER_CRITICAL()` (Task):** Masks `NVIC` priorities `5..15` (`BASEPRI = 0x50`) with nesting count `uxCriticalNesting`; protects `< 1 us` `Task <-> ISR` variables while leaving priorities `0..4` live.
* **`taskENTER_CRITICAL_FROM_ISR()` (`ISR`):** Saves and restores previous `BASEPRI` for nested `ISR <-> ISR` critical sections.
* **`vTaskSuspendAll()` (Task):** Stops **only task switches** while leaving **all hardware `ISR`s enabled**—ideal for longer task-only updates without adding interrupt jitter.

---

## Corrections

- 2026-11-17: Initial publication (revisions 2–3 added `P1` example and initial `Queue_t` notes).
- 2026-11-20 (revision 4): Complete ground-up rewrite verified against official `FreeRTOS-Kernel/queue.c` and *Mastering the FreeRTOS Real Time Kernel* (Chapters 5, 7, and 8): traced exact `pcHead`/`pcTail`/`pcWriteTo`/`pcReadFrom` cursor positions (`Slot 0` vs. `Slot 1`), all 4 arrival windows (*before*, *during* [`BASEPRI` + post-`Receive`], *after*, and *full*), non-overwriting `errQUEUE_FULL` vs. `xQueueSendToFront` vs. length-`1` `xQueueOverwrite`/`xQueuePeek` Mailboxes, Ch 7.4.4's 3-interrupt burst & `while`-drain pattern, Counting Semaphores, Critical Sections vs. `vTaskSuspendAll`, Simplified Priority Inheritance caveats, Deadlock / Recursive Mutexes / Same-Priority starvation, and Gatekeeper Tasks.
- 2026-11-20 (revision 5): Full line-by-line audit against `FreeRTOS-Kernel/queue.c`, `semphr.h`, and `GCC/ARM_CM4F/portmacro.h`: replaced single-row cursor arrows with aligned hex-address (`0x100`/`0x108`/`0x110`) 6-step diagrams (`Steps 0–5`), fixed `pxQueue->u.xQueue.pcTail` member path, documented `pcHead != NULL` (Semaphore) vs. `pcHead == NULL` (Mutex) + `xSemaphoreCreateBinary()` vs. deprecated `vSemaphoreCreateBinary()`, added backward-wrap (`< pcHead`) address traces for `xQueueSendToFront` and `xQueueOverwrite`, clarified `errQUEUE_FULL` (`0 == pdFAIL`), added C examples for Recursive Mutexes and Gatekeeper Tasks, and hardened `p1_sync_complete.c` (`NVIC_SetPriorityGrouping(3U)`, stale `EXTI->PR` clear before `NVIC_EnableIRQ`, and checked `xQueuePeek == pdPASS`). Next planned topic: `C04` stop/drain ownership.
