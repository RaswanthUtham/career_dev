# W01-L18: Bare-Metal Cortex-M4 Privilege (`CONTROL`), `PSP` vs. `MSP`, `MPU` Memory Protection, `SVC`, And `MemManage` Faults

> **Scope & verification status:** `Document/source-reviewed, not build-verified: local Arm DUI0553B, STM32 RM0390 Rev 6, UM1724 Rev 14, DS10693 Rev 10 (stm32f446mc.pdf), and STM32F446RETX_FLASH.ld on 2026-11-01.`
> **Build model (`RegistersDebug`):** Complete bare-metal C (`Core/Src/mpu_privilege_app.c`) linked only with `STM32F446RETX_FLASH.ld` and `-nostartfiles`—zero HAL, zero CMSIS headers, zero `.s` startup file, and zero `system_stm32f4xx.c`. Check silicon errata (`ES0298`) and build inputs before flashing.

Previous: [Complete bare-metal 84 MHz startup, vector table, B1 interrupt handoff, and LD2 debounce](W01_L17_b1_exti_interrupt_handoff_and_debounce.md).

---

## Unified Example: How Everything in This Lesson Works Together

In [Lesson 17](W01_L17_b1_exti_interrupt_handoff_and_debounce.md), all of our C code ran in **Privileged mode**. That means every line of code in the `for (;;)` loop had full hardware permission to overwrite any RAM address, change any peripheral register, or disable all CPU interrupts. If the application loop has a bug, it can corrupt system timers or freeze the microcontroller.

In this lesson, we protect the microcontroller in **5 simple steps**:

1. **Step 1 — Use Two Execution Modes (`Privileged` vs. `Unprivileged`) and Two Stack Pointers (`MSP` vs. `PSP`):**
   * Normal C code runs in **Thread mode**. Interrupt handlers always run in **Handler mode**.
   * Interrupt handlers (**Handler mode**) are **always Privileged** and always use the **Main Stack Pointer (`MSP`)**.
   * At reset, `main()` starts in **Privileged Thread mode** on **`MSP`** so it can configure the clocks, GPIO pins, interrupts, and memory protection.
   * After setup is done, we switch the application `for (;;)` loop to **Unprivileged Thread mode** and point its stack to a separate `1 KB` RAM array (`app_psp_stack`) using the **Process Stack Pointer (`PSP`)**.

2. **Step 2 — Protect Shared System Variables in RAM Using the `MPU` (`Memory Protection Unit`):**
   * We store our system variables (`app_ms` millisecond counter, `blink_period_ms` LED blink interval, `accepted_press_count` button counter, and a test variable `protected_cookie = 0xCAFE1234`) inside a `32-byte` structure in SRAM called `g_kernel_vault`.
   * Then we program 3 memory regions in the hardware `MPU` (`0xE000ED90`):
     * **Region 0 (`512 KB` Flash at `0x08000000`):** Point Region 0 to the `512 KB` Flash memory and set its register to allow **Read and Execute** access for both Privileged and Unprivileged code.
     * **Region 1 (`128 KB` SRAM at `0x20000000`):** Point Region 1 to the `128 KB` SRAM and set its register to allow **Read and Write** access (and block code execution) for both Privileged and Unprivileged code.
     * **Region 2 (`32-byte` `g_kernel_vault` inside SRAM):** Point Region 2 to `g_kernel_vault` and set its register so **Privileged** code can **Read and Write** it, but **Unprivileged** code can **only Read** it. Because Region 2 has a higher region number than Region 1 (`2 > 1`), Region 2 overrides Region 1 for those 32 bytes.

3. **Step 3 — Count Time and Debounce the Button Inside Privileged Interrupts:**
   * Because `g_kernel_vault` is Read-Only for Unprivileged code, the application loop cannot update `app_ms` or `accepted_press_count`.
   * Instead, when you press button `B1` (`PC13`), `EXTI15_10_IRQHandler` records the pin edge and `SysTick_Handler` (which runs every `1 ms` in **Privileged Handler mode** on `MSP`) checks that the button stayed stable for `20 ms` and increments `g_kernel_vault.accepted_press_count`.

4. **Step 4 — On Button Presses 1 and 2, Use `SVC` (`Supervisor Call`) to Request Privileged Changes:**
   * The Unprivileged application loop can **read** `g_kernel_vault.app_ms` and `g_kernel_vault.accepted_press_count` directly because Region 2 allows unprivileged reads.
   * However, Unprivileged code is not allowed to write `g_kernel_vault.blink_period_ms` and is not allowed to write the `GPIOA->BSRR` LED register.
   * So on the **1st and 2nd button presses**, the Unprivileged loop executes the assembly instruction `svc #1` (to change the blink speed between `500 ms` and `150 ms`) and `svc #2` (to toggle the LED `LD2`).
   * The `svc` instruction switches the CPU into **Privileged `SVC_Handler` on `MSP`**, which reads the request number (`1` or `2`), performs the write safely, and returns to the Unprivileged loop on `PSP`.

5. **Step 5 — On Button Press 3, Catch an Illegal Unprivileged Write in `MemManage_Handler`:**
   * On the **3rd button press**, the Unprivileged loop deliberately tries to write `g_kernel_vault.protected_cookie = 0xBAD0BEEFU;` directly.
   * The hardware `MPU` sees an **Unprivileged write** to **Region 2 (Read-Only for Unprivileged code)**.
   * The `MPU` **blocks the write in hardware** (so `protected_cookie` stays `0xCAFE1234U`) and immediately jumps to **`MemManage_Handler`**, which records the fault status (`MMFSR`), the exact blocked RAM address (`MMFAR`), and the instruction address (`PC`) that caused the fault, turns `LD2` steady ON, and stops.

