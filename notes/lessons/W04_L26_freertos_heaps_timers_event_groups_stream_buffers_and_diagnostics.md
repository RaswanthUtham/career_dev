# W04-L26: FreeRTOS Complete Interview Pillars — Heaps (`heap_1..5`), Software Timers, Event Groups, Stream/Message Buffers, Stack Safety & Tickless Idle

> **Status:** Official-source & training-book reviewed, 2026-11-20 (Day 1 of the
> [10-Day Interview-Focused Plan](../../plans/five_month_development_roadmap.md#5-weekly-schedule)).
> Grounded directly in upstream `FreeRTOS-Kernel` (`portable/MemMang/heap_1.c`..`heap_5.c`,
> `timers.c`, `event_groups.c`, `stream_buffer.c`, `include/stack_macros.h`, `tasks.c`)
> and *Mastering the FreeRTOS Real Time Kernel* (Chapters 3, 6, 9, 11, 12, and 13)
> on `STM32F446RE` (`Cortex-M4F`, `8-byte` ABI alignment). Concept & reference-code
> walkthrough for interview mastery; no board build or flash is required.

Prerequisites: [FreeRTOS tasks & notifications](W03_L22_freertos_tasks_states_delays_and_notifications.md)
and [FreeRTOS queues, semaphores & mutexes](W04_L24_freertos_synchronization_and_priority_inversion.md).

---

## Know This First

With [W03-L22](W03_L22_freertos_tasks_states_delays_and_notifications.md) and
[W04-L24](W04_L24_freertos_synchronization_and_priority_inversion.md), you already
know Tasks, Notifications, Queues, Semaphores, Critical Sections, Mutex Priority
Inheritance, and Gatekeeper Tasks. This chapter covers **selected FreeRTOS
interview topics**. For a guided first pass, start with the [Day 1 worked walkthrough](W04_L26_day_1_tutoring.md).

| Term | Plain-English meaning (verified against official `FreeRTOS-Kernel` source) |
| --- | --- |
| **Heap Fragmentation** | Total free RAM is large (say `4 KB`), yet split into tiny non-adjacent holes (`64 B` each), so a `256 B` allocation fails with `NULL`! |
| **Coalescing (`heap_4` / `heap_5`)** | Merging two **neighboring** freed RAM blocks back into **one larger contiguous block** so fragmentation does not accumulate |
| **RTOS Daemon / Timer Service Task (`"Tmr Svc"`)** | A standard FreeRTOS background task (`prvTimerTask` in `timers.c`) that executes **all** Software Timer callbacks and deferred `ISR` callbacks—**never** `SysTick_Handler`! |
| **Event Group (`EventBits_t`)** | A `24`-bit shared flag word (`bits 0..23` on `32`-bit `TickType_t`) allowing tasks to sleep until **`ANY` (`OR`)** or **`ALL` (`AND`)** of a chosen mask of bits become `1` |
| **Stream Buffer vs. Message Buffer** | Lightweight **1-writer / 1-reader (`SPSC`)** byte ring (`stream_buffer.c`): **Stream** passes raw continuous bytes; **Message** prepends a `4`-byte length header so variable-size packets stay whole |
| **Stack High-Water Mark** | Minimum free **32-bit words** (`StackType_t`) ever remaining on a task's stack since boot (`0` = stack already hit its limit!) |

---

## Unified `P1` Scenario Used Across Every Scene

Picture our `P1` motor-control / BMS IC diagnostic subsystem (connecting directly
to your day-to-day cyclic driver & safety engineering):

```text
  [Boot / Memory]  ---> Static RAM (xTaskCreateStatic) OR heap_4 / heap_5 (CCM + SRAM1)
  [UART DMA RX]    ---> Stream/Message Buffer (1 ISR writer -> 1 Parser Task, 0 queue locks!)
  [3 Subsystem Tasks]-> Event Group (xEventGroupSync: wait until Sensor + Comms + NVM ALL ready!)
  [Timeout / LED]  ---> Software Timer ("Tmr Svc" daemon task -> non-blocking callback!)
  [Health & Power] ---> Stack Overflow Check (0xA5 canary + MPU) + Runtime %CPU + Tickless Idle
```

---

## Scene 1: FreeRTOS Memory Management (`heap_1`..`heap_5` vs. Static Allocation — `Ch 3`)

### 1. Why Standard C `malloc()` / `free()` Fails In Real-Time Firmware

Standard C library `malloc()`/`free()` (`newlib`/`glibc`) has **four embedded problems**:
1. **Not thread-safe** by default across preempting RTOS tasks.
2. **Non-deterministic execution time** (can take wildly different CPU cycles as the heap fragments).
3. **Heap fragmentation** over days/months of uptime can return `NULL` and crash a product in the field.
4. Pulls in large C-library code size and linker `_sbrk` plumbing.

FreeRTOS therefore moves allocation into its portable layer (`pvPortMalloc()` / `vPortFree()`) and gives you **five `heap_x.c` choices** plus **100% Static Allocation**:

| Scheme | Can Free (`vPortFree`)? | Coalesces Adjacent Free Blocks? | How It Works In Plain English | Where To Use / Avoid |
| --- | --- | --- | --- | --- |
| **`heap_1.c`** | **NO** | N/A | Simple bump pointer inside `ucHeap[configTOTAL_HEAP_SIZE]`. Create all tasks/queues before `vTaskStartScheduler()` and never delete them. `O(1)` deterministic! | Small safety/fixed-topology systems that never delete RTOS objects |
| **`heap_2.c`** | Yes | **NO!** | Picks the **smallest** free block that fits (*best-fit*), **without** merging adjacent freed blocks. Fragments badly if allocation sizes vary! | **Legacy only** — replaced by `heap_4.c` in all modern projects |
| **`heap_3.c`** | Yes | Compiler-dependent | Thin wrapper around standard C `malloc()`/`free()` guarded by `vTaskSuspendAll()` / `xTaskResumeAll()`. | Only when forced to share one linker heap with third-party C libraries |
| **`heap_4.c`** | **Yes** | **YES!** | **Default recommended dynamic heap.** Keeps free blocks sorted by **RAM address** (*first-fit*) and **merges neighboring free blocks** on `vPortFree()`! | General FreeRTOS firmware needing dynamic creation/deletion |
| **`heap_5.c`** | **Yes** | **YES!** | Identical coalescing algorithm to `heap_4.c`, **plus spans multiple non-contiguous RAM banks** initialized via `vPortDefineHeapRegions()`! | MCUs with split RAM (e.g., `STM32F4` `64 KB CCMRAM` at `0x10000000` + `112 KB SRAM1` at `0x20000000`) |
| **Static (`*CreateStatic`)** | Manual reuse | Zero heap! | `configSUPPORT_DYNAMIC_ALLOCATION = 0`, `configSUPPORT_STATIC_ALLOCATION = 1`. Caller passes compile-time `StaticTask_t` / `StaticQueue_t` + buffer arrays. | **Automotive / MISRA-C / Safety-critical (`P1` / AUTOSAR-grade)** — `0` fragmentation, linker proves RAM fits at build time! |

---

### 2. Inside `heap_4.c` In RAM — How Coalescing & Thread-Safety Actually Work

Look at what `heap_4.c` places inside `ucHeap[]` (aligned to `portBYTE_ALIGNMENT = 8` bytes on Cortex-M4):
* Every block starts with an **`8`-byte `BlockLink_t` header**: `{ BlockLink_t *pxNextFreeBlock; size_t xBlockSize; }`.
* **Clever MSB Ownership Bit (`heapBLOCK_ALLOCATED_BITMASK`):** In `heap_4.c`, **bit `31` (the top bit) of `xBlockSize`** is set to **`1`** when the block belongs to the application, and cleared to **`0`** when the block is free (`configASSERT` catches double-frees immediately!).
* **Why `pvPortMalloc()` uses `vTaskSuspendAll()` instead of `taskENTER_CRITICAL()`:**
  Searching the free list can take dozens of loop iterations. Wrapping it in `vTaskSuspendAll()` / `xTaskResumeAll()` stops other tasks from touching the heap **while leaving all hardware `ISR`s `100%` enabled with zero interrupt jitter**!
  * **Interview Consequence:** **`pvPortMalloc()` and `vPortFree()` can NEVER be called from an `ISR`!**

```text
WHY heap_2 FRAGMENTS vs. HOW heap_4 COALESCES NEIGHBORS (Address-Sorted Free List):

1. Suppose 3 adjacent blocks A (64B), B (64B), C (128B) sit at 0x20001000..0x200010FF:
   [ Block A: 64B USED ] [ Block B: 64B USED ] [ Block C: 128B FREE ]

2. Application frees Block A, then frees Block B:
   - In heap_2 (no coalescing):  [ 64B FREE ] [ 64B FREE ] [ 128B FREE ] -> pvPortMalloc(160) FAILS (NULL)!
   - In heap_4 (address-sorted): A+B merge -> 128B, then (A+B)+C merge -> [ ONE 256B FREE BLOCK! ]
                                 -> pvPortMalloc(160) SUCCEEDS cleanly!
```

* **Diagnostic APIs & Hooks (`Ch 3.3`):**
  * `xPortGetFreeHeapSize()`: current free bytes right now (cannot reveal fragmentation by itself!).
  * `xPortGetMinimumEverFreeHeapSize()`: lowest free-byte level since boot (**heap watermark**—check this in integration tests!).
  * `vPortGetHeapStats(&stats)`: reports **both** `xAvailableHeapSpaceInBytes` **and** `xSizeOfLargestFreeBlockInBytes`—if `Total Free = 4096 B` while `Largest Block = 64 B`, you have proven **heap fragmentation**!
  * Static-only kernel note (`Ch 3.4.2`): when `configSUPPORT_STATIC_ALLOCATION = 1` and `configSUPPORT_DYNAMIC_ALLOCATION = 0`, you supply `vApplicationGetIdleTaskMemory()` (and `vApplicationGetTimerTaskMemory()` if `configUSE_TIMERS = 1`) in later FreeRTOS V10.x or FreeRTOS provides defaults when `configKERNEL_PROVIDED_STATIC_MEMORY = 1`.

```c
/* heap_5 example: spanning two physically separate SRAM banks on STM32: */
static uint8_t ucCcmHeap[32U * 1024U]  __attribute__((section(".ccmram"))); /* Note: NOT DMA-accessible on F4! */
static uint8_t ucSramHeap[64U * 1024U];

void p1_init_split_heap5(void)
{
    /* Regions MUST be listed in ascending memory-address order, terminated by { NULL, 0 }! */
    const HeapRegion_t regions[] = {
        { ucCcmHeap,  sizeof(ucCcmHeap)  }, /* 0x10000000 (lower address FIRST!) */
        { ucSramHeap, sizeof(ucSramHeap) }, /* 0x20000000 (higher address SECOND)*/
        { NULL,       0U                 }  /* Mandatory end marker              */
    };
    vPortDefineHeapRegions(regions);        /* MUST be called BEFORE any xTaskCreate / xQueueCreate! */
}
```

---

## Scene 2: Software Timers & The `"Tmr Svc"` Daemon Task (`timers.c` — `Ch 6`)

### 1. The #1 Software Timer Truth: Callbacks Do NOT Run In `SysTick_Handler`!

When `configUSE_TIMERS = 1`, `vTaskStartScheduler()` automatically creates:
1. A standard FreeRTOS task named **`"Tmr Svc"`** (`prvTimerTask` in `timers.c`) at priority `configTIMER_TASK_PRIORITY` with stack `configTIMER_TASK_STACK_DEPTH`.
2. A private FreeRTOS Queue **`xTimerQueue`** of length `configTIMER_QUEUE_LENGTH`.

```text
  [Your App Task] --xTimerStart(tmr, wait)--> [ xTimerQueue ] ---> [ "Tmr Svc" Daemon Task ]
  [Hardware ISR ] --xTimerResetFromISR(...)--> (DaemonMsg_t)       |-> Maintains sorted expiry lists
                                                                   +-> Calls vMyTimerCallback(tmr)!
```

* **Why did FreeRTOS design Software Timers this way?**
  To keep `SysTick_Handler` (`ISR`) tiny, fast, and deterministic! Timer expiry math and user callback functions run in **normal `Thread` (`Task`) mode** inside `"Tmr Svc"`, **not** inside an interrupt!
* **Two Timer Types (`uxAutoReload` parameter in `xTimerCreate`):**
  1. **One-Shot (`pdFALSE`):** Fires its callback **once** after `xTimerPeriodInTicks`, then enters the **Dormant** state (until started/reset again)—ideal for **protocol response timeouts** or **button/relay pulse shutoff**.
  2. **Auto-Reload (`pdTRUE`):** Automatically reschedules itself every `xTimerPeriodInTicks`—ideal for **periodic status LED blink** or **lightweight housekeeping** without spending a whole `1 KB` task stack.
* **Three Golden Interview Rules For Software Timers:**
  1. **Callbacks MUST NEVER BLOCK (`0` wait ticks only!):** Every software timer in your entire firmware runs sequentially inside **the single `"Tmr Svc"` task**. If Timer A's callback calls `vTaskDelay(100)` or blocks on a Mutex, **Timer B, Timer C, and `xEventGroupSetBitsFromISR()` all freeze for `100 ms`!**
  2. **`xTimerReset()` re-zeros the countdown ("re-triggerable monostable"):** Calling `xTimerReset(watchdog_ui_tmr, 0)` every time a valid `CAN`/`UART` heartbeat frame arrives pushes expiry forward; the callback fires **only if silence exceeds the timeout**!
  3. **`configTIMER_TASK_PRIORITY` sets timer accuracy:** If `"Tmr Svc"` has priority `1`, a busy priority-`2` math task delays your timer callback! Give `"Tmr Svc"` a high priority when timer callbacks guard communication timeouts.

```c
static TimerHandle_t g_ic_heartbeat_tmr;

/* Runs inside "Tmr Svc" Daemon Task — MUST NOT call vTaskDelay() or blocking APIs! */
static void vHeartbeatTimeoutCallback(TimerHandle_t xTimer)
{
    uint32_t missed_cnt = (uint32_t)(uintptr_t)pvTimerGetTimerID(xTimer) + 1U;
    vTimerSetTimerID(xTimer, (void *)(uintptr_t)missed_cnt); /* Per-timer scratch ID in Timer_t */
    /* Notify high-priority Fault Task immediately with 0 blocking: */
    /* xTaskNotifyGive(g_fault_task_handle); */
}

void p1_heartbeat_timer_init(void)
{
    /* One-shot (pdFALSE), 200 ms timeout; starts in Dormant state until xTimerStart(): */
    g_ic_heartbeat_tmr = xTimerCreate("HbTmr", pdMS_TO_TICKS(200U), pdFALSE, (void *)0, vHeartbeatTimeoutCallback);
    (void)xTimerStart(g_ic_heartbeat_tmr, pdMS_TO_TICKS(5U)); /* Posts START command to xTimerQueue */
}

/* Called whenever a valid BMS IC frame arrives — restarts the 200 ms countdown from 0! */
void p1_on_valid_ic_frame_isr(BaseType_t *woken)
{
    (void)xTimerResetFromISR(g_ic_heartbeat_tmr, woken);
}
```

---

## Scene 3: Event Groups (`event_groups.c` — `Ch 9`) & The `FromISR` Interview Trap

### 1. Inside `EventGroup_t` — Why Only `24` Bits Are Usable On 32-Bit Cortex-M!

An Event Group stores one shared **`EventBits_t uxEventBits`** variable plus one waiting list (`xTasksWaitingForBits`).
* **Crucial Source Fact (`event_groups.c`):** On Cortex-M4 with standard `32`-bit `TickType_t`, **`EventBits_t` is `32` bits wide, YET ONLY BITS `0..23` (`24` bits) ARE USABLE!**
  * **Why?** FreeRTOS stores the **top `8` bits (`bits 24..31`, `0xFF000000UL`) as kernel control flags** (`CLEAR_ON_EXIT`, `WAIT_FOR_ALL_BITS`, `UNBLOCKED_DUE_TO_BIT_SET`) inside each sleeping task's list item! Passing bit `24..31` hits `configASSERT` immediately! *(If `configTICK_TYPE_WIDTH_IN_BITS` is `16`-bit, only `8` bits `0..7` are usable.)*

```text
32-bit EventBits_t Layout in FreeRTOS (when TickType_t is 32-bit):
  Bits 31 .. 24 (8 bits)              | Bits 23 .. 0 (24 bits)
  [ KERNEL CONTROL FLAGS - FORBIDDEN! ] [ APPLICATION EVENT FLAGS (BIT 0 .. BIT 23) ]
```

### 2. Three Things Event Groups Do That Queues & Semaphores Cannot

1. **Wait For `ANY` (`OR`) vs. `ALL` (`AND`) Bits (`xEventGroupWaitBits`):**
   * `xWaitForAllBits = pdFALSE` (`OR`): wake me if `FAULT_OVERVOLT | FAULT_OVERTEMP` happens.
   * `xWaitForAllBits = pdTRUE` (`AND`): wake me **only when** `INIT_SPI_OK & INIT_ADC_OK & INIT_NVM_OK` are **all `1`**!
2. **Broadcast Wakeup:** Calling `xEventGroupSetBits(eg, BIT_SHUTDOWN)` wakes **every task** waiting on `BIT_SHUTDOWN` simultaneously!
3. **Race-Free Multi-Task Rendezvous (`xEventGroupSync` — *Ch 9.4*):**
   * **Why NOT call `xEventGroupSetBits()` then `xEventGroupWaitBits()` for a barrier?** Because between those two lines, another task could wake up, see all bits set, and **clear** the bits before your task even reaches `WaitBits()`—hanging your task forever!
   * **`xEventGroupSync(eg, my_bit, all_3_bits, timeout)`** sets `my_bit` **and** tests/waits for `all_3_bits` **atomically inside one `vTaskSuspendAll()` block**, waking all 3 tasks together and clearing `all_3_bits` cleanly!

### 3. Classic Staff Interview Trap: Why Is `xEventGroupSetBitsFromISR()` Non-Deterministic?

Look at `xEventGroupSetBits()` in `event_groups.c`: it loops across **every** task in `xTasksWaitingForBits` (`while (pxListItem != pxListEnd)`), potentially waking `5` or `10` tasks!
* FreeRTOS **never** runs unbounded loops inside an `ISR` or masks interrupts for an unknown number of tasks.
* Therefore, **`xEventGroupSetBitsFromISR(eg, bits, &woken)` does NOT set bits inside the `ISR`!** Instead, it calls **`xTimerPendFunctionCallFromISR(&vEventGroupSetBitsCallback, ...)`**, sending a message to **`xTimerQueue`** so **`"Tmr Svc"` (`prvTimerTask`)** sets the bits later in task context!
* **Interview Takeaway:**
  * `xEventGroupSetBitsFromISR` requires **`configUSE_TIMERS = 1` AND `INCLUDE_xTimerPendFunctionCall = 1`**, can return **`pdFAIL` if `xTimerQueue` is full**, and depends on `"Tmr Svc"` priority!
  * If an `ISR` only needs to send bit flags to **one specific task**, use **`xTaskNotifyFromISR(task, bits, eSetBits, &woken)`** ([W03-L22](W03_L22_freertos_tasks_states_delays_and_notifications.md))—it executes in **`O(1)` directly inside the `ISR`** with zero Timer Queue overhead!

```c
#define BIT_SPI_READY   (1UL << 0)          /* Always keep within bits 0..23!                       */
#define BIT_ADC_READY   (1UL << 1)
#define BIT_NVM_READY   (1UL << 2)
#define ALL_SUBSYS_BITS (BIT_SPI_READY | BIT_ADC_READY | BIT_NVM_READY)

static EventGroupHandle_t g_boot_eg;

/* Called by each subsystem task (SPI, ADC, NVM) once its hardware self-test passes: */
bool p1_wait_for_all_subsystems_rendezvous(EventBits_t my_ready_bit)
{
    /* Atomically sets my_ready_bit AND blocks until ALL 3 tasks reach this barrier: */
    EventBits_t result = xEventGroupSync(g_boot_eg, my_ready_bit, ALL_SUBSYS_BITS, pdMS_TO_TICKS(500U));
    return ((result & ALL_SUBSYS_BITS) == ALL_SUBSYS_BITS); /* False if any task timed out!         */
}
```

---

## Scene 4: Stream Buffers & Message Buffers (`stream_buffer.c` — Fast `SPSC` `DMA -> Task`)

### 1. Why FreeRTOS Added `StreamBuffer` & `MessageBuffer` Alongside `Queue_t`

Suppose `USART2` `IDLE`-line `DMA` receives variable-length diagnostic frames (`5` bytes, then `47` bytes, then `12` bytes) and hands them to `1` Parser Task:
* Pushing `47` individual `1`-byte items into a `Queue_t` runs `47` critical sections! Putting `64`-byte structs into a `Queue_t` wastes `59` bytes when a `5`-byte packet arrives!
* **`StreamBuffer_t` (`stream_buffer.c`)** solves this for **strictly `1` Producer + `1` Consumer (`SPSC`)**:
  * **Hard Precondition:** **At most ONE writer (Task or `ISR`) and ONE reader (Task or `ISR`) may access a Stream/Message Buffer concurrently!** (Multiple writers require an external Mutex/critical section.)
  * **Exact Ring Math in `stream_buffer.c`:** Asking for `N = 64` bytes allocates **`xLength = 65` (`N + 1`) bytes** so **`xHead == xTail` always means Empty** and **`N` usable bytes** fit without a critical section on a shared counter! Wakes the reader using a fast **Task Notification** (`xTaskWaitingToReceive`).

| Feature | **Stream Buffer** (`xStreamBufferCreate(N, trig)`) | **Message Buffer** (`xMessageBufferCreate(N)`) |
| --- | --- | --- |
| **Data Model** | Continuous **unstructured byte stream** (write `10 B` + `6 B` -> reader asking for `16 B` gets all `16 B` together!) | **Discrete variable-length packets** (write `10 B`, then `6 B` -> 1st `Receive` gets **exact `10 B`**, 2nd `Receive` gets **exact `6 B`**!) |
| **Hidden Overhead** | `0` per-write header bytes; unblocks reader once `>= xTriggerLevelBytes` accumulate | Prepends **`4` bytes (`sizeof(size_t)`)** of packet length before each message (`10 B` packet consumes **`14 B`** in buffer!) |
| **Small Read Buffer** | Reads as many bytes as fit (`min(avail, rx_len)`) | **All-or-nothing:** if `rx_len < 10 B`, returns **`0` bytes** and leaves the `10 B` packet untouched in the buffer! |

```c
static MessageBufferHandle_t g_diag_msg_buf; /* Created via xMessageBufferCreate(128U)              */

/* Single Producer (UART IDLE-Line DMA ISR): pushes exact variable-length frame (e.g., 6..48 bytes) */
void uart_dma_rx_frame_isr(const uint8_t *dma_chunk, size_t len, BaseType_t *woken)
{
    /* Writes 4-byte length + len payload bytes; returns 0 if (len + 4) > free space! */
    (void)xMessageBufferSendFromISR(g_diag_msg_buf, dma_chunk, len, woken);
}

/* Single Consumer (Protocol Parser Task): pops one complete discrete packet at a time */
static void Task_DiagParser(void *arg)
{
    (void)arg;
    uint8_t rx_pkt[64];
    for (;;) {
        size_t rx_len = xMessageBufferReceive(g_diag_msg_buf, rx_pkt, sizeof(rx_pkt), portMAX_DELAY);
        if (rx_len > 0U) {
            /* rx_pkt[0 .. rx_len-1] holds ONE exact packet! */
        }
    }
}
```

---

## Scene 5: Stack Overflow Detection, Runtime `%CPU` Stats & Tickless Idle (`Ch 11–13`)

### 1. Stack Sizing (`Words` vs. `Bytes`!) & Comparing Method 1, Method 2, And Hardware `MPU` (`stack_macros.h`)

* **Word-vs-Byte Trap:** On 32-bit Cortex-M4 (`StackType_t = uint32_t`), `xTaskCreate(..., 256U, ...)` allocates **`256` words = `1,024` bytes** (NOT `256` bytes!), and `uxTaskGetStackHighWaterMark(NULL)` returns remaining unused **words** (`< 32` words left -> increase stack!).

```text
High Address: +-------------------------------+ <-- Initial Top of Stack
              | Saved frames & local vars     |   | Stack grows DOWNWARD on Cortex-M4!
              |           v   v   v           |   v
              | 0xA5A5A5A5  (unused painted)  | <-- High-Water Mark measures lowest point ever reached!
Low Address:  | pxStack[0..3] = 0xA5A5A5A5    | <-- 16-byte Canary Zone checked by Method 2!
              +-------------------------------+ <-- pxCurrentTCB->pxStack (Stack Limit!)
              [ Neighboring TCB / Global RAM! ] <-- Overwritten FIRST if stack overflows without MPU!
```

| Protection Mechanism | When It Runs | How It Works (`stack_macros.h`) | Blind Spot Interviewers Ask About! |
| --- | --- | --- | --- |
| **`configCHECK_FOR_STACK_OVERFLOW = 1`** (*Method 1*) | **Only at context switch** (swap-out) | Checks `if (pxTopOfStack <= pxStack)` -> calls `vApplicationStackOverflowHook()` | **Misses transient overflows** if a deep function overflowed and returned *before* `PendSV` ran! |
| **`configCHECK_FOR_STACK_OVERFLOW = 2`** (*Method 2*) | **Only at context switch** (swap-out) | Paints stack with `0xA5` at creation; checks Method 1 **OR** if bottom `16` bytes (`pulStack[0..3] != 0xA5A5A5A5U`) changed | **Reactive, not preventive!** Neighboring RAM is *already corrupted* before swap-out—and an uninitialized `uint8_t big[256]` skipping past `pulStack[0..3]` can miss the `0xA5` canary! |
| **Cortex-M4 `MPU` Guard / Armv8-M `PSPLIM`** ([W01-L18](W01_L18_cortexm4_mpu_privilege_psp_svc_and_memmanage.md)) | **Every single CPU memory instruction!** | `32-byte` `No-Access` `MPU` region at `pxStack` triggers `MemManage_Handler` **instantly** | **Zero silent RAM corruption!** Catches the exact faulting `PC` *before* neighboring data is touched! |

---

### 2. Per-Task `%CPU` Runtime Stats (`Ch 12.5`) — Why `1 kHz` `SysTick` Cannot Be Used!

Why does `configGENERATE_RUN_TIME_STATS = 1` require a **separate fast timer (`10 kHz – 100 kHz`, `10x–100x` faster than `SysTick`)** or Cortex-M `DWT->CYCCNT`?
* Suppose `Task H` wakes every `1 ms` on `SysTick`, finishes its `DMA` check in **`50 us` (`5%` CPU)**, and sleeps for `950 us`.
* If FreeRTOS measured time using `1 ms` `SysTick` integers, `0` ticks elapsed between `0.00 ms` and `0.05 ms`—reporting **`Task H = 0%`** (or `100%` if sampled on the boundary)! A `100 kHz` (`10 us`) hardware counter or `84 MHz` `DWT->CYCCNT` measures the exact `50 us` slice on every `PendSV` switch!

---

### 3. Low-Power Tickless Idle (`configUSE_TICKLESS_IDLE = 1` — `Ch 11`)

In battery-powered BMS / sensor nodes, waking the CPU `1,000` times per second just to increment `xTickCount` wastes power when the next periodic task isn't due for `200 ms`!
* **How `vPortSuppressTicksAndSleep(xExpectedIdleTime)` works when `Idle` runs:**
  1. Checks `xExpectedIdleTime >= configEXPECTED_IDLE_TIME_BEFORE_SLEEP` (e.g., `200` ticks).
  2. Stops `SysTick`, masks interrupts briefly, and calls `eTaskConfirmSleepModeStatus()` (if an `ISR` just arrived, aborts sleep immediately!).
  3. Loads the sleep timer (`SysTick->LOAD` for light `Sleep`, or an external `LPTIM`/`RTC` if `STOP` mode turns off the core clock!) for `200` ticks and executes **`__WFI()`** (*Wait For Interrupt*).
  4. Whether woken after all `200` ticks **or** early after `37` ticks by a `CAN`/`EXTI` interrupt, computes exact elapsed ticks (`37`), calls **`vTaskStepTick(37)`** to jump kernel time forward in one step, restores `1 ms` `SysTick`, and schedules the woken task!

---

## Master Interview Cheat-Sheet: Choosing The Right FreeRTOS Primitive In 5 Seconds

| Exact Communication / Locking Problem | Best FreeRTOS Choice | Why (In One Spoken Sentence) |
| --- | --- | --- |
| `1 ISR` (or Task) wakes **`1` known Task** (event counter or `32`-bit status mask) | **Task Notification** (`vTaskNotifyGiveFromISR` / `xTaskNotifyFromISR`) | `~45%` faster and `0` extra RAM (`uint32_t` already inside `TCB_t`)! |
| Pass small fixed-size structs (`4..16 B` `FaultRecord`) in strict FIFO order (`N` producers/consumers) | **Queue** (`xQueueCreate(N, sizeof(T))`) | Copies item bytes safely across stack/`MPU` boundaries; never overwrites unread slots. |
| Broadcast "latest sensor value" to multiple readers (`Display`, `Logger`, `Comms`) | **`1`-Slot Mailbox** (`xQueueCreate(1, S)` + `xQueueOverwrite` + `xQueuePeek`) | Always overwrites `Slot 0` with newest reading; `Peek` leaves it in place for all readers. |
| `1 DMA/UART ISR` streams variable-length packets to **`1` Parser Task** | **Stream / Message Buffer** | Lock-free `SPSC` byte ring (`MessageBuffer` frames discrete packets via a `4`-byte length prefix). |
| Wait until **ALL (`AND`)** of `3` subsystems finish init (multi-task rendezvous) | **Event Group** (`xEventGroupSync`) | Atomically sets own bit and blocks until `BIT_A & BIT_B & BIT_C` are all `1` (task-context best!). |
| Lock shared struct / peripheral between **Tasks** of different priorities | **Mutex** (`xSemaphoreCreateMutex`) | Tracks `xMutexHolder` and boosts `Task L -> Task H` via **Priority Inheritance** (`ISR` forbidden!). |
| Both **Tasks AND `ISR`s** must log/transmit on one shared `UART`/`SPI`/`I2C` bus | **Gatekeeper Task** + Queue | Single owner task serializes bus hardware—zero mutexes, zero priority inversion, `ISR`-safe! |
| Protect `< 1 us` `2`-word variable shared between **Task and `ISR`** | **`taskENTER_CRITICAL()`** (`BASEPRI`) | Masks `NVIC` priorities `5..15` for a few cycles while keeping `0..4` motor/trip `ISR`s live! |

---

## Control Flow In Three Parts (Software Timer + Deferred `EventGroupSetBitsFromISR` Path)

**(a) One-Line Chain:**
`EXTI_IRQHandler (MSP) -> xEventGroupSetBitsFromISR() -> xTimerPendFunctionCallFromISR() -> [xTimerQueue + PendSV] -> prvTimerTask "Tmr Svc" (PSP) -> vEventGroupSetBitsCallback() -> xEventGroupSetBits() -> Wakes ALL matching tasks!`

**(b) Step Table:**

| # | Where (`FreeRTOS-Kernel`) | Goal / State Change | Concrete Data In -> Out | Context / Mode |
| --- | --- | --- | --- | --- |
| 1 | `EXTI_IRQHandler` (`event_groups.c`) | Defer bit-set out of `ISR` | `xEventGroupSetBitsFromISR(eg, 0x04, &woken)` | Handler (`MSP`, prio `5`) |
| 2 | `timers.c : xTimerPendFunctionCallFromISR` | Queue callback msg to `"Tmr Svc"` | `DaemonTaskMessage_t` -> `xTimerQueue` | Handler (`MSP`, prio `5`) |
| 3 | `timers.c : prvTimerTask` (`"Tmr Svc"`) | Pop command & invoke callback | Calls `vEventGroupSetBitsCallback(eg, 0x04)` | Thread (`PSP`, `configTIMER_TASK_PRIORITY`) |
| 4 | `event_groups.c : xEventGroupSetBits` | Set bits `0..23` & wake all waiters | `uxEventBits \|= 0x04` -> wakes matching tasks | Thread (`PSP`, under `vTaskSuspendAll`) |
| 5 | `tasks.c : taskCHECK_FOR_STACK_OVERFLOW` | Verify outgoing stack on swap-out | Checks `pxTopOfStack` & `pulStack[0..3] == 0xA5A5A5A5` | Handler (`MSP`, inside `PendSV_Handler`) |

**(c) Execution Context Timeline:**

```text
Hardware ISR (MSP)           xTimerQueue (RAM)          "Tmr Svc" Daemon (PSP)        Waiting App Tasks (PSP)
------------------           -----------------          ----------------------        -----------------------
IRQ fires!
xEventGroupSetBitsFromISR()
  +-> Posts callback msg --> [ Slot 0: SetBits(0x04) ]
portYIELD_FROM_ISR(woken) ----------------------------> Pops Slot 0 from xTimerQueue
                                                        Runs xEventGroupSetBits(0x04)
                                                          +-> Broadcast unblocks ---> Task A & Task B wake!
```

---

## Authoritative Official Source & Documentation Lookup

| Topic | Official `FreeRTOS-Kernel` File | Official Training Book Chapter |
| --- | --- | --- |
| `heap_1`..`heap_5`, `BlockLink_t` coalescing & `vPortGetHeapStats` | [`portable/MemMang/heap_4.c`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/portable/MemMang/heap_4.c) | [*Mastering the FreeRTOS Real Time Kernel*, Ch 3](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book/blob/main/ch03.md) |
| Software Timers, `"Tmr Svc"` (`prvTimerTask`) & `xTimerQueue` | [`timers.c`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/timers.c) | [*Mastering the FreeRTOS Real Time Kernel*, Ch 6](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book/blob/main/ch06.md) |
| `EventGroup_t` (`24` usable bits), `xEventGroupSync` & `SetBitsFromISR` | [`event_groups.c`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/event_groups.c) | [*Mastering the FreeRTOS Real Time Kernel*, Ch 9](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book/blob/main/ch09.md) |
| `StreamBuffer_t` (`N + 1` ring) & `MessageBuffer` (`4-byte` length prefix) | [`stream_buffer.c`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/stream_buffer.c) | [FreeRTOS Official Stream & Message Buffers Docs](https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/04-Stream-and-message-buffers/01-Stream-and-message-buffers) |
| Tickless Idle (`vPortSuppressTicksAndSleep`), `%CPU` Stats & Stack Overflow | [`include/stack_macros.h`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/main/include/stack_macros.h) | [*Mastering the FreeRTOS Real Time Kernel*, Ch 11–13](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book/blob/main/ch13.md) |

---

## Feynman Recap

Think of these five remaining FreeRTOS tools by the exact real-world bug each one prevents: **`heap_4` / `heap_5`** merge neighboring freed RAM blocks so heap holes don't splinter over months of uptime (while **Static Allocation** eliminates heap RAM altogether in safety/MISRA firmware); **Software Timers** keep `SysTick` tiny by running all timer callbacks inside one background **`"Tmr Svc"` task** (which is why a timer callback must never sleep or block); **Event Groups** hold `24` application flag bits (`0..23`) so tasks can wait on `AND` combinations or synchronize at a barrier (`xEventGroupSync`)—remembering that `SetBitsFromISR` defers to `"Tmr Svc"`; **Stream & Message Buffers** give a single `DMA ISR -> 1 Task` pair a fast, lock-free byte/packet pipe; and **Stack Watermarks + `MPU` guards** catch stack overflows *before* they silently corrupt neighboring RAM!

---

## Spoken-English Interview Answers

### 1. Compare `heap_1`, `heap_2`, `heap_4`, `heap_5`, and Static Allocation in FreeRTOS. When can `pvPortMalloc` be called from an `ISR`?
* **Simple answer:** **Never** call `pvPortMalloc` or `vPortFree` from an `ISR`—they suspend the scheduler (`vTaskSuspendAll`), not interrupts!
* **How they differ:** `heap_1` never frees; `heap_2` frees best-fit without merging neighbors (fragments on varying sizes); `heap_4` sorts free blocks by address and **coalesces adjacent free blocks**; `heap_5` adds **multi-region non-contiguous SRAM** support (`CCM` + `SRAM1`); and `*CreateStatic()` uses `0` dynamic heap—my default choice for MISRA/automotive production code.

### 2. In what context do FreeRTOS Software Timer callbacks execute, and what happens if a callback blocks?
* **Simple answer:** They execute in **Task context** inside the **`"Tmr Svc"` Timer Daemon Task** (`prvTimerTask`), **not** inside `SysTick_Handler`!
* **Why blocking is fatal:** All software timers and `xTimerPendFunctionCallFromISR` callbacks share that **one** `"Tmr Svc"` task and `xTimerQueue`—so if one callback calls `vTaskDelay()` or blocks on a mutex/queue, **every other software timer in the system stalls**!

### 3. Why does `EventBits_t` only give `24` usable bits on a 32-bit MCU, and why is `xEventGroupSetBitsFromISR()` slower than `xTaskNotifyFromISR()`?
* **Simple answer:** FreeRTOS reserves the **top `8` bits (`bits 24..31`)** for internal event-list control flags (`WAIT_FOR_ALL`, `CLEAR_ON_EXIT`).
* **ISR trap:** Setting an Event Group bit can broadcast-unblock *many* tasks in a loop, so `xEventGroupSetBitsFromISR()` refuses to run that loop in the `ISR` and instead posts a job to `xTimerQueue` for `"Tmr Svc"`. For deterministic `O(1)` `ISR -> 1 Task` flags, I use `xTaskNotifyFromISR(..., eSetBits, &woken)`.

### 4. How do FreeRTOS Stack Overflow Method 1 and Method 2 work, and why do safety-critical Cortex-M designs still add an `MPU` stack guard?
* **Simple answer:** Both Method 1 (`pxTopOfStack <= pxStack`) and Method 2 (checking `16` bytes of `0xA5` canary pattern at `pxStack[0..3]`) run **only during a context switch** (`PendSV`).
* **Trade-off:** Method 1 misses spikes that already returned before `PendSV`; Method 2 catches most spikes **only after** neighboring RAM was already corrupted! Putting a `32-byte` `No-Access` `MPU` region (or Cortex-M33 `PSPLIM`) at the bottom of the stack triggers `MemManage_Handler` on the **exact offending instruction** *before* any neighbor corruption occurs.

---

## Corrections

- 2026-11-20: Scoped coverage to selected interview topics rather than claiming completeness across interviews or the official book.

- 2026-11-20: Initial publication (Day 1 of the 10-Day Interview-Focused Plan), verified directly against upstream `FreeRTOS-Kernel` (`heap_4.c`, `timers.c`, `event_groups.c`, `stream_buffer.c`, `stack_macros.h`) and *Mastering the FreeRTOS Real Time Kernel* (Chapters 3, 6, 9, 11–13). Next planned lesson: Day 2 (`W04-L27`) — Embedded C Live-Coding & Concurrency Traps (`volatile`/barriers, alignment/endianness, wrap-safe tick math, Ring Buffer `C07`, and Producer/Consumer Stop & Drain `C04`).
