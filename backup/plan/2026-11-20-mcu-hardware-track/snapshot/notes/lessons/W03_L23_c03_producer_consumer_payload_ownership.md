# W03-L23: Event Producer/Consumer Ownership Model (`C03`) — Payload Lifetime, Torn Reads, Copy Queues, And Zero-Copy Slot Pools (`P1`)

> **Scope & verification status:** Source-reviewed C03 lesson, repaired with approval on 2026-11-17. The standalone C11 pool model is sequential; its critical-section hooks are no-ops. FreeRTOS/ISR/DMA snippets are interface illustrations. The learner selected source review only: no compilation, execution, target synchronization or timing result is claimed.

Previous: [FreeRTOS from the ground up — task states, TCB & lists, priorities, `vTaskDelayUntil`, and ISR task notifications (`P1`)](W03_L22_freertos_tasks_states_delays_and_notifications.md).

---

## The Core Idea: Notification Tells You *When*, Ownership Tells You *Whose Bytes They Are*

In [W03-L22](W03_L22_freertos_tasks_states_delays_and_notifications.md), `EXTI15_10_IRQHandler` woke `vButtonServiceTask` using `vTaskNotifyGiveFromISR()`. That worked because a button press had **no data payload**—knowing *"an edge happened"* was enough.

Now look at our real `P1` fault recorder and sensor service on `STM32F446RE`:
* When an interrupt or cyclic acquisition completes, the **Producer** (an ISR, DMA callback, or acquisition task) must hand a **multi-byte payload** (for example, a 16-byte fault record or a 64-byte SPI/ADC sample block) to a **Consumer** (the diagnostic/recorder task).
* A notification (`vTaskNotifyGiveFromISR`) **only wakes the consumer**—it does **not** protect the payload bytes in RAM!

Two useful ownership models for our P1 handoff are:

1. **Model A — Copy-By-Value Queue (For Small Payloads, e.g., `4` to `32` Bytes):**
   * The Producer fills a local struct, and the queue (`xQueueSendFromISR`) **copies all bytes** into the queue's own internal slot array while interrupts are masked.
   * **Ownership rule:** The Producer owns its local struct; the Queue owns the queued copy; the Consumer copies the bytes out into its own local struct (`xQueueReceive`). They never share the same buffer in RAM.
2. **Model B — Zero-Copy Static Slot Pool (`FREE -> FILLING -> READY -> IN_USE -> FREE`, For Larger Payloads `64+` Bytes):**
   * Copying `256 bytes` inside a critical section increases interrupt-masking time. Whether that exceeds the budget depends on the target, compiler and measured workload; these sizes are examples, not universal cutoffs.
   * Instead, we pre-allocate a static array of `N` fixed-size buffers (**slots**) in SRAM.
   * The Producer claims one `FREE` slot (`FILLING`), writes the `256` bytes **outside** any critical section, and then hands off **only the 1-byte/4-byte `slot_id`** (`READY`) to the Consumer.
   * **Ownership rule:** At every microsecond, **at most ONE actor (Pool, Producer, Queue, or Consumer) is allowed to touch a given slot**. Once the Producer publishes `slot_id` (`READY`), the Producer **loses permission to touch that slot** until the Consumer finishes reading (`IN_USE`) and explicitly returns it to `FREE`!

```text
MODEL A — Copy-By-Value Queue (Small Structs, e.g. 16-Byte Fault Event):
  Producer Stack Struct --(memcpy in Queue)--> [Queue Storage] --(memcpy out)--> Consumer Stack Struct
  (Producer can reuse its local variable immediately!)

MODEL B — Zero-Copy Static Slot Pool (Large Blocks, e.g. 64..256-Byte SPI/ADC Frame):
  [SLOT_FREE] ---alloc---> [SLOT_FILLING] ---publish---> [SLOT_READY] ---acquire---> [SLOT_IN_USE]
  (Pool owns)              (Producer writes              (In Queue:                  (Consumer reads
                            payload bytes)                nobody writes!)             payload, then
                                                                                      releases -> FREE)
```

---

## Step 1 — The 3 Classic Embedded C Payload Bugs (What Breaks Without Explicit Ownership)

Before building the two clean solutions, understand the **three exact bugs** that crash production MCU firmware when engineers mix up notification with ownership:

### Bug 1 — Dangling Stack Pointer (Object Lifetime Ended Before Consumer Ran)
* A Producer task creates a local struct on its stack (`p1_frame_t frame;`), pushes the **pointer `&frame`** into a queue, and exits the function (or loops to the next iteration).
* By the time the Consumer task wakes up and dereferences that pointer, the Producer's stack frame has already been overwritten by another function call!
* **C Lifetime Rule:** An automatic local object's lifetime ends when execution leaves its block. An asynchronous borrow is valid only if the object stays alive and unchanged until the borrower finishes. A loop may also reuse a still-live object too early; both lifetime and reuse must be controlled.

