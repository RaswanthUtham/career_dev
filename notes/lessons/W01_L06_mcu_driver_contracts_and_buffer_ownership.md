# W01-L06: MCU Driver Contracts And Buffer Ownership (The Notebook Handoff Story)

> **Lesson scope:** Topic 1 of the MCU/FreeRTOS track, 2026-10-27. Traces a
> concrete 2-byte asynchronous sensor transaction (`[validity, value] = [1, 42]`)
> across a `1 ms` cyclic driver service.

## 0. Know This First

Imagine **Project P1** running on your STM32F446RE. Every `1 ms`, a cyclic
service runs on the CPU. Your application asks a peripheral driver for one
sensor reading, and the sensor takes `0.6 ms` to send back two bytes:
`[validity = 1, value = 42]`.

Because `0.6 ms` is a long time for an `84 MHz` CPU (`50,400` clock cycles!),
the CPU starts the transfer and immediately returns to do other work while the
transport hardware receives the two bytes in the background.

| Term | What it does in our P1 story |
| --- | --- |
| **Driver Contract** | The rules connecting the Application, Driver, and Transport: legal requests, output format, deadlines, and who owns the buffer at each moment. |
| **Cyclic Service** | A function called every `1 ms` (spending at most `100 us` per call) that advances the driver state machine by one step. |
| **Asynchronous Transfer** | A transfer where `start()` kicks off the hardware and returns `ACCEPTED` immediately, while completion arrives later. |
| **Working Buffer (`B0`)** | A 2-byte array (`uint8_t B0[2]`) allocated inside the driver where the transport writes incoming bytes. |
| **Buffer Ownership** | Exclusive permission to touch `B0`. Only one actor (Driver or Transport) owns `B0` at any instant. |
| **Published Result** | A separate validated copy (`uint8_t value = 42`) handed to the Application after the transfer finishes and passes validation. |

```text
The Notebook Handoff Picture:
  1. Driver owns notebook B0 = [0, 0].
  2. At t = 0 ms, Driver hands B0 to Transport -> Transport owns B0 while writing [1, 42].
  3. At t = 0.6 ms, Transport finishes writing [1, 42].
  4. At t = 1.0 ms, Driver takes B0 back, checks B0[0] == 1, and copies 42 to Application.
```

---

## 1. Goal

Trace how a cyclic MCU driver starts an asynchronous 2-byte read, hands off
buffer `B0` to the transport, collects and validates the `[1, 42]` reply on a
later `1 ms` cycle, and protects `B0` when a transfer times out (`STOPPING`
state).

---

## 2. The Story: Why "Accepted" and "Finished" Are Two Separate Steps

### What Happens If the Application Reads `B0` Too Early?
Suppose `B0` starts at `[0, 0]`. At `t = 0 ms`, the Application asks for a
reading, the Driver starts the hardware transfer, and returns `ACCEPTED`.
If the Application immediately reads `B0[1]`, the hardware has barely started
shifting bits on the wire—so the Application reads `0` (or half-written bytes)
instead of `42`!

### How Our 3-Actor Story Solves This

```text
BEFORE (Reading Working Memory Early):
  App requests read -> Driver starts transfer (ACCEPTED) -> App reads B0 = [0, 0] (WRONG!)

AFTER (State Machine + Buffer Ownership Handoff):
  Scene 1 (t = 0.0 ms): App requests read -> Driver gives B0 to Transport (State = WAITING)
  Scene 2 (t = 0.6 ms): Transport writes [1, 42] into B0 in the background
  Scene 3 (t = 1.0 ms): Next 1 ms cyclic call collects B0, checks B0[0] == 1,
                        publishes copy `value = 42` to App (State = READY)
```

