# W01-L02: System Calls And Waiting For Buffered Data

> **Source-reviewed:** Linux checkout `f0100363d`, 2026-10-23, tutor inspection.
> **Not runtime-verified:** the numerical trace is a worked model of upstream
> Linux VFS/IIO code, not output from a running Project A build. No
> implementation files or hardware runs are required for this conceptual lesson.

Previous: [W01-L01: Process, Kernel And Device Boundaries](W01_L01_process_kernel_device_boundaries.md).

## 0. Know This First

In L01, the kernel buffer already had **3 scans (`12 bytes`)** when your
`recorder` app asked for **2 scans (`8 bytes`)**, so `read()` returned
immediately.

Now ask the next natural question: **What happens when your app calls
`read(3, user_buf, 8)` for 2 scans, but only 1 scan (`4 bytes`) has arrived from
the STM32 so far?**

| Term | Plain meaning with our Project A example |
| --- | --- |
| **File descriptor (`fd`)** | A small integer ticket (such as `fd = 3`) that your process gets from `open("/dev/iio:device0", ...)`. Passing `3` to `read(3, ...)` tells the kernel which open device you mean. |
| **`struct file`** | The kernel's internal object behind `fd = 3`. It stores the open flags (blocking vs. `O_NONBLOCK`) and the function-pointer table (`f_op`) for this device. |
| **`kfifo` (Data Queue)** | The kernel FIFO ring buffer that holds the actual **measurement bytes** (`[Scan A]`). Oldest scan in is the first scan read out. |
| **Wait Queue (`rb->pollq`)** | A completely separate list that holds **sleeping threads** waiting for new scans to arrive. |
| **Thread States (`Running` / `Sleeping` / `Runnable`)** | **Running:** executing on a CPU core right now. **Sleeping (`TASK_INTERRUPTIBLE`):** paused inside the kernel using `0%` CPU while waiting for an event. **Runnable:** woken up and ready to run as soon as the scheduler picks it. |
| **Wakeup** | A bell rung by the driver (`wake_up_interruptible_poll`) when it pushes a new scan into the buffer. It moves sleeping readers from **Sleeping -> Runnable**. |
| **`O_NONBLOCK` vs. Blocking** | **Blocking (default):** sleep if `avail < to_wait`. **Non-blocking (`O_NONBLOCK`):** never sleep for data; if none is ready right now, return `-1` immediately with `errno = EAGAIN`. |

```text
1. How fd = 3 finds the IIO driver inside the kernel:
   User process table: fd[3] ---> struct file ---> f_op->read() ---> iio_buffer_read()

2. Do NOT mix up the two queues inside IIO:
   +-------------------------------------------------------+
   | DATA QUEUE (kfifo): holds measurement scans           |
   |   [ Scan A: (100, 200) ]   (avail = 1 scan = 4 bytes) |
   +-------------------------------------------------------+
   +-------------------------------------------------------+
   | WAIT QUEUE (rb->pollq): holds sleeping reader threads |
   |   [ wait entry for our recorder thread ]              |
   +-------------------------------------------------------+
```

## 1. Goal

* **Before:** "When `read()` waits for data, the CPU just pauses there, or waking up automatically hands the bytes to my program."
* **After:** Trace how `fd = 3` reaches IIO, why the kernel uses **two queues** (one for data, one for sleeping threads), how the driver wakes the reader when **Scan B** arrives, and why the reader **always rechecks `avail >= to_wait` in a `do-while` loop** before copying bytes to user space.

## 2. Why Not Just Spin in a `while` Loop Waiting for Scan B?

Suppose only **Scan A (`1 scan`)** is in the buffer, and your app needs **2 scans (`8 bytes`)**.

* **Bad Idea 1 — Busy-wait spinning:** If the kernel or your app spun in `while (avail < 2) {}`, that thread would burn CPU execution time doing nothing (increasing power use and potentially starving lower-priority tasks depending on scheduling and preemption).
* **Bad Idea 2 — Naive "check then sleep":** What if your thread checks `avail == 1`, decides to go to sleep, and **at that exact microsecond** the STM32 interrupt fires, pushes Scan B, and rings the wakeup bell *before* your thread finishes going to sleep? Your thread would miss the bell and sleep forever!