### Bug 2 — Torn Read on a Single Global Struct (`volatile` Does NOT Fix This!)
* An engineer declares one global struct `volatile p1_frame_t g_frame;` in SRAM.
* While the Consumer task (Thread mode) is halfway through reading `g_frame` (it read `g_frame.seq = 10`, has not read `g_frame.crc` yet), the Producer ISR preempts the Consumer and overwrites `g_frame` with `seq = 11` and the CRC for `11`!
* When the ISR returns, the Consumer reads the **new** `crc` with the **old** `seq = 10` — a **torn read** (half packet `10`, half packet `11`)!
* **Why `volatile` does NOT prevent this:** `volatile` only tells the C compiler *"emit real `ldr`/`str` instructions instead of caching in a CPU register."* It does **not** make a multi-word struct read atomic and does **not** stop an ISR from preempting between two `ldr` instructions!

### Bug 3 — Premature Buffer Reuse (Producer Overwrites While Consumer Is Still Reading)
* An ISR writes into `rx_buf`, notifies the Consumer task, and on the next fast hardware event writes into `rx_buf` again *before* the lower-priority Consumer task finished parsing the first packet.

```text
How Bug 2 (Torn Read on Single Global `g_frame`) Happens on Cortex-M4:

  Consumer Task (Thread Mode, PSP):
    1. Reads `g_frame.seq`     ---> gets `10` (Old packet)
       |
       +---> [PREEMPTED BY ISR!]
               Producer ISR (Handler Mode, MSP):
                 2. Writes `g_frame.seq = 11`
                 3. Writes `g_frame.value = 0xBEEF` (New packet)
               [ISR RETURNS]
       |
    4. Reads `g_frame.value`   ---> gets `0xBEEF` (New packet!)
    Result: Consumer combines `seq = 10` with `value = 0xBEEF` -> SILENT CORRUPTION!
```

### Step 1 Code: Contrasting the Broken Global Struct vs. Explicit Contract Rules

```c
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t seq;           /* Monotonic sequence number           */
    uint32_t timestamp_ms;  /* Capture tick                        */
    uint16_t fault_code;    /* Diagnostic fault identifier         */
    uint16_t raw_adc;       /* Sampled reading                     */
} p1_fault_frame_t;         /* 12 bytes (3 x 32-bit words)         */

/* BROKEN PATTERN (Bug 2): Marking this `volatile` does NOT stop an ISR from
 * preempting the consumer task between reading `seq` and reading `raw_adc`! */
static volatile p1_fault_frame_t g_broken_shared_frame;
```

---

## Step 2 — Model A: Copy-By-Value Ring Queue (For Small Fixed Structs Like `p1_fault_frame_t`)

When each event is small (such as our 12-byte `p1_fault_frame_t`), **copying the struct by value** inside a bounded ring queue is the simplest and safest design:

1. **Contract & Preconditions:**
   * **Capacity:** Fixed array of `CAP = 4` `p1_fault_frame_t` elements (no heap `malloc`).
   * **Output initialization:** `copy_queue_pop()` zeroes `*out_frame` **first** before any early `false` return, so a caller that forgets to initialize its variable never reads uninitialized stack garbage.
   * **Pointer checks:** Rejects `NULL` pointers (`q == NULL` or `frame == NULL`) cleanly with `false`.
   * **Overload policy (`count == CAP`):** An ISR or bounded cyclic producer **cannot sleep** when the queue is full. We use an explicit **Drop-Newest** policy: reject the new item, leave the `4` already-queued items intact, and increment `q->dropped_count` so diagnostics know an overload occurred!
2. **Why Lifetime Is Safe:**
   * `q->items[q->head] = *in_frame;` copies the value into independent queue storage. The Producer keeps ownership of its input and may reuse it after `copy_queue_push()` returns.
   * `copy_queue_pop()` copies the value into `*out_frame`. After a successful pop, that queue slot is free and the Consumer owns its private copy.

**Model boundary:** The functions below are sequential and contain no interrupt
masking. For concurrent use, synchronize each entire operation, including
capacity checks, copies, indexes and counters, against every participating
context; masking only priorities `5..15` does not exclude an ISR at priority
`0..4`. Prefer the real FreeRTOS queue APIs for the RTOS transfer.
Initialize `q` to zero before use. Non-NULL pointers must address valid objects
of the declared type; input/output objects must not overlap queue storage or
metadata. A NULL check cannot validate a dangling pointer.

```text
Copy-By-Value Queue (`CAP = 4`, `count` tracks `0..4` items):

  push(&in_frame):
    - If `count == 4` (Full): `++dropped_count`; return `false` (Drop-Newest, O(1))
    - Else: copy `*in_frame` into `items[head]`, `head = (head + 1) % 4`, `++count`

  pop(&out_frame):
    - First zero `*out_frame = {0}`
    - If `count == 0` (Empty): return `false`
    - Else: copy `items[tail]` into `*out_frame`, `tail = (tail + 1) % 4`, `--count`
```

### Step 2 Code: Bounded Copy-By-Value Queue (`copy_queue_push` / `copy_queue_pop`)