| Actor | What it does |
| --- | --- |
| **Application** | Requests a reading and reads the validated result copy (`42`) once published. |
| **Device Driver** | Tracks state (`READY`, `WAITING`, `STOPPING`, `FAILED`), checks the `3 ms` deadline, validates `[validity, value]`, and publishes the copy. |
| **Transport** | Moves raw bytes into `B0` while owning `B0`, and reports when the transfer completes or stops. |

---

## 3. Starting Snapshot: Numbers for Our Worked Story

| Parameter | Value in our trace | What it means |
| --- | --- | --- |
| **Working buffer `B0`** | `uint8_t B0[2] = {0, 0}` | 2-byte persistent buffer owned by the driver |
| **Sensor reply format** | `[validity, value]` | `B0[0] == 1` means valid; `B0[1]` is the reading (`0..255`) |
| **Incoming sensor data** | `[1, 42]` | Valid reading with value `42` |
| **Cyclic service period** | `1 ms` (`1000 us`) | Driver service function runs once every `1 ms` |
| **CPU budget per call** | `<= 100 us` (`10%` of `1 ms`) | Each `1 ms` call checks state and returns quickly |
| **Result deadline** | `3 ms` (`3` cyclic ticks) | Reply must be collected and validated before `3 ms` elapses |
| **Stop confirmation window** | `+2 ms` (until `t = 5 ms`) | Time allowed for hardware cancellation to finish before `STOP_FAILED` |

---

## 4. Before And After of Each Scene

### Scene 1: Request Accepted (`t = 0 ms`)
```text
BEFORE Scene 1:                          AFTER Scene 1 (t = 0 ms):
+------------------------------------+   +--------------------------------------------+
| Driver State = READY               |   | Driver State = WAITING (deadline = 3 ms)   |
| B0           = [0, 0]              |   | B0           = [0, 0] (in flight)          |
| B0 Owner     = Driver              |   | B0 Owner     = Transport (exclusive!)      |
| App Result   = none                |   | App Result   = PENDING                     |
+------------------------------------+   +--------------------------------------------+
*(If a second request arrives while State == WAITING, the driver immediately returns BUSY.)*
```

### Scene 2 & 3: Transfer Completes (`t = 0.6 ms`) & Cyclic Service Collects (`t = 1 ms`)
```text
AT t = 0.6 ms (Hardware Finishes):       AFTER Scene 3 Cyclic Service (t = 1.0 ms):
+------------------------------------+   +--------------------------------------------+
| Driver State = WAITING             |   | Driver State = READY                       |
| B0           = [1, 42]             |   | B0           = [1, 42]                     |
| B0 Owner     = Transport (holding) |   | B0 Owner     = Driver (regained!)          |
| App Result   = PENDING             |   | App Result   = {status = OK, value = 42}   |
+------------------------------------+   +--------------------------------------------+
```

### Scene 4: What If the Sensor Stalls? (`TIMEOUT` at `3 ms` -> `STOPPING` -> `FAILED`)
Why can't the driver reuse `B0` the instant the clock hits `3 ms`?
Because a slow or stuck DMA/transport engine might still write a late byte into
`B0` at `t = 3.2 ms`! To keep memory safe, `TIMEOUT` transitions the driver into
**`STOPPING`**, keeping `B0` locked with the transport until the hardware
confirms the transfer has stopped:

```text
AT t = 3.0 ms (Deadline Expired):        AT t = 4.0 ms (Transport Stop Confirmed):
+------------------------------------+   +--------------------------------------------+
| Driver State = STOPPING            |   | Driver State = FAILED (ready for recovery) |
| Action       = cancel_transfer()   |   | Action       = stop confirmed              |
| B0 Owner     = Transport (STILL!)  |   | B0 Owner     = Driver (safe to reuse now!) |
| App Result   = TIMEOUT             |   | App Result   = TIMEOUT                     |
+------------------------------------+   +--------------------------------------------+
*(If stop is still not confirmed by t = 5.0 ms, the driver reports STOP_FAILED and keeps B0 locked.)*
```

---

