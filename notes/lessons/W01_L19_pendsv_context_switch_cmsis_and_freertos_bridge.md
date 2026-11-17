# W01-L19: Bare-Metal `PendSV` Context Switching, Official CMSIS Headers, And FreeRTOS Tasks (`P1` Bridge)

> **Scope & verification status:** `Document/source-reviewed, not build-verified: local Arm DUI0553B, STM32 RM0390 Rev 6, UM1724 Rev 14, project CMSIS headers (Drivers/CMSIS/Device/ST/STM32F4xx/Include/stm32f446xx.h, Drivers/CMSIS/Include/core_cm4.h), and STM32F446RETX_FLASH.ld on 2026-11-03.`
> **Build model (`RegistersDebug`):** Compiles `Core/Src/pendsv_cmsis_bridge_app.c` with `#include "stm32f446xx.h"` (official CMSIS register headers, **zero HAL**) and links with `STM32F446RETX_FLASH.ld` and `-nostartfiles`.

Previous: [Bare-metal Cortex-M4 privilege (`CONTROL`), `PSP` vs. `MSP`, `MPU` memory protection, `SVC`, and `MemManage` faults](W01_L18_cortexm4_mpu_privilege_psp_svc_and_memmanage.md).

---

## The Core Idea: How One CPU Runs Two Tasks (`PUSH` To Pause, `POP` To Resume)

Our STM32F446RE has **one** Cortex-M4 CPU core with **one** set of 16 registers (`R0..R12, SP, LR, PC, xPSR`), but we want to run **two independent `for (;;)` functions**:
* **Task 0 (`task_button_check`):** Checks if button `B1` (`PC13`) was pressed and changes `blink_period_ms` (`500 ms <-> 150 ms`).
* **Task 1 (`task_led_blink`):** Toggles `LD2` (`PA5`) every `blink_period_ms` milliseconds.

To share one CPU between two tasks, each task gets its own stack in RAM (`task0_stack` and `task1_stack`) using the **Process Stack Pointer (`PSP`)**, while `main()` and interrupt handlers use the **Main Stack Pointer (`MSP`)**:

1. **To pause a running task (Task 0):** We **PUSH (save)** all 16 of Task 0's current CPU registers onto `task0_stack`, and save Task 0's stack pointer address in `g_tcbs[0].psp`.
2. **To run the other task (Task 1):** We load Task 1's saved stack pointer from `g_tcbs[1].psp` and **POP (restore)** all 16 of Task 1's registers from `task1_stack` into the CPU.

### Why Are the 16 Registers Split Into 8 Hardware Words + 8 Software Words?

Whenever an interrupt occurs while a task is running on `PSP`:
* **Top 8 Words (`R0, R1, R2, R3, R12, LR, Return_PC, xPSR`) — Handled by CPU Hardware:**
  * On interrupt entry, **CPU hardware automatically pushes** these 8 registers onto the task's `PSP`.
  * On interrupt exit (`bx lr` with `LR = 0xFFFFFFFD`), **CPU hardware automatically pops** these 8 registers from the task's `PSP`.
* **Bottom 8 Words (`R4, R5, R6, R7, R8, R9, R10, R11`) — Handled by Our Software:**
  * CPU hardware does **not** touch `R4..R11`.
  * So inside `PendSV_Handler`, **our assembly code pushes `R4..R11`** onto the outgoing task's `PSP` (`stmdb r0!, {r4-r11}`) and **pops `R4..R11`** from the incoming task's `PSP` (`ldmia r0!, {r4-r11}`).

```text
Normal Context Switch When Task 0 Is Already Running:

  [Task 0 Running]
         |
         | 1. Interrupt enters PendSV_Handler:
         |    - Hardware PUSHES {R0..R3, R12, LR, Return_PC, xPSR} onto task0_stack (8 words)
         |    - Software PUSHES {R4..R11} onto task0_stack (8 words) -> saves address in g_tcbs[0].psp
         v
  [Switch Stack Pointer: r0 = g_tcbs[1].psp (task1_stack)]
         |
         | 2. PendSV_Handler exits to Task 1:
         |    - Software POPS {R4..R11} from task1_stack (8 words)
         |    - Hardware POPS {R0..R3, R12, LR, Return_PC, xPSR} from task1_stack (8 words)
         v
  [Task 1 Running!]
```

---

## Step 1 — Give Each Task Its Own Stack (`task0_stack`, `task1_stack`) And Pre-Push 16 Initial Registers (`task_stack_init`)

### Why Do We Pre-Push 16 Words onto Each Stack Before Starting?

Before the scheduler starts in `main()`, **neither Task 0 nor Task 1 has ever run yet**.
If we want to start Task 0 (or switch into Task 1 for the first time) by **popping** 16 registers off its stack, those 16 registers **must already be sitting on its stack**!

So in `main()`, we call `task_stack_init()` to **push 16 initial words (`64 bytes`)** onto the top of `task0_stack` and `task1_stack` by hand in C (`*(--sp) = ...`). That makes a brand-new task's stack look **identical** to a task that was already paused by an interrupt.

### What Is the Difference Between `Return_PC` (`sp[14]`) and `LR` (`sp[13]`)?

Beginners often confuse `Return_PC` and `LR` because both hold code addresses. Here is the exact difference:
* **`Return_PC` (`sp[14]`, loaded into CPU `PC` / `R15`):**
  * **Where the CPU jumps when the interrupt handler exits (`bx lr` with `0xFFFFFFFD`) to start or resume the task.**
  * When a running task is interrupted between two lines of C code, hardware saves the address of the **next instruction inside the task** into `Return_PC` so the task resumes at that exact line later.
  * When a task is brand-new (`task_stack_init`), we set `Return_PC = task_fn` (`task_button_check` for Task 0, `task_led_blink` for Task 1) so the very first time the task is popped off the stack, the CPU jumps to the first line of `task_fn`!