```text
+-----------------------------------------------------------------------------------+
| PRIVILEGED SETUP IN main() (Thread Mode, Privileged, Stack = MSP)                 |
|   1. Configure 84 MHz clock, PA5 (LD2), PC13 (B1 interrupt), and 1 ms SysTick     |
|   2. Configure MPU regions:                                                       |
|        Region 0 -> 512 KB Flash (0x08000000): Priv & Unpriv Read + Execute        |
|        Region 1 -> 128 KB SRAM  (0x20000000): Priv & Unpriv Read + Write          |
|        Region 2 -> 32 B Vault in SRAM:        Priv Read/Write, Unpriv READ-ONLY   |
|   3. Switch CPU to Unprivileged Thread Mode on PSP (CONTROL = 3)                  |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
| UNPRIVILEGED APPLICATION LOOP (Thread Mode, Unprivileged, Stack = PSP)            |
|                                                                                   |
|   - Reads `app_ms` and `accepted_press_count` from Vault ----> ALLOWED (Read-Only)|
|                                                                                   |
|   - Press 1 & 2: Executes `svc #1` (speed) / `svc #2` (LED)                       |
|                  ----> Enters Privileged `SVC_Handler` (on MSP)                   |
|                  ----> Updates `blink_period_ms` or `GPIOA->BSRR` and returns     |
|                                                                                   |
|   - Press 3:     Tries `g_kernel_vault.protected_cookie = 0xBAD0BEEFU;`           |
|                  ----> BLOCKED by MPU Region 2 (Cookie stays 0xCAFE1234U!)        |
|                  ----> Enters Privileged `MemManage_Handler` (on MSP)             |
|                  ----> Records fault address (`MMFAR`) & PC, turns LD2 ON, stops  |
+-----------------------------------------------------------------------------------+
```

---

## Scene 1: Processor Privilege (`CONTROL`), Dual Stacks (`MSP` vs. `PSP`), And Vector Table Slots

### 1. How the `CONTROL` Register Selects Privilege Level and Stack Pointer

The Cortex-M4 CPU has an internal register called `CONTROL` ([DUI0553B](../../docs/stm32/um-cortexm4/DUI0553.pdf) Sec 2.1.3). Its lowest two bits control how **Thread mode** (normal C code) runs:

* **Bit `0` (`nPRIV` — Thread Mode Privilege):**
  * Set bit `0` to `0` (reset default) to run Thread mode as **Privileged**, which allows access to all memory, peripheral registers, and CPU control registers.
  * Set bit `0` to `1` to run Thread mode as **Unprivileged**, which blocks access to core registers (`SysTick`, `NVIC`, `SCB`, `MPU` at `0xE000E000..0xE00FFFFF`) and ignores instructions that disable interrupts (`cpsid i`) or change `CONTROL`.
  * **Otherwise:** If bit `0` stays `0`, a bug in the application loop can disable interrupts or overwrite system registers.
* **Bit `1` (`SPSEL` — Thread Mode Stack Pointer Select):**
  * Set bit `1` to `0` (reset default) so Thread mode uses the **Main Stack Pointer (`MSP`)**.
  * Set bit `1` to `1` so Thread mode uses the **Process Stack Pointer (`PSP`)**, while interrupt handlers (**Handler mode**) continue using **`MSP`**.
  * **Otherwise:** If the application loop and interrupt handlers share `MSP`, a stack overflow in the application loop overwrites the interrupt stack.

```text
CONTROL Register (Read/Write with MRS/MSR assembly instructions):
  Bit 1 (SPSEL) | Bit 0 (nPRIV) | Value | Meaning in Thread Mode
  --------------+---------------+-------+----------------------------------------------
        0       |       0       |  0x00 | Privileged,   uses MSP (Reset default)
        1       |       1       |  0x03 | Unprivileged, uses PSP (Our application loop)
  (Note: Handler mode is ALWAYS Privileged and ALWAYS uses MSP.)
```

```c
/* Read the CPU CONTROL register using the `mrs` instruction: */
static inline uint32_t __get_CONTROL(void)
{
    uint32_t result;
    __asm volatile ("mrs %0, control" : "=r" (result));
    return result; /* Bit 0 = nPRIV (0=Priv, 1=Unpriv), Bit 1 = SPSEL (0=MSP, 1=PSP) */
}

/* Allocate a separate 1 KB (256 words) stack in SRAM for Unprivileged Thread mode (PSP).
 * Cortex-M4 requires stacks to be 8-byte aligned (__attribute__((aligned(8)))).
 * Stacks grow downward from high address to low address, so the top is &app_psp_stack[256]: */
static uint32_t app_psp_stack[256] __attribute__((aligned(8)));
```

---

### 2. Adding `MemManage_Handler` (Slot `4`) and `SVC_Handler` (Slot `11`) to the Vector Table

In our vector table `g_pfnVectors[]` at `0x08000000`, we set two new hardware exception slots:
* **Point slot `4` (`0x08000010`) to `MemManage_Handler`** so the CPU jumps to `MemManage_Handler` whenever the `MPU` blocks an illegal memory access.
* **Point slot `11` (`0x0800002C`) to `SVC_Handler`** so the CPU jumps to `SVC_Handler` whenever code executes the `svc #imm` instruction.
* **Otherwise:** If slots `4` and `11` are left pointing to `Default_Handler`, an `MPU` fault or `svc` instruction traps in `Default_Handler` (`0xF0U`) instead of running our handlers.

```c
__attribute__((section(".isr_vector"), used))
const isr_vector_fn_t g_pfnVectors[113] = {
    [0]       = (isr_vector_fn_t)&_estack, /* 0x08000000: Initial MSP = 0x20020000 */
    [1]       = Reset_Handler,             /* 0x08000004: Reset (#1)               */
    [2]       = Default_Handler,           /* 0x08000008: NMI (#2)                 */
    [3]       = HardFault_Handler,         /* 0x0800000C: HardFault (#3)           */
    [4]       = MemManage_Handler,         /* 0x08000010: MemManage (#4) - MPU!    */
    [5]       = Default_Handler,           /* 0x08000014: BusFault (#5)            */
    [6]       = Default_Handler,           /* 0x08000018: UsageFault (#6)          */
    [11]      = SVC_Handler,               /* 0x0800002C: SVCall (#11) - Syscalls! */
    [12]      = Default_Handler,           /* 0x08000030: DebugMon (#12)           */
    [14]      = Default_Handler,           /* 0x08000038: PendSV (#14)             */
    [15]      = SysTick_Handler,           /* 0x0800003C: SysTick (#15)            */
    [16 + 40] = EXTI15_10_IRQHandler,      /* 0x080000E0: EXTI15_10 (IRQ 40, #56)  */
};
```

---

## Scene 2: How the Cortex-M4 `MPU` (`0xE000ED90`) Protects Memory Regions

### 1. The 5 Registers of the `MPU` Hardware Block (`0xE000ED90`)

The Cortex-M4 **Memory Protection Unit (`MPU`)** is a hardware block at address `0xE000ED90` ([DUI0553B](../../docs/stm32/um-cortexm4/DUI0553.pdf) Sec 4.5, Table 4-28) that supports `8` regions (`Region 0` through `Region 7`). It has 5 registers:

1. **`MPU->TYPE` (`+0x00`):** Bits `[15:8]` report how many regions exist in hardware (`8` on STM32F446).
2. **`MPU->CTRL` (`+0x04`):** Bit `0` (`ENABLE`) turns the `MPU` on; Bit `2` (`PRIVDEFENA`) keeps the default memory map open for Privileged code.
3. **`MPU->RNR` (`+0x08`):** Selects which region number (`0` to `7`) we are configuring right now.
4. **`MPU->RBAR` (`+0x0C`):** Holds the starting memory address of the selected region.
5. **`MPU->RASR` (`+0x10`):** Holds the region's **execute permission (`XN`, bit `28`)**, **read/write permission (`AP`, bits `[26:24]`)**, **size (`SIZE`, bits `[5:1]`)**, and **enable bit (`ENABLE`, bit `0`)**.

```c
/* Cortex-M4 MPU register block at 0xE000ED90 (DUI0553B Table 4-28): */
typedef struct {
    volatile const uint32_t TYPE; /* +0x00: bits [15:8] = number of regions (8)     */
    volatile uint32_t       CTRL; /* +0x04: bit 2 = PRIVDEFENA, bit 0 = ENABLE      */
    volatile uint32_t       RNR;  /* +0x08: selects region number 0..7              */
    volatile uint32_t       RBAR; /* +0x0C: region start address                    */
    volatile uint32_t       RASR; /* +0x10: permissions, size, and region enable    */
} MPU_Type;

#define MPU ((MPU_Type *)0xE000ED90UL)
```