**The Linux Solution (Register -> Check -> Sleep -> Wake -> Recheck):**
1. Put your thread's `wait` ticket onto `rb->pollq` **first**.
2. In a `do-while` loop, check `iio_buffer_ready()` (`avail >= to_wait`).
3. If `false` (`1 < 2`), sleep with `wait_woken()` (`0%` CPU; the kernel scheduler runs other programs). If a wakeup arrived right between step 2 and step 3, `wait_woken()` sees the recorded wakeup flag (`WQ_FLAG_WOKEN`) and does not get stuck!
4. When the STM32 driver pushes **Scan B**, it rings `rb->pollq`. Your thread becomes **Runnable**, gets scheduled onto a CPU, loops back to the top of `do-while`, **rechecks** `avail >= 2` (now `2 >= 2` is `true`!), and copies Scan A + Scan B (`8 bytes`) to `user_buf`.

## 3. Starting Snapshot (And How Producer Wakeups Meet `watermark`)

To follow one concrete path through the code, here are our starting values:

| Variable | Starting value | Plain meaning |
| --- | --- | --- |
| `fd` | `3` | Open file descriptor for `/dev/iio:device0` (blocking mode) |
| `n` | `8` bytes | Recorder calls `read(3, user_buf, 8)` |
| `datum_size` | `4` bytes | 1 scan = `4` bytes (`ch0` + `ch1`) |
| `watermark` | `2` scans | `to_wait = min(8 / 4, 2) = 2` scans needed before `read()` can finish |
| `avail` (at call start) | **`1` scan (`4` bytes)** | Only **Scan A** is in the buffer (`1 < 2` -> must wait!) |
| `avail` (after STM32 push) | **`2` scans (`8` bytes)** | Driver pushes **Scan B** -> now `2 >= 2` is satisfied! |

### Subtle Detail: When Does the Driver Ring the Wakeup Bell?
In upstream IIO (`iio_push_to_buffer()`), the producer rings `rb->pollq` **every time it pushes a scan**—it does not wait for the bucket to hit `watermark` before ringing the bell.
Instead, **the reader's `do-while` loop enforces the watermark**: if the reader wakes up when `avail` is still `< to_wait`, `iio_buffer_ready()` returns `false` and the reader goes right back to sleep.

| Scans in buffer (`avail`) | Does producer notify `rb->pollq`? | Reader's check (`avail >= 2`) | What the reader does |
| --- | --- | --- | --- |
| `1` scan | Yes | `1 >= 2` is **False** | Stays in loop and sleeps in `wait_woken()` |
| `2` scans | Yes | `2 >= 2` is **True** | Calls `kfifo_to_user()`, copies 8 bytes, returns `8` |

## 4. How the Kernel Code Actually Does This (Short Annotated Excerpt)

Because we are **not implementing a file today**, we show **only the essential lines** from our local Linux tree (`f0100363d`), with **what is happening and how it works explained right inside the comments** using our numbers (`fd = 3`, `n = 8`, `to_wait = 2`, `avail = 1 -> 2`).

### Step A: Turning `fd = 3` into the IIO Read Function (`fs/read_write.c`)

