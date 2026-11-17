# W01-L03: Privilege And Address Spaces — RISC-V To Arm

> **Source-reviewed:** Linux checkout `f0100363d`, 2026-10-23, tutor inspection.
> **Not runtime-verified:** the addresses and `read(3, 0x00401000, 8)` trace
> below are worked examples, not output from a physical board. No implementation
> file is created in Lesson 3 (Lesson 4 / `C01` is your first C implementation).

Previous: [W01-L02: System Calls And Waiting For Buffered Data](W01_L02_system_calls_and_waiting_for_data.md).

## 0. Know This First

In L01 and L02, your `recorder` app called `read(3, user_buf, 8)` and stepped
across the user/kernel boundary. Now we answer two questions that every embedded
interview tests:

1. **Privilege ("What badge am I wearing?"):** What stops a buggy user app from
   disabling interrupts or overwriting kernel page tables?
2. **Address Space ("Which map am I looking at?"):** When your app passes the
   pointer `user_buf = 0x00401000` into the kernel, why is `0x00401000` **still
   a Virtual Address (VA)** and **not** a Physical RAM Address (PA)?

| Term | Plain meaning with our Project A example |
| --- | --- |
| **Privilege Level / Mode** | A hardware badge inside the CPU that controls which instructions and registers current code is allowed to touch. |
| **`struct pt_regs`** | A struct saved on the thread's kernel stack when entering the kernel. It holds the saved general-purpose CPU registers (`a0..a7` on RISC-V, `x0..x30` on Arm64) and exception state so the kernel can read syscall arguments and write back the return value (`8`). *(FP/vector registers are saved separately.)* |
| **Virtual Address (VA)** | The "room number" a program uses in its pointers (like `user_buf = 0x00401000`). Two different processes can both have `0x00401000` pointing to completely different physical RAM! |
| **Physical Address (PA)** | The real hardware address on the RAM chip or peripheral bus (such as `0x81234000`). |
| **MMU & Page Tables** | **MMU (Memory Management Unit)** is the hardware translator on the CPU chip. **Page tables** are the lookup tables in RAM that tell the MMU which Virtual Page maps to which Physical Page and who is allowed to read/write it. |

```text
1. Privilege Badge vs. Address Map (Two Separate Things!):
   - Privilege Badge (U-mode / S-mode, or EL0 / EL1): "Am I allowed in this room?"
   - Address Map (Page Tables in MMU):                "Where is room 0x00401000 in physical RAM?"

2. Do NOT mix up the privilege names across our 4 platforms:
   +------------------------+--------------------+-----------------------+-----------------------+
   | Role                   | RISC-V (64-bit)    | Arm64 (64-bit SoCs)   | BeagleBone Black      |
   |                        | (my-linux)         | (Qualcomm/NVIDIA)     | (32-bit Armv7-A)      |
   +------------------------+--------------------+-----------------------+-----------------------+
   | User App (recorder)    | U-mode (User)      | EL0                   | User mode (USR)       |
   | Linux Kernel + Drivers | S-mode (Supervisor)| EL1                   | Supervisor mode (SVC) |
   | Hypervisor (if used)   | (H-extension)      | EL2                   | (Hyp mode on v7-VE)   |
   | Low-Level Firmware     | M-mode (OpenSBI)   | EL3 (TF-A / Monitor)  | (TrustZone Monitor)   |
   +------------------------+--------------------+-----------------------+-----------------------+
   * Note on STM32F446RE (Cortex-M4 MCU): Runs Zephyr/bare-metal using Thread mode & Handler mode
     (with an optional MPU, not a Linux virtual-memory MMU).
```

## 1. Goal

* **Before:** "Entering kernel mode turns pointers into physical addresses, and BeagleBone Black uses Arm64 `EL1`."
* **After:** Use the exact right mode names for **RISC-V (`U/S/M`)**, **Arm64 (`EL0/EL1/EL2/EL3`)**, **BeagleBone Black (`32-bit Armv7-A User/SVC`)**, and **STM32 (`Cortex-M Thread/Handler`)**, trace how `read(3, 0x00401000, 8)` passes arguments and return value `8` through `struct pt_regs`, and compute a Virtual-to-Physical page translation by hand.

## 2. Why Separate Privilege From Virtual Addresses?

Suppose your `recorder` app has a bug and tries to write to an invalid pointer:

1. **Without privilege separation:** Any app could execute privileged instructions to turn off the MMU or reprogram the SPI controller, crashing the whole system.
2. **Without memory protection and per-process address spaces:** Without memory protection (via an MMU or MPU), Process B could overwrite Process A's variables, and a `read(3, user_buf, 8)` call could trick the kernel into overwriting kernel memory! On Linux, the MMU provides both **per-process virtual address translation** and **page-level permission checks**.