---

### 2. How `MPU->RASR` Encodes Region Size, Alignment, and Permissions

When we configure a memory region in `MPU->RASR`, the hardware checks three rules:

* **Rule 1 — Region Size (`SIZE` in `MPU->RASR` bits `[5:1]`):**
  * Hardware calculates the region size as $\text{Bytes} = 2^{(\text{SIZE} + 1)}$, which means $\text{SIZE} = \log_2(\text{Bytes}) - 1$:
    * For **`32 bytes`** ($2^5$ bytes, minimum allowed region size): set `SIZE = 5 - 1 = 4` (`4UL << 1`).
    * For **`128 KB`** ($2^{17}$ bytes, full SRAM): set `SIZE = 17 - 1 = 16` (`16UL << 1`).
    * For **`512 KB`** ($2^{19}$ bytes, full Flash): set `SIZE = 19 - 1 = 18` (`18UL << 1`).
* **Rule 2 — Starting Address Alignment:**
  * A region of size $2^N$ bytes **must start at an address that is a multiple of $2^N$**.
  * Our `g_kernel_vault` structure is `32 bytes` (`8` `uint32_t` words), so we align its starting address to a `32-byte` boundary using `__attribute__((aligned(32)))`.
  * **Otherwise:** If `g_kernel_vault` is not 32-byte aligned, hardware clears the lowest 5 bits of `MPU->RBAR` and protects the wrong 32 bytes in RAM.
* **Rule 3 — Access Permissions (`AP` in `MPU->RASR` bits `[26:24]`) and Execute-Never (`XN` in bit `28`):**
  * Set **`XN = 0` (`0UL << 28`)** to allow instruction execution (used for Flash), or **`XN = 1` (`1UL << 28`)** to block instruction execution (used for SRAM).
  * Set **`AP = 6` (`6UL << 24`)** to allow **Read-Only** access for both Privileged and Unprivileged code (used for Flash).
  * Set **`AP = 3` (`3UL << 24`)** to allow **Read and Write** access for both Privileged and Unprivileged code (used for general SRAM).
  * Set **`AP = 2` (`2UL << 24`)** to allow **Read and Write** access for Privileged code, but **Read-Only** access for Unprivileged code (used for `g_kernel_vault`).

```c
/* 32-byte structure in SRAM protected by MPU Region 2:
 * 8 uint32_t fields = 32 bytes, aligned to a 32-byte boundary. */
typedef struct {
    volatile uint32_t app_ms;               /* +0x00: 1 ms counter updated by SysTick_Handler */
    volatile uint32_t blink_period_ms;      /* +0x04: 500 or 150 ms, updated by SVC #1        */
    volatile uint32_t accepted_press_count; /* +0x08: debounced button press count            */
    volatile uint32_t svc_call_count;       /* +0x0C: number of SVC calls handled             */
    volatile uint32_t protected_cookie;     /* +0x10: 0xCAFE1234U (unprivileged write faults!)*/
    volatile uint32_t reserved[3];          /* +0x14..0x1F: padding so total size = 32 bytes  */
} __attribute__((aligned(32))) kernel_vault_t;

static kernel_vault_t g_kernel_vault;
```

---

### 3. Configuring Regions 0, 1, and 2 in `mpu_init()`

On Cortex-M4, **when two enabled `MPU` regions overlap, the higher region number overrides the lower region number (`Region 2` overrides `Region 1`)**. We configure the `MPU` in 6 steps:

1. **Disable the `MPU` (`MPU->CTRL = 0`) during setup and enable the `MemManage` fault handler by setting bit `16` (`MEMFAULTENA = 1`) in `SCB->SHCSR`.**
   * **Otherwise:** If `SCB->SHCSR` bit `16` is left at `0` (reset default), any `MPU` permission fault escalates into `HardFault_Handler` (`#3`) instead of running `MemManage_Handler` (`#4`).
2. **Point Region 0 (`MPU->RNR = 0`) to the `512 KB` Flash at `0x08000000` (`MPU->RBAR = 0x08000000UL`) and protect it by setting `MPU->RASR` with `XN = 0` (execute allowed), `AP = 6` (Privileged & Unprivileged Read-Only), `SIZE = 18` (`512 KB`), and `ENABLE = 1`.**
   * **Otherwise:** As soon as the CPU switches to Unprivileged mode, instruction fetches from Flash would be blocked and fault immediately.
3. **Point Region 1 (`MPU->RNR = 1`) to the `128 KB` SRAM at `0x20000000` (`MPU->RBAR = 0x20000000UL`) and protect it by setting `MPU->RASR` with `XN = 1` (no execute), `AP = 3` (Privileged & Unprivileged Read/Write), `SIZE = 16` (`128 KB`), and `ENABLE = 1`.**
   * **Otherwise:** Unprivileged code would not be allowed to read or write its own stack (`app_psp_stack`) or normal variables in SRAM.
4. **Point Region 2 (`MPU->RNR = 2`) to the `32-byte` `g_kernel_vault` (`MPU->RBAR = (uint32_t)&g_kernel_vault`) and protect it by setting `MPU->RASR` with `XN = 1`, `AP = 2` (Privileged Read/Write, Unprivileged Read-Only), `SIZE = 4` (`32 bytes`), and `ENABLE = 1`.**
   * **Otherwise:** Because `g_kernel_vault` is inside SRAM, Region 1 alone would allow Unprivileged code to overwrite it. Because **`Region 2 > Region 1`**, Region 2 overrides Region 1 for those 32 bytes and makes them Read-Only for Unprivileged code.
5. **Clear unused Regions `3` through `7` by setting `MPU->RBAR = 0` and `MPU->RASR = 0`.**
   * **Otherwise:** Leftover values in unused regions could match unexpected addresses.
6. **Turn the `MPU` ON by setting `MPU->CTRL` bit `2` (`PRIVDEFENA = 1`) and bit `0` (`ENABLE = 1`).**
   * **Otherwise:** If `PRIVDEFENA` (`bit 2`) is `0`, even **Privileged** interrupt handlers (`SysTick`, `EXTI15_10`, `SVC`) would fault when accessing peripheral registers (`GPIOA`, `GPIOC`, `EXTI`, `RCC` at `0x40000000+`) because we did not create an `MPU` region for peripherals.

```text
SRAM (0x20000000 .. 0x2001FFFF, 128 KB):
+-------------------------------------------------------------------------+
| Region 1 (128 KB, SIZE = 16, AP = 3):                                   |
|   Privileged: Read/Write   |   Unprivileged: Read/Write                 |
|                                                                         |
|   +-----------------------------------------------------------------+   |
|   | Region 2: g_kernel_vault (32 Bytes, SIZE = 4, AP = 2)           |   |
|   |   Region 2 > Region 1, so Region 2 wins for these 32 bytes:     |   |
|   |   Privileged: Read/Write   |   Unprivileged: READ-ONLY!         |   |
|   +-----------------------------------------------------------------+   |
+-------------------------------------------------------------------------+
```