Source links: [fs/read_write.c:705](../../../linux/fs/read_write.c#L705) and [fs/read_write.c:554](../../../linux/fs/read_write.c#L554)

```c
/* 1. System-call entry lands in the read syscall and calls ksys_read():
 *    fd = 3, buf = user_buf, count = 8 bytes.
 *    Inside ksys_read(), the kernel looks up fd = 3 in this process's open
 *    file table to resolve `struct file *file`, then passes it to vfs_read(). */
return ksys_read(fd, buf, count);

/* 2. Inside vfs_read():
 *    Calls that open file's .read function pointer (which IIO set to its
 *    buffer read handler in Lesson 1). Same thread, running in kernel mode! */
ret = file->f_op->read(file, buf, count, pos);
```

### Step B: Reader Registers on Wait Queue, Sleeps (`avail = 1`), Then Copies (`avail = 2`)

Source links: [drivers/iio/industrialio-buffer.c:142](../../../linux/drivers/iio/industrialio-buffer.c#L142) and [drivers/iio/buffer/kfifo_buf.c:101](../../../linux/drivers/iio/buffer/kfifo_buf.c#L101)

```c
/* Inside iio_buffer_read(filp, buf = user_buf, n = 8, f_ps): */

/* 1. CREATE A WAIT TICKET FOR THIS THREAD:
 *    `wait` represents our sleeping recorder thread; woken_wake_function()
 *    is the callback that marks it woken when the driver rings rb->pollq. */
DEFINE_WAIT_FUNC(wait, woken_wake_function);

/* 2. COMPUTE WAIT TARGET (IN SCANS):
 *    n = 8 bytes, datum_size = 4 bytes/scan -> 8 / 4 = 2 scans requested.
 *    watermark = 2 scans -> to_wait = min(2, 2) = 2 scans. */
to_wait = min_t(size_t, n / datum_size, rb->watermark); /* to_wait = 2 */

/* 3. JOIN THE WAIT QUEUE BEFORE CHECKING DATA:
 *    Registering BEFORE checking prevents the "missed wakeup race" if a new
 *    scan arrives right as we decide to sleep. */
add_wait_queue(&rb->pollq, &wait);

do {
	/* 4. CHECK IF ENOUGH SCANS ARE IN THE BUFFER (avail >= to_wait):
	 *
	 *    - PASS 1 (before Scan B arrives):
	 *      avail = 1, to_wait = 2 -> iio_buffer_ready() is FALSE!
	 *      If no Ctrl+C signal is pending, wait_woken() puts this thread
	 *      to sleep (TASK_INTERRUPTIBLE, 0% CPU).
	 *
	 *    - PASS 2 (after Step C below pushes Scan B and wakes us):
	 *      Thread resumes after wait_woken(), hits `continue`, loops back
	 *      here, and RECHECKS iio_buffer_ready(). Now avail = 2 >= 2 is
	 *      TRUE, so we skip this `if` block and copy the data! */
	if (!iio_buffer_ready(indio_dev, rb, to_wait, n / datum_size)) {
		if (signal_pending(current)) {
			ret = -ERESTARTSYS; /* interrupted by a signal */
			break;
		}
		wait_woken(&wait, TASK_INTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
		continue; /* woke up! Jump back to top of do-while to recheck */
	}

	/* 5. COPY SCANS FROM KERNEL KFIFO TO USER SPACE (iio_read_kfifo):
	 *    Inside rb->access->read() (iio_read_kfifo in buffer/kfifo_buf.c:116):
	 *    - kfifo_to_user(&kf->kf, buf, 8, &copied) copies Scan A + Scan B into
	 *      user_buf, sets copied = 8, and returns status 0 (meaning no -EFAULT).
	 *    - iio_read_kfifo() then returns `copied` (8), so outer `ret` becomes 8. */
	ret = rb->access->read(rb, n, buf); /* ret = 8 */
} while (ret == 0);

/* 6. UNREGISTER FROM WAIT QUEUE AND RETURN 8 BYTES TO USER SPACE: */
remove_wait_queue(&rb->pollq, &wait);
return ret; /* returns 8 to recorder's read() call */
```

### Step C: Meanwhile, Producer Pushes Scan B and Wakes `rb->pollq`

Source link: [drivers/iio/industrialio-buffer.c:2386](../../../linux/drivers/iio/industrialio-buffer.c#L2386)

```c
/* Inside iio_push_to_buffer(buffer, dataout = Scan B):
 * Runs asynchronously when the driver receives Scan B (101, 201) from STM32. */

/* 1. STORE SCAN B INTO THE DATA QUEUE (kfifo):
 *    Queue changes from [Scan A] (avail = 1) to [Scan A, Scan B] (avail = 2). */
ret = buffer->access->store_to(buffer, dataout);
if (ret)
	return ret;

/* 2. RING THE WAIT QUEUE BELL:
 *    Wakes any thread sleeping on buffer->pollq (moving our recorder thread
 *    from Sleeping -> Runnable so Step B Pass 2 can run).
 *    Note: this does NOT copy bytes to user space—the reader does that in Step B! */
wake_up_interruptible_poll(&buffer->pollq, EPOLLIN | EPOLLRDNORM);
```

## 5. Control Flow

### (a) One-Line Call Chain

```text
/* 1. Reader enters kernel and goes to sleep (avail = 1 < to_wait = 2) */
recorder read(3, user_buf, 8) -> [syscall] -> ksys_read / vfs_read (fs/read_write.c) -> iio_buffer_read (industrialio-buffer.c) -> wait_woken (kernel/sched/wait.c) [SLEEPS]

/* 2. Producer pushes Scan B asynchronously and wakes the wait queue */
driver receives Scan B -> iio_push_to_buffer (industrialio-buffer.c) -> store_to kfifo (avail = 2) -> wake_up_interruptible_poll(&rb->pollq)

/* 3. Reader wakes up, rechecks, copies 8 bytes, and returns */
wait_woken resumes -> iio_buffer_read rechecks iio_buffer_ready (avail = 2 >= 2) -> iio_read_kfifo / kfifo_to_user (buffer/kfifo_buf.c) -> return 8
```

### (b) Step Table (Following `avail = 1 -> sleep -> avail = 2 -> return 8`)

| # | Where (`file : function`) | Goal / state change | Concrete data in -> out | Context / mode |
| --- | --- | --- | --- | --- |
| 1 | `recorder` (user app) | Call blocking `read()` for 2 scans | `fd = 3, user_buf, n = 8` -> syscall | User thread (U-mode) |
| 2 | `fs/read_write.c : ksys_read / vfs_read` | Look up `fd = 3` and call `.read` | `fd = 3` -> `file->f_op->read` | Same thread in kernel |
| 3 | `industrialio-buffer.c : iio_buffer_read` | Join `rb->pollq` and check readiness | `avail = 1 < to_wait = 2` -> `false` | Kernel task context |
| 4 | `kernel/sched/wait.c : wait_woken` | Put reader thread to sleep (`0%` CPU) | State -> `TASK_INTERRUPTIBLE` (sleeping) | Kernel; CPU runs other work |
| 5 | `industrialio-buffer.c : iio_push_to_buffer` | Store Scan B in `kfifo` and ring `rb->pollq` | `[Scan A]` -> `[Scan A, Scan B]` (`avail = 2`) | Asynchronous producer context |
| 6 | Scheduler + `iio_buffer_read` loop | Resume reader and **recheck** readiness | `avail = 2 >= to_wait = 2` -> `true` | Reader thread back on CPU |
| 7 | `buffer/kfifo_buf.c : iio_read_kfifo` | Copy Scan A + Scan B to `user_buf` | `kfifo_to_user` copies `8` bytes | Kernel task context |
| 8 | `industrialio-buffer.c : iio_buffer_read` | Leave `rb->pollq` and return byte count | `ret = 8` -> returned to `recorder` | Kernel -> User return |

### (c) Timeline: Sleeping, Waking, Rechecking

```text
Recorder Thread (Reader)            Driver / Producer               Kernel Scheduler
------------------------            -----------------               ----------------
call read(3, user_buf, 8)
add_wait_queue(&rb->pollq, &wait)
check: avail (1) < to_wait (2)
wait_woken() -> SLEEP  -------------------------------------------> run other tasks
                                    receive Scan B from STM32
                                    store_to kfifo (avail = 2)
                                    wake_up(&rb->pollq) ----------> mark reader RUNNABLE
resume after wait_woken()  <--------------------------------------- pick reader to run
loop & RECHECK: avail (2) >= 2!
kfifo_to_user() copies Scan A, B
remove_wait_queue(&rb->pollq)
return 8 to user space
```

## 6. Return Values, Errors And Limitations

Your user-space C code (`recorder`) must check what `read(fd, user_buf, 8)` returns:

| `read()` return value | When it happens | What your C code should do |
| --- | --- | --- |
| **`8` (full count)** | 2 scans (`8 bytes`) copied into `user_buf` | Process both 4-byte scans |
| **`4` (short read, `0 < ret < 8`)** | Fewer bytes were copied (e.g., buffer drained/disabled after 1 scan) | Process **only `ret` bytes** (`ret / 4` scans); never assume all 8 bytes were filled! |
| **`-1` with `errno == EAGAIN`** | File was opened with `O_NONBLOCK` (`to_wait = 0`) and `0` scans were available | Not a fatal error; wait using `poll()`/`select()` before calling `read()` again |
| **`-1` with `errno == EINTR` (or automatic restart)** | A signal interrupted the sleeping thread (`-ERESTARTSYS` inside the kernel). If the signal handler was installed with `SA_RESTART`, the syscall restarts; if installed without `SA_RESTART`, `read()` returns `-1` (`EINTR`). *(Default `SIGINT` without a handler terminates the process.)* | If your app installed a `SIGINT` handler that sets a stop flag, check that flag and exit cleanly; otherwise retry if still running |
| **`-1` with `errno == ENODEV` / `EBADF`** | Device was unregistered (`!indio_dev->info`) or `fd` was invalid | Log device loss and exit cleanly; do not spin in an infinite retry loop |

* **Evidence status:** Source-reviewed against Linux `f0100363d`. No code is built or run today.

## 7. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
When your program asks for `2` scans (`8 bytes`) and only `1` scan is in the kernel's data bucket (`kfifo`), the kernel does not waste CPU spinning in a loop. Instead, your thread writes its name on a **waiting list** (`rb->pollq`) and goes to sleep (`0%` CPU). When the driver drops the second scan into the data bucket, it rings the waiting-list bell. Your thread wakes up, **double-checks the bucket** to make sure at least `2` scans are really there (`avail >= to_wait`), copies both scans into your program's memory (`kfifo_to_user`), erases its name from the waiting list, and returns `8`.

---

### Interview Q1: Does calling a system call like `read()` always cause a context switch to another process?
* **Simple answer:** **No.** Entering a system call only switches the **CPU privilege mode** (from User mode to Kernel mode) on the **exact same thread**.
* **When NO context switch is needed:** If the data (`>= 2` scans) is already in the kernel buffer, your thread can copy the bytes and return to user space without needing to block.
* **When a context switch DOES happen:** If not enough data is ready (`avail < to_wait`) and your blocking `read()` calls `wait_woken()` to **sleep**, the scheduler switches the CPU to another runnable task (and a normal task can also be switched if **preempted** by the scheduler).

### Interview Q2: Why must a kernel thread recheck its condition in a `while` loop after waking up from a wait queue?
* **Simple answer:** Because **waking up is just a hint ("something changed"), not a guarantee that your data is ready and reserved for you**.
* **What could happen between wakeup and running:**
  1. Only `1` scan arrived, but your watermark (`to_wait`) requires `2` scans.
  2. Another thread read the data before the scheduler picked your thread to run.
  3. The device was unplugged (`!indio_dev->info`) or a signal (`Ctrl+C`) woke your thread early.
* **Rule:** Always check `iio_buffer_ready()` inside a loop (`do { ... } while`) after waking up before copying data.

### Interview Q3: What is the "lost wakeup" (check-then-sleep) race, and how does Linux prevent it?
* **The bug (if written naively):** Suppose your thread checks `avail == 1` (not enough data) and decides to go to sleep—but **one microsecond before it actually sleeps**, the driver interrupt fires, pushes Scan B, and sends a wakeup. Because your thread wasn't asleep yet, it misses the wakeup and sleeps forever!
* **How Linux fixes it:**
  1. **Join the wait queue (`add_wait_queue`) BEFORE checking the data (`iio_buffer_ready`).**
  2. If the driver sends a wakeup after the check but before `wait_woken()` sleeps, the wakeup callback sets a "woken" flag (`WQ_FLAG_WOKEN`) on your wait ticket.
  3. When `wait_woken()` runs, it sees that flag already set and **skips going to sleep**.

### Interview Q4: If I set the IIO watermark to `2` scans, does the driver wait for `2` scans before notifying the wait queue?
* **Simple answer:** **No.** In the software IIO buffer path (`iio_push_to_buffer()`), the producer rings the wait queue (`wake_up_interruptible_poll`) **after every scan it pushes**.
* **Who enforces the watermark:** The **reader loop** in `iio_buffer_read()` enforces the watermark: when woken after Scan 1, it checks `avail (1) >= to_wait (2)`, sees `false`, and goes back to sleep until Scan 2 arrives.
* **Key takeaway:** Watermark controls **when `read()` is allowed to finish and copy a batch to user space**, not how often the producer calls `wake_up()`.

## 8. Corrections

- 2026-10-23: First publication of W01-L02.
- 2026-10-23 (revision 2): Rewrote in concise Feynman style matching L01. Consolidated scattered code fragments into Section 4 (`Step A`, `Step B`, `Step C`) with inline C comments explaining what happens and how (`fd = 3`, `n = 8`, `avail = 1 -> 2`, `to_wait = 2`), and clarified the two-queue (`kfifo` vs. `rb->pollq`) model.
- 2026-10-23 (revision 3): Rewrote all 4 interview questions and answers in Section 7 into simple, spoken-English bullets (`Simple answer`, `Why / What happens`, `Rule / Key takeaway`).
- 2026-10-23 (revision 4): Applied technical review fixes: moved `fd = 3` lookup comment to `ksys_read()`, refined busy-wait and preemption wording, and clarified `-ERESTARTSYS` restart vs. `EINTR` / signal-handler behavior.
- 2026-10-25 (revision 5): Separated `kfifo_to_user()`'s return status (`0` on success) from `copied = 8` and `iio_read_kfifo()`'s return value (`8`) in Step B item 5 (`LR5`).