```c
#define P1_COPY_Q_CAP 4U

typedef struct {
    p1_fault_frame_t items[P1_COPY_Q_CAP];
    uint32_t         head;          /* Write index: 0 .. P1_COPY_Q_CAP - 1 */
    uint32_t         tail;          /* Read  index: 0 .. P1_COPY_Q_CAP - 1 */
    uint32_t         count;         /* Current queued items: 0 .. 4        */
    uint32_t         dropped_count; /* Incremented on Drop-Newest overload */
} p1_copy_queue_t;

/* Note: On STM32 bare-metal/RTOS, wrap push/pop in critical section (BASEPRI/PRIMASK)
 * or use FreeRTOS `xQueueSendFromISR` / `xQueueReceive`, which do this exact copy! */
static bool copy_queue_push(p1_copy_queue_t *q, const p1_fault_frame_t *in_frame)
{
    if ((q == (p1_copy_queue_t *)0) || (in_frame == (const p1_fault_frame_t *)0)) {
        return false;
    }
    if (q->count >= P1_COPY_Q_CAP) {
        ++q->dropped_count;         /* Explicit Drop-Newest overload accounting */
        return false;
    }

    q->items[q->head] = *in_frame;  /* Value copy: producer's struct stays independent */
    q->head = (q->head + 1U) % P1_COPY_Q_CAP;
    ++q->count;
    return true;
}

static bool copy_queue_pop(p1_copy_queue_t *q, p1_fault_frame_t *out_frame)
{
    if (out_frame == (p1_fault_frame_t *)0) {
        return false;
    }
    *out_frame = (p1_fault_frame_t){0}; /* Always zero output BEFORE early returns! */

    if ((q == (p1_copy_queue_t *)0) || (q->count == 0U)) {
        return false;
    }

    *out_frame = q->items[q->tail];     /* Value copy out to consumer's private struct */
    q->tail = (q->tail + 1U) % P1_COPY_Q_CAP;
    --q->count;
    return true;
}
```

---

## Step 3 — Model B: Zero-Copy Slot Pool With Explicit 4-State Ownership (`FREE -> FILLING -> READY -> IN_USE`)

Now suppose each acquisition block is **`64 bytes` (or `256 bytes`)** of SPI/ADC samples. We do **not** want to copy `256 bytes` inside a critical section!

Instead, we build a **Zero-Copy Slot Pool** where **only a tiny handle (`slot_id` + `generation`)** moves through the queue, while the `64-byte` payload sits still in SRAM and changes **ownership state**:

### 1. What Each of the 4 Slot States Means (And Who Owns the Bytes)

| Slot State | Who Owns `slot.payload[]` Right Now? | Allowed Operations |
| --- | --- | --- |
| **`SLOT_FREE` (`0`)** | **The Pool** | Waiting to be allocated. Neither Producer nor Consumer may read or write `payload[]`. |
| **`SLOT_FILLING` (`1`)** | **Producer Exclusively** | Producer writes `payload[]` **without** holding any critical section! Consumer cannot see it yet. |
| **`SLOT_READY` (`2`)** | **Handoff Queue (In-Flight)** | Producer finished writing and queued `(slot_id, generation)`. **Producer is now FORBIDDEN to touch this slot!** |
| **`SLOT_IN_USE` (`3`)** | **Consumer Exclusively** | Consumer dequeued `(slot_id, generation)` and reads `payload[]` at its own pace while future ISRs fill *other* `SLOT_FREE` slots! |

### 2. Why Do We Include a `generation` Counter Inside the Handle?

The generation is a finite 32-bit reuse counter, not a permanent identity.
After `2^32` increments it repeats. The contract excludes a stale handle
surviving that many reuses (or a pool reset); a stronger session/retirement
policy is needed otherwise. Raw payload pointers are borrowed views: a
generation check cannot revoke a pointer already retained by buggy code.

Suppose a buggy Consumer accidentally calls `slot_pool_release(pool, handle)` **twice** with the same `handle`:
* On the 1st release, Slot `0` returns to `SLOT_FREE`, **increments `slot->generation` (`1 -> 2`)**, and gets allocated to a new ISR event (`SLOT_FILLING`, `generation = 2`).
* On the 2nd (buggy) release, the Consumer passes its old `handle` (`slot_id = 0, generation = 1`).
* Our pool checks `slot->generation == handle.generation` (`2 != 1`) **and** `slot->state == SLOT_IN_USE` -> **immediately catches and rejects the stale double-release** without corrupting the new ISR's buffer!

```text
Zero-Copy Slot Lifecycle & Generation Protection:

  1. Producer calls `pool_alloc()`       : SLOT_FREE    ---> SLOT_FILLING (gen = 1, returns handle{id=0, gen=1})
  2. Producer writes `buf[0..63]`        : (Outside lock! Fast, zero interrupt masking!)
  3. Producer calls `pool_publish()`     : SLOT_FILLING ---> SLOT_READY   (pushes handle{id=0, gen=1} to queue)
  4. Consumer calls `pool_acquire()`     : SLOT_READY   ---> SLOT_IN_USE  (returns const pointer to buf[0..63])
  5. Consumer reads `buf[0..63]`         : (Outside lock! Meanwhile Producer can alloc Slot 1!)
  6. Consumer calls `pool_release()`     : SLOT_IN_USE  ---> SLOT_FREE    (gen becomes 2! Old handle{0,1} is now invalid!)
```