* **`LR` (`sp[13]`, loaded into the task's `LR` / `R14`):**
  * **Where the task function itself would jump if the task function ever executed `return;` in C.**
  * In normal C code, when `main()` calls `foo()`, `LR` holds the address back in `main()` where `foo()` returns when `foo()` finishes.
  * An RTOS task is an infinite loop (`for (;;)`) that must **never** execute `return;`. We set `sp[13]` (`LR`) to `task_exit_trap` so that if someone accidentally writes `return;` inside a task, the CPU jumps safely to `task_exit_trap()` and stops instead of crashing at a random address.

### What Every One of the 16 Words (`sp[15]` Down to `sp[0]`) Contains

Stacks on Cortex-M4 grow **downward** from high memory address (`&task_stack[256]`) toward low memory address (`&task_stack[0]`):

| Stack Slot | Register Restored | Initial Value Written by `task_stack_init()` | Why It Has This Value |
| --- | --- | --- | --- |
| **`sp[15]`** (`+60`) | **`xPSR`** (Status Register) | `0x01000000UL` | **Bit `24` (`T` bit) MUST be `1`** so the CPU runs in Thumb mode (if `0`, the CPU faults with `UsageFault`). All condition flags (`N,Z,C,V`) start at `0`. |
| **`sp[14]`** (`+56`) | **`Return_PC`** (`PC` / `R15`) | `(uint32_t)task_fn & ~1UL` | **Task entry address** (`task_button_check` or `task_led_blink`). Hardware pops this into `PC` so the task starts at the beginning of its function. |
| **`sp[13]`** (`+52`) | **`LR`** (`R14`) | `(uint32_t)task_exit_trap` | **Task return trap.** Loaded into `LR` in case the task function ever accidentally executes `return;`. |
| **`sp[12]`** (`+48`) | **`R12`** (Scratch Register) | `0x12121212UL` | Temporary scratch register. A new task does not care about `R12`'s starting value; `0x12121212UL` is a debug pattern so you can see `R12` in a memory dump. |
| **`sp[11]`** (`+44`) | **`R3`** (4th Argument / Scratch) | `0x03030303UL` | 4th function argument register (unused at task start; `0x03030303UL` debug marker). |
| **`sp[10]`** (`+40`) | **`R2`** (3rd Argument / Scratch) | `0x02020202UL` | 3rd function argument register (unused at task start; `0x02020202UL` debug marker). |
| **`sp[9]`** (`+36`) | **`R1`** (2nd Argument / Scratch) | `0x01010101UL` | 2nd function argument register (unused at task start; `0x01010101UL` debug marker). |
| **`sp[8]`** (`+32`) | **`R0`** (1st Argument) | `0x00000000UL` | **1st function argument register.** (In FreeRTOS `xTaskCreate`, `pvParameters` is placed here so the task receives its argument in `R0`; our task takes `void`, so we put `0`.) |
| **`sp[7..0]`** (`+28..+0`) | **`R11` down to `R4`** | `0x11111111UL` down to `0x04040404UL` | **Callee-saved local variable registers (`R4..R11`).** Popped by software (`ldmia r0!, {r4-r11}`). `sp[0]` holds `R4`, and `task_stack_init()` returns `&sp[0]` to store in `g_tcbs[i].psp`. |

```text
Task Stack in RAM After `task_stack_init(&task0_stack[256], task_button_check)`:

  High Address: &task0_stack[256] (Top of 1 KB stack)
  +-------------------------------------------------------------------+
  | sp[15]: xPSR      = 0x01000000UL        (Bit 24 T = 1 for Thumb)  | \
  | sp[14]: Return_PC = &task_button_check  (Loaded into PC on exit)  |  |
  | sp[13]: LR        = &task_exit_trap     (If task ever returns)    |  | Top 8 Words:
  | sp[12]: R12       = 0x12121212UL        (Scratch register)        |  | Popped by CPU HARDWARE
  | sp[11]: R3        = 0x03030303UL        (4th arg / scratch)       |  | when `bx lr` runs with
  | sp[10]: R2        = 0x02020202UL        (3rd arg / scratch)       |  | `LR = 0xFFFFFFFD`
  | sp[ 9]: R1        = 0x01010101UL        (2nd arg / scratch)       |  |
  | sp[ 8]: R0        = 0x00000000UL        (1st arg to task)         | /
  +-------------------------------------------------------------------+ <-- `r0` points here after `ldmia`
  | sp[ 7]: R11       = 0x11111111UL                                  | \
  | sp[ 6]: R10       = 0x10101010UL                                  |  |
  | sp[ 5]: R9        = 0x09090909UL                                  |  | Bottom 8 Words:
  | sp[ 4]: R8        = 0x08080808UL                                  |  | Popped by SOFTWARE
  | sp[ 3]: R7        = 0x07070707UL                                  |  | `ldmia r0!, {r4-r11}`
  | sp[ 2]: R6        = 0x06060606UL                                  |  |
  | sp[ 1]: R5        = 0x05050505UL                                  |  |
  | sp[ 0]: R4        = 0x04040404UL                                  | /
  +-------------------------------------------------------------------+ <-- `g_tcbs[0].psp` points HERE!
  Low Address (Free stack space below sp[0])
```

### Step 1 Code: TCBs, Stacks, and `task_stack_init()`

```c
/* Task Control Block (TCB): stores the task's saved stack pointer (PSP): */
typedef struct {
    uint32_t *psp;                     /* Points to sp[0] (R4) when task is paused */
} tcb_t;

static volatile tcb_t    g_tcbs[2];
static volatile uint32_t g_current_task = 0U;

/* Separate 1 KB (256-word) Process Stacks for Task 0 and Task 1 (8-byte aligned): */
static uint32_t task0_stack[256] __attribute__((aligned(8)));
static uint32_t task1_stack[256] __attribute__((aligned(8)));

/* Pre-pushes the 16 initial register words onto a brand-new task stack: */
static uint32_t *task_stack_init(uint32_t *stack_top, void (*task_fn)(void))
{
    uint32_t *sp = (uint32_t *)((uintptr_t)stack_top & ~7UL); /* Ensure 8-byte alignment */

    /* 1. Push Top 8 Words (Hardware Exception Frame: R0..R3, R12, LR, Return_PC, xPSR): */
    *(--sp) = 0x01000000UL;                          /* sp[15]: xPSR (Bit 24 T = 1 for Thumb)  */
    *(--sp) = ((uint32_t)(uintptr_t)task_fn) & ~1UL; /* sp[14]: Return_PC (task start address) */
    *(--sp) = (uint32_t)(uintptr_t)task_exit_trap;   /* sp[13]: LR (trap if task ever returns) */
    *(--sp) = 0x12121212UL;                          /* sp[12]: R12 (scratch)                  */
    *(--sp) = 0x03030303UL;                          /* sp[11]: R3  (4th arg / scratch)        */
    *(--sp) = 0x02020202UL;                          /* sp[10]: R2  (3rd arg / scratch)        */
    *(--sp) = 0x01010101UL;                          /* sp[ 9]: R1  (2nd arg / scratch)        */
    *(--sp) = 0x00000000UL;                          /* sp[ 8]: R0  (1st arg = 0)              */

    /* 2. Push Bottom 8 Words (Software Context Frame: R4..R11): */
    *(--sp) = 0x11111111UL;                          /* sp[ 7]: R11                            */
    *(--sp) = 0x10101010UL;                          /* sp[ 6]: R10                            */
    *(--sp) = 0x09090909UL;                          /* sp[ 5]: R9                             */
    *(--sp) = 0x08080808UL;                          /* sp[ 4]: R8                             */
    *(--sp) = 0x07070707UL;                          /* sp[ 3]: R7                             */
    *(--sp) = 0x06060606UL;                          /* sp[ 2]: R6                             */
    *(--sp) = 0x05050505UL;                          /* sp[ 1]: R5                             */
    *(--sp) = 0x04040404UL;                          /* sp[ 0]: R4 <- g_tcbs[i].psp points here*/

    return sp;                                       /* Returns address of sp[0]               */
}
```

---

## Step 2 — Launch The First Task (`Task 0`) Using `svc #0` -> `SVC_Handler` (`#11`)

### Why Does `SVC_Handler` Only `POP` From `task0_stack` Instead of Pushing?

This is the most important question when reading an RTOS startup:
1. **What was running before `svc #0`?**
   * `main()` was running using the **Main Stack Pointer (`MSP`)** just to initialize the clocks, GPIO pins, and task stacks.
   * Once `main()` starts the scheduler (`svc #0`), **`main()` is finished forever**—we will never return to `main()`. Therefore, we **do NOT want to push/save `main()`'s registers** onto `task0_stack`!
2. **Wait—what did the CPU hardware push when `main()` executed `svc #0`?**
   * Because `main()` was using **`MSP`**, when `main()` executed `svc #0`, the CPU hardware pushed `main()`'s 8 registers onto **`MSP`** (not `PSP`).
   * Inside `SVC_Handler`, we reclaim that space on `MSP` by resetting `MSP` back to `&_estack` (`0x20020000`) so future interrupts have the full Main Stack.
3. **Why can `SVC_Handler` immediately `POP` Task 0?**
   * Because in **Step 1 (`task_stack_init`)**, we **already pushed** Task 0's 16 initial registers (`sp[0..15]`) onto `task0_stack`!
   * So `SVC_Handler` simply performs the **POP (restore) half** of a context switch on `task0_stack`:
     * **Sub-step A:** Load `r0 = g_tcbs[0].psp` (which points to `sp[0]` = `R4` on `task0_stack`).
     * **Sub-step B (`ldmia r0!, {r4-r11}`):** Software **pops the bottom 8 words (`sp[0..7]` -> `R4..R11`)** from `task0_stack` into the CPU registers. The `!` adds `32 bytes` (`8 words`) to `r0`, so `r0` now points to `sp[8]` (`R0`, the bottom of the top 8 hardware words).
     * **Sub-step C (`msr psp, r0`):** Write `r0` (`&sp[8]`) into the CPU's `PSP` register.
     * **Sub-step D (`ldr lr, =0xFFFFFFFD` and `bx lr`):** Normally, `SVC_Handler` entered from `main()` (which was on `MSP`, so `LR` was `0xFFFFFFF9`). By changing `LR` to **`0xFFFFFFFD`** (`EXC_RETURN` bit `2 = 1`) before executing `bx lr`, we tell the CPU hardware: *"Do not return to `main()` on `MSP`! Instead, switch Thread mode to **`PSP`** and **pop the top 8 words (`sp[8..15]` -> `R0, R1, R2, R3, R12, LR, Return_PC, xPSR`) from `PSP` (`task0_stack`)**!"*
     * **Sub-step E:** Hardware pops `sp[14]` (`Return_PC = task_button_check`) into `PC` and `sp[15]` (`xPSR = 0x01000000`) into `xPSR`, and **Task 0 starts running at line 1 of `task_button_check()`!**

```text
What `SVC_Handler` Does on `task0_stack` to Launch Task 0 for the First Time:

  sp[15..8]: {R0..R3, R12, LR, Return_PC, xPSR}  <-- 3. `bx lr` (0xFFFFFFFD) makes CPU HARDWARE
  ---------------------------------------------         pop these 8 words from PSP -> starts Task 0!
  sp[ 7..0]: {R4..R11}                           <-- 2. `msr psp, r0` sets PSP to sp[8]
  ^                                              <-- 1. `ldmia r0!, {r4-r11}` POPS R4..R11
  |                                                     and moves r0 up to sp[8]
  `g_tcbs[0].psp` starts here at sp[0]
```

### Step 2 Code: `SVC_Handler` Launching Task 0

```c
__attribute__((used))
uint32_t *svc_first_task_psp(void)
{
    return g_tcbs[0].psp;                   /* Returns &task0_stack sp[0] (where R4 is stored) */
}

/* Exception #11 (SVCall): Runs once at `svc #0` to pop Task 0 from `task0_stack` and start it: */
__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        "push  {r3, lr}                 \n" /* Save LR on MSP before calling C helper           */
        "bl    svc_first_task_psp       \n" /* R0 = g_tcbs[0].psp (points to sp[0] = R4)        */
        "pop   {r3, lr}                 \n"
        "ldmia r0!, {r4-r11}            \n" /* 1. Software POPS sp[0..7] into R4..R11 (R0+=32)  */
        "msr   psp, r0                  \n" /* 2. Set PSP = R0 (now pointing at sp[8] = R0)     */
        "ldr   r1, =_estack             \n" /* 3. Reset MSP back to top of RAM (0x20020000)     */
        "msr   msp, r1                  \n" /*    (discards main()'s old stack frame on MSP)    */
        "movs  r1, #0                   \n" /* 4. Clear BASEPRI = 0 so all interrupts are live  */
        "msr   basepri, r1              \n"
        "isb   0xF                      \n"
        "ldr   lr, =0xFFFFFFFD          \n" /* 5. Set LR = 0xFFFFFFFD (Return to Thread on PSP) */
        "bx    lr                       \n" /* 6. Hardware POPS sp[8..15] from PSP -> Task 0!   */
    );
}
```

---

## Step 3 — Request A Context Switch By Setting `PENDSVSET` (`SCB->ICSR` Bit `28`)

### Why Do Interrupts and Tasks Trigger `PendSV` (`#14`, Priority `15`) to Switch Tasks?