## 5. How the Driver State Machine Looks in C (Annotated Model)

```c
/* Conceptual C model of the P1 cyclic driver state and buffer ownership */
typedef enum {
	DRV_STATE_READY,     /* Idle; Driver owns B0 and can accept 1 request */
	DRV_STATE_WAITING,   /* Transfer in flight; Transport owns B0 */
	DRV_STATE_STOPPING,  /* Timed out at 3 ms; waiting for Transport to stop using B0 */
	DRV_STATE_FAILED     /* Fault recorded; explicit recovery needed */
} drv_state_t;

/* Called every 1 ms by the cyclic scheduler: */
void p1_driver_cyclic_1ms(drv_ctx_t *ctx, uint32_t now_ms)
{
	/* SCENE 3 & 4A: WHILE WAITING, CHECK THE 3 MS DEADLINE FIRST */
	if (ctx->state == DRV_STATE_WAITING) {
		bool timed_out = ((now_ms - ctx->start_ms) >= 3U);

		if (transport_collect(ctx->B0, &rx_count)) {
			/* Transport finished and handed B0 back to Driver! */
			if (timed_out) {
				/* Finished at or after 3 ms: too late for our < 3 ms deadline!
				 * B0 is already back with Driver, so go straight to FAILED. */
				ctx->published_status = DRV_ERR_TIMEOUT;
				ctx->state            = DRV_STATE_FAILED;
			} else if (rx_count == 2U && ctx->B0[0] == 1U) {
				/* Completed before 3 ms (e.g., at 1 ms) and valid! */
				ctx->published_value  = ctx->B0[1]; /* copy 42 to output */
				ctx->published_status = DRV_OK;
				ctx->state            = DRV_STATE_READY;
			} else {
				ctx->published_status = DRV_ERR_INVALID_REPLY;
				ctx->state            = DRV_STATE_FAILED;
			}
			return;
		}

		/* Transfer still in flight when 3 ms deadline expires: request cancel */
		if (timed_out) {
			ctx->published_status = DRV_ERR_TIMEOUT;
			transport_request_cancel();
			ctx->state = DRV_STATE_STOPPING; /* Transport STILL owns B0! */
		}
	}

	/* SCENE 4B: WHILE STOPPING, WAIT FOR TRANSPORT TO RELEASE B0 */
	else if (ctx->state == DRV_STATE_STOPPING) {
		if (transport_is_stopped()) {
			/* Hardware writer is stopped; Driver safely regains B0 */
			ctx->state = DRV_STATE_FAILED;
		} else if ((now_ms - ctx->start_ms) >= 5U) {
			/* Still not stopped after 3 + 2 = 5 ms: keep B0 locked! */
			ctx->published_status = DRV_ERR_STOP_FAILED;
			ctx->state            = DRV_STATE_FAILED;
		}
	}
}
```

---

## 6. Control Flow

### (a) One-Line Call Chain

```text
App requests read (t = 0 ms) -> Driver submits B0 to Transport (READY -> WAITING)
  -> [Transport receives [1, 42] asynchronously at t = 0.6 ms]
  -> Next 1 ms cyclic call (t = 1.0 ms) collects B0 -> validates count == 2 & B0[0] == 1
  -> copies 42 to App result (WAITING -> READY)
```

### (b) Step Table (Following the Successful `[1, 42]` Read)

| # | Time | Actor : Action | State Change | Concrete Data & `B0` Owner |
| --- | --- | --- | --- | --- |
| 1 | `0.0 ms` | App : request reading | `READY` | Request submitted; `B0 = [0, 0]` (Driver owns `B0`) |
| 2 | `0.0 ms` | Driver : start transfer | `READY -> WAITING` | `B0` handed to Transport (`B0` owner = **Transport**) |
| 3 | `0.6 ms` | Transport : receive bytes | Still `WAITING` | Hardware writes `B0 = [1, 42]` (`B0` owner = **Transport**) |
| 4 | `1.0 ms` | Driver : `1 ms` cyclic call | Collects `B0` | `rx_count = 2`, `B0 = [1, 42]` (`B0` owner = **Driver**) |
| 5 | `1.0 ms` | Driver : validate & publish | `WAITING -> READY` | Checks `B0[0] == 1`, copies `42` to App output |
| 6 | `1.0 ms` | App : read published value | `READY` | Reads separate copy `value = 42` |