```c
static void mpu_init(void)
{
    /* 1. Verify hardware has >= 3 MPU regions and g_kernel_vault is 32-byte aligned: */
    if (((MPU->TYPE >> 8U) & 0xFFUL) < 3U) {
        fault_stop(9U);
    }
    if ((((uintptr_t)&g_kernel_vault & 0x1FU) != 0U) || (sizeof(g_kernel_vault) != 32U)) {
        fault_stop(10U);
    }

    /* 2. Disable MPU during setup and enable MemManage fault (SCB->SHCSR bit 16 = 1): */
    __DSB();
    __ISB();
    MPU->CTRL   = 0U;
    SCB->SHCSR |= (1UL << 16); /* MEMFAULTENA = 1 */

    /* 3. Point Region 0 to 512 KB Flash (0x08000000):
     *    XN=0 (allow execute), AP=6 (Priv/Unpriv Read-Only), C=1, SIZE=18 (512 KB), ENABLE=1: */
    MPU->RNR  = 0U;
    MPU->RBAR = 0x08000000UL;
    MPU->RASR = (0UL << 28) | (6UL << 24) | (1UL << 17) | (18UL << 1) | 1UL;

    /* 4. Point Region 1 to 128 KB SRAM (0x20000000):
     *    XN=1 (no execute), AP=3 (Priv/Unpriv Read/Write), S=1, C=1, SIZE=16 (128 KB), ENABLE=1: */
    MPU->RNR  = 1U;
    MPU->RBAR = 0x20000000UL;
    MPU->RASR = (1UL << 28) | (3UL << 24) | (1UL << 18) | (1UL << 17) | (16UL << 1) | 1UL;

    /* 5. Point Region 2 to 32-byte g_kernel_vault (overrides Region 1):
     *    XN=1, AP=2 (Priv Read/Write, Unpriv READ-ONLY), S=1, C=1, SIZE=4 (32 B), ENABLE=1: */
    MPU->RNR  = 2U;
    MPU->RBAR = (uint32_t)(uintptr_t)&g_kernel_vault;
    MPU->RASR = (1UL << 28) | (2UL << 24) | (1UL << 18) | (1UL << 17) | (4UL << 1) | 1UL;

    /* 6. Clear unused Regions 3..7 and turn MPU ON with PRIVDEFENA (bit 2) + ENABLE (bit 0): */
    for (uint32_t r = 3U; r < 8U; ++r) {
        MPU->RNR  = r;
        MPU->RBAR = 0U;
        MPU->RASR = 0U;
    }
    MPU->CTRL = (1UL << 2) | 1UL;
    __DSB();
    __ISB();
}
```

---

## Scene 3: Interrupt Priorities And Privileged Button Debounce In `SysTick_Handler`

### 1. Giving `SysTick`, `EXTI15_10`, and `SVCall` the Same Priority (`5`) So They Never Preempt Each Other

In Unprivileged mode, hardware ignores `__disable_irq()`. To make sure our three interrupt/system-call handlers (`SysTick_Handler`, `EXTI15_10_IRQHandler`, and `SVC_Handler`) never interrupt each other while updating `g_kernel_vault`:
* Set `MemManage` (`SCB->SHP[0]`) to **Priority `0` (`0x00`, highest urgency)** so an `MPU` fault is handled immediately.
* Set `SysTick` (`SCB->SHP[11]`), `EXTI15_10` (`NVIC->IP[40]`), and `SVCall` (`SCB->SHP[7]`) all to the **same Preemption Priority `5` (`5U << 4 = 0x50`)**.
* **Otherwise:** If they had different priorities, one handler could preempt another in the middle of updating `g_kernel_vault`. Because all three share Priority `5`, the Cortex-M4 `NVIC` runs them **one at a time** without racing.

```text
STM32F446 PERIPHERALS (Outside CPU)                  ARM CORTEX-M4 CORE (Inside CPU)
+----------------------------------------------+     +-----------------------------------------------+
| [PC13 Pin] -> [SYSCFG EXTICR4] -> [EXTI13]   |---->| [NVIC IRQ 40]  Priority 5 \                   |
+----------------------------------------------+     | [SysTick #15]  Priority 5  +-> Run one at a   |
                                                     | [SVCall  #11]  Priority 5 /    time on MSP!   |
                                                     |                                               |
                                                     | [MemManage #4] Priority 0 ---> Highest urgency|
                                                     +-----------------------------------------------+
```

```c
    /* Set 4 preemption-priority bits (PRIGROUP = 3 in SCB->AIRCR bits [10:8]): */
    uint32_t aircr = SCB->AIRCR & ~((0xFFFFUL << 16) | (7UL << 8));
    SCB->AIRCR = aircr | (0x5FAUL << 16) | (3UL << 8);

    /* Set MemManage to Priority 0, and SysTick / EXTI15_10 / SVCall all to Priority 5: */
    SCB->SHP[0]  = (uint8_t)(0U << 4); /* MemManage (#4)     = Priority 0 (highest) */
    SCB->SHP[11] = (uint8_t)(5U << 4); /* SysTick   (#15)    = Priority 5           */
    NVIC->IP[40] = (uint8_t)(5U << 4); /* EXTI15_10 (IRQ 40) = Priority 5           */
    SCB->SHP[7]  = (uint8_t)(5U << 4); /* SVCall    (#11)    = Priority 5           */
```

---

### 2. Updating `app_ms` and `accepted_press_count` Inside Privileged `SysTick_Handler`

Because `g_kernel_vault` is **Read-Only** for Unprivileged code (`MPU` Region 2), the Unprivileged loop cannot update `app_ms` or `accepted_press_count`.

Instead, `SysTick_Handler` runs every `1 ms` in **Privileged Handler mode on `MSP`**. Because Handler mode is Privileged, `MPU` Region 2 allows `SysTick_Handler` to increment `g_kernel_vault.app_ms` every millisecond and increment `g_kernel_vault.accepted_press_count` whenever button `B1` (`PC13`) has been stable for `20` ticks.

```c
void SysTick_Handler(void)
{
    /* Privileged Handler mode on MSP: allowed to write MPU Region 2 (g_kernel_vault): */
    uint32_t now_ms = g_kernel_vault.app_ms + 1U;
    g_kernel_vault.app_ms = now_ms;

    if (b1_irq_pending != 0U) {
        b1_irq_pending     = 0U;
        candidate_since_ms = b1_irq_tick_ms;
        candidate_pressed  = button_pressed();
        settling           = true;
    }

    if (settling) {
        bool raw_pressed = button_pressed();
        if (raw_pressed != candidate_pressed) {
            candidate_pressed  = raw_pressed;
            candidate_since_ms = now_ms;
        }
        if (((EXTI->PR & B1_MASK) == 0U) && ((now_ms - candidate_since_ms) >= 20U)) {
            settling = false;
            if (stable_pressed != candidate_pressed) {
                stable_pressed = candidate_pressed;
                if (!stable_pressed) {
                    armed = true;
                } else if (armed) {
                    armed = false;
                    ++g_kernel_vault.accepted_press_count; /* Privileged write to Vault */
                }
            }
        }
    }
}
```

---

## Scene 4: How `SVC_Handler` And `MemManage_Handler` Work

### 1. Checking Bit `2` of `LR` (`tst lr, #4`) to Find the 8 Saved Registers on `PSP` or `MSP`