**How Linux Protects Both:**
* Your app runs unprivileged (**U-mode** on RISC-V, **EL0** on Arm64, **User mode** on BBB).
* To call `read(3, 0x00401000, 8)`, it executes a controlled system-call instruction (`ecall` on RISC-V, `svc` on Arm).
* An ordinary `read()` only goes **one step down** into the OS kernel (`U -> S` or `EL0 -> EL1`) and returns (`S -> U` or `EL1 -> EL0`). It does **not** visit `M-mode` or `EL3` firmware unless the kernel specifically needs a firmware service.

## 3. Starting Snapshot: The Same `read(3, 0x00401000, 8)` on RISC-V vs. Arm64

When your C code calls `read(3, user_buf, 8)` with `user_buf = 0x00401000`, the C library (`libc`) loads those values into CPU registers before triggering the syscall instruction:

| What is being passed | Concrete value in our trace | RISC-V Linux register | Arm64 Linux register |
| --- | --- | --- | --- |
| **Syscall number ("read")** | `__NR_read` (`63`) | `a7` | `x8` |
| **Arg 1: File descriptor (`fd`)** | `3` | `a0 = 3` | `x0 = 3` |
| **Arg 2: User buffer VA (`buf`)** | `0x00401000` | `a1 = 0x00401000` | `x1 = 0x00401000` |
| **Arg 3: Byte count (`n`)** | `8` bytes | `a2 = 8` | `x2 = 8` |
| **Return value (bytes read)** | **`8` bytes** (overwrites Arg 1 slot!) | **`a0 = 8`** | **`x0 = 8`** |

*(Note: BeagleBone Black is 32-bit Armv7-A, which uses `r7` for the syscall number and `r0..r2` for the three arguments.)*

## 4. How the Kernel Code Actually Does This (Short Annotated Excerpt)

When the CPU traps from User mode into Kernel mode, low-level entry assembly saves the user thread's general-purpose registers (and trap state) into a `struct pt_regs` frame on the kernel stack. Notice how **both** RISC-V and Arm64 write the return value (`8`) into the **saved Arg 1 slot** of `pt_regs` so the user thread sees `8` when it returns to user space:

### Step A: RISC-V Saves Return Value `8` into `regs->a0` (`arch/riscv/include/asm/syscall.h`)