### Step 3 Code: Zero-Copy Slot Pool (`alloc -> publish -> acquire -> release`)

```c
#define P1_POOL_SLOTS      3U    /* 3 buffers: 1 filling in ISR, 1 queued, 1 in consumer */
#define P1_PAYLOAD_BYTES  64U    /* 64-byte sensor/diagnostic block (zero-copy!)         */

typedef enum {
    SLOT_FREE    = 0,
    SLOT_FILLING = 1,
    SLOT_READY   = 2,
    SLOT_IN_USE  = 3
} slot_state_t;

/* Lightweight 8-byte token passed by value through the queue (instead of 64 bytes!): */
typedef struct {
    uint16_t slot_id;            /* Index 0 .. P1_POOL_SLOTS - 1                */
    uint16_t payload_len;        /* Valid bytes written by producer (0 .. 64)   */
    uint32_t generation;         /* Must match slot->generation to be valid     */
} slot_handle_t;

typedef struct {
    slot_state_t state;
    uint32_t     generation;
    uint8_t      payload[P1_PAYLOAD_BYTES];
} p1_slot_t;
```

---

## Step 4 — Using Model B Inside FreeRTOS On `STM32F446RE` (`P1` ISR -> Queue -> Task)

Look at how cleanly Model B maps onto FreeRTOS on our `NUCLEO-F446RE` (`P1`):
1. **At `init()`:** Create a FreeRTOS queue that holds **`slot_handle_t` (only `8 bytes` per item)** instead of `64-byte` buffers:
   ```c
   xReadyQueue = xQueueCreate(P1_POOL_SLOTS, sizeof(slot_handle_t));
   ```
2. **Inside the Producer ISR (`DMA` / `SPI` / `TIM` ISR at HW Priority `5..14`):**
   * Claims a `SLOT_FREE` buffer (`SLOT_FILLING`), writes the valid payload bytes, transitions that slot to `SLOT_READY`, and sends the 8-byte `slot_handle_t` with **`xQueueSendFromISR(xReadyQueue, &handle, &xHigherPriorityTaskWoken)`** + **`portYIELD_FROM_ISR(xHigherPriorityTaskWoken)`**!
   * For DMA, keep the slot `FILLING` until transfer completion is confirmed and the required memory-visibility rules are satisfied. Starting DMA does not make the payload ready. On cancellation, stop DMA and establish quiescence before returning the slot.
   * If `xQueueSendFromISR()` ever fails, the Producer unwinds that slot (`SLOT_READY -> SLOT_FREE`) so the slot is never leaked!
3. **Inside the Consumer Task (`vDiagRecorderTask`):**
   * Sleeps at **`0% CPU` (`Blocked`)** inside **`xQueueReceive(xReadyQueue, &handle, portMAX_DELAY)`**.
   * When woken, transitions the slot `SLOT_READY -> SLOT_IN_USE`, processes the `64` bytes directly in place (zero copy!), and calls `pool_release()` (`SLOT_IN_USE -> SLOT_FREE`) when done!

### Step 4 Code: FreeRTOS Zero-Copy ISR Producer -> Consumer Task Pattern

```c
/* Illustrative FreeRTOS integration snippet for P1 (requires FreeRTOS.h & queue.h):
 *
 * Producer ISR (HW Priority 5..14):
 *   uint8_t *buf = NULL;
 *   slot_handle_t h;
 *   if (p1_pool_alloc_isr(&g_pool, &h, &buf)) {
 *       buf[0] = status_reg;               // Exactly one byte has been initialized
 *       h.payload_len = 1U;                // Publish only that valid byte
 *       if (p1_pool_mark_ready_isr(&g_pool, &h)) {
 *           BaseType_t woken = pdFALSE;
 *           if (xQueueSendFromISR(xReadyQueue, &h, &woken) != pdPASS) {
 *               record_queue_full_isr();  // Bounded diagnostic, no blocking/printing
 *               if (!p1_pool_abort_ready_isr(&g_pool, h)) {
 *                   latch_pool_fault_isr(); // Stop new acquisition; retain bad slot
 *               }
 *           }
 *           portYIELD_FROM_ISR(woken);
 *       } else {
 *           if (!p1_pool_abort_filling_isr(&g_pool, h)) {
 *               latch_pool_fault_isr();
 *           }
 *           record_publish_error_isr();
 *       }
 *   }
 *
 * Consumer Task (Thread Mode on PSP):
 *   for (;;) {
 *       slot_handle_t h;
 *       if (xQueueReceive(xReadyQueue, &h, portMAX_DELAY) == pdPASS) { // 0% CPU while waiting!
 *           const uint8_t *ro_data = NULL;
 *           uint16_t len = 0U;
 *           if (p1_pool_begin_consume(&g_pool, h, &ro_data, &len)) {
 *               process_p1_block(ro_data, len); // Read only len initialized bytes
 *               if (!p1_pool_end_consume(&g_pool, h)) {
 *                   latch_pool_fault();  // Failed release needs recovery
 *               }
 *           } else {
 *               latch_pool_fault();
 *           }
 *       }
 *   }
 */
```

---

