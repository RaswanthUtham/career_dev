# W01-L05: The Complete Boot Movie — From Power-On Reset To `head.S`, `init/main.c`, Drivers And User Space

> **Source-reviewed:** Linux checkout `f0100363d` (`arch/riscv/kernel/head.S`,
> `arch/arm/kernel/head.S`, `init/init_task.c`, `init/main.c`,
> `drivers/base/init.c`, `kernel/fork.c`, `fs/exec.c`), 2026-10-26, tutor
> inspection.
> **Not runtime-verified:** the end-to-end boot movie and `initcall` traces
> below are source walkthroughs, not output from a booted Week 2 QEMU guest yet.

Previous: [W01-L04 (C01): Checked Header-Plus-Payload Size Calculation](W01_L04_c01_checked_packet_size.md).

## 0. Know This First

In Lessons 1–3, your user-space `recorder` process was already running and
calling `read(3, user_buf, 8)` on `/dev/iio:device0`.

Now let's watch the **prequel movie**: **What happens from the microsecond you
plug power into the board until `/dev/iio:device0` and your `recorder` process
exist?**

Before the movie starts, learn three core building blocks:
1. **What a `struct task_struct` is** (the kernel's ID card for every thread/process).
2. **The difference between `fork`/`clone`, `exec`, and `spawn`.**
3. **The boot stages from Mask ROM to User Space.**

### A. What Is `struct task_struct`? (The Kernel's ID Card for Every Task)

In Linux, every kernel thread and every user-space process/thread is represented
in kernel RAM by one C structure: **`struct task_struct`** (defined in
`<linux/sched.h>`). Even before the memory allocator exists, Linux has **one
hand-crafted `task_struct` baked into the kernel's `.data` section at compile
time** called **`init_task` (`PID 0`)** in [init/init_task.c:96](../../../linux/init/init_task.c#L96):

```text
+-------------------------------------------------------------------------+
| struct task_struct  (1 per thread/process in the system)                |
+-------------------------------------------------------------------------+
| .pid        = 0 (init_task), 1 (kernel_init), 2 (kthreadd), 42 (app)   |
| .comm       = "swapper" (PID 0) -> "init" / "sh" (PID 1) -> "recorder" |
| .__state    = TASK_RUNNING (0) | TASK_INTERRUPTIBLE (sleeping) | ...    |
| .flags      = PF_KTHREAD (if a kernel thread) or 0 (if a user process)  |
| .stack      = pointer to this thread's 8 KB / 16 KB kernel stack        |
| .mm         = NULL for kernel threads! Points to struct mm_struct       |
|               (user virtual memory map / page table) for user processes |
| .active_mm  = borrowed page table (&init_mm) when a kthread is running  |
| .files      = open file descriptor table (where fd = 3 lives in L02!)   |
| .thread     = saved CPU registers (ra, sp, s0..s11 / r4..r11) on switch |
+-------------------------------------------------------------------------+
```

* **Key interview insight (`mm == NULL` vs. `mm != NULL`):**
  * A **kernel thread** (`PID 0`, `PID 2 kthreadd`, or `PID 1` *before* it launches user space) only touches kernel virtual memory, so its `.mm` pointer is **`NULL`**!
  * The moment `PID 1` calls `kernel_execve("/sbin/init")`, the kernel allocates a real **`struct mm_struct`** for `PID 1` (`.mm != NULL`), giving it a user-space virtual address space!

### B. `fork` (`clone`) vs. `exec` vs. `spawn` (Do Not Mix These Up!)

```text
1. CLONE / FORK ("Photocopy the worker — creates a NEW PID"):
   [ PID 0 (init_task) ] --- kernel_clone() ---> [ PID 0 ] + [ PID 1 (new task_struct,new stack) ]
   * Creates a brand-new task_struct and kernel stack with a NEW PID.
   * Does NOT load a new program file from disk; the new task starts in a C function.

2. EXEC ("Change clothes & replace memory — keeps the SAME PID"):
   [ PID 1 (kernel thread, mm = NULL) ]
          |
          | kernel_execve("/sbin/init")
          v
   [ PID 1 (SAME task_struct & SAME PID 1, now has user mm != NULL running /sbin/init!) ]
   * Does NOT create a new PID!
   * Wipes the task's old execution state, creates a user address space (mm_struct),
     loads the ELF binary (/init or /sbin/init), and drops the CPU into User Mode!

3. SPAWN ("Create a new worker AND give it a program"):
   * In kernel space: `kernel_thread()` / `kthread_create()` spawns a kernel thread.
   * In user space (how `/sbin/init` starts your `recorder` app):
     Combines BOTH: `fork()` (make new child `PID 42`) + `execve("/usr/bin/recorder")`
     (load the `recorder` binary into `PID 42`).
```

### C. Quick Glossary of the Boot Cast

| Actor / Stage | Plain meaning in our boot movie |
| --- | --- |
| **Mask Boot ROM** | Tiny code hardwired into the CPU silicon at the factory. Runs first at power-on with MMU off and external DDR RAM still uninitialized. |
| **SPL / MLO / OpenSBI** | **First-stage loader / firmware:** On BBB, **U-Boot SPL (`MLO`)** runs in tiny on-chip SRAM to turn on the 512 MB DDR3 RAM. On RISC-V, **M-mode firmware (OpenSBI)** sets up hardware protection (`PMP`) and trap delegation. |
| **U-Boot** | **Bootloader** running in DDR RAM (MMU off). Reads the Linux kernel (`zImage`/`Image`) and Device Tree (`.dtb`) from SD/flash into RAM and jumps to `head.S`. |
| **`head.S`** | The very first assembly file inside the Linux kernel. Clears `.bss`, builds temporary page tables, turns **ON the MMU**, adopts `init_task` (`PID 0`), and jumps to `start_kernel()`. |
| **`init_task` (`PID 0`)** | The initial kernel thread (`"swapper"`). Runs `start_kernel()`, spawns `PID 1` and `PID 2`, and retires to become the **CPU idle loop**. |
| **`kernel_init` (`PID 1`)** | Cloned by `PID 0`. Initializes the driver model (`driver_init()`), runs all built-in drivers (`do_initcalls()`), frees `__init` memory, and calls `kernel_execve()` to become user-space **`PID 1`**. |
| **`kthreadd` (`PID 2`)** | Cloned by `PID 0`. The kernel thread daemon that services normal `kthread_create()` requests from drivers and workqueues. |

---

## 1. Goal

* **Before:** Seeing boot as disconnected pieces ("U-Boot does something, `head.S` does assembly, `init/main.c` runs, and somehow my driver and app start").
* **After:** Narrate the entire 6-scene movie from **Power-On Reset** -> **Boot ROM** -> **SPL/Firmware** -> **U-Boot** -> **`head.S`** -> **`start_kernel()` (`PID 0`)** -> **`rest_init()` (`PID 1` & `PID 2`)** -> **`driver_init()` & `do_initcalls()`** -> **`kernel_execve()` (`PID 1` User Space)** -> **`fork() + execve()` (`PID 42 recorder`)**, with the exact **Before -> After** state of each function.

---

## 2. The Complete Boot Movie: Scene 1 to Scene 6

```text
SCENE 1: Power-On Reset -> Mask Boot ROM (Silicon ROM, MMU OFF, No DDR RAM yet)
  |  CPU resets -> PC jumps to on-chip ROM -> reads boot pins (SD/eMMC)
  |  -> loads tiny SPL (MLO) into internal on-chip SRAM (64-128 KB)
  v
SCENE 2: SPL / Firmware -> U-Boot (Physical DDR RAM, MMU OFF, No OS / No Threads)
  |  - BBB (32-bit Arm): SPL initializes 512 MB DDR3 RAM -> loads U-Boot into DDR
  |  - RISC-V: OpenSBI (M-mode) configures PMP/delegation -> hands off to S-mode U-Boot
  |  - U-Boot loads Linux Kernel (`zImage`/`Image`) + Device Tree (`.dtb`) into DDR RAM,
  |    puts `.dtb` physical address in register (`r2` on Arm / `a1` on RISC-V),
  |    masks interrupts, keeps MMU OFF, and jumps to kernel entry (`head.S`)!
  v
SCENE 3: Linux `head.S` (Physical RAM -> Virtual RAM, MMU turns ON, PID 0 Adopted!)
  |  File: arch/arm/kernel/head.S (`stext`) or arch/riscv/kernel/head.S (`_start`)
  |  1. Zeroes `.bss` (so uninitialized C globals start at 0)
  |  2. Builds temporary early page tables & turns ON the MMU (CPU now uses Virtual Addresses!)
  |  3. Sets current task = `init_task` (PID 0) and stack pointer `sp = init_stack`
  |  4. Jumps into C: `start_kernel()` in `init/main.c`
  v
SCENE 4: `start_kernel()` in `init/main.c` (Runs as PID 0 "swapper", Interrupts OFF -> ON)
  |  PID 0 builds the OS foundation:
  |  `setup_arch()` (DTB + final MMU map) -> `mm_core_init()` (Buddy + SLUB `kmalloc`)
  |  -> `sched_init()` -> `init_IRQ()` -> `time_init()` -> `local_irq_enable()`
  |  -> calls `rest_init()`
  v
SCENE 5: `rest_init()` & `kernel_init()` (PID 0 Clones PID 1 & PID 2; Drivers Register!)
  |  - PID 0 calls `kernel_clone()`  -> creates PID 1 (`kernel_init`, waits on `kthreadd_done`)
  |  - PID 0 calls `kernel_thread()` -> creates PID 2 (`kthreadd`, services `kthread_create()`)
  |  - PID 0 calls `complete(&kthreadd_done)` and retires into `cpu_startup_entry()` (Idle Loop)
  |  - PID 1 wakes up -> calls `driver_init()` (/sys/bus, /sys/devices)
  |                   -> calls `do_initcalls()` (SPI bus -> IIO core -> Project A driver!)
  |                   -> calls `free_initmem()` (reclaims built-in `__init` code pages)
  v
SCENE 6: `kernel_execve()` -> User Space PID 1 -> Spawns Your `recorder` App!
     - PID 1 calls `run_init_process()` -> `kernel_execve()` (allocates user `mm_struct`,
       loads `/init`, `rdinit=/bin/sh`, or `/sbin/init`, and drops to User Mode!)
     - User-space PID 1 calls `fork()` + `execve("/usr/bin/recorder")` -> creates PID 42!
     - PID 42 (`recorder`) calls `open("/dev/iio:device0")` & `read(3, user_buf, 8)` (Lesson 1!)
```

### Why Each Handoff Exists (The "Why" Behind the Movie)
1. **Why can't Boot ROM load the Linux kernel directly?** Because external DDR RAM is off at power-on, and Boot ROM inside the SoC silicon doesn't know which brand/timing of DDR RAM chip is soldered onto your board. That's why Boot ROM loads a tiny board-specific **SPL** into on-chip SRAM first.
2. **Why can't U-Boot jump straight to `start_kernel()` in C?** Because C code requires a valid stack pointer (`sp`), zeroed `.bss` globals, and **Virtual Addresses** matching the linker script (`KERNEL_LINK_ADDR`). `head.S` sets those up in assembly first.
3. **Why can't `PID 0` (`start_kernel`) initialize drivers or become `/sbin/init`?**
   * Because Linux needs an **idle thread (`PID 0`)** per CPU core that is *always* available to run `wfi` (wait-for-interrupt) when every real thread is sleeping.
   * If `PID 0` called a driver's `probe()` that slept on an SPI transfer (or if `PID 0` turned into `/sbin/init`), the CPU would have no idle thread left!

---

## 3. Before And After of Every Key Boot Sub-Function

Here is what changes in kernel memory **before and after** each function in Scenes 3–6:

### 1. `head.S` (`_start` / `stext` -> `start_kernel`)
* **Files:** [arch/riscv/kernel/head.S:22](../../../linux/arch/riscv/kernel/head.S#L22), [arch/arm/kernel/head.S:84](../../../linux/arch/arm/kernel/head.S#L84), [arch/arm/kernel/head-common.S:78](../../../linux/arch/arm/kernel/head-common.S#L78)

```text
BEFORE head.S:                           AFTER head.S (entering start_kernel):
+------------------------------------+   +--------------------------------------------+
| PC  = Physical RAM (0x80200000)    |   | PC      = Kernel Virtual Address           |
| MMU = OFF                          |   | MMU     = ON (early page tables active)    |
| .bss= uninitialized RAM garbage    |   | .bss    = all zeroes (0x00000000)          |
| sp  = undefined / bootloader stack |   | sp      = &init_thread_union + THREAD_SIZE |
| current task = none                |   | current = &init_task (PID 0 "swapper")     |
+------------------------------------+   +--------------------------------------------+
```

### 2. `setup_arch()` + `mm_core_init()` (Inside `start_kernel`, `PID 0`)
* **File:** [init/main.c:942](../../../linux/init/main.c#L942) and [init/main.c:981](../../../linux/init/main.c#L981)

```text
BEFORE setup_arch() & mm_core_init():    AFTER setup_arch() & mm_core_init():
+------------------------------------+   +--------------------------------------------+
| - DTB is just raw bytes in RAM     |   | - DTB parsed; RAM regions registered       |
| - Only temporary boot page table   |   | - Final kernel page tables (`swapper_pg_dir`)|
| - Calling kmalloc() would CRASH!   |   | - Buddy page allocator + SLUB `kmalloc()`  |
|                                    |   |   are LIVE and ready for drivers!          |
+------------------------------------+   +--------------------------------------------+
```

### 3. `rest_init()` (Spawning `PID 1` & `PID 2` from `PID 0`)
* **File:** [init/main.c:691](../../../linux/init/main.c#L691)

```text
BEFORE rest_init():                      AFTER rest_init():
+------------------------------------+   +--------------------------------------------+
| Task list has ONLY 1 task:         |   | Task list now has 3 kernel threads:        |
|   [ PID 0: "swapper" (init_task) ] |   |   1. [ PID 0: "swapper" ] -> CPU idle loop |
|                                    |   |   2. [ PID 1: "swapper/0" -> kernel_init ] |
|                                    |   |   3. [ PID 2: "kthreadd"  -> kthread daemon]|
+------------------------------------+   +--------------------------------------------+
```

### 4. `driver_init()` (Called by `PID 1` inside `do_basic_setup()`)
* **File:** [drivers/base/init.c:16](../../../linux/drivers/base/init.c#L16)
* **Plain English:** Builds the **empty shelves** of the Linux Driver Model (`devices_init()`, `buses_init()`, `classes_init()`, `of_core_init()`, `platform_bus_init()`) before any individual driver is allowed to register.

```text
BEFORE driver_init():                    AFTER driver_init():
+------------------------------------+   +--------------------------------------------+
| No driver model in RAM!            |   | Empty Driver Model hierarchy exists:       |
| - Calling spi_register_driver()    |   |   /sys/devices/                            |
|   or iio_device_register() here    |   |   /sys/bus/  (with `platform` bus ready)   |
|   would fail/crash—no bus lists!   |   |   /sys/class/                              |
|                                    |   |   /dev/      (devtmpfs initialized)        |
+------------------------------------+   +--------------------------------------------+
```

### 5. `do_initcalls()` (Called by `PID 1` right after `driver_init()`)
* **File:** [init/main.c:1422](../../../linux/init/main.c#L1422)
* **Plain English:** Walks through levels `0` to `7` and Stocks the shelves created by `driver_init()`—registering bus cores first (`postcore`/`subsys`) and device drivers second (`device` level 6):

```text
BEFORE do_initcalls():                   AFTER do_initcalls() (Levels 0..7):
+------------------------------------+   +--------------------------------------------+
| /sys/bus/ only has `platform`      |   | - Level 2 (postcore): `spi_init()` adds    |
| No SPI bus, no IIO subsystem,      |   |   `/sys/bus/spi`                           |
| no `/dev/iio:device0`              |   | - Level 4 (subsys): `iio_init()` adds      |
|                                    |   |   `/sys/bus/iio`                           |
|                                    |   | - Level 6 (device): built-in `module_init` |
|                                    |   |   drivers probe hardware -> `/dev/iio:...` |
+------------------------------------+   +--------------------------------------------+
```

### 6. `free_initmem()` (Called by `PID 1` after `do_initcalls()`)
* **File:** [init/main.c:1550](../../../linux/init/main.c#L1550)

```text
BEFORE free_initmem():                   AFTER free_initmem():
+------------------------------------+   +--------------------------------------------+
| `[ .text ] [ .init.text ] [ .data ]`|   | `[ .text ] [ FREED RAM! ] [ .data ]`       |
| All built-in `__init` functions    |   | Built-in `.init.text` pages returned to    |
| still sit in RAM                   |   | Buddy allocator for apps/buffers to use!   |
+------------------------------------+   +--------------------------------------------+
```

### 7. `run_init_process()` -> `kernel_execve()` (Called by `PID 1` at the end of `kernel_init()`)
* **File:** [init/main.c:1460](../../../linux/init/main.c#L1460) and [fs/exec.c:1956](../../../linux/fs/exec.c#L1956)

```text
BEFORE kernel_execve() (PID 1):          AFTER kernel_execve() (STILL PID 1!):
+------------------------------------+   +--------------------------------------------+
| struct task_struct (PID 1):        |   | struct task_struct (PID 1):                |
|   .pid   = 1                       |   |   .pid   = 1 (SAME PID!)                   |
|   .flags = PF_KTHREAD              |   |   .flags = 0 (now a normal user process!)  |
|   .mm    = NULL (no user memory!)  |   |   .mm    = 0xffff... (new user mm_struct!) |
|   Mode   = Kernel (SVC / S / EL1)  |   |   Mode   = User (USR / U-mode / EL0)       |
|   Code   = `kernel_init()` in C    |   |   Code   = `/init`, `/bin/sh`, or `/sbin/init`|
+------------------------------------+   +--------------------------------------------+
```

---

## 4. Starting Snapshot: The 8 Initcall Levels (`do_initcalls`)

Why can't Linux call every built-in `module_init()` function in random alphabetical order?
Because your **Project A SPI client driver** cannot register on the SPI bus until the **SPI core subsystem** (`spi_init`) and **IIO core subsystem** (`iio_init`) have already initialized!

| Level # | Name in `init/main.c` | Macro in `<linux/init.h>` | What registers at this level (Project A example) |
| --- | --- | --- | --- |
| `1` | `"core"` | `core_initcall()` | Core kernel buses/allocators |
| `2` | `"postcore"` | `postcore_initcall()` | **SPI bus core (`spi_init`)** and pinmux core |
| `4` | `"subsys"` | `subsys_initcall()` | **IIO core (`iio_init`)** and I2C core |
| `6` | `"device"` | `device_initcall()` / **`module_init()`** | **Controller drivers (`spi-omap2-mcspi`) and our Project A driver** |
| `7` | `"late"` | `late_initcall()` | Late cleanup/debug checks after all normal drivers exist |

---

## 5. How the Kernel Code Actually Does This (Short Annotated Excerpts)

Below are the 4 key code checkpoints from `head.S` to `kernel_init()`, with what happens and how it works explained right inside the comments.

### Step A: `head.S` Adopts `init_task` (`PID 0`) and Jumps to `start_kernel()`

Source links: [arch/riscv/kernel/head.S:282](../../../linux/arch/riscv/kernel/head.S#L282) and [init/init_task.c:96](../../../linux/init/init_task.c#L96)

```c
/* 1. In init/init_task.c (compiled into the kernel's .data section):
 *    This is PID 0 ("swapper"), the very first task_struct in Linux! */
struct task_struct init_task = {
	.__state   = 0,           /* TASK_RUNNING */
	.stack     = init_stack,  /* PID 0's static kernel stack */
	.flags     = PF_KTHREAD,  /* starts as a kernel thread */
	.mm        = NULL,        /* kernel threads have no user address space */
	.active_mm = &init_mm,    /* uses the kernel master page table */
	.comm      = "swapper",   /* name shown in ps/top for PID 0 */
};

/* 2. In arch/riscv/kernel/head.S (32-bit Arm does the same in head-common.S):
 *    - Clear .bss to 0
 *    - Enable the MMU (setup_vm + relocate_enable_mmu)
 *    - Point `tp` (current task register) to `init_task` (PID 0)
 *    - Point `sp` (stack pointer) to the top of `init_thread_union`
 *    - Jump into C (`tail start_kernel`)! */
	la tp, init_task
	la sp, init_thread_union + THREAD_SIZE
	tail start_kernel
```

### Step B: `rest_init()` Clones `PID 1` (`kernel_init`) and `PID 2` (`kthreadd`)

Source link: [init/main.c:691](../../../linux/init/main.c#L691)

```c
static noinline void __ref __noreturn rest_init(void)
{
	/* 1. CLONE PID 1 (kernel_init) FIRST SO IT GETS PID = 1:
	 *    kernel_clone() copies PID 0's setup into a new task_struct (PID 1)
	 *    that will run kernel_init(). PID 1 pauses on `kthreadd_done` until
	 *    PID 2 exists. */
	pid = kernel_clone(&init_args); /* creates PID 1: kernel_init */

	/* 2. CLONE PID 2 (kthreadd) TO SERVICE NORMAL kthread_create() REQUESTS: */
	pid = kernel_thread(kthreadd, NULL, NULL, CLONE_FS | CLONE_FILES);

	/* 3. UNLOCK PID 1 NOW THAT kthreadd (PID 2) IS READY: */
	system_state = SYSTEM_SCHEDULING;
	complete(&kthreadd_done); /* wakes up PID 1 (kernel_init) */

	/* 4. PID 0 BECOMES THE CPU IDLE LOOP FOREVER: */
	schedule_preempt_disabled();
	cpu_startup_entry(CPUHP_ONLINE);
}
```

### Step C: `PID 1` Builds the Driver Model (`driver_init`) and Runs `do_initcalls()`

Source links: [drivers/base/init.c:16](../../../linux/drivers/base/init.c#L16), [init/main.c:1422](../../../linux/init/main.c#L1422), and [init/main.c:1445](../../../linux/init/main.c#L1445)

```c
/* Inside drivers/base/init.c: */
void __init driver_init(void)
{
	/* Creates the core driver model kobjects/folders in RAM: */
	devtmpfs_init();      /* prepares /dev */
	devices_init();       /* prepares /sys/devices */
	buses_init();         /* prepares /sys/bus */
	classes_init();       /* prepares /sys/class */
	of_core_init();       /* prepares Device Tree (/sys/firmware/devicetree) */
	platform_bus_init();  /* registers /sys/bus/platform */
}

/* Inside init/main.c: */
static void __init do_basic_setup(void)
{
	/* 1. Build the empty driver-model shelves (/sys/devices, /sys/bus): */
	driver_init();

	/* 2. Run built-in initcalls in level order (level 0 -> level 7):
	 *    - Level 2 (postcore): spi_init() registers /sys/bus/spi
	 *    - Level 4 (subsys):   iio_init() registers /sys/bus/iio
	 *    - Level 6 (device):   module_init() drivers register & probe! */
	do_initcalls();
}
```

### Step D: `PID 1` Frees Built-In `__init` Memory and `execve`s User Space (`PID 1`)

Source link: [init/main.c:1555](../../../linux/init/main.c#L1555)

```c
static int __ref kernel_init(void *unused)
{
	/* 1. WAIT UNTIL PID 2 (kthreadd) IS CREATED BY rest_init(): */
	wait_for_completion(&kthreadd_done);

	/* 2. RUN do_basic_setup() -> driver_init() + do_initcalls(): */
	kernel_init_freeable();
	async_synchronize_full();

	/* 3. RECLAIM BUILT-IN BOOT CODE (__init) AND LOCK KERNEL .rodata:
	 *    Built-in `__init` pages are freed back to the page allocator here
	 *    (and loadable modules free their own init section after module_init).
	 *    Never call an `__init` function from a runtime callback after init
	 *    finishes—jumping into reclaimed/reused pages can fault or corrupt state! */
	free_initmem();
	mark_readonly();
	system_state = SYSTEM_RUNNING;

	/* 4. TRANSFORM PID 1 FROM A KERNEL THREAD INTO USER SPACE (kernel_execve):
	 *    Still PID 1, but now gets a user `mm_struct` and drops to User Mode!
	 *    kernel_init() tries these in order (init/main.c:1580-1616):
	 *    (a) ramdisk_execute_command (default "/init", or set by rdinit=...)
	 *    (b) execute_command (set by init=... on the kernel command line)
	 *    (c) CONFIG_DEFAULT_INIT (if configured)
	 *    (d) Fallback list below: /sbin/init, /etc/init, /bin/init, /bin/sh */
	if (ramdisk_execute_command) {
		ret = run_init_process(ramdisk_execute_command);
		if (!ret)
			return 0;
	}
	if (execute_command) {
		ret = run_init_process(execute_command);
		if (!ret)
			return 0;
		panic("Requested init %s failed (error %d).",
		      execute_command, ret);
	}
	if (!try_to_run_init_process("/sbin/init") ||
	    !try_to_run_init_process("/etc/init") ||
	    !try_to_run_init_process("/bin/init") ||
	    !try_to_run_init_process("/bin/sh"))
		return 0;

	panic("No working init found. Try passing init= option to kernel.");
}
```

---

## 6. Control Flow

### (a) One-Line Call Chain (Power-On to `recorder` App)

```text
Mask Boot ROM -> SPL / OpenSBI -> U-Boot -> head.S (_start / stext, adopts PID 0)
  -> start_kernel [PID 0] -> rest_init [PID 0 clones PID 1 & PID 2, then idles]
  -> kernel_init [PID 1] -> driver_init -> do_initcalls (levels 0..7) -> free_initmem
  -> run_init_process / kernel_execve [PID 1 enters User Space!]
  -> fork() + execve("/usr/bin/recorder") [PID 42 calls read(3, user_buf, 8)!]
```

### (b) Step Table (Following the Boot Relay End to End)

| # | Stage / File : Function | Who runs (`PID`) | What changes (`Before -> After`) | Mode / MMU |
| --- | --- | --- | --- | --- |
| 1 | **Boot ROM -> SPL / Firmware** | No OS | Power-on reset -> DDR RAM initialized | Machine/SVC, **MMU OFF** |
| 2 | **U-Boot** | No OS | Loads `zImage`/`Image` + `.dtb` into DDR RAM; jumps to `head.S` | SVC / S-mode, **MMU OFF** |
| 3 | `head.S` (`_start` / `stext`) | Becomes **`PID 0`** | Zeroes `.bss`, enables **MMU**, sets `current = &init_task`, `sp = init_stack` | Kernel mode, **MMU ON** |
| 4 | `init/main.c : start_kernel` | **`PID 0`** (`swapper`) | Parses `.dtb`, initializes Buddy + SLUB (`kmalloc`), IRQs, timer, scheduler | Kernel (`PID 0`, `mm = NULL`) |
| 5 | `init/main.c : rest_init` | **`PID 0`** | Clones **`PID 1`** (`kernel_init`) & **`PID 2`** (`kthreadd`); `PID 0` enters idle loop | Kernel (`PID 0, 1, 2`) |
| 6 | `drivers/base/init.c : driver_init` | **`PID 1`** | Empty `/sys/devices`, `/sys/bus`, `/sys/class`, `/dev` created in RAM | Kernel (`PID 1`, can sleep) |
| 7 | `init/main.c : do_initcalls` | **`PID 1`** | Runs levels `0..7`: `spi_init` (L2) -> `iio_init` (L4) -> `module_init` drivers (L6) | Kernel (`PID 1`) |
| 8 | `init/main.c : free_initmem` | **`PID 1`** | Frees built-in `.init.text` pages; `system_state = SYSTEM_RUNNING` | Kernel (`PID 1`) |
| 9 | `init/main.c : run_init_process` | **`PID 1`** | `kernel_execve()` gives `PID 1` a user `mm_struct` and starts `/init` or `/sbin/init` | **User Mode (`PID 1`, `mm != NULL`)** |
| 10 | User `/init` or shell | **`PID 1` -> `PID 42`** | `fork()` creates `PID 42` -> `execve("recorder")` -> opens `/dev/iio:device0` | **User Mode (`PID 42`)** |

### (c) Timeline Across Actors

```text
Boot ROM / U-Boot        PID 0 (init_task)          PID 1 (kernel_init -> init)     PID 2 (kthreadd)
-----------------        -----------------          ---------------------------     ----------------
ROM -> SPL -> DDR on
U-Boot loads Image+DTB
jump to head.S --------> zero .bss, turn MMU ON
                         start_kernel():
                         setup_arch, kmalloc, IRQs
                         rest_init():
                           kernel_clone() --------> [spawned as PID 1, waits]
                           kernel_thread() ---------------------------------------> [spawned as PID 2]
                           complete(kthreadd_done)-> wakes up!
                         cpu_startup_entry()        driver_init() (/sys/bus...)
                         (idles when CPU free)      do_initcalls() (SPI, IIO...) -> services any
                                                    free_initmem()                  kthread_create()
                                                    kernel_execve("/sbin/init")
                                                    [NOW IN USER SPACE!]
                                                    fork() + execve("recorder")
```

---

## 7. Week 2 QEMU Linux Preflight (Our Chosen Direct-Boot Recipe)

Look at Scene 2 and Scene 6 above: on real hardware, U-Boot loads the kernel, `.dtb`, and initramfs/rootfs. In **Week 2**, QEMU can play the role of the bootloader directly when we pass our **4-part direct-boot recipe**:

| # | QEMU Flag in Our Recipe | Role in Our Week 2 Guest | What happens if omitted or mismatched in this setup |
| --- | --- | --- | --- |
| 1 | `-M virt -nographic` | **Emulated Machine & Serial Routing** | Mismatched `-M` selects the wrong board/memory map; `-nographic` routes the emulated UART to our current terminal |
| 2 | `-kernel zImage` (or `Image`) | **Compiled Linux Kernel** | QEMU has no direct kernel image to load (unless booting via U-Boot) |
| 3 | `-initrd rootfs.cpio` (or `-drive`) | **External Initramfs / Rootfs** containing `/init` or `/bin/sh` | Unless an initramfs is built into the kernel, `kernel_init()` has no root filesystem to execute `/init` or `/bin/sh` |
| 4 | `-append "console=ttyAMA0 rdinit=/bin/sh"` | **Kernel Command Line** (`saved_command_line`) | `console=ttyAMA0` tells Linux which UART to print boot logs to; `rdinit=/bin/sh` tells `ramdisk_execute_command` to start `/bin/sh` as PID 1 |

---

## 8. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
Booting Linux is a 6-scene relay race:
1. **Mask ROM** wakes up inside the chip and loads a tiny helper (**SPL**) into internal SRAM because the main DDR RAM chip isn't awake yet.
2. **SPL** turns on the DDR RAM and loads **U-Boot**, which copies the **Linux kernel** and **Device Tree map (`.dtb`)** from the SD card into RAM and jumps into kernel assembly (**`head.S`**).
3. **`head.S`** clears `.bss`, turns on the **MMU** (switching from physical to virtual addresses), puts on the ID badge of **Worker 0 (`PID 0`, `init_task`)**, and jumps into C (**`start_kernel()`**).
4. **Worker 0** sets up memory allocation (`kmalloc`), interrupts, and the scheduler, then clones **Worker 1 (`PID 1`, `kernel_init`)** and **Worker 2 (`PID 2`, `kthreadd`)** before retiring to become the CPU's night watchman (idle loop).
5. **Worker 1 (`PID 1`)** builds the empty driver shelves (`driver_init()`), calls every built-in driver in order (`do_initcalls()`: SPI bus first, IIO core second, device drivers third), and throws away the one-time boot code (`free_initmem()`).
6. Finally, **Worker 1 (`PID 1`)** calls `kernel_execve()`—keeping the same `PID 1` badge, but trading its kernel outfit for a user-space address space (`mm_struct`) to become `/init` or `/sbin/init`, which then uses **`fork() + execve()`** to launch your `recorder` app!

---

### Interview Q1: What is the difference between `fork()`/`kernel_clone()` and `execve()`/`kernel_execve()` during Linux boot?
* **Simple answer:**
  * **`fork()` / `kernel_clone()` makes a new task (`new PID`)**: In `rest_init()`, `PID 0` calls `kernel_clone()` to create a second kernel thread with **`PID 1`** (`kernel_init`). Both are still in kernel space.
  * **`execve()` / `kernel_execve()` replaces the program inside the *same* task (`same PID`)**: At the end of `kernel_init()`, `PID 1` calls `kernel_execve("/sbin/init")`. No new PID is created—`PID 1` gets a new user-space `struct mm_struct`, loads the `/sbin/init` binary, and transitions from a kernel thread into the first user-space process.

### Interview Q2: In `struct task_struct`, why is `.mm` set to `NULL` for kernel threads (`PID 0`, `PID 2`), and when does `PID 1` get a non-NULL `.mm`?
* **Simple answer:** `.mm` points to a task's **user-space virtual memory descriptor (`struct mm_struct`)**.
* **Why kernel threads have `.mm = NULL`:** Kernel threads (`PF_KTHREAD`) never run in user mode and only access the shared kernel half of virtual memory, so they don't own a user-space page table (they borrow the previous task's page table or `init_mm` via `.active_mm`).
* **When `PID 1` gets `.mm != NULL`:** `PID 1` starts life as a kernel thread (`.mm = NULL`) while running `kernel_init()`. When it calls `kernel_execve()`, `fs/exec.c` allocates a fresh `struct mm_struct` for its user-space code, data, and stack.

### Interview Q3: Why does `module_init()` work for both built-in drivers (`=y`) and loadable kernel modules (`=m`)?
* **Simple answer:** Because `<linux/module.h>` changes what `module_init(fn)` expands to depending on whether the file is compiled into `vmlinux` (`=y`) or as a `.ko` module (`=m`).
* **How it works:**
  * **When built-in (`=y`):** `module_init(fn)` becomes `device_initcall(fn)`, which places a pointer to `fn` into the `.initcall6.init` linker section so `do_initcalls()` calls it automatically at boot (after `postcore` and `subsys` buses from `driver_init()` are ready).
  * **When compiled as a module (`=m`):** `module_init(fn)` creates the module's `init_module` entry point, which the kernel calls later when user space runs `insmod` or `modprobe`.

### Interview Q4: What does the `__init` macro do on a driver's init function, and what bug happens if a runtime callback calls an `__init` function?
* **Simple answer:** `__init` places the function into an initialization-only code section (`.init.text`) that the kernel frees once initialization finishes.
* **Why Linux uses it:** For built-in code, `free_initmem()` reclaims `.init.text` right after `do_initcalls()` at boot; for loadable `.ko` modules, `do_init_module()` frees the module's init text right after `module_init` returns `0`.
* **The bug:** If a timer, interrupt, or `read()` callback calls an `__init` function after its init phase has finished, the CPU jumps into freed or reused memory—which is invalid and can trigger a kernel Oops/page fault or execute corrupted/overwritten memory.

### Interview Q5: During Linux boot, why does `rest_init()` create `PID 1` (`kernel_init`) first, but make it wait for `PID 2` (`kthreadd`) before running `do_initcalls()`?
* **Simple answer:**
  1. `kernel_init` must be cloned **first** by `PID 0` so it receives the reserved process ID **`PID 1`** (which Unix/Linux requires for the initial user-space process).
  2. However, `do_initcalls()` runs driver initialization code that requests background kernel threads and workqueues via `kthread_create()`—and **`kthreadd` (`PID 2`) is the daemon that services normal `kthread_create()` requests**.
* **Result:** If `PID 1` ran `do_initcalls()` before `PID 2` (`kthreadd`) was ready, any driver requesting a kernel thread via `kthread_create()` would crash the boot!

## 9. Corrections

- 2026-10-25: First publication of W01-L05, completing Week 1's source-grounded boot/ownership trace and Week 2 QEMU guest preflight.
- 2026-10-25 (revision 2): Applied lesson-review fixes (`LR1`–`LR4`): showed `kernel_init()`'s actual init selection order (`ramdisk_execute_command` / `rdinit=`, `execute_command` / `init=`, `CONFIG_DEFAULT_INIT`, then `/sbin/init`..`/bin/sh` fallbacks), distinguished built-in `free_initmem()` from loadable-module init-memory reclamation (and noted freed-page reuse vs. page fault), scoped `kthreadd` to servicing normal `kthread_create()` requests after `PID 0` bootstrap, and labelled Section 7 as our chosen direct-boot QEMU recipe.
- 2026-10-26 (revision 3): Expanded into the complete 6-scene boot movie (Power-On Reset -> Boot ROM -> SPL/OpenSBI -> U-Boot -> `head.S` -> `start_kernel` -> `rest_init` -> `driver_init` / `do_initcalls` -> `kernel_execve` -> `fork + execve` user app), added the `struct task_struct` diagram (`init_task` in `init/init_task.c`), `fork`/`clone` vs. `exec` vs. `spawn` visuals, and explicit **Before -> After** ASCII boxes for every key boot sub-function.