Once Task 0 is running, we want to switch between Task 0 and Task 1 whenever:
* `SysTick_Handler` fires (every `1 ms`, Priority `1`),
* `EXTI15_10_IRQHandler` fires (when button `B1` is pressed, Priority `5`), or
* A task finishes its check and wants to yield the CPU to the other task.

Why don't we switch `PSP` directly inside `SysTick_Handler`?
* Suppose `EXTI15_10_IRQHandler` (Priority `5`) is halfway through running, and `SysTick_Handler` (Priority `1`, higher urgency) interrupts it.
* If `SysTick_Handler` tried to switch to a Thread-mode task while `EXTI15_10_IRQHandler` was still unfinished underneath, the Cortex-M4 hardware would fault (`UsageFault` `INVPC`)!
* Instead, we give **`PendSV_Handler` (`#14`) Priority `15` (the lowest possible urgency)**.
* Whenever `SysTick`, `EXTI15_10`, or a task wants a context switch, it simply writes **`SCB->ICSR = SCB_ICSR_PENDSVSET_Msk` (`1UL << 28`)**.
* Because `PendSV` has Priority `15`, the CPU waits until `SysTick` (`Priority 1`) and `EXTI15_10` (`Priority 5`) have completely finished, and then runs `PendSV_Handler` to switch tasks safely!

```text
Priority 0  (Highest): SVCall (#11)      -> Runs once at startup (`svc #0`) to launch Task 0
Priority 1:            SysTick (#15)     -> Increments `app_ms` every 1 ms & sets PENDSVSET
Priority 5:            EXTI15_10 (IRQ40) -> Latches button press & sets PENDSVSET
Priority 15 (Lowest):  PendSV (#14)      -> Waits until all ISRs finish, then switches PSP!
```

### Step 3 Code: Requesting `PendSV` in `SysTick_Handler` and `EXTI15_10_IRQHandler`

```c
void SysTick_Handler(void)
{
    ++app_ms;                               /* Advance 1 ms tick counter                      */
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;     /* Set bit 28: request PendSV (#14) context switch*/
}

void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & B1_MASK) != 0U) {
        EXTI->PR       = B1_MASK;           /* Clear EXTI13 pending flag (Write-1-to-Clear)   */
        b1_irq_pending = 1U;                /* Tell Task 0 that button B1 was pressed         */
        ++b1_irq_count;
        SCB->ICSR      = SCB_ICSR_PENDSVSET_Msk; /* Request PendSV (#14) context switch       */
        __DSB();
    }
}
```

---

## Step 4 — Switch Tasks Inside `PendSV_Handler` (`#14`) (`PUSH` Outgoing -> Swap TCB -> `POP` Incoming)

Now trace what happens step by step when **Task 0 is running on `PSP`** and `PendSV_Handler` runs:

1. **Step 4A — Hardware `PUSH` (Saves Outgoing Task 0's Top 8 Words onto `task0_stack`):**
   * At the exact moment `PendSV_Handler` is entered from Task 0, **Cortex-M4 hardware automatically pushes Task 0's live `{R0, R1, R2, R3, R12, LR, Return_PC, xPSR}`** (`8 words = 32 bytes`) onto Task 0's `PSP` (`task0_stack`), where `Return_PC` is the exact instruction in `task_button_check()` where Task 0 was paused.
   * Hardware then switches the CPU's active stack pointer to `MSP` for Handler mode and puts `0xFFFFFFFD` into `LR`.
2. **Step 4B — Software `PUSH` (Saves Outgoing Task 0's Bottom 8 Words `R4..R11` onto `task0_stack`):**
   * Inside `PendSV_Handler`:
     * `mrs r0, psp` copies Task 0's current `PSP` (pointing at `sp[8]` = `R0`) into `r0`.
     * `stmdb r0!, {r4-r11}` subtracts `32 bytes` (`8 words`) from `r0` and **pushes Task 0's live `R4..R11`** into `sp[0..7]` on `task0_stack`.
     * Now **all 16 of Task 0's registers** are saved on `task0_stack`, and `r0` points to `sp[0]` (`R4`)!
3. **Step 4C — Swap TCB Pointer (`pendsv_switch_context`):**
   * `pendsv_switch_context(r0)` stores `r0` into `g_tcbs[0].psp` (so Task 0's stack position is remembered), switches `g_current_task` from `0` to `1`, and returns `g_tcbs[1].psp` in `r0` (pointing to `sp[0]` = `R4` on `task1_stack`).
4. **Step 4D — Software `POP` (Restores Incoming Task 1's Bottom 8 Words `R4..R11` from `task1_stack`):**
   * `ldmia r0!, {r4-r11}` **pops Task 1's `R4..R11`** from `sp[0..7]` on `task1_stack` into the CPU registers, and adds `32 bytes` to `r0` so `r0` points to `sp[8]` (`R0`) on `task1_stack`.
   * `msr psp, r0` updates the CPU's `PSP` register to point to `sp[8]` on `task1_stack`.
5. **Step 4E — Hardware `POP` (Restores Incoming Task 1's Top 8 Words from `task1_stack`):**
   * `bx lr` (with `LR = 0xFFFFFFFD`) tells the CPU hardware to **pop `{R0, R1, R2, R3, R12, LR, Return_PC, xPSR}` from `PSP` (`sp[8..15]` on `task1_stack`)** and resume **Task 1** in Thread mode!

```text
          (1) Outgoing Task 0 (task0_stack)      (2) PendSV_Handler (on MSP)           (3) Incoming Task 1 (task1_stack)
+----------------------------------------+   +-------------------------------------+   +------------------------------------+
| 4A. Hardware already PUSHED {R0..xPSR} |-->| 4B. mrs r0, psp                     |-->| 4D. ldmia r0!, {r4-r11}            |
|     onto task0_stack (sp[8..15])       |   |     stmdb r0!, {r4-r11} (PUSH R4-11)|   |     (Software POPS Task 1 R4..R11) |
|                                        |   | 4C. g_tcbs[0].psp = r0              |   |     msr psp, r0                    |
|                                        |   |     r0 = g_tcbs[1].psp              |   | 4E. bx lr (0xFFFFFFFD):            |
|                                        |   |                                     |   |     Hardware POPS Task 1 {R0..xPSR}|
+----------------------------------------+   +-------------------------------------+   +------------------------------------+
```

### Step 4 Code: `pendsv_switch_context()` and `PendSV_Handler()`

```c
__attribute__((used))
uint32_t *pendsv_switch_context(uint32_t *current_psp)
{
    uint32_t cur    = g_current_task;
    g_tcbs[cur].psp = current_psp;          /* 4C. Save outgoing task's PSP (points to R4)   */

    uint32_t next   = (cur == 0U) ? 1U : 0U;/* Select the other task (0 -> 1, or 1 -> 0)     */
    g_current_task  = next;
    ++context_switch_count;

    return g_tcbs[next].psp;                /* Return incoming task's saved PSP (points to R4)*/
}

/* Exception #14 (PendSV, Priority 15): Saves outgoing task and restores incoming task: */
__attribute__((naked))
void PendSV_Handler(void)
{
    __asm volatile (
        "mrs   r0, psp                  \n" /* 4B. R0 = outgoing task's PSP (at sp[8] = R0)  */
        "isb   0xF                      \n"
        "stmdb r0!, {r4-r11}            \n" /*     Software PUSHES outgoing R4..R11 (R0-=32) */
        "push  {r3, lr}                 \n" /*     Save EXC_RETURN (0xFFFFFFFD) on MSP       */
        "cpsid i                        \n" /* 4C. Disable IRQs briefly during TCB swap      */
        "bl    pendsv_switch_context    \n" /*     Save outgoing PSP; R0 = incoming PSP      */
        "cpsie i                        \n"
        "pop   {r3, lr}                 \n" /*     Restore EXC_RETURN (0xFFFFFFFD) into LR   */
        "ldmia r0!, {r4-r11}            \n" /* 4D. Software POPS incoming R4..R11 (R0+=32)   */
        "msr   psp, r0                  \n" /*     Update PSP to incoming task's sp[8]       */
        "isb   0xF                      \n"
        "bx    lr                       \n" /* 4E. Hardware POPS incoming {R0..xPSR} -> Run! */
    );
}
```

---

## Step 5 — The Two Simple Task Functions (`Task 0` & `Task 1`) And `main()`

To keep the task code easy to read so you can focus on context switching:
1. **`Task 0` (`task_button_check` running on `task0_stack`):**
   * Checks if `b1_irq_pending != 0` (set by `EXTI15_10_IRQHandler` when button `B1` is pressed) and at least `200 ms` have passed since the last accepted press (`(app_ms - last_press_ms) >= 200U`, simple cooldown debounce).
   * Toggles `blink_period_ms` between `500 ms` and `150 ms`, increments `accepted_press_count`, and sets `SCB->ICSR = SCB_ICSR_PENDSVSET_Msk` to switch to Task 1.
2. **`Task 1` (`task_led_blink` running on `task1_stack`):**
   * Checks if `(app_ms - last_toggle_ms) >= blink_period_ms`.
   * Toggles `LD2` (`PA5`) using `GPIOA->BSRR`, and sets `SCB->ICSR = SCB_ICSR_PENDSVSET_Msk` to switch back to Task 0.
3. **`main()`:**
   * Initializes the `84 MHz` clock, `PA5`/`PC13` GPIO pins, `EXTI13 -> IRQ 40`, `1 ms` `SysTick`, and the 4 exception priorities (`SVCall = 0`, `SysTick = 1`, `EXTI15_10 = 5`, `PendSV = 15`).
   * Calls `task_stack_init()` to pre-push the 16 registers onto `task0_stack` and `task1_stack`.
   * Sets `BASEPRI = (1U << 4)` (so `SysTick` at Priority `1` cannot trigger `PendSV` before `SVC_Handler` loads `PSP`), enables interrupts, and executes `svc #0` to launch Task 0!

### Step 5 Code: `task_button_check()`, `task_led_blink()`, and `main()`

```c
/* TASK 0 (Runs on `task0_stack` via PSP): Checks B1 press event and toggles blink speed: */
static void task_button_check(void)
{
    uint32_t last_press_ms = 0U;

    for (;;) {
        ++task0_loop_count;
        if (b1_irq_pending != 0U) {
            b1_irq_pending = 0U;
            uint32_t now_ms = app_ms;
            if ((now_ms - last_press_ms) >= 200U) { /* Simple 200 ms press cooldown */
                last_press_ms = now_ms;
                ++accepted_press_count;
                blink_period_ms = (blink_period_ms == 500U) ? 150U : 500U;
            }
        }

        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;         /* Yield to Task 1 via PendSV (#14) */
        __DSB();
        __ISB();
    }
}

/* TASK 1 (Runs on `task1_stack` via PSP): Toggles LD2 (PA5) every `blink_period_ms` ms: */
static void task_led_blink(void)
{
    uint32_t last_toggle_ms = 0U;
    bool     led_on         = false;

    for (;;) {
        ++task1_loop_count;
        uint32_t now_ms = app_ms;
        if ((now_ms - last_toggle_ms) >= blink_period_ms) {
            last_toggle_ms = now_ms;
            led_on         = !led_on;
            GPIOA->BSRR    = led_on ? LED_MASK : (LED_MASK << 16);
        }

        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;         /* Yield to Task 0 via PendSV (#14) */
        __DSB();
        __ISB();
    }
}

int main(void)
{
    __disable_irq();
    SysTick->CTRL = 0U;
    SCB->ICSR     = (1UL << 25) | (1UL << 27);      /* Clear stale SysTick & PendSV     */

    clock_init_84mhz();
    gpio_init();
    rtos_vectors_and_irq_init();

    /* Step 1: Pre-push 16 words onto task0_stack and task1_stack: */
    g_tcbs[0].psp  = task_stack_init(&task0_stack[256], task_button_check);
    g_tcbs[1].psp  = task_stack_init(&task1_stack[256], task_led_blink);
    g_current_task = 0U;

    /* Step 2: Block priorities 1..15 until SVC_Handler (Prio 0) loads PSP, then run `svc #0`: */
    __set_BASEPRI(1U << 4);
    __DSB();
    __ISB();
    __enable_irq();

    __asm volatile ("svc #0" : : : "memory");       /* Enters SVC_Handler -> starts Task 0! */

    fault_stop(0xFEU);                              /* Never reached */
}
```

---

## Complete Standalone Code: `Core/Src/pendsv_cmsis_bridge_app.c`

Compile with `#include "stm32f446xx.h"` (keep `-DSTM32F446xx` and the two CMSIS include paths; remove `USE_HAL_DRIVER`) and link with `STM32F446RETX_FLASH.ld` and `-nostartfiles` in `RegistersDebug`:

```c
#include <stdbool.h>
#include <stdint.h>
#include "stm32f446xx.h" /* Official CMSIS register structs & intrinsics (zero HAL) */

#define LED_MASK   (1UL << 5)    /* PA5 (LD2) */
#define B1_MASK    (1UL << 13)   /* PC13 / EXTI13 (B1) */
#define IRQ40_MASK (1UL << 8)    /* EXTI15_10_IRQn (40) in NVIC word 1 */
#define POLL_LIMIT 1000000U

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;

void Reset_Handler(void);
void Default_Handler(void);
void HardFault_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTick_Handler(void);
void EXTI15_10_IRQHandler(void);
int  main(void);

typedef void (*isr_vector_fn_t)(void);

__attribute__((section(".isr_vector"), used))
const isr_vector_fn_t g_pfnVectors[113] = {
    [0]       = (isr_vector_fn_t)&_estack,
    [1]       = Reset_Handler,
    [2]       = Default_Handler,
    [3]       = HardFault_Handler,
    [4]       = Default_Handler,
    [5]       = Default_Handler,
    [6]       = Default_Handler,
    [11]      = SVC_Handler,               /* #11: Launches first task on PSP */
    [12]      = Default_Handler,
    [14]      = PendSV_Handler,            /* #14: Deferred PSP context switcher */
    [15]      = SysTick_Handler,           /* #15: 1 ms tick + pends PendSV */
    [16 + 40] = EXTI15_10_IRQHandler,      /* #56 (IRQ 40): B1 edge + pends PendSV */
};

typedef struct {
    uint32_t *psp;                         /* Points to sp[0] (R4) when task is paused */
} tcb_t;

static volatile tcb_t    g_tcbs[2];
static volatile uint32_t g_current_task = 0U;

static uint32_t task0_stack[256] __attribute__((aligned(8)));
static uint32_t task1_stack[256] __attribute__((aligned(8)));

uint32_t SystemCoreClock = 16000000U;

static volatile uint32_t app_ms;
static volatile uint32_t b1_irq_pending;
static volatile uint32_t b1_irq_count;
static volatile uint32_t blink_period_ms = 500U;
static volatile uint32_t accepted_press_count;
static volatile uint32_t task0_loop_count;
static volatile uint32_t task1_loop_count;
static volatile uint32_t context_switch_count;
static volatile uint32_t fault_code;

static _Noreturn void fault_stop(uint32_t code)
{
    __disable_irq();
    fault_code = code;
    for (;;) {
        __NOP();
    }
}

static _Noreturn void task_exit_trap(void) { fault_stop(0xFEU); }
void Default_Handler(void)                 { fault_stop(0xF0U); }
void HardFault_Handler(void)               { fault_stop(0xF1U); }

/* STEP 1: Pre-push 16 initial register words onto a task stack: */
static uint32_t *task_stack_init(uint32_t *stack_top, void (*task_fn)(void))
{
    uint32_t *sp = (uint32_t *)((uintptr_t)stack_top & ~7UL);

    /* Top 8 words (Hardware Exception Frame: popped by CPU on `bx lr` = 0xFFFFFFFD): */
    *(--sp) = 0x01000000UL;                          /* sp[15]: xPSR (Bit 24 T = 1 for Thumb)  */
    *(--sp) = ((uint32_t)(uintptr_t)task_fn) & ~1UL; /* sp[14]: Return_PC (task start address) */
    *(--sp) = (uint32_t)(uintptr_t)task_exit_trap;   /* sp[13]: LR (trap if task ever returns) */
    *(--sp) = 0x12121212UL;                          /* sp[12]: R12 (scratch)                  */
    *(--sp) = 0x03030303UL;                          /* sp[11]: R3  (4th arg / scratch)        */
    *(--sp) = 0x02020202UL;                          /* sp[10]: R2  (3rd arg / scratch)        */
    *(--sp) = 0x01010101UL;                          /* sp[ 9]: R1  (2nd arg / scratch)        */
    *(--sp) = 0x00000000UL;                          /* sp[ 8]: R0  (1st arg = 0)              */

    /* Bottom 8 words (Software Context Frame: popped by `ldmia r0!, {r4-r11}`): */
    *(--sp) = 0x11111111UL;                          /* sp[ 7]: R11                            */
    *(--sp) = 0x10101010UL;                          /* sp[ 6]: R10                            */
    *(--sp) = 0x09090909UL;                          /* sp[ 5]: R9                             */
    *(--sp) = 0x08080808UL;                          /* sp[ 4]: R8                             */
    *(--sp) = 0x07070707UL;                          /* sp[ 3]: R7                             */
    *(--sp) = 0x06060606UL;                          /* sp[ 2]: R6                             */
    *(--sp) = 0x05050505UL;                          /* sp[ 1]: R5                             */
    *(--sp) = 0x04040404UL;                          /* sp[ 0]: R4 <- g_tcbs[i].psp points here*/

    return sp;
}

/* STEP 2: Launch Task 0 for the first time by popping its 16 pre-pushed words: */
__attribute__((used))
uint32_t *svc_first_task_psp(void)
{
    return g_tcbs[0].psp;
}

__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        "push  {r3, lr}                 \n"
        "bl    svc_first_task_psp       \n" /* R0 = g_tcbs[0].psp (sp[0] = R4)             */
        "pop   {r3, lr}                 \n"
        "ldmia r0!, {r4-r11}            \n" /* Software POPS sp[0..7] into R4..R11         */
        "msr   psp, r0                  \n" /* Set PSP = &sp[8] (top 8 hardware words)     */
        "ldr   r1, =_estack             \n" /* Reset MSP = 0x20020000                      */
        "msr   msp, r1                  \n"
        "movs  r1, #0                   \n" /* Clear BASEPRI = 0 so priorities 1..15 run   */
        "msr   basepri, r1              \n"
        "isb   0xF                      \n"
        "ldr   lr, =0xFFFFFFFD          \n" /* EXC_RETURN: return to Thread mode using PSP */
        "bx    lr                       \n" /* Hardware POPS sp[8..15] -> starts Task 0!   */
    );
}

/* STEP 3 & 4: Request PendSV and switch PSP between Task 0 and Task 1: */
void SysTick_Handler(void)
{
    ++app_ms;
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
}

void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & B1_MASK) != 0U) {
        EXTI->PR       = B1_MASK;
        b1_irq_pending = 1U;
        ++b1_irq_count;
        SCB->ICSR      = SCB_ICSR_PENDSVSET_Msk;
        __DSB();
    }
}

__attribute__((used))
uint32_t *pendsv_switch_context(uint32_t *current_psp)
{
    uint32_t cur    = g_current_task;
    g_tcbs[cur].psp = current_psp;

    uint32_t next   = (cur == 0U) ? 1U : 0U;
    g_current_task  = next;
    ++context_switch_count;

    return g_tcbs[next].psp;
}

__attribute__((naked))
void PendSV_Handler(void)
{
    __asm volatile (
        "mrs   r0, psp                  \n" /* R0 = outgoing task's PSP                    */
        "isb   0xF                      \n"
        "stmdb r0!, {r4-r11}            \n" /* Software PUSHES outgoing R4..R11            */
        "push  {r3, lr}                 \n"
        "cpsid i                        \n"
        "bl    pendsv_switch_context    \n" /* Save outgoing PSP; R0 = incoming task's PSP */
        "cpsie i                        \n"
        "pop   {r3, lr}                 \n"
        "ldmia r0!, {r4-r11}            \n" /* Software POPS incoming R4..R11              */
        "msr   psp, r0                  \n" /* Update PSP to incoming task's HW frame      */
        "isb   0xF                      \n"
        "bx    lr                       \n" /* Hardware POPS incoming {R0..xPSR} -> Run!   */
    );
}

/* STEP 5: Two simple tasks (Task 0 = Button Check, Task 1 = LED Blink): */
static void task_button_check(void)
{
    uint32_t last_press_ms = 0U;

    for (;;) {
        ++task0_loop_count;
        if (b1_irq_pending != 0U) {
            b1_irq_pending = 0U;
            uint32_t now_ms = app_ms;
            if ((now_ms - last_press_ms) >= 200U) {
                last_press_ms = now_ms;
                ++accepted_press_count;
                blink_period_ms = (blink_period_ms == 500U) ? 150U : 500U;
            }
        }

        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
        __DSB();
        __ISB();
    }
}

static void task_led_blink(void)
{
    uint32_t last_toggle_ms = 0U;
    bool     led_on         = false;

    for (;;) {
        ++task1_loop_count;
        uint32_t now_ms = app_ms;
        if ((now_ms - last_toggle_ms) >= blink_period_ms) {
            last_toggle_ms = now_ms;
            led_on         = !led_on;
            GPIOA->BSRR    = led_on ? LED_MASK : (LED_MASK << 16);
        }

        SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
        __DSB();
        __ISB();
    }
}

/* Startup, 84 MHz Clock, GPIO, and Interrupt Init: */
static void wait_field(volatile const uint32_t *reg,
                       uint32_t mask, uint32_t expected, uint32_t code)
{
    for (uint32_t n = 0U; n < POLL_LIMIT; ++n) {
        if ((*reg & mask) == expected) {
            return;
        }
    }
    fault_stop(code);
}

void Reset_Handler(void)
{
    __disable_irq();
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __DSB();
    __ISB();

    const uint32_t *src = &_sidata;
    volatile uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    volatile uint32_t *bss = &_sbss;
    while (bss < &_ebss) {
        *bss++ = 0U;
    }

    __DSB();
    __ISB();
    (void)main();
    fault_stop(0xFFU);
}

static void clock_init_84mhz(void)
{
    if (((RCC->CFGR & 0x0CUL) != 0U) ||
        ((RCC->CR & ((1UL << 24) | (1UL << 26) | (1UL << 28))) != 0U)) {
        fault_stop(1U);
    }
    RCC->CR |= 1UL;
    wait_field(&RCC->CR, 1UL << 1, 1UL << 1, 2U);
    RCC->CR &= ~(1UL << 24);
    wait_field(&RCC->CR, 1UL << 25, 0U, 3U);

    RCC->APB1ENR |= 1UL << 28;
    (void)RCC->APB1ENR;
    __DSB();
    PWR->CR = (PWR->CR & ~(3UL << 14)) | (1UL << 14);

    FLASH->ACR = (FLASH->ACR & ~15UL) | 2UL;
    wait_field(&FLASH->ACR, 15UL, 2UL, 4U);

    RCC->CFGR = (RCC->CFGR & ~((15UL << 4) | (7UL << 10) | (7UL << 13)))
              | (4UL << 10);

    const uint32_t pll_mask  = 0x7F437FFFUL;
    const uint32_t pll_value = 16UL | (336UL << 6) | (1UL << 16)
                             | (7UL << 24) | (2UL << 28);
    RCC->PLLCFGR = (RCC->PLLCFGR & ~pll_mask) | pll_value;
    RCC->CR |= 1UL << 24;
    wait_field(&RCC->CR, 1UL << 25, 1UL << 25, 5U);
    wait_field(&PWR->CSR, 1UL << 14, 1UL << 14, 6U);

    RCC->CFGR = (RCC->CFGR & ~3UL) | 2UL;
    wait_field(&RCC->CFGR, 3UL << 2, 2UL << 2, 7U);
    __DSB();
    __ISB();
    SystemCoreClock = 84000000U;
}

static void gpio_init(void)
{
    RCC->AHB1ENR |= (1UL << 0) | (1UL << 2);
    (void)RCC->AHB1ENR;
    __DSB();

    GPIOA->BSRR    = LED_MASK << 16;
    GPIOA->OTYPER  &= ~LED_MASK;
    GPIOA->OSPEEDR &= ~(3UL << 10);
    GPIOA->PUPDR   &= ~(3UL << 10);
    GPIOA->MODER   = (GPIOA->MODER & ~(3UL << 10)) | (1UL << 10);

    GPIOC->MODER   &= ~(3UL << 26);
    GPIOC->PUPDR   = (GPIOC->PUPDR & ~(3UL << 26)) | (1UL << 26);
}

static void rtos_vectors_and_irq_init(void)
{
    NVIC->ICER[1] = IRQ40_MASK;
    EXTI->IMR    &= ~B1_MASK;

    RCC->APB2ENR |= 1UL << 14;
    (void)RCC->APB2ENR;
    __DSB();
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~(15UL << 4)) | (2UL << 4);

    EXTI->EMR  &= ~B1_MASK;
    EXTI->RTSR &= ~B1_MASK;
    EXTI->FTSR |= B1_MASK;                  /* Falling edge on PC13 (button press) */

    uint32_t aircr = SCB->AIRCR & ~((0xFFFFUL << 16) | (7UL << 8));
    SCB->AIRCR = aircr | (0x5FAUL << 16) | (3UL << 8);
    SCB->VTOR  = (uint32_t)g_pfnVectors;
    SCB->CCR  |= 1UL << 9;

    SCB->SHP[7]   = (uint8_t)(0U  << 4);    /* SVCall    (#11) = prio 0  (launch)  */
    SCB->SHP[11]  = (uint8_t)(1U  << 4);    /* SysTick   (#15) = prio 1            */
    NVIC->IP[40]  = (uint8_t)(5U  << 4);    /* EXTI15_10 (40)  = prio 5            */
    SCB->SHP[10]  = (uint8_t)(15U << 4);    /* PendSV    (#14) = prio 15 (lowest!) */

    EXTI->PR      = B1_MASK;
    NVIC->ICPR[1] = IRQ40_MASK;
    EXTI->IMR    |= B1_MASK;
    NVIC->ISER[1] = IRQ40_MASK;

    app_ms        = 0U;
    SysTick->LOAD = 84000U - 1U;
    SysTick->VAL  = 0U;
    SysTick->CTRL = (1UL << 2) | (1UL << 1) | 1UL;
    __DSB();
    __ISB();
}

int main(void)
{
    __disable_irq();
    if ((__get_IPSR() != 0U) || ((__get_CONTROL() & 3U) != 0U)) {
        fault_stop(8U);
    }
    SysTick->CTRL = 0U;
    SCB->ICSR     = (1UL << 25) | (1UL << 27);

    clock_init_84mhz();
    gpio_init();
    rtos_vectors_and_irq_init();

    g_tcbs[0].psp  = task_stack_init(&task0_stack[256], task_button_check);
    g_tcbs[1].psp  = task_stack_init(&task1_stack[256], task_led_blink);
    g_current_task = 0U;

    __set_BASEPRI(1U << 4);
    __DSB();
    __ISB();
    __enable_irq();

    __asm volatile ("svc #0" : : : "memory");

    fault_stop(0xFEU);
}
```

---

## Complete Scenario Walkthrough: Boot, First Button Press (`t = 1000 ms`), And Second Press Within `200 ms` (`t = 1050 ms`)

Here is one complete trace of the code above from power-on through two button presses:

### 1. Boot & Scheduler Start (`t = 0 ms`)

1. **`Reset_Handler()` (on `MSP`):** Copies `.data` (`blink_period_ms = 500U`), zeros `.bss` (`app_ms = 0`, `b1_irq_pending = 0`), and calls `main()`.
2. **`main()` (on `MSP`):**
   * Configures the `84 MHz` clock, `PA5` (`LD2`), `PC13` (`B1` falling-edge interrupt on `IRQ 40`), and `1 ms` `SysTick`.
   * Sets exception priorities: `SVCall = 0` (highest urgency), `SysTick = 1`, `EXTI15_10 = 5`, and **`PendSV = 15` (lowest urgency)**.
   * Calls `task_stack_init()` twice to pre-push 16 words onto `task0_stack` (`Return_PC = task_button_check`) and `task1_stack` (`Return_PC = task_led_blink`).
   * Executes `svc #0` -> **`SVC_Handler` pops the 16 words from `task0_stack` into the CPU registers and starts running Task 0 (`task_button_check`) on `PSP`!**

---

### 2. Normal Alternating Execution Before Any Button Press (`t = 0 .. 999 ms`)

1. **Task 0 (`task_button_check`) runs on `task0_stack`:**
   * Sees `b1_irq_pending == 0`.
   * Writes `SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;` to yield the CPU.
2. **`PendSV_Handler` (Priority `15`, on `MSP`) switches `Task 0 -> Task 1`:**
   * Hardware pushes Task 0's `{R0..xPSR}` onto `task0_stack`; software pushes Task 0's `{R4..R11}` onto `task0_stack` and saves `g_tcbs[0].psp`.
   * Software pops Task 1's `{R4..R11}` from `task1_stack`; hardware pops Task 1's `{R0..xPSR}` from `task1_stack`.
3. **Task 1 (`task_led_blink`) runs on `task1_stack`:**
   * Every `500 ms` (`t = 500 ms`, `t = 1000 ms`), `(now_ms - last_toggle_ms) >= 500U` is true -> updates `last_toggle_ms` and toggles `LD2` (`GPIOA->BSRR`).
   * Writes `SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;` -> `PendSV_Handler` switches `Task 1 -> Task 0`.
4. **Meanwhile, every `1 ms`, `SysTick_Handler` (Priority `1`) fires:**
   * Increments `app_ms` (`1, 2, 3...`) and sets `PENDSVSET`.

---

### 3. First Button Press at `t = 1000 ms` (Accepted: Speed Changes `500 ms -> 150 ms`)

Suppose Task 1 is running at `t = 1000 ms` when you press button `B1` (`PC13` falls from `1` to `0`):

1. **`EXTI15_10_IRQHandler` (Priority `5`) preempts Task 1 immediately:**
   * Hardware pushes Task 1's 8 registers (`R0..xPSR`) onto `task1_stack` (`PSP`) and switches to `MSP`.
   * `EXTI15_10_IRQHandler` clears `EXTI->PR = B1_MASK`, sets **`b1_irq_pending = 1U`**, and sets **`SCB->ICSR = SCB_ICSR_PENDSVSET_Msk`**.
2. **`EXTI15_10_IRQHandler` finishes and tail-chains directly into `PendSV_Handler` (Priority `15`):**
   * Because no other ISR (`Priority 1..5`) is running now, `PendSV_Handler` runs immediately after `EXTI15_10_IRQHandler`.
   * `PendSV_Handler` saves Task 1's `R4..R11` onto `task1_stack` and restores Task 0 from `task0_stack`.
3. **Task 0 (`task_button_check`) runs on `task0_stack`:**
   * Sees **`b1_irq_pending == 1U`**!
   * Clears `b1_irq_pending = 0U` and reads `now_ms = 1000U`.
   * Checks `(now_ms - last_press_ms) >= 200U`:
     * Since `last_press_ms` was `0`, `1000 - 0 = 1000 >= 200U` is **TRUE**!
     * Updates **`last_press_ms = 1000U`**, increments `accepted_press_count` (`0 -> 1`), and changes **`blink_period_ms` from `500U` to `150U`**!
   * Yields via `PENDSVSET` -> Task 1 now blinks `LD2` every **`150 ms`**.

---

### 4. Second Button Press (or Contact Bounce) Within `200 ms` at `t = 1050 ms` (Ignored!)

Suppose `50 ms` later (at `t = 1050 ms`), the button bounces or is pressed again:

1. **`EXTI15_10_IRQHandler` (Priority `5`) fires again:**
   * Clears `EXTI->PR = B1_MASK`, sets **`b1_irq_pending = 1U`**, and sets `PENDSVSET`.
2. **`PendSV_Handler` (Priority `15`) switches to Task 0 (`task_button_check`):**
   * Task 0 sees `b1_irq_pending == 1U` and clears **`b1_irq_pending = 0U`**.
   * Reads `now_ms = 1050U` and checks `(now_ms - last_press_ms) >= 200U`:
     * `1050 - 1000 = 50 ms`, which is **`< 200U` (FALSE)**!
     * Because only `50 ms` have elapsed since `last_press_ms = 1000U`, Task 0 **ignores** this edge (`last_press_ms` stays `1000U`, `accepted_press_count` stays `1`, and `blink_period_ms` stays `150U`).
3. **What if you press the button again later at `t = 1300 ms`?**
   * At `t = 1300 ms`, `1300 - 1000 = 300 ms >= 200U` is **TRUE**, so Task 0 accepts the press, updates `last_press_ms = 1300U`, and switches `blink_period_ms` back from `150U` to `500U`.

---

## Two Key Hardware Questions (To Remember Forever)

### Question 1: When a task executes `SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;`, does `PendSV` fire immediately, or only after `__DSB()` and `__ISB()`?

* **Short answer:** If no interrupt is running and `PRIMASK == 0`, `PendSV` becomes pending as soon as the store reaches `SCB->ICSR` (typically 1–2 clock cycles) and **can fire either right after the store instruction OR during `__DSB()` / `__ISB()`**. We add `__DSB()` and `__ISB()` to **guarantee** that `PendSV` has already fired and switched tasks before the CPU is allowed to execute any instruction *after* `__ISB()`.
* **Why (Step-by-Step):**
  1. `SCB->ICSR` (`0xE000ED04`) is a memory-mapped register on the bus, while the CPU pipeline fetches 1–2 instructions ahead.
  2. When the CPU executes the store instruction (`str`) for `SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;`, that write travels through the write buffer to `SCB->ICSR`.
  3. **`__DSB()` (`dsb 0xF`):** Stops the CPU pipeline until the write to `SCB->ICSR` has physically finished on the bus, so `PENDSVSET` is guaranteed to be `1`.
  4. **`__ISB()` (`isb 0xF`):** Flushes the instruction pipeline so the CPU recognizes the pending `PendSV` **before** any C code after `__ISB()` can run.
  5. When `PendSV_Handler` later switches **back** to this task, the task simply continues from that barrier and loops back to the top of `for (;;)`.

```text
Task writes: SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
  -> __DSB() waits until PENDSVSET = 1 is latched in hardware
  -> __ISB() flushes pipeline -> PendSV_Handler is GUARANTEED to run here!
  -> (When this task is scheduled back in later, it continues after __ISB())
```

---

### Question 2: When `PendSV_Handler` (`Priority 15`) is ongoing, what happens if the button is pressed (`EXTI15_10_IRQHandler`, `Priority 5`)?

* **Short answer:** Yes, `EXTI15_10_IRQHandler` (`Priority 5`, higher urgency) **pauses `PendSV_Handler` (`Priority 15`, lowest urgency)**, runs on **`MSP`** as a **nested interrupt** (NOT a task `PSP` context switch!), sets `b1_irq_pending = 1`, and returns to `PendSV_Handler` so `PendSV_Handler` finishes switching tasks!
* **There are 2 exact cases depending on which line of `PendSV_Handler` is running when the button is pressed:**

  1. **Case A — Button is pressed while `PendSV_Handler` is running with `PRIMASK == 0` (before `cpsid i` or after `cpsie i`, including during `stmdb` or `ldmia`):**
     * Because `EXTI15_10` (`Priority 5`) is more urgent than `PendSV` (`Priority 15`), the CPU **immediately pauses `PendSV_Handler`**.
     * Because `PendSV_Handler` is already in **Handler mode on `MSP`**, hardware pushes `PendSV_Handler`'s 8 registers onto **`MSP`** (not `PSP`!), sets `LR = 0xFFFFFFF1` (*"Return to Handler mode on `MSP`"*), and runs `EXTI15_10_IRQHandler`.
     * *(Even if `PendSV_Handler` was in the middle of pushing/popping the 8 registers in `stmdb r0!, {r4-r11}` or `ldmia r0!, {r4-r11}`, Cortex-M4 saves the register index in `EPSR.ICI`, runs `EXTI15_10_IRQHandler`, and resumes `stmdb`/`ldmia` from the exact register it left off!)*
     * `EXTI15_10_IRQHandler` sets `b1_irq_pending = 1`, sets `PENDSVSET = 1`, and returns (`0xFFFFFFF1`) back into `PendSV_Handler`, which finishes its task switch.

  2. **Case B — Button is pressed during the ~10 clock cycles between `cpsid i` and `cpsie i` inside `PendSV_Handler`:**
     * While `cpsid i` (`PRIMASK = 1`) is active around `bl pendsv_switch_context`, the CPU temporarily blocks new interrupts from starting so `g_tcbs[cur].psp` and `g_current_task` are updated atomically.
     * However, **hardware `EXTI` still latches the button edge immediately** (`EXTI->PR` bit `13 = 1`), so the press is **never lost**.
     * A few nanoseconds later, as soon as `PendSV_Handler` executes `cpsie i` (`PRIMASK = 0`), `EXTI15_10_IRQHandler` (`Priority 5`) immediately preempts `PendSV_Handler` on `MSP`, sets `b1_irq_pending = 1`, and returns to `PendSV_Handler`!

* **And what happens at the end of `PendSV_Handler` because `EXTI15_10_IRQHandler` set `PENDSVSET = 1` again? (The 6-Step `Active + Pending` Rule):**
  1. **Step 1 (`PendSV_Handler` starts):** At the instant the CPU enters the first `PendSV_Handler`, Cortex-M4 hardware **automatically clears `PendSV`'s Pending bit (`Pending = 0`)** and sets its **Active bit (`Active = 1`)**.
  2. **Step 2 (Button pressed):** `EXTI` asserts `IRQ 40` (`Priority 5`).
  3. **Step 3 (`PendSV_Handler` paused):** `EXTI15_10_IRQHandler` (`Priority 5`) preempts `PendSV_Handler` (`Priority 15`) on `MSP` while `PendSV` stays `Active = 1`.
  4. **Step 4 (`PENDSVSET` set in `EXTI15_10_IRQHandler`):** Writing `SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;` sets `PendSV` **`Pending = 1` again** while the first `PendSV_Handler` is still `Active = 1`!
  5. **Step 5 (`EXTI15_10_IRQHandler` finishes):** Returns (`0xFFFFFFF1`) to the paused first `PendSV_Handler`, which finishes its current switch (`Task 0 -> Task 1`).
  6. **Step 6 (First `PendSV_Handler` finishes -> Immediately runs `PendSV_Handler` a 2nd time!):** When the first `PendSV_Handler` executes `bx lr` (`0xFFFFFFFD`), its `Active` bit clears to `0`, and hardware sees `PendSV` is **`Pending == 1`**! Without running even a single instruction of `Task 1`, the CPU **tail-chains immediately into `PendSV_Handler` a second time** (clearing `Pending = 0`) and switches straight back to `Task 0` (`task_button_check`)!
     * **Why this hardware behavior is essential in FreeRTOS:** If the button interrupt arrived *after* the first `PendSV_Handler` had already chosen a low-priority task (`pendsv_switch_context`), re-running `PendSV_Handler` immediately in Step 6 guarantees the CPU switches to the newly woken button task right away instead of making it wait until the next `1 ms` `SysTick`!

```text
1. PendSV_Handler #1 starts (Active = 1, Pending auto-cleared to 0)
     |
     | 2 & 3. Button pressed -> EXTI15_10_IRQHandler (Prio 5) pauses PendSV #1
     v
   EXTI15_10_IRQHandler (nested on MSP, LR = 0xFFFFFFF1)
     - 4. Sets b1_irq_pending = 1 and sets PENDSVSET = 1 (Now Active = 1 AND Pending = 1!)
     - 5. Returns (0xFFFFFFF1) to PendSV_Handler #1
     |
     v
   PendSV_Handler #1 finishes (`bx lr` = 0xFFFFFFFD -> Active becomes 0, but Pending is 1!)
     |
     v
6. CPU tail-chains IMMEDIATELY into PendSV_Handler #2 (Pending cleared to 0)
     -> Switches straight to Task 0 (`task_button_check`)!
```

---

### Question 3: Are `Active` Bits and `Pending` Bits Set and Cleared by Hardware or by Software?

You must separate **three different status bits** along the interrupt path ([Arm DUI0553B](../../docs/stm32/um-cortexm4/DUI0553.pdf) Sec 2.3.2 & [RM0390](../../docs/stm32/um-f446re/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) Sec 10.3.6):

| Status Bit & Location | Who Sets It (`0 -> 1`)? | Who Clears It (`1 -> 0`)? |
| --- | --- | --- |
| **1. `Active` Bit** (Inside Arm CPU Core: `SCB->SHCSR` / `NVIC->IABR`) | **Hardware ONLY** (automatically at the instant the CPU enters the handler) | **Hardware ONLY** (automatically at the instant the handler executes `bx lr` to exit) |
| **2. CPU Core `Pending` Bit** (Inside Arm CPU Core: `SCB->ICSR` / `NVIC->ISPR` & `ICPR`) | **Hardware** (when `SysTick` wraps or a peripheral asserts its `IRQ` wire) **OR Software** (e.g. `SCB->ICSR = PENDSVSET` for `PendSV`, or `NVIC->ISPR` for testing) | **Hardware automatically** at the instant the CPU enters the handler! *(Software only writes `ICPR` / `PENDSVCLR` at startup to wipe out old stale events before enabling interrupts.)* |
| **3. STM32 Peripheral `Pending` Flag** (Outside CPU in MCU Peripheral: `EXTI->PR` bit `13`, `TIM->SR`, etc.) | **Hardware** (when the button pin `PC13` has a falling edge) | **Software ONLY inside the ISR (`EXTI->PR = B1_MASK;`)!** Hardware **never** clears `EXTI->PR` automatically! |

#### Why Must Software Clear `EXTI->PR` Inside `EXTI15_10_IRQHandler`, But Does NOT Need to Clear `PendSV` or `SysTick`?

Look at the difference between a core exception (`PendSV` / `SysTick`) and an external STM32 peripheral interrupt (`EXTI15_10`):

* **`PendSV` and `SysTick` live inside the Arm Core:**
  * They only have the **CPU Core `Pending` bit** (`SCB->ICSR`).
  * The instant the CPU enters `PendSV_Handler` or `SysTick_Handler`, **hardware automatically clears that `Pending` bit to `0`**. Your handler does not need to clear any pending bit!
* **`EXTI15_10` (`IRQ 40`) comes from an STM32 peripheral outside the Arm Core:**
  * It has **two** latches in series: **`EXTI->PR` bit `13`** (inside the STM32 `EXTI` peripheral) driving wire `IRQ 40` into the **Arm `NVIC` `Pending` bit**.
  * When the CPU enters `EXTI15_10_IRQHandler`, Arm `NVIC` hardware automatically clears its own **`NVIC` `Pending` bit** to `0`.
  * **However, `EXTI->PR` bit `13` (outside the CPU) is still `1`!**
  * If your C code inside `EXTI15_10_IRQHandler` forgets to write **`EXTI->PR = B1_MASK;`** (Write-1-to-Clear), the `EXTI` peripheral keeps holding the `IRQ 40` wire HIGH, which immediately forces the `NVIC` `Pending` bit back to `1` and traps the CPU in an infinite interrupt loop!

```text
STM32 EXTI Peripheral (Outside CPU)              Arm Cortex-M4 NVIC & Core (Inside CPU)
+-----------------------------------+            +----------------------------------------+
| Latch 1: `EXTI->PR` bit 13        |   IRQ 40   | Latch 2: `NVIC` Pending Bit & Active   |
| - NO Active bit exists here!      |            | - Active  SET on entry, CLEARED on exit|
| - Pending SET by Hardware (edge)  | ---------> | - Pending SET by Hardware (IRQ40 wire) |
| - Pending CLEARED by SOFTWARE ONLY|    wire    | - Pending CLEARED by Hardware on entry |
|   (`EXTI->PR = B1_MASK;` in ISR)  |            |   (Software never clears NVIC in ISR!) |
+-----------------------------------+            +----------------------------------------+
```

* **Three Golden Rules to Remember:**
  1. **Inside the Arm CPU Core (`NVIC` and `SCB`), both the `Pending` bit and the `Active` bit are managed automatically by hardware when an interrupt runs:** Hardware sets `Pending = 1` when the interrupt signal arrives (or software sets `PENDSVSET = 1` for `PendSV`), hardware **automatically clears `Pending = 0` and sets `Active = 1`** when the handler starts, and hardware **automatically clears `Active = 0`** when the handler exits (`bx lr`). Software **never** clears `NVIC->ICPR` inside the ISR! *(We only write `NVIC->ICPR` once in `main()` before enabling the interrupt just in case a pin glitch happened during boot setup.)*
  2. **Inside an STM32 Peripheral (`EXTI`, `TIM`, `USART`), there is NO `Active` bit — only a peripheral `Pending` flag (`EXTI->PR`):** The STM32 peripheral has no idea whether the CPU is inside the ISR. It simply sets `EXTI->PR` bit `13 = 1` when the pin edge happens and holds the `IRQ 40` wire HIGH until **your software clears `EXTI->PR = B1_MASK;` inside the ISR**.
  3. **Why doesn't the Arm CPU clear `EXTI->PR` automatically?** Because 6 different STM32 pins (`Pin 10` through `Pin 15`) share that single `IRQ 40` (`EXTI15_10`) wire into the Arm `NVIC`! The Arm `NVIC` only sees wire `IRQ 40` go HIGH—it does not know whether `Pin 10` or `Pin 13` caused it, and has no wire back to `EXTI->PR`. Only your ISR code reading and writing `EXTI->PR` knows which pin to acknowledge.

---

## How Every Function Above Maps To Official FreeRTOS (`P1`)

| What We Built in `W01-L19` | Official FreeRTOS Equivalent | What FreeRTOS Adds |
| --- | --- | --- |
| `task_stack_init()` (pre-pushes 16 words, `xPSR = 0x01000000`) | `pxPortInitialiseStack()` | Also saves `EXC_RETURN` per task so `PendSV` can save `{S16..S31}` when the FPU is used |
| `svc #0` -> `SVC_Handler` popping `R4..R11` and setting `PSP` | `prvPortStartFirstTask()` + `vPortSVCHandler()` | Reclaims `MSP` (`_estack`) and starts the highest-priority Ready task |
| `PendSV_Handler` (`stmdb r0!, {r4-r11}` -> swap TCB -> `ldmia r0!, {r4-r11}`) | `xPortPendSVHandler()` -> `vTaskSwitchContext()` | Selects the highest-priority Ready task instead of simple 2-task alternation |
| `SysTick_Handler` (`++app_ms; SCB->ICSR = PENDSVSET`) | `xPortSysTickHandler()` -> `xTaskIncrementTick()` | Wakes sleeping tasks when their `vTaskDelay` / `vTaskDelayUntil` tick deadline expires |
| `b1_irq_pending = 1U; SCB->ICSR = PENDSVSET` + task checking flag | Task calls `ulTaskNotifyTake()` -> ISR calls `xTaskNotifyFromISR()` + `portYIELD_FROM_ISR()` | Task sleeps in the **Blocked** state (`0% CPU`) until the ISR wakes it |
| `__disable_irq()` (`PRIMASK`) | `taskENTER_CRITICAL()` (`BASEPRI`) | Masks only priorities numerically `>= configMAX_SYSCALL_INTERRUPT_PRIORITY` (e.g., `5..15`), leaving `0..4` unmasked |