When the Unprivileged loop (running on `PSP`) executes `svc #1` or triggers a `MemManage` fault:
1. The CPU hardware automatically pushes 8 registers (`R0, R1, R2, R3, R12, LR, Return_PC, xPSR`) onto **`PSP`**.
2. Then the CPU switches to **Handler mode** (which uses **`MSP`**) and writes a special return value (`EXC_RETURN = 0xFFFFFFFD`) into `LR`.
3. **Bit `2` (value `4`) of `LR`** tells the handler which stack pointer holds those 8 pushed words:
   * If bit `2` of `LR` is `0`, the 8 words were pushed onto **`MSP`**.
   * If bit `2` of `LR` is `1`, the 8 words were pushed onto **`PSP`**.
* **Otherwise:** If `SVC_Handler` or `MemManage_Handler` read from `MSP` without checking bit `2` of `LR`, it would read the wrong stack and miss the saved `Return_PC` (`frame[6]`).

```text
8-Word Hardware Frame Pushed Onto PSP When `svc` or Fault Occurs:
  frame[0] = R0
  frame[1] = R1
  frame[2] = R2
  frame[3] = R3
  frame[4] = R12
  frame[5] = LR
  frame[6] = Return_PC  <-- Address of next instruction (or faulting instruction!)
  frame[7] = xPSR
```

```c
/* Naked assembly entry for SVC_Handler:
 * Tests bit 2 of LR (`tst lr, #4`), copies the interrupted stack pointer (MSP or PSP)
 * into R0 (the 1st C function parameter `uint32_t *frame`), and jumps to `svc_handler_c`: */
__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        "tst lr, #4         \n" /* Test bit 2 of EXC_RETURN in LR (0 = MSP, 1 = PSP) */
        "ite eq             \n"
        "mrseq r0, msp      \n" /* If bit 2 == 0, R0 = MSP */
        "mrsne r0, psp      \n" /* If bit 2 == 1, R0 = PSP */
        "b svc_handler_c    \n" /* Call svc_handler_c(uint32_t *frame) */
    );
}
```

---

### 2. Reading the `SVC` Number (`#1` or `#2`) in `svc_handler_c()`

When Unprivileged code executes `svc #1` or `svc #2`:
* The Thumb `svc` instruction is `2 bytes` long in Flash (`0xDF01` for `svc #1`, `0xDF02` for `svc #2`), where the byte at `Return_PC - 2` holds the request number (`1` or `2`).
* Inside `svc_handler_c(uint32_t *frame)`, `frame[6]` is `Return_PC`. Reading `((const uint8_t *)(uintptr_t)frame[6])[-2]` extracts that request number (`1` or `2`).
* Because `svc_handler_c` runs in **Privileged Handler mode**, it can legally write `g_kernel_vault.blink_period_ms` (`SVC #1`) or toggle `LD2` via `GPIOA->BSRR` (`SVC #2`).
* **Otherwise:** Unprivileged code cannot write `blink_period_ms` or `GPIOA->BSRR` directly without faulting.

```c
__attribute__((used))
static void svc_handler_c(uint32_t *frame)
{
    /* frame[6] is Return_PC (2 bytes after the start of the `svc #imm` instruction).
     * Reading return_pc[-2] fetches the request number (1 or 2): */
    const uint8_t *return_pc = (const uint8_t *)(uintptr_t)frame[6];
    uint8_t svc_number = return_pc[-2];

    ++g_kernel_vault.svc_call_count;

    if (svc_number == 1U) {
        /* SVC #1: Privileged write to toggle blink_period_ms (500 ms <-> 150 ms): */
        uint32_t cur = g_kernel_vault.blink_period_ms;
        g_kernel_vault.blink_period_ms = (cur == 500U) ? 150U : 500U;
    } else if (svc_number == 2U) {
        /* SVC #2: Privileged write to toggle LD2 (PA5) via GPIOA->BSRR: */
        led_on = !led_on;
        GPIOA->BSRR = led_on ? LED_MASK : (LED_MASK << 16);
    }
}
```

---

### 3. Recording the Blocked RAM Address and Instruction in `MemManage_Handler` (`#4`)

When the Unprivileged loop tries to write `g_kernel_vault.protected_cookie = 0xBAD0BEEFU;` on the 3rd button press, `MPU` Region 2 blocks the write (`protected_cookie` remains `0xCAFE1234U`) and jumps to `MemManage_Handler`:
* Read the lowest 8 bits of `SCB->CFSR` (`0xE000ED28`), which is the **MemManage Fault Status Register (`MMFSR`)**:
  * **Bit `1` (`DACCVIOL = 0x02`):** A data read/write violated `MPU` permissions.
  * **Bit `7` (`MMARVALID = 0x80`):** `SCB->MMFAR` holds the valid data address that caused the fault (`0x02 | 0x80 = 0x82`).
* When bit `7` is `1`, read **`SCB->MMFAR` (`0xE000ED34`)** to get the exact RAM address that was blocked (`&g_kernel_vault.protected_cookie`), and read **`frame[6]`** to get the exact `PC` of the store instruction that failed.
* **Otherwise:** Without saving `MMFSR`, `SCB->MMFAR`, and `frame[6]`, you would not know which address or instruction caused the `MPU` fault.

```c
__attribute__((used))
static _Noreturn void memmanage_handler_c(uint32_t *frame)
{
    /* 1. Read lowest byte of SCB->CFSR (MMFSR: 0x82 = DACCVIOL bit 1 | MMARVALID bit 7): */
    uint8_t mmfsr        = (uint8_t)(SCB->CFSR & 0xFFUL);
    mmfsr_snapshot       = mmfsr;

    /* 2. If MMARVALID (bit 7) is 1, read SCB->MMFAR for the exact blocked RAM address: */
    mmfar_snapshot       = ((mmfsr & (1U << 7)) != 0U) ? SCB->MMFAR : 0U;

    /* 3. Save the faulting instruction address from frame[6], turn LD2 ON, and stop: */
    faulting_pc_snapshot = frame[6];
    GPIOA->BSRR          = LED_MASK;
    fault_stop(0xE4U);
}

__attribute__((naked))
void MemManage_Handler(void)
{
    __asm volatile (
        "tst lr, #4              \n"
        "ite eq                  \n"
        "mrseq r0, msp           \n"
        "mrsne r0, psp           \n"
        "b memmanage_handler_c   \n"
    );
}
```

---

## Scene 5: Switching To Unprivileged `PSP` And Running The Application Loop

### 1. Switching from Privileged `MSP` to Unprivileged `PSP` Using `enter_unprivileged_psp_loop()`

At the end of `main()`, we call a small assembly function `enter_unprivileged_psp_loop(&app_psp_stack[256])` that does 4 instructions:
1. `msr psp, r0`: Point `PSP` to `&app_psp_stack[256]`.
2. `movs r1, #3` and `msr control, r1`: Write `3` to `CONTROL` so bit `1` (`SPSEL = 1`) selects `PSP` and bit `0` (`nPRIV = 1`) switches the CPU to **Unprivileged Thread mode**.
3. `isb 0xF`: Flush the pipeline so all following instructions run in Unprivileged mode on `PSP`.
4. `b unprivileged_app_loop`: Jump into `unprivileged_app_loop()`.
* **Otherwise:** Changing `CONTROL` from `MSP` to `PSP` inside an ordinary C function would switch stacks while that C function's local variables were still on `MSP`.

