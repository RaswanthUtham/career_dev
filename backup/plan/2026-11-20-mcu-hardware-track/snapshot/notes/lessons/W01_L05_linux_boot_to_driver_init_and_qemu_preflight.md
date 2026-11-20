# W01-L05: The Complete Boot Movie — From Power-On Reset To `head.S`, `init/main.c`, Drivers And User Space

> **Source-reviewed:** Linux checkout `f0100363d` (`arch/riscv/kernel/head.S`,
> `arch/arm/kernel/head.S`, `init/init_task.c`, `init/main.c`,
> `drivers/base/init.c`, `kernel/fork.c`, `fs/exec.c`), 2026-10-27.

Previous: [W01-L04 (C01): Checked Header-Plus-Payload Size Calculation](W01_L04_c01_checked_packet_size.md).

## 0. Know This First

In Lessons 1–3, your user-space `recorder` process was already running and
calling `read(3, user_buf, 8)` on `/dev/iio:device0`.

Now let's watch the **prequel movie**: **What happens from the microsecond you
plug power into the board until `/dev/iio:device0` and your `recorder` process
come alive?**

We start with three building blocks:
1. **`struct task_struct`** (the kernel's ID card for every thread and process).
2. **`fork`/`clone`, `exec`, and `spawn`** (how Linux creates tasks and loads programs).
3. **The 6 boot stages** from Mask ROM to User Space.

### A. What Is `struct task_struct`? (The Kernel's ID Card for Every Task)

In Linux, every kernel thread and every user-space process/thread is represented
in kernel RAM by one C structure: **`struct task_struct`** (defined in
`<linux/sched.h>`). At compile time, Linux bakes **one hand-crafted
`task_struct` directly into the kernel's `.data` section** called **`init_task`
(`PID 0`)** in [init/init_task.c:96](../../../linux/init/init_task.c#L96):

```text
+-------------------------------------------------------------------------+
| struct task_struct  (1 per thread/process in the system)                |
+-------------------------------------------------------------------------+
| .pid        = 0 (init_task), 1 (kernel_init), 2 (kthreadd), 42 (app)   |
| .comm       = "swapper" (PID 0) -> "init" / "sh" (PID 1) -> "recorder" |
| .__state    = TASK_RUNNING (0) | TASK_INTERRUPTIBLE (sleeping) | ...    |
| .flags      = PF_KTHREAD (for a kernel thread) or 0 (for a user process)|
| .stack      = pointer to this thread's 8 KB / 16 KB kernel stack        |
| .mm         = NULL for kernel threads; points to struct mm_struct       |
|               (user virtual memory map / page table) for user processes |
| .active_mm  = borrowed page table (&init_mm) while a kthread runs       |
| .files      = open file descriptor table (where fd = 3 lives in L02!)   |
| .thread     = saved CPU registers (ra, sp, s0..s11 / r4..r11) on switch |
+-------------------------------------------------------------------------+
```

* **How `.mm` changes from Kernel Thread to User Process:**
  * A **kernel thread** (`PID 0`, `PID 2 kthreadd`, and `PID 1` while running `kernel_init()`) uses only kernel virtual memory, so its `.mm` field is **`NULL`** and it borrows `&init_mm` through `.active_mm`.
  * When `PID 1` calls `kernel_execve("/sbin/init")`, the kernel allocates a **`struct mm_struct`** for `PID 1` (`.mm != NULL`), giving it its own user-space virtual address space.

### B. How `clone` (`fork`), `exec`, and `spawn` Work

```text
1. CLONE / FORK ("Photocopy the worker — creates a NEW PID"):
   [ PID 0 (init_task) ] --- kernel_clone() ---> [ PID 0 ] + [ PID 1 (new task_struct, new stack) ]
   * Creates a brand-new task_struct and kernel stack with a NEW PID.
   * For kernel-spawned tasks (`kernel_clone` with `.fn` set), the child starts at that C function (`kernel_init` or `kthreadd`).
   * For user-space `fork()`, the child resumes right after the `fork()` call with return value `0` (while the parent receives the child's new PID).

2. EXEC ("Change clothes & replace memory — keeps the SAME PID"):
   [ PID 1 (in kernel_init(), .mm = NULL) ]
          |
          | kernel_execve("/sbin/init")
          v
   [ PID 1 (SAME task_struct & SAME PID 1, now has user .mm != NULL & user pt_regs for /sbin/init!) ]
   * Keeps the same task_struct and the same PID.
   * Allocates a fresh user address space (mm_struct), loads the ELF binary
     (/init or /sbin/init), sets up the user trap frame, and enters User Mode when returning to user space.

3. SPAWN ("Create a new worker AND give it a program"):
   * In kernel space: `kernel_thread()` / `kthread_create()` creates a new kernel thread.
   * In user space (when `/sbin/init` starts your `recorder` app):
     Combines `fork()` (creates child `PID 42`) + `execve("/usr/bin/recorder")`
     (loads the `recorder` binary into `PID 42`).
```

### C. Quick Glossary of the Boot Cast

| Actor / Stage | What it does in our boot movie |
| --- | --- |
| **Mask Boot ROM** | Factory code wired inside the SoC silicon. Runs first at power-on with MMU off and loads the first-stage loader into internal SRAM. |
| **SPL / MLO / OpenSBI** | **First-stage loader / firmware:** On BBB, **U-Boot SPL (`MLO`)** runs in internal SRAM and initializes the 512 MB DDR3 RAM. On RISC-V, **OpenSBI (M-mode)** sets up physical memory protection (`PMP`) and trap delegation. |
| **U-Boot** | **Bootloader** running in DDR RAM (MMU off). Loads the Linux kernel (`zImage`/`Image`) and Device Tree (`.dtb`) from storage into RAM and jumps to `head.S`. |
| **`head.S`** | First assembly file inside the Linux kernel. Zeroes `.bss`, builds early page tables, turns **ON the MMU**, adopts `init_task` (`PID 0`), and jumps to `start_kernel()`. |
| **`init_task` (`PID 0`)** | The initial kernel thread (`"swapper"`). Runs `start_kernel()`, clones `PID 1` and `PID 2`, and becomes the **CPU idle loop**. |
| **`kernel_init` (`PID 1`)** | Cloned by `PID 0`. Initializes the driver model (`driver_init()`), runs all built-in drivers (`do_initcalls()`), frees `__init` memory, and calls `kernel_execve()` to become user-space **`PID 1`**. |
| **`kthreadd` (`PID 2`)** | Cloned by `PID 0`. Kernel thread daemon that services `kthread_create()` requests from drivers and workqueues. |

---

## 1. Goal

Trace the complete 6-scene movie from **Power-On Reset** -> **Boot ROM** ->
**SPL/Firmware** -> **U-Boot** -> **`head.S`** -> **`start_kernel()` (`PID 0`)**
-> **`rest_init()` (`PID 1` & `PID 2`)** -> **`driver_init()` & `do_initcalls()`**
-> **`kernel_execve()` (`PID 1` User Space)** -> **`fork() + execve()` (`PID 42 recorder`)**,
with the exact **Before -> After** state of each function.

---

## 2. The Complete Boot Movie: Scene 1 to Scene 6

```text
SCENE 1: Power-On Reset -> Mask Boot ROM (Silicon ROM, MMU OFF, Internal SRAM)
  |  CPU resets -> PC jumps to on-chip ROM -> reads boot pins (SD/eMMC)
  |  -> loads tiny SPL (MLO) into internal on-chip SRAM (64-128 KB)
  v
SCENE 2: SPL / Firmware -> U-Boot (Physical DDR RAM, MMU OFF)
  |  - BBB (32-bit Arm): SPL initializes 512 MB DDR3 RAM -> loads U-Boot into DDR
  |  - RISC-V: OpenSBI (M-mode) configures PMP/delegation -> hands off to S-mode U-Boot
  |  - U-Boot loads Linux Kernel (`zImage`/`Image`) + Device Tree (`.dtb`) into DDR RAM,
  |    puts `.dtb` physical address in register (`r2` on Arm / `a1` on RISC-V),
  |    masks interrupts, and jumps to kernel entry (`head.S`)!
  v
SCENE 3: Linux `head.S` (Physical RAM -> Virtual RAM, MMU turns ON, PID 0 Adopted!)
  |  File: arch/riscv/kernel/head.S (`_start`) or arch/arm/kernel/head.S (`stext` after `zImage` decompressor)
  |  - On RISC-V: zeroes `.bss`, builds early page tables & turns ON the MMU, sets `tp = &init_task` (PID 0) and `sp = init_stack`, and jumps to `start_kernel()`.
  |  - On 32-bit Arm: builds early page tables & turns ON the MMU first, then in `__mmap_switched` (`head-common.S`) zeroes `.bss`, sets `current = &init_task` (PID 0) and `sp = init_stack`, and branches to `start_kernel()`.
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
  |  - PID 0 calls `complete(&kthreadd_done)` and enters `cpu_startup_entry()` (Idle Loop)
  |  - PID 1 wakes up -> calls `driver_init()` (/sys/bus, /sys/devices)
  |                   -> calls `do_initcalls()` (SPI bus -> IIO core -> device drivers!)
  |                   -> calls `free_initmem()` (reclaims built-in `__init` code pages)
  v
SCENE 6: `kernel_execve()` -> User Space PID 1 -> Spawns Your `recorder` App!
     - PID 1 calls `run_init_process()` -> `kernel_execve()` (allocates user `mm_struct`,
       loads `/init`, `rdinit=/bin/sh`, or `/sbin/init`, and enters User Mode!)
     - User-space PID 1 calls `fork()` + `execve("/usr/bin/recorder")` -> creates PID 42!
     - PID 42 (`recorder`) calls `open("/dev/iio:device0")` & `read(3, user_buf, 8)` (Lesson 1!)
```

### Why Each Handoff Exists
1. **Why Boot ROM loads SPL first:** External DDR RAM powers up unconfigured. Boot ROM loads a small board-specific **SPL** into internal SRAM so SPL can configure the board's DDR controller before loading U-Boot and Linux into DDR RAM.
2. **Why `head.S` runs before `start_kernel()`:** C functions need a valid stack pointer (`sp`), zeroed `.bss` globals, and **Virtual Addresses** matching the linker script (`KERNEL_LINK_ADDR`). `head.S` prepares all three in assembly.
3. **Why `PID 0` clones `PID 1` to run drivers:** Linux keeps **`PID 0`** as the dedicated per-CPU idle thread (`cpu_startup_entry`). Cloning **`PID 1` (`kernel_init`)** gives Linux a normal process-context thread that can sleep on timers, wait queues, and hardware probes while `PID 0` stays ready to idle the CPU.

---

## 3. Before And After of Every Key Boot Sub-Function

Here is how kernel memory changes **before and after** each function in Scenes 3–6:

### 1. `head.S` (`_start` / `stext` -> `start_kernel`)
* **Files:** [arch/riscv/kernel/head.S:22](../../../linux/arch/riscv/kernel/head.S#L22), [arch/arm/kernel/head.S:84](../../../linux/arch/arm/kernel/head.S#L84), [arch/arm/kernel/head-common.S:78](../../../linux/arch/arm/kernel/head-common.S#L78)

```text
BEFORE head.S:                           AFTER head.S (entering start_kernel):
+------------------------------------+   +--------------------------------------------+
| PC  = Physical RAM (0x80200000)    |   | PC      = Kernel Virtual Address           |
| MMU = OFF                          |   | MMU     = ON (early page tables active)    |
| .bss= uninitialized RAM bytes      |   | .bss    = all zeroes (0x00000000)          |
| sp  = bootloader stack             |   | sp      = &init_thread_union + THREAD_SIZE |
| current task = none                |   | current = &init_task (PID 0 "swapper")     |
+------------------------------------+   +--------------------------------------------+
```

### 2. `setup_arch()` + `mm_core_init()` (Inside `start_kernel`, `PID 0`)
* **File:** [init/main.c:942](../../../linux/init/main.c#L942) and [init/main.c:981](../../../linux/init/main.c#L981)

```text
BEFORE setup_arch() & mm_core_init():    AFTER setup_arch() & mm_core_init():
+------------------------------------+   +--------------------------------------------+
| - DTB is raw bytes in RAM          |   | - DTB parsed; RAM regions registered       |
| - Early boot page table active     |   | - Final kernel page tables (`swapper_pg_dir`)|
| - Early bootmem (`memblock`) only  |   | - Buddy page allocator + SLUB `kmalloc()`  |
|                                    |   |   are live and ready for drivers           |
+------------------------------------+   +--------------------------------------------+
```

### 3. `rest_init()` (Spawning `PID 1` & `PID 2` from `PID 0`)
* **File:** [init/main.c:691](../../../linux/init/main.c#L691)

```text
BEFORE rest_init():                      AFTER rest_init():
+------------------------------------+   +--------------------------------------------+
| Task list has 1 task:              |   | Task list has 3 kernel threads:            |
|   [ PID 0: "swapper" (init_task) ] |   |   1. [ PID 0: "swapper" ] -> CPU idle loop |
|                                    |   |   2. [ PID 1: "swapper/0" -> kernel_init ] |
|                                    |   |   3. [ PID 2: "kthreadd"  -> kthread daemon]|
+------------------------------------+   +--------------------------------------------+
```

### 4. `driver_init()` (Called by `PID 1` inside `do_basic_setup()`)
* **File:** [drivers/base/init.c:16](../../../linux/drivers/base/init.c#L16)
* **What it does:** Builds the core folders and lists of the Linux Driver Model (`devtmpfs_init()`, `devices_init()`, `buses_init()`, `classes_init()`, `of_core_init()`, `platform_bus_init()`).

```text
BEFORE driver_init():                    AFTER driver_init():
+------------------------------------+   +--------------------------------------------+
| Core memory & scheduler are up,    |   | Empty Driver Model hierarchy is ready:     |
| driver model kobjects uninitialized|   |   /sys/devices/                            |
|                                    |   |   /sys/bus/  (with `platform` bus ready)   |
|                                    |   |   /sys/class/                              |
|                                    |   |   /dev/      (devtmpfs initialized)        |
+------------------------------------+   +--------------------------------------------+
```

### 5. `do_initcalls()` (Called by `PID 1` right after `driver_init()`)
* **File:** [init/main.c:1422](../../../linux/init/main.c#L1422)
* **What it does:** Walks through initcall levels `0` to `7` (`early` through `late`) and runs every built-in subsystem and driver init function in level order (if a device's dependency such as a clock or regulator is not ready yet during `probe()`, the driver can return `-EPROBE_DEFER` so the driver core retries its probe after later initcalls finish):

```text
BEFORE do_initcalls():                   AFTER do_initcalls() (Levels 0..7):
+------------------------------------+   +--------------------------------------------+
| Empty driver-model shelves in      |   | - Level 2 (postcore): `spi_init()` adds    |
| `/sys/bus/` and `/sys/class/`      |   |   `/sys/bus/spi`                           |
|                                    |   | - Level 4 (subsys): `iio_init()` adds      |
|                                    |   |   `/sys/bus/iio`                           |
|                                    |   | - Level 6 (device): built-in `module_init` |
|                                    |   |   drivers register and probe hardware      |
+------------------------------------+   +--------------------------------------------+
```

### 6. `free_initmem()` (Called by `PID 1` after `do_initcalls()`)
* **File:** [init/main.c:1550](../../../linux/init/main.c#L1550)

```text
BEFORE free_initmem():                   AFTER free_initmem():
+------------------------------------+   +--------------------------------------------+
| `[ .text ] [ .init.text ] [ .data ]`|   | `[ .text ] [ FREED RAM! ] [ .data ]`       |
| Built-in `__init` boot functions   |   | Built-in `.init.text` pages returned to    |
| sit in RAM                         |   | Buddy allocator for runtime use            |
+------------------------------------+   +--------------------------------------------+
```

### 7. `run_init_process()` -> `kernel_execve()` (Called by `PID 1` at the end of `kernel_init()`)
* **File:** [init/main.c:1460](../../../linux/init/main.c#L1460) and [fs/exec.c:1956](../../../linux/fs/exec.c#L1956)
* **Note on `PID 1` flags:** In `rest_init()`, `PID 1` is spawned with `user_mode_thread(kernel_init, NULL, CLONE_FS)` (so unlike `PID 2` `kthreadd`, `PID 1` is created **without** `PF_KTHREAD` so `kernel_execve()` is allowed to give it a user address space!).

```text
BEFORE kernel_execve() (PID 1):          AFTER kernel_execve() + trap exit (SAME PID 1!):
+------------------------------------+   +--------------------------------------------+
| struct task_struct (PID 1):        |   | struct task_struct (PID 1):                |
|   .pid   = 1                       |   |   .pid   = 1 (SAME PID!)                   |
|   .mm    = NULL (in kernel space)  |   |   .mm    = 0xffff... (new user mm_struct!) |
|   Mode   = Kernel (SVC / S / EL1)  |   |   Mode   = User (USR / U-mode / EL0)       |
|   Code   = `kernel_init()` in C    |   |   Code   = `/init`, `/bin/sh`, or `/sbin/init`|
+------------------------------------+   +--------------------------------------------+
```

---

## 4. Starting Snapshot: The 8 Initcall Levels (`do_initcalls`)

Linux sorts built-in initialization functions into numbered linker sections (`0` to `7`) so bus cores initialize in level order before the device drivers that attach to them:

| Level # | Name in `init/main.c` | Macro in `<linux/init.h>` | What registers at this level |
| --- | --- | --- | --- |
| `1` | `"core"` | `core_initcall()` | Core kernel buses and allocators |
| `2` | `"postcore"` | `postcore_initcall()` | **SPI bus core (`spi_init`)** and pinmux core |
| `4` | `"subsys"` | `subsys_initcall()` | **IIO core (`iio_init`)** and I2C core |
| `6` | `"device"` | `device_initcall()` / **`module_init()`** | **Controller drivers (`spi-omap2-mcspi`) and client drivers** |
| `7` | `"late"` | `late_initcall()` | Late boot initcalls and deferred-probe retries |

---

## 5. How the Kernel Code Actually Does This (Short Annotated Excerpts)

### Step A: `head.S` Adopts `init_task` (`PID 0`) and Jumps to `start_kernel()`

Source links: [arch/riscv/kernel/head.S:282](../../../linux/arch/riscv/kernel/head.S#L282) and [init/init_task.c:96](../../../linux/init/init_task.c#L96)

```c
/* 1. In init/init_task.c (compiled into the kernel's .data section):
 *    This is PID 0 ("swapper"), the first task_struct in Linux. */
struct task_struct init_task = {
	.__state   = 0,           /* TASK_RUNNING */
	.stack     = init_stack,  /* PID 0's static kernel stack */
	.flags     = PF_KTHREAD,  /* kernel thread */
	.mm        = NULL,        /* uses kernel virtual memory */
	.active_mm = &init_mm,    /* borrows master kernel page table */
	.comm      = "swapper",   /* task name for PID 0 */
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
	 *    that starts in kernel_init() and waits on `kthreadd_done`. */
	pid = kernel_clone(&init_args); /* creates PID 1: kernel_init */

	/* 2. CLONE PID 2 (kthreadd) TO SERVICE NORMAL kthread_create() REQUESTS: */
	pid = kernel_thread(kthreadd, NULL, NULL, CLONE_FS | CLONE_FILES);

	/* 3. UNLOCK PID 1 NOW THAT kthreadd (PID 2) IS READY: */
	system_state = SYSTEM_SCHEDULING;
	complete(&kthreadd_done); /* wakes up PID 1 (kernel_init) */

	/* 4. PID 0 BECOMES THE CPU IDLE LOOP: */
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
	 *    (and loadable modules free their own init section after module_init). */
	free_initmem();
	mark_readonly();
	system_state = SYSTEM_RUNNING;

	/* 4. TRANSFORM PID 1 FROM A KERNEL THREAD INTO USER SPACE (kernel_execve):
	 *    Still PID 1, now with a user `mm_struct` running in User Mode!
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
| 1 | **Boot ROM -> SPL / Firmware** | Pre-OS | Power-on reset -> DDR RAM initialized | Machine/SVC, **MMU OFF** |
| 2 | **U-Boot** | Pre-OS | Loads `zImage`/`Image` + `.dtb` into DDR RAM; jumps to `head.S` | SVC / S-mode, **MMU OFF** |
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

## 7. Direct-Boot QEMU Recipe (How QEMU Plays the Role of U-Boot)

On hardware, U-Boot loads the kernel, `.dtb`, and initramfs/rootfs into RAM. In a direct-kernel QEMU boot, QEMU loads those same pieces using four flags:

| # | QEMU Flag | Role in a Direct-Kernel Boot |
| --- | --- | --- |
| 1 | `-M virt -nographic` | Selects the emulated board/memory map and routes the serial UART console to your terminal |
| 2 | `-kernel zImage` (or `Image`) | Loads the compiled Linux kernel image into guest RAM |
| 3 | `-initrd rootfs.cpio` | Provides the initial RAM filesystem (`initramfs`) archive containing `/init` or `/bin/sh` |
| 4 | `-append "console=ttyAMA0 rdinit=/bin/sh"` | Passes `saved_command_line` to select the serial console and initial user program |

---

## 8. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
Booting is a 6-scene relay race:
1. **Mask ROM** wakes up inside the chip and loads a tiny helper (**SPL**) into internal SRAM.
2. **SPL** turns on the main DDR RAM and loads **U-Boot**, which copies the **Linux kernel** and **Device Tree map (`.dtb`)** from storage into RAM and jumps into kernel assembly (**`head.S`**).
3. **`head.S`** clears `.bss`, turns on the **MMU** (switching from physical to virtual addresses), puts on the ID badge of **Worker 0 (`PID 0`, `init_task`)**, and jumps into C (**`start_kernel()`**).
4. **Worker 0** sets up memory allocation (`kmalloc`), interrupts, and the scheduler, then clones **Worker 1 (`PID 1`, `kernel_init`)** and **Worker 2 (`PID 2`, `kthreadd`)** before becoming the CPU's idle loop.
5. **Worker 1 (`PID 1`)** builds the empty driver shelves (`driver_init()`), calls every built-in driver in order (`do_initcalls()`: SPI bus first, IIO core second, device drivers third), and frees the one-time boot code (`free_initmem()`).
6. Finally, **Worker 1 (`PID 1`)** calls `kernel_execve()`—keeping the same `PID 1` badge while getting a user-space address space (`mm_struct`) to run `/init` or `/sbin/init`, which then uses **`fork() + execve()`** to launch your `recorder` app!

---

### Interview Q1: What is the difference between `fork()`/`kernel_clone()` and `execve()`/`kernel_execve()` during Linux boot?
* **Simple answer:**
  * **`fork()` / `kernel_clone()` makes a new task (`new PID`)**: In `rest_init()`, `PID 0` calls `kernel_clone()` to create a second kernel thread with **`PID 1`** (`kernel_init`).
  * **`execve()` / `kernel_execve()` replaces the program inside the *same* task (`same PID`)**: At the end of `kernel_init()`, `PID 1` calls `kernel_execve()`. `PID 1` receives a new user-space `struct mm_struct`, loads the `/init` or `/sbin/init` binary, and transitions from a kernel thread into the first user-space process.

### Interview Q2: In `struct task_struct`, why is `.mm` set to `NULL` for kernel threads (`PID 0`, `PID 2`), and when does `PID 1` get a non-NULL `.mm`?
* **Simple answer:** `.mm` points to a task's **user-space virtual memory descriptor (`struct mm_struct`)**.
* **How kernel threads work (`.mm = NULL`):** Kernel threads (`PF_KTHREAD`) access only the shared kernel half of virtual memory, so they borrow `&init_mm` (or the previous task's page table) via `.active_mm`.
* **When `PID 1` gets `.mm != NULL`:** `PID 1` starts as a kernel thread (`.mm = NULL`) while running `kernel_init()`. When it calls `kernel_execve()`, `fs/exec.c` allocates a fresh `struct mm_struct` for its user-space code, data, and stack.

### Interview Q3: Why does `module_init()` work for both built-in drivers (`=y`) and loadable kernel modules (`=m`)?
* **Simple answer:** `<linux/module.h>` expands `module_init(fn)` differently depending on whether the driver is built into `vmlinux` (`=y`) or compiled as a `.ko` module (`=m`).
* **How it works:**
  * **When built-in (`=y`):** `module_init(fn)` becomes `device_initcall(fn)`, placing a pointer to `fn` in `.initcall6.init` so `do_initcalls()` calls it during boot after the bus cores are ready.
  * **When compiled as a module (`=m`):** `module_init(fn)` creates the module's `init_module` entry point, which runs when user space loads the `.ko` with `insmod` or `modprobe`.

### Interview Q4: What does the `__init` macro do on a driver's init function, and what happens if a runtime callback calls an `__init` function?
* **Simple answer:** `__init` places the function into an initialization-only code section (`.init.text`) that the kernel frees once initialization finishes.
* **Why Linux uses it:** For built-in code, `free_initmem()` reclaims `.init.text` right after `do_initcalls()` at boot; for loadable `.ko` modules, `do_init_module()` frees the module's init text right after `module_init` returns `0`.
* **What happens on a late call:** Calling an `__init` function from a runtime timer, interrupt, or `read()` callback jumps into freed or reused memory, causing a kernel Oops/page fault or executing overwritten memory.

### Interview Q5: During Linux boot, why does `rest_init()` create `PID 1` (`kernel_init`) first, and have it wait for `PID 2` (`kthreadd`) before running `do_initcalls()`?
* **Simple answer:**
  1. `kernel_init` is cloned **first** by `PID 0` so it gets process ID **`PID 1`**.
  2. `do_initcalls()` runs driver initialization code that creates background kernel threads and workqueues via `kthread_create()`—and **`kthreadd` (`PID 2`) is the daemon that services `kthread_create()` requests**.
* **Result:** Waiting on `kthreadd_done` guarantees `PID 2` is ready before any built-in driver requests a worker thread.

## 9. Corrections

- 2026-10-25: First publication of W01-L05.
- 2026-10-25 (revision 2): Applied lesson-review fixes (`LR1`–`LR4`).
- 2026-10-26 (revision 3): Expanded into the 6-scene boot movie with `struct task_struct` and Before/After diagrams.
- 2026-10-27 (revision 4): Polished into direct, positive story mode removing negative/defensive filler.
