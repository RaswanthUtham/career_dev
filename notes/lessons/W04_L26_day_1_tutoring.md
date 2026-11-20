# Day 1 tutoring: three bytes arrive while the driver task is asleep

**Goal:** Follow three diagnostic bytes from the device to a sleeping parser. By the end, distinguish four things that are easy to call “the event”: the bytes in memory, a task becoming ready, a timer expiring, and useful work actually finishing.

**Reading route (about 90 minutes, flexible):** Spend 15 minutes on storage, 30 tracing the bytes, 20 on timers and flags, 15 on diagnostics and sleep, and 10 on the spoken answers. Use [the detailed W04-L26 note](W04_L26_freertos_heaps_timers_event_groups_stream_buffers_and_diagnostics.md) for kernel diagrams. This is a paper walkthrough against FreeRTOS Kernel V11.1.0, not a board measurement or a quiz.

## Know this first: two memories and two owners

In the P1 reference design a peripheral receives bytes. Direct memory access (DMA) means the DMA controller can write them into a designated memory area without a processor instruction for every byte. An interrupt service routine (ISR) is the processor code that handles a completion event. A real-time operating system (RTOS) **task** is code the scheduler can put to sleep and later run again.

```text
device -> DMA fills staging area -> completion ISR copies stable bytes
                                               |
                                      FreeRTOS stream buffer
                                               |
                                    parser task -> valid record
```

The staging area belongs to the transfer until the hardware has finished with it. The stream buffer is a **separate copy**. For this example assume one completed DMA block stays stable until copied, one ISR writes the stream, and one task reads it. A circular DMA setup that can overwrite the block mid-copy needs a different ownership argument.

## Scene 1: reserve storage before enabling the producer

Suppose the parser needs a stream holding **four unread bytes**. FreeRTOS V11.1.0's static stream API takes the size of the **backing array** and leaves one slot unused to distinguish full from empty. Supply five bytes for four usable slots. Set the trigger to one so the first byte can wake a waiting reader.

```c
static StaticStreamBuffer_t control;  /* Kernel bookkeeping; outlives the ISR. */
static uint8_t bytes[5];              /* Five physical slots, four usable. */
/* In startup task context, with static allocation and streams enabled: */
StreamBufferHandle_t rx = xStreamBufferCreateStatic(sizeof bytes, 1, bytes, &control);
```

`sizeof bytes` is the array size in bytes here; `&control` points to the caller's control storage. Publish the valid handle to the task and ISR **before** enabling reception. Keep both objects alive until all users and the producer are quiescent.

**Why not `heap_4`?** It is a reasonable alternative, but a different contract. With static creation, *you supply the storage*. With dynamic creation, *the allocator must find it*, even at startup. `heap_1` never reclaims blocks; `heap_2` frees but does not merge neighbours; `heap_3` wraps the C library allocator; `heap_4` merges adjacent free blocks; `heap_5` also supports multiple defined regions. Two free holes separated by a live allocation cannot satisfy a single request larger than either hole. None of these choices establishes that a particular memory bank is DMA-accessible.

## Scene 2: follow the bytes, not just the wake-up

The receiver calls `xStreamBufferReceive(rx, out, 2, waitTicks)` with space for two bytes and a finite wait. An empty stream lets the task block. In an RTOS-callable completion interrupt the producer uses this pattern (C excerpt; not a device handler):

```c
BaseType_t woken = pdFALSE;
size_t sent = xStreamBufferSendFromISR(rx, stable_block, count, &woken);
/* sent is bytes copied. Account for count - sent as lost under this policy. */
portYIELD_FROM_ISR(woken);  /* Request a switch if a higher-priority task woke. */
```

`&woken` lets the kernel update the switch request. It says nothing about whether the record passed validation. On the reader side, the return value is the number of bytes actually received; a finite wait may expire with zero. Parse only the returned bytes.

| Step | Where / context | Before → after (`xHead` = next write, `xTail` = next read) |
|---|---|---|
| 1 | Parser / task | Empty: head 0, tail 0. Reader blocks; no cursor moves. |
| 2 | Send `A B C` / ISR, **before** reader runs | Write slots 0,1,2. Head 0 → 3, tail 0. Reader becomes ready; no byte has yet been parsed. |
| 3 | Receive two / task | Copy `A B` from slots 0,1 into private `out`. Tail 0 → 2. `C` is still in slot 2. |
| 4 | Send `D E F G` / ISR, **during** processing of private `A B` | Three free slots: write `D E F` to 3,4,0. Head 3 → 1, tail 2. Return 3; `G` is lost under the chosen drop policy. No unread byte was overwritten. |
| 5 | Receive twice / task | Read `C D` (tail 2 → 4), then `E F` (tail 4 → 1). Head = tail = 1: empty again. |
| 6 | Send `H` **after** draining | Write slot **1**, not slot 0. If the reader has blocked again, wake it; if it hasn't, its next receive finds `H`. |