### (c) Requirement Verification Matrix

| Requirement ID | Scenario to Test | Expected Driver Behavior |
| --- | --- | --- |
| **R1 (Valid reply)** | Transport completes at `0.6 ms` with `[1, 42]` (`count = 2`) | Publishes `{DRV_OK, 42}` at `1.0 ms`; returns to `READY` |
| **R2 (Single capacity)** | Second request arrives while `WAITING` or `STOPPING` | Returns `BUSY` immediately; active transfer and `B0` untouched |
| **R3 (Malformed reply)** | Transport returns `count = 1` or `[0, 42]` | Reports `DRV_ERR_INVALID_REPLY`; enters `FAILED` |
| **R4 (Timeout at 3 ms)** | No completion by `3.0 ms` | Reports `DRV_ERR_TIMEOUT`; calls cancel; enters `STOPPING` |
| **R5 (Safe stop at 4 ms)** | Transport confirms stop at `4.0 ms` | Regains `B0` ownership; enters `FAILED` for clean recovery |
| **R6 (Stuck hardware at 5 ms)** | Transport still active at `5.0 ms` | Reports `DRV_ERR_STOP_FAILED`; keeps `B0` locked |

---

## 7. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
Imagine handing a blank notebook (`B0`) to a courier (`Transport`) to fetch a
2-byte sensor reading (`[1, 42]`). The moment you hand over the notebook, your
hands are off it (`WAITING`). Every `1 ms`, you check if the courier has handed
the notebook back. At `1 ms`, the courier returns it with `[1, 42]`. You check
that the first number is `1` (valid), copy `42` onto a separate slip of paper
for the Application, and put the notebook back on your desk (`READY`). And if
the courier is late (`TIMEOUT` at `3 ms`), you tell the courier to stop (`STOPPING`)
and wait until they actually let go of the notebook before you ever write in it
again!

---

### Interview Q1: What is an MCU driver contract, and why must `ACCEPTED` be separate from `COMPLETED`?
* **Simple answer:** A driver contract defines the legal inputs, outputs, timing bounds, and buffer ownership between caller and driver.
* **Why they are separate:** In an asynchronous driver, `ACCEPTED` means the hardware transfer has been started and the working buffer (`B0`) now belongs to the transport. `COMPLETED` happens later when the transfer finishes, ownership of `B0` returns to the driver, and the payload passes validation.

### Interview Q2: Why must a receive buffer (`B0`) stay allocated and untouched after the start function returns?
* **Simple answer:** Because the transport (ISR or DMA controller) writes into `B0` asynchronously while the CPU is running other code.
* **What happens if violated:** If `B0` were a local stack variable that went out of scope—or if the application read/reused `B0` while in flight—the background transfer would corrupt stack memory or mix old and new bytes.

### Interview Q3: When a peripheral transfer hits its timeout (`3 ms`), why can't the driver immediately reuse the buffer for the next request?
* **Simple answer:** A software timeout means **"time is up"**, not **"the hardware has stopped writing to memory."**
* **How to handle it safely:** Transition to a `STOPPING` state, issue a hardware abort/cancel, and wait for confirmation that the DMA/ISR channel is quiescent before reclaiming the buffer.

## 8. Corrections

- 2026-10-26: Initial publication for the MCU track.
- 2026-10-27 (revision 2): Rewrote in direct story mode with Before/After scene boxes and an annotated C state-machine model.