```text
Before `enter_unprivileged_psp_loop`:        After `enter_unprivileged_psp_loop`:
+-----------------------------------+        +-----------------------------------+
| Mode:      Thread, PRIVILEGED     |        | Mode:      Thread, UNPRIVILEGED   |
| CONTROL:   0x00 (SPSEL=0,nPRIV=0) | -----> | CONTROL:   0x03 (SPSEL=1,nPRIV=1) |
| Active SP: MSP  (0x20020000)      |        | Active SP: PSP  (app_psp_stack)   |
+-----------------------------------+        +-----------------------------------+
```

```c
__attribute__((naked))
static _Noreturn void enter_unprivileged_psp_loop(uint32_t psp_top)
{
    __asm volatile (
        "msr psp, r0              \n" /* 1. Load PSP with &app_psp_stack[256] (in R0)   */
        "movs r1, #3              \n" /* 2. Set CONTROL = 3 (bit 1=PSP, bit 0=Unpriv)   */
        "msr control, r1          \n"
        "isb 0xF                  \n" /* 3. Flush pipeline so new privilege takes effect*/
        "b unprivileged_app_loop  \n" /* 4. Jump to unprivileged C loop using PSP       */
    );
}
```

---

### 2. What `unprivileged_app_loop()` Does on Button Presses 1, 2, and 3

Inside `unprivileged_app_loop()`:
1. Read `g_kernel_vault.app_ms` and `g_kernel_vault.accepted_press_count` (allowed because `MPU` Region 2 permits Unprivileged reads).
2. On **button presses 1 and 2**, execute `svc #1` to toggle `blink_period_ms` (`500 ms <-> 150 ms`), and execute `svc #2` each period to blink `LD2`.
3. On **button press 3** (or when `trigger_mpu_fault = 1` in the debugger), execute `g_kernel_vault.protected_cookie = 0xBAD0BEEFU;`, which `MPU` Region 2 blocks and catches in `MemManage_Handler`.

```c
__attribute__((used))
static _Noreturn void unprivileged_app_loop(void)
{
    uint32_t last_toggle_ms  = g_kernel_vault.app_ms;
    uint32_t last_seen_press = g_kernel_vault.accepted_press_count;

    for (;;) {
        /* 1. Legal unprivileged reads from MPU Region 2 (AP = 2 allows reading): */
        uint32_t now_ms    = g_kernel_vault.app_ms;
        uint32_t press_seq = g_kernel_vault.accepted_press_count;

        /* 2. When SysTick_Handler accepts a new button press: */
        if (press_seq != last_seen_press) {
            last_seen_press = press_seq;
            if (press_seq >= 3U) {
                trigger_mpu_fault = 1U;                   /* 3rd press triggers fault demo */
            } else {
                __asm volatile ("svc #1" : : : "memory"); /* Press 1 & 2: SVC #1 changes speed */
            }
        }

        /* 3. Deliberate illegal unprivileged write to Region 2 (Read-Only):
         *    MPU blocks this write (cookie stays 0xCAFE1234U) and runs MemManage_Handler! */
        if (trigger_mpu_fault != 0U) {
            g_kernel_vault.protected_cookie = 0xBAD0BEEFU;
        }

        /* 4. Every blink_period_ms ticks, call SVC #2 to toggle LD2: */
        if ((now_ms - last_toggle_ms) >= g_kernel_vault.blink_period_ms) {
            last_toggle_ms = now_ms;
            __asm volatile ("svc #2" : : : "memory");
        }
    }
}
```

---

## Complete Standalone Code: `Core/Src/mpu_privilege_app.c`

Link with `STM32F446RETX_FLASH.ld` and `-nostartfiles` in `RegistersDebug`:

```c
#include <stdbool.h>
#include <stdint.h>

#define LED_MASK   (1UL << 5)
#define B1_MASK    (1UL << 13)
#define IRQ40_MASK (1UL << 8)
#define POLL_LIMIT 1000000U

static inline void __disable_irq(void) { __asm volatile ("cpsid i" : : : "memory"); }
static inline void __enable_irq(void)  { __asm volatile ("cpsie i" : : : "memory"); }
static inline void __NOP(void)         { __asm volatile ("nop"); }
static inline void __DSB(void)         { __asm volatile ("dsb 0xF" : : : "memory"); }
static inline void __ISB(void)         { __asm volatile ("isb 0xF" : : : "memory"); }

static inline uint32_t __get_IPSR(void)
{
    uint32_t result;
    __asm volatile ("mrs %0, ipsr" : "=r" (result));
    return result;
}

static inline uint32_t __get_CONTROL(void)
{
    uint32_t result;
    __asm volatile ("mrs %0, control" : "=r" (result));
    return result;
}

static inline void __set_BASEPRI(uint32_t basePri)
{
    __asm volatile ("msr basepri, %0" : : "r" (basePri) : "memory");
}

typedef struct { volatile uint32_t ACR, KEYR, OPTKEYR, SR, CR, OPTCR; } FLASH_TypeDef;
typedef struct { volatile uint32_t CR, CSR; } PWR_TypeDef;
typedef struct {
    volatile uint32_t CR, PLLCFGR, CFGR, CIR;
    volatile uint32_t AHB1RSTR, AHB2RSTR, AHB3RSTR, RESERVED0;
    volatile uint32_t APB1RSTR, APB2RSTR, RESERVED1[2];
    volatile uint32_t AHB1ENR, AHB2ENR, AHB3ENR, RESERVED2;
    volatile uint32_t APB1ENR, APB2ENR;
} RCC_TypeDef;
typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR, LCKR, AFR[2];
} GPIO_TypeDef;
typedef struct { volatile uint32_t MEMRMP, PMC, EXTICR[4]; } SYSCFG_TypeDef;
typedef struct { volatile uint32_t IMR, EMR, RTSR, FTSR, SWIER, PR; } EXTI_TypeDef;
typedef struct { volatile uint32_t CTRL, LOAD, VAL, CALIB; } SysTick_Type;
typedef struct {
    volatile uint32_t ISER[8], RESERVED0[24], ICER[8], RESERVED1[24];
    volatile uint32_t ISPR[8], RESERVED2[24], ICPR[8], RESERVED3[24];
    volatile uint32_t IABR[8], RESERVED4[56];
    volatile uint8_t  IP[240];
} NVIC_Type;
typedef struct {
    volatile uint32_t CPUID, ICSR, VTOR, AIRCR, SCR, CCR;
    volatile uint8_t  SHP[12];
    volatile uint32_t SHCSR, CFSR, HFSR, DFSR, MMFAR, BFAR, AFSR, RESERVED0[18], CPACR;
} SCB_Type;

/* Cortex-M4 MPU at 0xE000ED90 (DUI0553B Table 4-28): */
typedef struct {
    volatile const uint32_t TYPE;
    volatile uint32_t       CTRL, RNR, RBAR, RASR;
} MPU_Type;

#define PWR     ((PWR_TypeDef *)   0x40007000UL)
#define SYSCFG  ((SYSCFG_TypeDef *)0x40013800UL)
#define EXTI    ((EXTI_TypeDef *)  0x40013C00UL)
#define GPIOA   ((GPIO_TypeDef *)  0x40020000UL)
#define GPIOC   ((GPIOC_TypeDef *) 0x40020800UL)
#define RCC     ((RCC_TypeDef *)   0x40023800UL)
#define FLASH   ((FLASH_TypeDef *) 0x40023C00UL)
#define SysTick ((SysTick_Type *)  0xE000E010UL)
#define NVIC    ((NVIC_Type *)     0xE000E100UL)
#define SCB     ((SCB_Type *)      0xE000ED00UL)
#define MPU     ((MPU_Type *)      0xE000ED90UL)

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;

void Reset_Handler(void);
void Default_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void SVC_Handler(void);
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
    [4]       = MemManage_Handler,         /* #4:  MPU MemManage Fault */
    [5]       = Default_Handler,
    [6]       = Default_Handler,
    [11]      = SVC_Handler,               /* #11: Supervisor Call     */
    [12]      = Default_Handler,
    [14]      = Default_Handler,
    [15]      = SysTick_Handler,           /* #15: 1 ms Tick + Debounce*/
    [16 + 40] = EXTI15_10_IRQHandler,      /* #56: B1 Button Edge      */
};

/* 32-Byte MPU-Protected Kernel Vault (Region 2: Priv R/W, Unpriv Read-Only): */
typedef struct {
    volatile uint32_t app_ms;
    volatile uint32_t blink_period_ms;
    volatile uint32_t accepted_press_count;
    volatile uint32_t svc_call_count;
    volatile uint32_t protected_cookie;
    volatile uint32_t reserved[3];
} __attribute__((aligned(32))) kernel_vault_t;

static kernel_vault_t g_kernel_vault;
static uint32_t       app_psp_stack[256] __attribute__((aligned(8)));

uint32_t SystemCoreClock = 16000000U;

static volatile uint32_t b1_irq_pending, b1_irq_tick_ms, b1_irq_count;
static volatile uint32_t candidate_since_ms;
static volatile bool     candidate_pressed, stable_pressed, armed, settling, led_on;
static volatile uint32_t fault_code;
static volatile uint8_t  mmfsr_snapshot;
static volatile uint32_t mmfar_snapshot;
static volatile uint32_t faulting_pc_snapshot;
static volatile uint32_t trigger_mpu_fault;

static _Noreturn void fault_stop(uint32_t code)
{
    __disable_irq();
    fault_code = code;
    for (;;) {
        __NOP();
    }
}

void Default_Handler(void)   { fault_stop(0xF0U); }
void HardFault_Handler(void) { fault_stop(0xF1U); }

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

static void mpu_init(void)
{
    if (((MPU->TYPE >> 8U) & 0xFFUL) < 3U) {
        fault_stop(9U);
    }
    if ((((uintptr_t)&g_kernel_vault & 0x1FU) != 0U) ||
        (sizeof(g_kernel_vault) != 32U)) {
        fault_stop(10U);
    }

    __DSB();
    __ISB();
    MPU->CTRL   = 0U;
    SCB->SHCSR |= (1UL << 16);              /* MEMFAULTENA = 1 */

    MPU->RNR  = 0U;                         /* Region 0: Flash 512 KB, Priv/Unpriv RO, Exec */
    MPU->RBAR = 0x08000000UL;
    MPU->RASR = (0UL << 28) | (6UL << 24) | (1UL << 17) | (18UL << 1) | 1UL;

    MPU->RNR  = 1U;                         /* Region 1: SRAM 128 KB, Priv/Unpriv RW, XN=1  */
    MPU->RBAR = 0x20000000UL;
    MPU->RASR = (1UL << 28) | (3UL << 24) | (1UL << 18) | (1UL << 17) | (16UL << 1) | 1UL;

    MPU->RNR  = 2U;                         /* Region 2: Vault 32 B, Priv RW / Unpriv RO    */
    MPU->RBAR = (uint32_t)(uintptr_t)&g_kernel_vault;
    MPU->RASR = (1UL << 28) | (2UL << 24) | (1UL << 18) | (1UL << 17) | (4UL << 1) | 1UL;

    for (uint32_t r = 3U; r < 8U; ++r) {
        MPU->RNR  = r;
        MPU->RBAR = 0U;
        MPU->RASR = 0U;
    }

    MPU->CTRL = (1UL << 2) | 1UL;           /* PRIVDEFENA | ENABLE */
    __DSB();
    __ISB();
}

static bool button_pressed(void)
{
    return (GPIOC->IDR & B1_MASK) == 0U;
}

static void exti_nvic_systick_init(void)
{
    NVIC->ICER[1] = IRQ40_MASK;
    EXTI->IMR    &= ~B1_MASK;

    RCC->APB2ENR |= 1UL << 14;
    (void)RCC->APB2ENR;
    __DSB();
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~(15UL << 4)) | (2UL << 4);

    EXTI->EMR  &= ~B1_MASK;
    EXTI->RTSR |= B1_MASK;
    EXTI->FTSR |= B1_MASK;

    uint32_t aircr = SCB->AIRCR & ~((0xFFFFUL << 16) | (7UL << 8));
    SCB->AIRCR = aircr | (0x5FAUL << 16) | (3UL << 8);
    SCB->VTOR  = (uint32_t)g_pfnVectors;
    SCB->CCR  |= 1UL << 9;

    /* MemManage = prio 0; SysTick, EXTI15_10, and SVCall share prio 5 (serialized): */
    SCB->SHP[0]   = (uint8_t)(0U << 4);
    SCB->SHP[11]  = (uint8_t)(5U << 4);
    NVIC->IP[40]  = (uint8_t)(5U << 4);
    SCB->SHP[7]   = (uint8_t)(5U << 4);

    EXTI->PR      = B1_MASK;
    NVIC->ICPR[1] = IRQ40_MASK;
    EXTI->IMR    |= B1_MASK;
    NVIC->ISER[1] = IRQ40_MASK;

    SysTick->LOAD = 84000U - 1U;
    SysTick->VAL  = 0U;
    SysTick->CTRL = (1UL << 2) | (1UL << 1) | 1UL;
    __DSB();
    __ISB();
}

void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & B1_MASK) != 0U) {
        EXTI->PR       = B1_MASK;
        b1_irq_tick_ms = g_kernel_vault.app_ms;
        b1_irq_pending = 1U;
        ++b1_irq_count;
        __DSB();
    }
}

void SysTick_Handler(void)
{
    uint32_t now_ms = g_kernel_vault.app_ms + 1U;
    g_kernel_vault.app_ms = now_ms;

    if (b1_irq_pending != 0U) {
        b1_irq_pending     = 0U;
        candidate_since_ms = b1_irq_tick_ms;
        candidate_pressed  = button_pressed();
        settling           = true;
    }

    if (settling) {
        bool raw_pressed = button_pressed();
        if (raw_pressed != candidate_pressed) {
            candidate_pressed  = raw_pressed;
            candidate_since_ms = now_ms;
        }
        if (((EXTI->PR & B1_MASK) == 0U) &&
            ((now_ms - candidate_since_ms) >= 20U)) {
            settling = false;
            if (stable_pressed != candidate_pressed) {
                stable_pressed = candidate_pressed;
                if (!stable_pressed) {
                    armed = true;
                } else if (armed) {
                    armed = false;
                    ++g_kernel_vault.accepted_press_count;
                }
            }
        }
    }
}

__attribute__((used))
static void svc_handler_c(uint32_t *frame)
{
    const uint8_t *return_pc = (const uint8_t *)(uintptr_t)frame[6];
    uint8_t svc_number = return_pc[-2];

    ++g_kernel_vault.svc_call_count;

    if (svc_number == 1U) {
        uint32_t cur = g_kernel_vault.blink_period_ms;
        g_kernel_vault.blink_period_ms = (cur == 500U) ? 150U : 500U;
    } else if (svc_number == 2U) {
        led_on = !led_on;
        GPIOA->BSRR = led_on ? LED_MASK : (LED_MASK << 16);
    }
}

__attribute__((naked))
void SVC_Handler(void)
{
    __asm volatile (
        "tst lr, #4         \n"
        "ite eq             \n"
        "mrseq r0, msp      \n"
        "mrsne r0, psp      \n"
        "b svc_handler_c    \n"
    );
}

__attribute__((used))
static _Noreturn void memmanage_handler_c(uint32_t *frame)
{
    uint8_t mmfsr        = (uint8_t)(SCB->CFSR & 0xFFUL);
    mmfsr_snapshot       = mmfsr;
    mmfar_snapshot       = ((mmfsr & (1U << 7)) != 0U) ? SCB->MMFAR : 0U;
    faulting_pc_snapshot = frame[6];
    GPIOA->BSRR          = LED_MASK;
    fault_stop(0xE4U);
}

__attribute__((naked))
void MemManage_Handler(void)
{
    __asm volatile (
        "tst lr, #4              \n"
        "ite eq                  \n"
        "mrseq r0, msp           \n"
        "mrsne r0, psp           \n"
        "b memmanage_handler_c   \n"
    );
}

__attribute__((used))
static _Noreturn void unprivileged_app_loop(void)
{
    uint32_t last_toggle_ms  = g_kernel_vault.app_ms;
    uint32_t last_seen_press = g_kernel_vault.accepted_press_count;

    for (;;) {
        uint32_t now_ms    = g_kernel_vault.app_ms;
        uint32_t press_seq = g_kernel_vault.accepted_press_count;

        if (press_seq != last_seen_press) {
            last_seen_press = press_seq;
            if (press_seq >= 3U) {
                trigger_mpu_fault = 1U;
            } else {
                __asm volatile ("svc #1" : : : "memory");
            }
        }

        if (trigger_mpu_fault != 0U) {
            g_kernel_vault.protected_cookie = 0xBAD0BEEFU;
        }

        if ((now_ms - last_toggle_ms) >= g_kernel_vault.blink_period_ms) {
            last_toggle_ms = now_ms;
            __asm volatile ("svc #2" : : : "memory");
        }
    }
}

__attribute__((naked))
static _Noreturn void enter_unprivileged_psp_loop(uint32_t psp_top)
{
    __asm volatile (
        "msr psp, r0              \n"
        "movs r1, #3              \n"
        "msr control, r1          \n"
        "isb 0xF                  \n"
        "b unprivileged_app_loop  \n"
    );
}

int main(void)
{
    __disable_irq();
    if ((__get_IPSR() != 0U) || ((__get_CONTROL() & 3U) != 0U)) {
        fault_stop(8U);
    }
    SysTick->CTRL = 0U;
    SCB->ICSR     = 1UL << 25;

    clock_init_84mhz();
    gpio_init();
    exti_nvic_systick_init();

    g_kernel_vault.app_ms               = 0U;
    g_kernel_vault.blink_period_ms      = 500U;
    g_kernel_vault.accepted_press_count = 0U;
    g_kernel_vault.svc_call_count       = 0U;
    g_kernel_vault.protected_cookie     = 0xCAFE1234U;

    candidate_pressed = button_pressed();
    stable_pressed    = true;
    armed             = false;
    settling          = true;

    mpu_init();

    __set_BASEPRI(0U);
    __DSB();
    __ISB();
    __enable_irq();

    enter_unprivileged_psp_loop((uint32_t)(uintptr_t)&app_psp_stack[256]);
}
```