## Complete Standalone Runnable C03 Code (`c03_ownership_handoff_test.c`)

This standalone host model checks a payload handoff, pool exhaustion, duplicate
release, stale release after reuse, and abort after an oversized publication.
Suggested learner command:
`gcc -std=c11 -Wall -Wextra -Werror c03_ownership_handoff_test.c -o c03_test.exe`,
then `.\c03_test.exe` in PowerShell. Expected: `C03 ownership checks: PASS`,
exit status `0`; a failed check prints a failure and returns `1`.

**Pool contract:** All objects are valid and initialized; output parameters
point to separate writable objects outside the pool. Zero-length payloads are
allowed; lengths greater than capacity fail. Publish failure keeps a valid
slot `FILLING`, owned by its producer, who must retry or abort it.
Initialize/reset only when no owner or callback retains a handle/pointer.
Only the pool API mutates its metadata and ready queue. This invariant makes
internally queued IDs/lengths valid; the queue is not an untrusted-input parser.
The FreeRTOS illustration uses a separate queue and named synchronization/error
helpers that must be implemented for the target; the following host code uses
its own queue instead. Its no-op hooks do not establish ISR/DMA safety.

```c
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define P1_POOL_SLOTS      2U    /* Deliberately small (2 slots) to test burst overload! */
#define P1_PAYLOAD_BYTES  16U

typedef enum {
    SLOT_FREE    = 0,
    SLOT_FILLING = 1,
    SLOT_READY   = 2,
    SLOT_IN_USE  = 3
} slot_state_t;

typedef struct {
    uint16_t slot_id;
    uint16_t payload_len;
    uint32_t generation;
} slot_handle_t;

typedef struct {
    slot_state_t state;
    uint32_t     generation;
    uint8_t      payload[P1_PAYLOAD_BYTES];
} p1_slot_t;

typedef struct {
    p1_slot_t     slots[P1_POOL_SLOTS];
    slot_handle_t ready_q[P1_POOL_SLOTS];
    uint32_t      q_head;
    uint32_t      q_tail;
    uint32_t      q_count;
    uint32_t      pool_exhausted_drops;
    uint32_t      invalid_state_errors;
} p1_zero_copy_bus_t;

/* Enter/Exit critical section hooks:
 * On STM32 Cortex-M4 bare-metal: save/restore PRIMASK or BASEPRI.
 * In host unit test: single-threaded deterministic step interleaving. */
static inline uint32_t crit_enter(void) { return 0U; }
static inline void     crit_exit(uint32_t saved) { (void)saved; }

bool p1_bus_init(p1_zero_copy_bus_t *bus)
{
    if (bus == (p1_zero_copy_bus_t *)0) {
        return false;
    }
    *bus = (p1_zero_copy_bus_t){0};
    for (uint16_t i = 0U; i < P1_POOL_SLOTS; ++i) {
        bus->slots[i].state      = SLOT_FREE;
        bus->slots[i].generation = 1U; /* Non-zero starting generation */
    }
    return true;
}

/* STAGE 1 (Producer): Claim one SLOT_FREE -> SLOT_FILLING and get writable buffer: */
bool p1_producer_alloc(p1_zero_copy_bus_t *bus,
                       slot_handle_t *out_handle,
                       uint8_t **out_writable_buf)
{
    if (out_handle != (slot_handle_t *)0) {
        *out_handle = (slot_handle_t){0};
    }
    if (out_writable_buf != (uint8_t **)0) {
        *out_writable_buf = (uint8_t *)0;
    }
    if ((bus == (p1_zero_copy_bus_t *)0) ||
        (out_handle == (slot_handle_t *)0) ||
        (out_writable_buf == (uint8_t **)0)) {
        return false;
    }

    uint32_t mask = crit_enter();
    for (uint16_t i = 0U; i < P1_POOL_SLOTS; ++i) {
        if (bus->slots[i].state == SLOT_FREE) {
            bus->slots[i].state = SLOT_FILLING;
            *out_handle = (slot_handle_t){
                .slot_id     = i,
                .payload_len = 0U,
                .generation  = bus->slots[i].generation
            };
            *out_writable_buf = bus->slots[i].payload;
            crit_exit(mask);
            return true;
        }
    }
    ++bus->pool_exhausted_drops;
    crit_exit(mask);
    return false;
}

/* STAGE 2 (Producer): Publish filled slot (SLOT_FILLING -> SLOT_READY) into queue.
 * After this call succeeds, Producer MUST NOT read or write the slot's payload! */
bool p1_producer_publish(p1_zero_copy_bus_t *bus,
                         slot_handle_t handle,
                         uint16_t payload_len)
{
    if ((bus == (p1_zero_copy_bus_t *)0) ||
        (handle.slot_id >= P1_POOL_SLOTS) ||
        (payload_len > P1_PAYLOAD_BYTES)) {
        return false;
    }

    uint32_t mask = crit_enter();
    p1_slot_t *slot = &bus->slots[handle.slot_id];

    if ((slot->state != SLOT_FILLING) ||
        (slot->generation != handle.generation) ||
        (bus->q_count >= P1_POOL_SLOTS)) {
        ++bus->invalid_state_errors;
        crit_exit(mask);
        return false;
    }

    handle.payload_len = payload_len;
    slot->state        = SLOT_READY;

    bus->ready_q[bus->q_head] = handle;
    bus->q_head = (bus->q_head + 1U) % P1_POOL_SLOTS;
    ++bus->q_count;

    crit_exit(mask);
    return true;
}

/* Producer abort: return a still-owned FILLING slot after failed publication. */
bool p1_producer_abort(p1_zero_copy_bus_t *bus, slot_handle_t handle)
{
    if ((bus == (p1_zero_copy_bus_t *)0) || (handle.slot_id >= P1_POOL_SLOTS)) {
        return false;
    }
    uint32_t mask = crit_enter();
    p1_slot_t *slot = &bus->slots[handle.slot_id];
    if ((slot->state != SLOT_FILLING) || (slot->generation != handle.generation)) {
        ++bus->invalid_state_errors;
        crit_exit(mask);
        return false;
    }
    ++slot->generation;
    slot->state = SLOT_FREE;
    crit_exit(mask);
    return true;
}

/* STAGE 3 (Consumer): Pop ready handle and claim SLOT_READY -> SLOT_IN_USE: */
bool p1_consumer_acquire(p1_zero_copy_bus_t *bus,
                         slot_handle_t *out_handle,
                         const uint8_t **out_readonly_buf,
                         uint16_t *out_len)
{
    if (out_handle != (slot_handle_t *)0) {
        *out_handle = (slot_handle_t){0};
    }
    if (out_readonly_buf != (const uint8_t **)0) {
        *out_readonly_buf = (const uint8_t *)0;
    }
    if (out_len != (uint16_t *)0) {
        *out_len = 0U;
    }
    if ((bus == (p1_zero_copy_bus_t *)0) ||
        (out_handle == (slot_handle_t *)0) ||
        (out_readonly_buf == (const uint8_t **)0) ||
        (out_len == (uint16_t *)0)) {
        return false;
    }

    uint32_t mask = crit_enter();
    if (bus->q_count == 0U) {
        crit_exit(mask);
        return false;
    }

    slot_handle_t h = bus->ready_q[bus->q_tail];
    bus->q_tail = (bus->q_tail + 1U) % P1_POOL_SLOTS;
    --bus->q_count;

    p1_slot_t *slot = &bus->slots[h.slot_id];
    if ((slot->state != SLOT_READY) || (slot->generation != h.generation)) {
        ++bus->invalid_state_errors;
        crit_exit(mask);
        return false;
    }

    slot->state       = SLOT_IN_USE;
    *out_handle       = h;
    *out_readonly_buf = slot->payload;
    *out_len          = h.payload_len;
    crit_exit(mask);
    return true;
}

/* STAGE 4 (Consumer): Return finished slot to pool (SLOT_IN_USE -> SLOT_FREE)
 * and bump generation; old handles are invalid within the stated reuse window: */
bool p1_consumer_release(p1_zero_copy_bus_t *bus, slot_handle_t handle)
{
    if ((bus == (p1_zero_copy_bus_t *)0) || (handle.slot_id >= P1_POOL_SLOTS)) {
        return false;
    }

    uint32_t mask = crit_enter();
    p1_slot_t *slot = &bus->slots[handle.slot_id];

    if ((slot->state != SLOT_IN_USE) || (slot->generation != handle.generation)) {
        ++bus->invalid_state_errors;    /* Catches double-release or stale handle! */
        crit_exit(mask);
        return false;
    }

    ++slot->generation;                 /* Invalidate old handles to this slot     */
    slot->state = SLOT_FREE;
    crit_exit(mask);
    return true;
}

int main(void)
{
    p1_zero_copy_bus_t bus;
    if (!p1_bus_init(&bus)) {
        fprintf(stderr, "FAIL: pool init\n");
        return 1;
    }

    /* 1. Producer allocates Slot 0, writes "FAULT_01", and publishes it: */
    slot_handle_t p_h1;
    uint8_t      *wbuf1 = NULL;
    bool ok1 = p1_producer_alloc(&bus, &p_h1, &wbuf1);
    if (!ok1) {
        fprintf(stderr, "FAIL: first allocation\n");
        return 1;
    }
    memcpy(wbuf1, "FAULT_01", 8U);
    bool pub1 = p1_producer_publish(&bus, p_h1, 8U);

    /* 2. Consumer acquires Slot 0 (now SLOT_IN_USE) and starts reading it: */
    slot_handle_t  c_h1;
    const uint8_t *rbuf1 = NULL;
    uint16_t       rlen1 = 0U;
    bool acq1 = p1_consumer_acquire(&bus, &c_h1, &rbuf1, &rlen1);
    if (!acq1) {
        fprintf(stderr, "FAIL: first acquisition\n");
        return 1;
    }

    /* 3. While Consumer STILL holds Slot 0 in SLOT_IN_USE, Producer fires TWICE:
     *    - Burst A gets Slot 1 ("FAULT_02") -> succeeds without touching Slot 0!
     *    - Burst B finds 0 FREE slots -> cleanly increments pool_exhausted_drops! */
    slot_handle_t p_h2, p_h3;
    uint8_t      *wbuf2 = NULL, *wbuf3 = NULL;
    bool ok2 = p1_producer_alloc(&bus, &p_h2, &wbuf2);
    if (!ok2) {
        fprintf(stderr, "FAIL: second allocation\n");
        return 1;
    }
    memcpy(wbuf2, "FAULT_02", 8U);
    bool pub2 = p1_producer_publish(&bus, p_h2, 8U);
    bool ok3  = p1_producer_alloc(&bus, &p_h3, &wbuf3); /* Expected: false (drop!) */

    /* Verify Consumer's Slot 0 data ("FAULT_01") was NEVER overwritten by Burst A/B: */
    bool slot0_intact = (rlen1 == 8U) && (memcmp(rbuf1, "FAULT_01", 8U) == 0);

    /* 4. Consumer releases Slot 0 once (OK), then tries a buggy double-release (Rejected!): */
    bool rel1_ok        = p1_consumer_release(&bus, c_h1);
    bool rel1_duplicate = p1_consumer_release(&bus, c_h1); /* Expected: false! */

    bool pass = pub1 && pub2 && !ok3 && slot0_intact && rel1_ok
                && !rel1_duplicate && (bus.pool_exhausted_drops == 1U)
                && (bus.invalid_state_errors == 1U) && (wbuf3 == NULL)
                && (p_h3.generation == 0U);

    /* Reuse Slot 0 at generation 2; queue order is now Slot 1 then Slot 0. */
    slot_handle_t reused;
    uint8_t *new_buf = NULL;
    if (!p1_producer_alloc(&bus, &reused, &new_buf)) {
        fprintf(stderr, "FAIL: reuse allocation\n");
        return 1;
    }
    pass = pass && (reused.slot_id == c_h1.slot_id)
                && (reused.generation == 2U);
    new_buf[0] = 0xA5U;
    bool republished = p1_producer_publish(&bus, reused, 1U);

    slot_handle_t current;
    const uint8_t *data = NULL;
    uint16_t len = 0U;
    if (!p1_consumer_acquire(&bus, &current, &data, &len)) {
        fprintf(stderr, "FAIL: second payload acquisition\n");
        return 1;
    }
    pass = pass && (len == 8U) && (memcmp(data, "FAULT_02", 8U) == 0);
    bool second_released = p1_consumer_release(&bus, current);
    if (!p1_consumer_acquire(&bus, &current, &data, &len)) {
        fprintf(stderr, "FAIL: reused payload acquisition\n");
        return 1;
    }
    bool stale_released = p1_consumer_release(&bus, c_h1);
    pass = pass && republished && second_released && !stale_released
                && (current.slot_id == reused.slot_id)
                && (len == 1U) && (data[0] == 0xA5U)
                && (bus.slots[current.slot_id].state == SLOT_IN_USE)
                && (bus.invalid_state_errors == 2U);
    bool current_released = p1_consumer_release(&bus, current);

    /* An oversized publish leaves FILLING ownership available for abort. */
    slot_handle_t aborted;
    uint8_t *abort_buf = NULL;
    if (!p1_producer_alloc(&bus, &aborted, &abort_buf)) {
        fprintf(stderr, "FAIL: abort-case allocation\n");
        return 1;
    }
    bool oversized = p1_producer_publish(&bus, aborted, P1_PAYLOAD_BYTES + 1U);
    pass = pass && !oversized
                && (bus.slots[aborted.slot_id].state == SLOT_FILLING);
    bool abort_ok = p1_producer_abort(&bus, aborted);
    bool empty = !p1_consumer_acquire(&bus, &current, &data, &len);
    pass = pass && current_released && abort_ok && empty
                && (data == NULL) && (len == 0U) && (current.generation == 0U)
                && (bus.q_count == 0U);
    for (uint16_t i = 0U; i < P1_POOL_SLOTS; ++i) {
        pass = pass && (bus.slots[i].state == SLOT_FREE);
    }
    printf("C03 ownership checks: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
```