This stream preserves byte order, **not packet boundaries**. A parser that loses `G` needs a protocol rule for finding the next valid frame. If whole messages must be kept or rejected together, a message buffer stores a length plus payload. Budget the configured length field as well as payload bytes. If the receiver's destination is too small, the next whole message remains queued. Neither buffer is a general multiple-writer queue.

## Scene 3: a deadline and a readiness flag solve different problems

A software timer can remind a worker to check a transaction. The timer service task processes timer commands and runs callbacks. A successful `xTimerStart(timer, 0)` means **command accepted**, not “callback has run.” A full command queue can reject a zero-wait start; the caller must handle failure. Have the callback signal the worker and return promptly. If it waits for slow bus I/O, other callbacks and deferred commands sharing that task wait behind it. The worker should check the actual transaction deadline and generation when it runs.

An event group answers a different question: **which conditions are true?** Suppose bit 0 means “parser initialized” and bit 1 means “transport initialized.” A task waiting for **all** of mask `0x03` sees `00 → 01` (still waiting), then `01 → 11` (condition met). Waiting for **any** would be satisfied by `01`. Use `xEventGroupSync` when participating tasks must each set their bit and wait at the same barrier. Setting an already-set bit does not count a second arrival.

```text
Time      Interrupt                     Timer service task          Waiting worker
t0        post SetBitsFromISR request   not running                 blocked on ALL
t1        returns pdPASS                request queued              still blocked
t2                                      sets bit; tests waiters     ready if mask met
t3                                                                  runs when scheduled
```

`xEventGroupSetBitsFromISR` defers the set operation through the timer command queue. If the post fails, the bit was not scheduled to be set: record the failure and use a defined recovery policy. For one worker needing every completion, a counted handoff or a queue may fit better than level flags.

## Scene 4: observe the parser, then let the processor sleep

| Observation | Worked interpretation | What to check next |
|---|---|---|
| Stack high-water mark is 24 stack words | On a port with four-byte stack words, 24 × 4 = 96 bytes of untouched fill pattern. This is historical evidence under the exercised paths, not a bound for untested paths. | Consider the deepest error path. Method 1 checks the saved stack pointer against a boundary at a kernel check point; method 2 also checks boundary fill bytes. A memory protection unit (MPU) guard instead faults on a prohibited access when correctly configured. |
| Parser accumulated 200 counter counts over a 1,000-count observation window | `200 / 1000 = 20%` of that accounting interval, assuming a common counter, valid deltas and no ambiguous wrap. Runtime statistics describe processor use, not end-to-end response time. | Compare arrival and completion timestamps. A task may miss a deadline while using little processor time because it waited on another task. |
| No task is ready except idle; the next timeout is 10 ticks away | With tickless idle enabled and the port's sleep conditions met, the kernel can suppress periodic ticks and account for elapsed time on wake. An interrupt can wake the processor earlier. | Check timer availability in the chosen sleep mode and wake latency. Ten predicted idle ticks are not ten measured ticks asleep. |

**Workplace bridge:** In an AURIX cyclic driver, distinguish “my cyclic function ran” from “transaction N completed, its response was valid, and the consumer received it.” The same distinction tells you what P1 should supervise. The scheduling mechanism and interrupt rules must come from the actual platform, not the analogy.

## Spoken interview answers

- **“Is allocation at startup static?”** No. Startup says *when*. Static creation says *who supplied the storage*. A startup `heap_4` request can still fail.
- **“The task woke, so did we receive the packet?”** A wake makes the task eligible to run. Check the receive length, reconstruct the frame and validate it before reporting progress.
- **“Can a timer callback perform the retry?”** It can signal that a check is due. I let the driver task own the retry so a slow bus does not stall unrelated timers, and so only one context decides whether the original transfer still owns the buffer.
- **“How would you distinguish overload from corruption?”** First correlate the received byte count, short-send counter and frame sequence. If loss starts exactly at a short send, test the overload policy. If all bytes were accepted, compare the stable input block to the decoded record before changing queue depth.

**Recap:** The allocator provides space. The ISR copies bytes. The scheduler lets a ready parser run. Only the parser can say a valid record was produced. Those are four different promises; design a separate check for each.

### Official lookup

- [Kernel book contents](https://github.com/FreeRTOS/FreeRTOS-Kernel-Book/blob/main/toc.md): look up heap management, software timers and event groups by name.
- [V11.1.0 stream implementation](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V11.1.0/stream_buffer.c): `xStreamBufferGenericCreateStatic`, `xHead`, `xTail`, `xStreamBufferSendFromISR`; [API contract](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V11.1.0/include/stream_buffer.h): single writer and single reader.
- [Heap implementation](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V11.1.0/portable/MemMang/heap_4.c), [timer service](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V11.1.0/timers.c), [event groups](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V11.1.0/event_groups.c), [stack checks](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V11.1.0/include/stack_macros.h) and [scheduler](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/V11.1.0/tasks.c). These are reference sources, not a claim about a locally built kernel.