Source link: [arch/riscv/include/asm/syscall.h:99](../../../linux/arch/riscv/include/asm/syscall.h#L99)

```c
static inline void syscall_handler(struct pt_regs *regs, ulong syscall)
{
	syscall_t fn;

	/* 1. LOOK UP THE SYSCALL FUNCTION IN THE TABLE:
	 *    `syscall` came from register a7 (read). `fn` points to the kernel's
	 *    read syscall wrapper. */
	fn = sys_call_table[syscall];

	/* 2. RUN THE SYSCALL AND WRITE THE RESULT (8) INTO SAVED a0:
	 *    - Earlier in do_trap_ecall_u() (traps.c:335), the trap entry preserved
	 *      the incoming first argument (fd = 3) in `regs->orig_a0` and initialized
	 *      `regs->a0 = -ENOSYS`.
	 *    - fn(regs) reads `orig_a0` (3), `a1` (0x00401000), `a2` (8), runs
	 *      ksys_read(3, 0x00401000, 8), and returns 8.
	 *    - Storing 8 into regs->a0 means when trap-exit assembly restores
	 *      registers from `regs` and executes `sret`, user space sees a0 = 8! */
	regs->a0 = fn(regs);
}
```

### Step B: Arm64 Saves Return Value `8` into `regs->regs[0]` (`arch/arm64/include/asm/syscall.h`)

Source link: [arch/arm64/include/asm/syscall.h:51](../../../linux/arch/arm64/include/asm/syscall.h#L51)

```c
static inline void syscall_set_return_value(struct task_struct *task,
					    struct pt_regs *regs,
					    int error, long val)
{
	/* 1. IF AN ERROR OCCURRED, USE THE NEGATIVE KERNEL ERROR (e.g., -EAGAIN):
	 *    In our successful trace, error = 0 and val = 8 (bytes read). */
	if (error)
		val = error;

	/* 2. WRITE THE RESULT (8) INTO SAVED x0 (regs->regs[0]):
	 *    regs->regs[0] is the saved x0 slot on the kernel stack (NOT user_buf!).
	 *    When exception-return assembly restores `regs` and executes `eret`
	 *    back to EL0, the user thread receives x0 = 8. */
	regs->regs[0] = val;
}
```

## 5. Why Kernel Privilege Does NOT Turn a Virtual Address into a Physical Address

Look at `a1` / `x1` in our trace: `user_buf = 0x00401000`.
When the CPU enters `S-mode` or `EL1`, **the MMU is still turned ON!**

### Step 1: Tiny Hand Example (16-Byte Pages)
Before looking at 4 KiB (`4096-byte`) pages, see how the MMU splits an address using tiny **16-byte pages**:
* Suppose **Virtual Page `4`** is mapped in the page table to **Physical Page `9`**.
* What is the physical address of **byte offset `3`** inside Virtual Page `4`?
  * `Virtual Address  = (Page 4 * 16) + 3 = 67`
  * `Physical Address = (Page 9 * 16) + 3 = 147`
* **Rule:** The MMU replaces the **Page Number** (`4 -> 9`) using the page table, and copies the **Offset inside the page** (`+ 3`) unchanged!

### Step 2: Real 4 KiB (`0x1000`-Byte) Page Example for `user_buf`
In standard 4 KiB pages, the last 3 hex digits (`12 bits`, `0x000` to `0xfff`) are the **page offset**:

```text
Virtual Address (user_buf + 3):   0x00401_003
                                  |_____| |__|
                                     |     +---> Offset inside page = 0x003 (unchanged!)
                                     +---------> Virtual Page = 0x00401
                                                       |
                                           [MMU checks Page Table]
                                                       v
                                                 Physical Page = 0x81234
                                                       |
Physical RAM Address:             0x81234_003 <--------+
```

| Address Type | Example Value | Who uses it and why |
| --- | --- | --- |
| **User Virtual Address (User VA)** | `0x00401000` | Used by `recorder` in user space. Only valid for **this process's** page table (`TTBR0_EL1` on Arm64).Kernel must access it via `copy_to_user()`. |
| **Kernel Virtual Address (Kernel VA)** | `0xffff800082000000` | Used by kernel code & `kfifo` buffers (`TTBR1_EL1` on Arm64 / high half on RISC-V). User mode cannot read or write this range. |
| **Physical Address (PA)** | `0x81234000` | Real RAM chip address produced by the MMU after walking the page table. |
| **DMA / Bus Address (`dma_addr_t`)** | `0x81234000` (or IOMMU-mapped) | Address programmed into a hardware DMA controller (like STM32 DMA or BBB EDMA). Never pass a raw Virtual Address to a DMA register! |

## 6. Control Flow

### (a) One-Line Call Chain

```text
recorder read(3, 0x00401000, 8) [U-mode / EL0]
  -> [syscall trap saves registers into struct pt_regs on kernel stack]
  -> syscall_handler / el0_svc [S-mode / EL1]
  -> ksys_read -> iio_buffer_read -> kfifo_to_user(0x00401000, 8)
  -> regs->a0 = 8 (RISC-V) or regs->regs[0] = 8 (Arm64)
  -> [trap return restores pt_regs] -> recorder receives 8 [U-mode / EL0]
```

### (b) Step Table (Following `read(3, 0x00401000, 8)` Across the Privilege Boundary)

| # | Where (`file : function`) | Goal / state change | Concrete data in -> out | Mode |
| --- | --- | --- | --- | --- |
| 1 | `recorder` / `libc` | Put syscall number & args in CPU registers | `fd = 3`, `buf = 0x00401000`, `n = 8` | `U-mode` / `EL0` |
| 2 | CPU + kernel entry assembly | Switch privilege and save registers to `pt_regs` | Live registers -> `regs` on kernel stack | `U -> S` / `EL0 -> EL1` |
| 3 | `fs/read_write.c` + IIO buffer | Copy 2 scans (`8 B`) into user VA `0x00401000` | Kernel `kfifo` -> User VA `0x00401000..007` | `S-mode` / `EL1` |
| 4 | `asm/syscall.h` helper | Store return byte count `8` into saved Arg 0 slot | `ret = 8` -> `regs->a0` or `regs->regs[0]` | `S-mode` / `EL1` |
| 5 | Kernel exit assembly | Restore registers from `pt_regs` and return | `a0 = 8` or `x0 = 8` back in user thread | `S -> U` / `EL1 -> EL0` |

### (c) Privilege Timeline

```text
User Mode (U-mode / EL0)          Kernel Mode (S-mode / EL1)        Firmware (M-mode / EL3)
------------------------          --------------------------        -----------------------
a0/x0 = 3
a1/x1 = 0x00401000
a2/x2 = 8
trigger syscall  ---------------> save registers into pt_regs
                                  run ksys_read -> iio_buffer_read
                                  copy 8 bytes to VA 0x00401000     (Not visited during an
                                  write 8 into regs->a0 / regs[0]    ordinary IIO read!)
receive a0/x0 = 8  <------------- restore pt_regs & return
```

## 7. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
Think of **privilege mode** as your security badge, and a **virtual address** as a room number written on your personal floor map. When your program calls `read()`, it walks through a guarded door (`syscall`) where the CPU puts on the **Kernel badge** (`S-mode` on RISC-V, `EL1` on Arm64, `SVC` mode on BeagleBone Black), saves your registers in a notepad (`struct pt_regs`), copies the 8 bytes of sensor data, writes `8` onto the notepad's return slot (`a0` or `x0`), and hands you back your **User badge** (`U-mode` or `EL0`). Putting on the Kernel badge does not change the fact that room `0x00401000` is still a virtual room number translated by the MMU!

---

### Interview Q1: How do RISC-V privilege modes map to Arm64, BeagleBone Black (Armv7-A), and STM32 (Cortex-M4)?
* **Simple answer:** They solve the same protection problem using different architecture names:
  * **RISC-V (64-bit Linux):** App in **U-mode**, Kernel in **S-mode**, Firmware (OpenSBI) in **M-mode**.
  * **Arm64 (64-bit Linux):** App in **EL0**, Kernel in **EL1**, Hypervisor in **EL2**, Secure Firmware (TF-A) in **EL3**.
  * **BeagleBone Black (32-bit Armv7-A Cortex-A8):** App in **User mode**, Kernel in **Supervisor (`SVC`) mode** (not `EL0`/`EL1`!).
  * **STM32F446RE (Cortex-M4 RTOS/MCU):** Normal code in **Thread mode**, interrupts/exceptions in **Handler mode** (no Linux MMU).

### Interview Q2: Does an application's `read()` system call go all the way down to M-mode (RISC-V) or EL3 (Arm64)?
* **Simple answer:** **No.** An ordinary Linux system call only transitions **one level** from User mode into the OS Kernel (`U-mode -> S-mode` on RISC-V, or `EL0 -> EL1` on Arm64) and returns directly back to User mode.
* **When M-mode / EL3 runs:** The kernel only calls down into M-mode (via SBI `ecall`) or EL3 (via `smc`) when it needs a platform-firmware service such as CPU core power-on, system reset, or secure monitor operations.

### Interview Q3: How do syscall arguments go in and how does the return value come back out in Linux?
* **Simple answer:** Through CPU registers saved in **`struct pt_regs`** on the kernel stack.
* **How it works:** Before the syscall instruction, user space puts the syscall number (`a7` on RISC-V, `x8` on Arm64) and arguments (`a0..a2` or `x0..x2`) in registers. Kernel entry assembly saves those registers into `struct pt_regs`. When the C syscall handler finishes (`ret = 8`), it writes `8` into `regs->a0` (RISC-V) or `regs->regs[0]` (Arm64) so trap-exit assembly restores `8` into the caller's return register.

### Interview Q4: Once the CPU is in Kernel mode (`S-mode` or `EL1`), can a driver pass a user pointer or `kmalloc` virtual pointer directly to a hardware DMA register?
* **Simple answer:** **Never.** Kernel privilege changes **what the CPU is allowed to execute**, not how memory addresses are wired on the hardware bus.
* **Why:** While Linux is running, the **MMU is active**, so both user pointers (`0x00401000`) and kernel pointers are **Virtual Addresses**. A hardware DMA engine sits on the bus outside the CPU's MMU and requires a mapped **DMA/physical bus address (`dma_addr_t`)**, while user pointers must be accessed by the CPU via `copy_to_user()` / `copy_from_user()`.

## 8. Corrections

- 2026-10-23: First publication of W01-L03.
- 2026-10-23 (revision 2): Rewrote in the learner-approved Feynman format: added the 4-platform privilege comparison table (RISC-V, Arm64, BBB Armv7-A, STM32 Cortex-M4), visual `0x00401003 -> 0x81234003` page-offset diagram, inline C comments in `syscall_handler()` / `syscall_set_return_value()`, and spoken-English interview Q&As. Next step is Lesson 4 (`W01_L04_c01_checked_packet_size.md`).
- 2026-10-23 (revision 3): Applied technical review fixes: clarified that `pt_regs` saves general-purpose registers and exception state (not FP/vector registers), distinguished virtual address translation from memory protection (MMU vs. MPU), and traced `regs->orig_a0 = 3` and `regs->a0 = -ENOSYS` prior to `regs->a0 = fn(regs)` in RISC-V trap dispatch.