---

## Complete Scenario Walkthrough: Tracing Slot 0 And Slot 1 Through Burst Overload And Double-Release

Trace the initial burst and duplicate-release case in `main()` step by step:

1. **After `p1_bus_init(&bus)`:**
   * `Slot 0`: `state = SLOT_FREE`, `generation = 1`
   * `Slot 1`: `state = SLOT_FREE`, `generation = 1`
2. **Producer Event 1 (`"FAULT_01"`):**
   * `p1_producer_alloc()` claims **`Slot 0`** (`SLOT_FREE -> SLOT_FILLING`, `handle = {id=0, gen=1}`).
   * Producer writes `"FAULT_01"` into `Slot 0`'s `payload[]` outside any lock.
   * `p1_producer_publish()` moves `Slot 0` to **`SLOT_READY`** and pushes `{id=0, len=8, gen=1}` into `ready_q`.
3. **Consumer Starts Processing Event 1:**
   * `p1_consumer_acquire()` pops `{id=0, len=8, gen=1}`, moves `Slot 0` to **`SLOT_IN_USE`**, and gives the Consumer a read-only `const uint8_t *` to `Slot 0`'s `payload[]`.
4. **Two Fast Interrupt Bursts Arrive Before Consumer Finishes `Slot 0`:**
   * **Burst A (`"FAULT_02"`):** `p1_producer_alloc()` sees `Slot 0` is `SLOT_IN_USE` (skips it!) and claims **`Slot 1`** (`SLOT_FREE -> SLOT_FILLING`, `{id=1, gen=1}`), writes `"FAULT_02"`, and publishes `Slot 1` (`SLOT_READY`). **`Slot 0` (`"FAULT_01"`) is 100% untouched!**
   * **Burst B:** `p1_producer_alloc()` scans `Slot 0` (`SLOT_IN_USE`) and `Slot 1` (`SLOT_READY`). Zero `SLOT_FREE` slots exist -> increments **`pool_exhausted_drops` (`0 -> 1`)** and returns `false`. Allocation scans at most `P1_POOL_SLOTS` entries: `O(N)` in slot count, bounded to two checks here.
