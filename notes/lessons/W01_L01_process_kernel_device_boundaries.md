# W01-L01: Linux Process, Kernel And Device Boundaries

> **Source-reviewed:** Linux checkout `f0100363d`, 2026-10-23, tutor inspection.
> **Not runtime-verified:** the numerical example is a worked model, not output
> from Project A. No implementation files, guest boots or hardware tests are
> required for this conceptual lesson. Learner understanding remains unassessed.

## 0. Know This First

Imagine our **Project A Fault Recorder**. An STM32 chip measures two voltages
every `100 us` (`10,000` times per second). On the BeagleBone Black (BBB), your
C program (`recorder`) wants to read those numbers and save them to a file.

Before looking at kernel code, picture each word using that real setup:

| Term | Plain meaning with our Project A example |
| --- | --- |
| **Program vs. Process** | A **program** is the compiled file `/usr/bin/recorder` on disk. A **process** is that program running in memory, with its own private virtual address space and open file table. |
| **Thread** | The single flow of CPU instructions executing inside your process. When your code calls `read()`, your thread steps across the boundary into the kernel. |
| **User space vs. Kernel space** | **User space** is unprivileged: your app cannot touch hardware registers or other processes' RAM. **Kernel space** is privileged OS code that controls hardware and memory. |
| **System call (`syscall`)** | The controlled doorway (such as `read()`) where a user-space thread asks the kernel to perform a privileged job on its behalf. |
| **Driver** | Kernel code that knows how to talk to a specific piece of hardware or protocol. |
| **Channel** | One measurement signal. In Project A, `ch0` (Voltage A, `2 bytes`) and `ch1` (Voltage B, `2 bytes`) are two channels. |
| **Scan (or `datum`)** | One buffered record of **all active channels for one trigger/tick**. Two `2-byte` channels = **`4 bytes` per scan** (`bytes_per_datum = 4`). *(Electrical simultaneity depends on the ADC hardware.)* |
| **Kernel Buffer** | A waiting room in kernel RAM (`struct iio_buffer`) where the driver drops incoming scans so they are not lost while your user-space app is temporarily busy (as long as the buffer does not overflow). |
| **Blocking `read()`** | If your app calls `read()` and the buffer does not have enough scans yet, the kernel puts your thread to **sleep** (`0%` CPU) until enough scans arrive. |
| **Watermark** | The **minimum batch size (in scans)** that must be sitting in the kernel buffer before a blocking `read()` finishes waiting and copies data to user space. |

```text
One Scan ("datum" in kernel code) = 4 bytes total:
+--------------------+--------------------+
| ch0: 16-bit (2 B)  | ch1: 16-bit (2 B)  |
+--------------------+--------------------+

Kernel Buffer holding 3 scans = 12 bytes total:
+-------------+-------------+-------------+
|   Scan A    |   Scan B    |   Scan C    |
| (100, 200)  | (101, 201)  | (102, 202)  |
|   4 bytes   |   4 bytes   |   4 bytes   |
+-------------+-------------+-------------+
```

## 1. Goal

* **Before:** "My C app calls `read()` to read the sensor over SPI." (Mental trap: thinking every `read()` directly toggles SPI wires.)
* **After:** See the exact boundary between **STM32 firmware**, **Linux drivers**, **the kernel IIO buffer**, and **your user-space process**, and trace how `read(fd, user_buf, 8)` pulls scans from kernel memory using `bytes_per_datum` and `watermark`.