---

## Document Lookup Table And Debugger Verification Checklist

| Feature / Register | Document in `docs/stm32/` | Exact Section / Table | `Ctrl+F` Search Term |
| --- | --- | --- | --- |
| **`CONTROL` (`nPRIV`, `SPSEL`, `FPCA`), `MSP`, `PSP`, `PRIMASK`** | [Arm DUI0553B](../../docs/stm32/um-cortexm4/DUI0553.pdf) | Sec 2.1.3 (*Core registers*), Table 2-2 | `"CONTROL register"` |
| **`EXC_RETURN` (`0xFFFFFFF9` vs `0xFFFFFFFD` vs `0xFFFFFFF1`)** | [Arm DUI0553B](../../docs/stm32/um-cortexm4/DUI0553.pdf) | Sec 2.3.7 (**Table 2-17 *Exception return behavior***) | `"Table 2-17"` |
| **`SCB->SHCSR` (`0xE000ED24`), `CFSR`/`MMFSR` (`0xE000ED28`), `MMFAR` (`0xE000ED34`)** | [Arm DUI0553B](../../docs/stm32/um-cortexm4/DUI0553.pdf) | Sec 4.3.9 (`SHCSR`), Sec 4.3.10 (`MMFSR`), Sec 4.3.12 (`MMFAR`) | `"MMFSR"` or `"0xE000ED34"` |
| **`MPU` (`0xE000ED90`..`0xE000EDA0`: `TYPE`, `CTRL`, `RNR`, `RBAR`, `RASR`)** | [Arm DUI0553B](../../docs/stm32/um-cortexm4/DUI0553.pdf) | Sec 4.5 (**Table 4-28** through **Table 4-35 *AP bit assignments***) | `"0xE000ED90"` or `"Table 4-35"` |

| Check | Expected Observation |
| --- | --- |
| **1. MPU Active** | `MPU->TYPE == 0x00000800` (`8` regions), `MPU->CTRL == 0x05` (`PRIVDEFENA | ENABLE`), `SCB->SHCSR` bit 16 `== 1` |
| **2. Unprivileged + `PSP`** | In `unprivileged_app_loop`: `IPSR == 0`, `CONTROL == 0x03`, `SP` is inside `app_psp_stack`, `MSP` is at/near `0x20020000` |
| **3. `SVC #1` & `SVC #2`** | Presses 1 & 2 toggle `g_kernel_vault.blink_period_ms` (`500U <-> 150U`) via `svc #1`; `LD2` blinks via `svc #2` |
| **4. `MemManage` Fault (`#4`)** | On 3rd press (or `trigger_mpu_fault = 1U`): stops at `fault_code == 0xE4U`, `mmfsr_snapshot == 0x82U`, `mmfar_snapshot == (uint32_t)&g_kernel_vault.protected_cookie`, and **`g_kernel_vault.protected_cookie` remains `0xCAFE1234U`** |