5. **Consumer Releases `Slot 0` (And Tests Buggy Double-Release):**
   * First `p1_consumer_release(c_h1)` (`{id=0, gen=1}`): matches `Slot 0` (`SLOT_IN_USE`, `gen == 1`) -> bumps **`Slot 0` `generation` from `1` to `2`** and sets `Slot 0` back to **`SLOT_FREE`**.
   * Second `p1_consumer_release(c_h1)` (`{id=0, gen=1}`): sees `Slot 0` is `SLOT_FREE` and `gen == 2 != 1` -> increments **`invalid_state_errors` (`0 -> 1`)** and returns `false`!

The remaining checks reuse Slot 0 at generation 2, consume Slot 1 first
(FIFO order), then reject the old generation-1 release while Slot 0 is
`IN_USE` again. The error counter becomes `2`. Finally, an oversized publish
leaves its slot `FILLING`; abort returns it to `FREE`. The expected ending
state is an empty queue with both slots free.

---

## Feynman Recap

A notification only rings a doorbell to say *"something happened"*—it does not protect the data bytes sitting in RAM. If a producer ISR and a consumer task share one global struct, the ISR can overwrite half the struct while the task is reading it, even if the struct is marked `volatile`. For small events (`<= 16–32 bytes`), use a **Copy-By-Value Queue** so the queue copies all bytes into its own storage and the producer and consumer never share a buffer. For larger blocks (`64+ bytes`), use a **Zero-Copy Static Slot Pool** where each buffer moves strictly through `FREE -> FILLING -> READY -> IN_USE -> FREE`: only one side owns a slot at any instant, only the small `(slot_id, generation)` handle goes into the queue, and bumping `generation` on release catches double-releases immediately.