This is Lesson 1 of [Week 1](README.md#week-1-reading-path) for
[Project A](../../plans/project_portfolio.md#a-embedded-faultevent-recorder).

## 2. Why Not Let the User App Touch SPI Directly?

What breaks if user-space apps map the BBB's SPI hardware registers and read samples directly?

1. **Missed samples when Linux schedules another task:** If logging or SSH runs for `5 ms`, your user app pauses for `5 ms`. At `10,000` samples/second, `50` samples arrive during that gap—unless a hardware FIFO, DMA, or kernel buffer collects them in the background (with enough capacity not to overflow), those samples are missed.
2. **Bus collisions:** If App A starts an SPI transfer and App B reconfigures the same SPI controller mid-transfer, the clock/chip-select state is corrupted.
3. **Zero hardware portability:** Every app would hardcode BeagleBone Black AM335x McSPI register addresses.

**The Solution (Divide the Job into 4 Layers):**

| Layer | Where it runs | One-line job | What it never touches |
| --- | --- | --- | --- |
| **1. STM32 Firmware** | STM32 MCU | Sample ADC at exact timer ticks and send packets over SPI | Linux files or user processes |
| **2. SPI Controller Driver** | Linux kernel (`spi-omap2-mcspi.c`) | Drive the BBB's physical SPI clocks, chip-select, and DMA/FIFO | What the bytes inside the packet mean |
| **3. Our IIO Client Driver** | Linux kernel (Project A module) | Unpack STM32 frames and push `4-byte` scans into the IIO buffer | How the user app saves or plots files |
| **4. Recorder Process** | Linux user space (`recorder`) | Call `read(fd, buf, 8)` to grab buffered scans and save fault records | SPI registers, interrupts, or pinmux |

## 3. How It Fits Together (And Why Watermark Exists)

```text
STM32 MCU                              BeagleBone Black (Linux)
+----------------------+               +---------------------------------------+
| ADC + Timer + DMA    |               | KERNEL SPACE                          |
| (samples every 100us)|-- SPI wires ->| SPI Controller + Client Driver        |
+----------------------+               |                  |                    |
                                       |                  v (pushes scans)     |
                                       |         IIO Ring Buffer               |
                                       |    [Scan A] [Scan B] [Scan C]         |
                                       +------------------|--------------------+
                                                          |
                                             read(fd, user_buf, 8)
                                             (copies 2 scans = 8 B)
                                                          |
                                       +------------------v--------------------+
                                       | USER SPACE                            |
                                       | Recorder Process                      |
                                       +---------------------------------------+
```

### What Is the Watermark Really Doing?

Think of the kernel buffer as a **bucket under a dripping tap** and your `read()` call as **coming to carry the bucket away**:

* Returning from `read()` and crossing the user/kernel boundary costs CPU time.
* If the driver drops **1 scan (`4 bytes`)** into the buffer at a time, you don't want `read()` returning to user space after every single `4-byte` drop when your app asked for a larger batch.
* The **watermark** (`rb->watermark`) is the **fill line painted on the bucket**: *"Do not complete this blocking `read()` until at least `watermark` scans have accumulated in the buffer."* (As we will see in Lesson 2, the producer may ring the wait-queue bell on each scan, and the reader's loop checks this fill line before returning data.)
* **What if your app asks `read()` for fewer scans than the fill line?**
  If `watermark` is `4 scans`, but your app only asks `read()` for `1 scan` (`4 bytes`), forcing the app to wait for `4 scans` would hang a small read. So the kernel waits for **whichever is smaller**:
  ```c
  to_wait = min(requested_scans, watermark);
  ```

| App requests (`n` bytes) | Requested scans (`n / 4`) | Configured `watermark` | Kernel waits for (`to_wait`) | Plain meaning |
| --- | --- | --- | --- | --- |
| `8 bytes` | `2 scans` | `2 scans` | **`2 scans`** | Both are 2; `read()` can finish once 2 scans are in the buffer |
| `40 bytes` | `10 scans` | `4 scans` | **`4 scans`** | App can take up to 10, and `read()` can finish as soon as 4 are ready |
| `4 bytes` | `1 scan` | `4 scans` | **`1 scan`** | App only asked for 1 scan, so `read()` finishes at 1 without waiting for 4 |

## 4. How the Kernel Code Actually Does This (Short Annotated Excerpt)

Because we are **not writing a driver file in Lesson 1**, we do not need full
kernel source files here. Below are **only the core lines** from our local Linux
tree (`f0100363d`), with **what is happening and how it works explained right
inside the comments** using our numbers (`n = 8` bytes, `datum_size = 4` bytes,
`watermark = 2` scans, `avail = 3` scans in the buffer).

### Step A: Connecting `read()` to the IIO Buffer (`drivers/iio/industrialio-core.c`)

Source link: [drivers/iio/industrialio-core.c:1908](../../../linux/drivers/iio/industrialio-core.c#L1908)

```c
static const struct file_operations iio_buffer_fileops = {
	/*
	 * WHAT HAPPENS: When user space calls read(fd, user_buf, 8) on
	 * /dev/iio:device0, the kernel's Virtual File System (VFS) looks up
	 * this .read function pointer and jumps into IIO's buffer read path
	 * (which calls iio_buffer_read() below).
	 */
	.read = iio_buffer_read_outer_addr,
};
```

### Step B: Computing `to_wait` and Copying Scans (`drivers/iio/industrialio-buffer.c`)

Source link: [drivers/iio/industrialio-buffer.c:142](../../../linux/drivers/iio/industrialio-buffer.c#L142)

```c
static ssize_t iio_buffer_read(struct file *filp, char __user *buf,
			       size_t n, loff_t *f_ps)
{
	/*
	 * STARTING SNAPSHOT FOR OUR TRACE:
	 * - buf  = user_buf (where in user space to copy the samples)
	 * - n    = 8 bytes  (recorder asked for 8 bytes)
	 * - rb   = kernel IIO buffer currently holding 3 scans (12 bytes):
	 *          [Scan A (4 B)] [Scan B (4 B)] [Scan C (4 B)]
	 */

	/*
	 * 1. HOW BIG IS ONE SCAN?
	 * bytes_per_datum is the size of 1 complete scan across active channels.
	 * In our example: ch0 (2 B) + ch1 (2 B) = 4 bytes per scan.
	 */
	datum_size = rb->bytes_per_datum; /* datum_size = 4 */

	/*
	 * 2. HOW MANY SCANS MUST WE WAIT FOR (to_wait)?
	 * - Requested scans = n / datum_size = 8 / 4 = 2 scans.
	 * - Watermark       = rb->watermark  = 2 scans.
	 * - Kernel takes the smaller of the two so a small read never hangs:
	 *   to_wait = min(2, 2) = 2 scans.
	 */
	to_wait = min_t(size_t, n / datum_size, rb->watermark); /* to_wait = 2 */

	do {
		/*
		 * 3. ARE ENOUGH SCANS ALREADY IN THE KERNEL BUFFER?
		 * iio_buffer_ready() checks: avail >= to_wait.
		 *
		 * - Case 1 (avail = 3 scans in buffer):
		 *   3 >= 2 is TRUE -> ready immediately! Do NOT sleep.
		 *
		 * - Case 2 (if avail were only 1 scan):
		 *   1 >= 2 is FALSE -> wait_woken() puts this thread to sleep
		 *   (0% CPU) until the STM32 SPI driver pushes another scan
		 *   and wakes this queue; then the loop repeats.
		 */
		if (!iio_buffer_ready(indio_dev, rb, to_wait, n / datum_size)) {
			wait_woken(&wait, TASK_INTERRUPTIBLE,
				   MAX_SCHEDULE_TIMEOUT);
			continue;
		}

		/*
		 * 4. COPY READY SCANS FROM KERNEL BUFFER TO USER SPACE:
		 * Buffer has [Scan A, Scan B, Scan C] (12 bytes), and n = 8.
		 * - Copies Scan A + Scan B (8 bytes) into user space (buf).
		 * - Leaves Scan C (4 bytes) inside the kernel buffer for next time.
		 * - Sets ret = 8 (bytes copied). Zero new SPI transfers needed!
		 */
		ret = rb->access->read(rb, n, buf); /* ret = 8 */
	} while (ret == 0);

	/* 5. Return 8 to the user-space recorder process. */
	return ret;
}
```

## 5. Control Flow

### (a) One-Line Call Chain

```text
/* Producer path (happens asynchronously when STM32 sends data) */
STM32 SPI frame -> [SPI IRQ/DMA] -> Project A client driver -> iio_push_to_buffers() -> rb (3 scans queued)

/* Consumer path (synchronous when recorder calls read) */
recorder read(fd, user_buf, 8) -> [syscall] -> iio_buffer_read_wrapper (industrialio-buffer.c) -> iio_buffer_read (industrialio-buffer.c) -> iio_buffer_ready (industrialio-buffer.c) -> rb->access->read (buffer backend) -> return 8
```

### (b) Step Table (Following the 8-Byte Read in Trace 1)

| # | Where (`file : function`) | Goal / state change | Concrete data in -> out | Context / mode |
| --- | --- | --- | --- | --- |
| 1 | `recorder` (user app) | Request 2 scans from open IIO device | `fd, user_buf, n = 8` -> `read()` syscall | User process thread (User mode / U-mode / EL0) |
| 2 | `industrialio-core.c : iio_buffer_fileops` | Route `.read` to IIO buffer wrapper | `filp, user_buf, n = 8` -> wrapper call | Same thread in kernel (SVC on BBB / S-mode / EL1) |
| 3 | `industrialio-buffer.c : iio_buffer_read_wrapper` | Verify legacy buffer path is not busy | `IIO_BUSY_BIT_POS == 0` -> call `iio_buffer_read` | Kernel task context |
| 4 | `industrialio-buffer.c : iio_buffer_read` | Compute scan size and watermark wait target | `n = 8, datum_size = 4, watermark = 2` -> `to_wait = 2` | Kernel task context |
| 5 | `industrialio-buffer.c : iio_buffer_ready` | Check if enough scans are already buffered | `avail = 3 >= to_wait = 2` -> `true` (ready) | Kernel task context |
| 6 | `rb->access->read` (selected buffer backend) | Copy 2 scans to user memory, keep 1 scan | `[Scan A, Scan B, Scan C]` -> `user_buf` gets A & B (`8 B`); C stays | Kernel task context |
| 7 | `industrialio-buffer.c : iio_buffer_read` | Unregister wait entry and return byte count | `ret = 8` -> returned to `recorder` | Kernel -> User return |

### (c) Boundary Timeline

```text
STM32 Firmware        Kernel Driver / Buffer          Recorder App (User Space)
--------------        ----------------------          -------------------------
sample ch0, ch1
  -- SPI frame -----> push Scan A (avail = 1)
sample ch0, ch1
  -- SPI frame -----> push Scan B (avail = 2)
sample ch0, ch1
  -- SPI frame -----> push Scan C (avail = 3)
                                                      call read(fd, user_buf, 8)
                      to_wait = min(8/4, 2) = 2  <---   [enters kernel via syscall]
                      avail (3) >= to_wait (2)!
                      copy Scan A, Scan B (8 B)  ---> receive 8 bytes (Scan C stays)
                                                      inspect / save fault window
```

## 6. Build, Evidence And Limitations

* **Evidence status:** Source-reviewed against Linux `f0100363d`. No code is built or executed in this conceptual lesson.

* **Important limitations to remember:**
  1. Real IIO scans often include alignment padding and an 8-byte timestamp (`16 bytes` per scan instead of `4 bytes`); user space must read `/sys/bus/iio/devices/iio:deviceX/scan_elements/` metadata rather than hardcoding `4`.

  2. Notice `iio_buffer_flush_hwfifo()` in `iio_buffer_ready()`: if a sensor chip has its own on-chip hardware FIFO, a `read()` *can* ask the driver to flush that hardware FIFO when `avail < to_wait`. The rule is: **do not assume every `read()` starts one SPI transfer—trace the buffer and FIFO path.**

## 7. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
Your user-space program is not allowed to reach out and wiggle the BeagleBone's SPI wires itself—and if it tried to poll samples directly, it could miss measurements whenever Linux paused your program to run another task. Instead, the kernel driver and buffers collect measurements from the STM32 in the background and stack them in a waiting room in kernel memory (the IIO buffer, as long as that waiting room does not overflow). Each measurement record is called a **scan** (`datum_size` bytes). When your program calls `read()`, it picks up ready-made scans from that waiting room, using the **watermark** as the "wait until at least this many scans are stacked up before finishing the read" rule.

---

### Interview Q1: What is the difference between a program and a process?
* **Simple answer:** A **program** is a dead file on disk containing instructions (like `/usr/bin/recorder`). A **process** is a live, running instance of that program in RAM.
* **What it owns:** Each process gets its own private virtual memory space and its own table of open files (`fd`s).
* **Example:** If you run `/usr/bin/recorder` in two terminals at the same time, there is **1 program** on disk, but **2 separate processes** in memory.

### Interview Q2: When an app calls a system call like `read()`, does it send a message to a separate "kernel process"?
* **Simple answer:** **No.** The kernel is not a separate process sitting on the side waiting for messages.
* **How it works:** Your **own application thread** switches the CPU privilege level from User mode to Kernel mode, runs the kernel's `read` code itself, and switches back to User mode when done.
* **When another process runs:** Entering a syscall does not automatically switch tasks; another thread runs if your thread **goes to sleep** waiting for data (or if the scheduler **preempts** it).

### Interview Q3: Why does a user app read sensors through a kernel driver instead of touching SPI hardware registers directly?
* **Simple answer:** For **safety, sharing, and buffering fast samples while user space is delayed**.
* **Why it matters (failure without it):**
  1. If Linux pauses your user app for `5 ms` to run SSH or logging, a `10 kHz` sensor produces `50` samples during that gap—hardware FIFOs/DMA and the kernel ring buffer can absorb that delay (as long as buffer capacity is not exceeded).
  2. If two apps touched the same SPI controller registers directly, they would corrupt each other's clock and chip-select settings.
* **Trade-off:** Crossing into the kernel via `read()` adds a small syscall/copy cost, which we amortize by reading batches of scans from a buffer.

### Interview Q4: Does every `read()` call on a sensor device trigger a new SPI transfer on the wires?
* **Simple answer:** **No.** On a buffered IIO device, **acquiring samples** and **reading samples** are decoupled.
* **How it works:** The driver pushes incoming SPI samples into a kernel ring buffer in the background. When the app calls `read()`, it just copies already-collected scans out of kernel RAM (as in our trace where 3 scans were already waiting).
* **Exception to mention in interviews:** If the buffer does not have enough scans yet (`avail < to_wait`) and the sensor chip has a hardware FIFO callback (`hwfifo_flush_to_buffer`), `iio_buffer_ready()` can ask the driver to flush that hardware FIFO.

### Interview Q5: What is the IIO buffer watermark, and what is the trade-off between a low vs. high watermark?
* **Simple answer:** The **watermark** sets the readiness threshold (`to_wait = min(requested_scans, watermark)`) that must accumulate in the kernel buffer before a blocking `read()` finishes and copies data to user space.
* **Low watermark (e.g., `1` scan):** **Lower read-completion latency, more frequent `read()` calls.** A blocking `read()` can return as soon as a single scan is available, which means more user/kernel round-trips at high sample rates.
* **High watermark (e.g., `64` scans):** **Larger batch per completed `read()`, higher delivery latency.** When your app requests at least 64 scans, a blocking `read()` waits until 64 scans accumulate before returning to user space (note: in software IIO buffers the producer still signals the wait queue per push while the reader loop rechecks the threshold, and the buffer capacity must be large enough not to overflow while waiting).

## 8. Corrections

- 2026-10-23: First Markdown publication of the current chat lesson. Clarified
  the illustrative scan layout, read-result assumptions and possible hardware
  FIFO flushing; no runtime result claimed.
- 2026-10-23 (revision 2): Rewrote in direct Feynman style per learner feedback.
  Added a concrete visual explanation of `scan`/`datum` and `watermark`
  (`to_wait = min(n / datum_size, watermark)`) with a 3-case comparison table,
  replaced the abstract assumptions list with a variable-by-variable starting
  snapshot table, and included the actual inspected C code from
  `industrialio-core.c`, `iio_core.h` and `industrialio-buffer.c` with both
  ready (`avail = 3`) and sleeping (`avail = 1`) traces.
- 2026-10-23 (revision 3): Simplified Section 4 per learner feedback—removed
  unnecessary full-function/header dumps not being implemented now, kept only
  the core lines of `iio_buffer_fileops` and `iio_buffer_read()`, and placed the
  step-by-step what/how trace directly inside inline C comments.
- 2026-10-23 (revision 4): Rewrote all 5 interview questions and answers in
  Section 7 into a simple, spoken-English `Simple answer / How it works / Trade-off`
  format.
- 2026-10-23 (revision 5): Applied technical review fixes: clarified that scan
  grouping does not by itself prove electrical simultaneity, kernel buffering
  prevents sample loss only within buffer capacity, preemption can also switch
  tasks, BBB uses Armv7-A `SVC` mode, and watermark governs `read()` completion
  batching rather than suppressing producer wait-queue notifications.