---

## Short Spoken-English Interview Q&As

### Q1: Does `volatile` make a shared struct safe between an ISR producer and a task consumer?
* **Simple answer:** No. `volatile` prevents compiler register-caching, not torn reads or race conditions.
* **How it fails:** Reading a 12-byte struct takes multiple `ldr` instructions. An ISR can preempt the task after the first `ldr` and overwrite the remaining fields, leaving the task with half of the old packet and half of the new packet.
* **Why / Fix:** Use either a copy-by-value queue (critical-section copy) or a multi-slot pool with explicit `FREE -> FILLING -> READY -> IN_USE` single-owner states.

### Q2: When do you choose a Copy Queue (`xQueueSendFromISR` copying the struct) vs. a Zero-Copy Pool (`xQueueSendFromISR` sending a slot ID)?
* **Simple answer:** Copy by value for small commands/events (`<= 16–32 bytes`); use a zero-copy static slot pool for larger sensor/DMA blocks (`64+ bytes`).
* **How it works:** Queue copying contributes to the masked interval. Instruction count and duration depend on the compiler, alignment, memory and API overhead; measure against the interrupt-latency budget rather than using a universal byte threshold.
* **Why / Trade-off:** Copy queues have simpler lifetime rules (no `release()` call to forget). Pool metadata work is independent of payload size, but this allocator scans `N` slots; ownership and error recovery add complexity.

### Q3: What should an ISR producer do when the queue or buffer pool is full?
* **Simple answer:** Never block or spin-wait inside an ISR; apply an explicit bounded overload policy (typically **Drop-Newest** with an atomic drop counter) and unwind any allocated slot back to `FREE`.
* **How it works:** This allocator already counts pool exhaustion; do not increment the same counter again in its caller. After a later handoff failure, record that failure and abort only a slot still owned by the producer. A DMA-owned slot first needs confirmed cancellation/completion.
* **Why / Trade-off:** Preserves already-captured older fault evidence in the queue (`Drop-Newest`) and makes overload observable in telemetry instead of silently losing data or deadlocking the MCU.

## Corrections

- 2026-11-17: Approved source-only repair: initialized payload length,
  DMA completion/quiescence, observable handoff errors, failed-publish abort,
  pointer/non-aliasing and generation-wrap contracts, bounded linear pool scan,
  and checks with a failing exit status. The host model remains sequential and
  unexecuted; no universal copy-size or instruction-count threshold is claimed.
