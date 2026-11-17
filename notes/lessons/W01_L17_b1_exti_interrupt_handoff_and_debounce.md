# W01-L17: Complete Bare-Metal 84 MHz Startup, Vector Table, B1 Interrupt Handoff, And LD2 Debounce (Linker Script Only)

> **Scope & verification status:** `Document/source-reviewed, not build-verified: local STM32 RM0390 Rev 6, UM1724 Rev 14, DS10693 Rev 10 (stm32f446mc.pdf), MB1136 C03 schematic, Arm DUI0553B, and STM32F446RETX_FLASH.ld on 2026-11-01.`
> **Build model (`RegistersDebug`):** Complete bare-metal C using **only `STM32F446RETX_FLASH.ld` and one file (`Core/Src/register_app.c`)** with `-nostartfiles`—zero HAL, zero CMSIS headers, zero `startup_stm32f446retx.s`, and zero `system_stm32f4xx.c`. Your working HAL code in `Core/Src/main.c` stays intact in the original `Debug` configuration. Check silicon errata (`ES0298`) and build inputs before flashing.

Previous: [Arm Cortex-M4 important core registers, reset state, and system register blocks](W01_L16_cortexm4_reset_registers.md).

---

## Unified Example: How Everything in This Lesson Works Together

In this lesson, we boot the `STM32F446RE` from Power-On Reset (`POR`) using only the linker script (`STM32F446RETX_FLASH.ld`) and one C file (`Core/Src/register_app.c`) in **6 simple steps**:

1. **Step 1 — Power-On Reset (`POR`) and the Vector Table (`g_pfnVectors` at `0x08000000`):**
   * When `BOOT0 = 0`, the STM32 maps Main Flash (`0x08000000`) to the boot address `0x00000000`.
   * The Cortex-M4 hardware reads word `0` (`0x08000000`) to set the initial **Main Stack Pointer (`MSP = 0x20020000`, top of `128 KB` SRAM)** and reads word `1` (`0x08000004`) to jump to **`Reset_Handler()`** in Thumb state (`bit 0 = 1`).
   * We also place `SysTick_Handler` in vector slot `15` (`0x0800003C`) and `EXTI15_10_IRQHandler` in vector slot `56` (`16 + 40`, at `0x080000E0`).

2. **Step 2 — Startup Memory Initialization in `Reset_Handler()`:**
   * Before calling `main()`, `Reset_Handler()` masks interrupts (`cpsid i`), enables the Floating-Point Unit (`SCB->CPACR` bits `[23:20] = 0b1111`), copies initialized global variables (`.data`) from Flash (`&_sidata`) to SRAM (`&_sdata..&_edata`), and writes `0` to all uninitialized global variables (`.bss`, `&_sbss..&_ebss`).

3. **Step 3 — Switching the System Clock (`SYSCLK`) from `16 MHz` `HSI` to `84 MHz` PLL (`clock_init_84mhz`):**
   * At reset, the CPU runs at `16 MHz` from the internal `HSI` oscillator.
   * To run at `84 MHz`, we enable the Power Controller clock (`RCC->APB1ENR` bit `28 = 1`) and set Voltage Scale `3` (`PWR->CR` bits `[15:14] = 0b01`), set Flash memory latency to **`2` wait states (`FLASH->ACR = 2`)** so Flash reads keep up with `84 MHz`, set `APB1` bus divider to `/2` (`42 MHz <= 45 MHz` max), configure the Main PLL (`16 MHz / 16 * 336 / 4 = 84 MHz`), turn the PLL on (`RCC->CR` bit `24 = 1`), wait for lock (`PLLRDY == 1`), and switch `RCC->CFGR` (`SW = 2`) to the `84 MHz` PLL.

4. **Step 4 — Enabling `GPIOA` and `GPIOC` Clocks and Pins (`gpio_init`):**
   * Every STM32 peripheral is powered off (clock-gated) at reset. We set bit `0` (`GPIOAEN`) and bit `2` (`GPIOCEN`) in `RCC->AHB1ENR` to turn on `GPIOA` and `GPIOC`.
   * Then we set `PA5` (`LD2` green LED) as an Output (`GPIOA->MODER` bits `[11:10] = 0b01`, pre-cleared LOW via `GPIOA->BSRR` bit `21 = 1`) and set `PC13` (`B1` blue button) as an Input (`GPIOC->MODER` bits `[27:26] = 0b00`) with Pull-Up (`GPIOC->PUPDR` bits `[27:26] = 0b01`).

5. **Step 5 — Routing the Button Interrupt From `PC13` (STM32 Peripheral) to the Arm Core (`exti_nvic_systick_init`):**
   * `GPIOC` cannot interrupt the Arm CPU directly. We enable the `SYSCFG` clock (`RCC->APB2ENR` bit `14 = 1`) and set `SYSCFG->EXTICR[3]` bits `[7:4] = 2` to connect **Port C pin `13` (`PC13`)** to **`EXTI` Line `13`**.
   * In `EXTI`, we enable rising and falling edge detection (`EXTI->RTSR` and `FTSR` bit `13 = 1`), clear any stale pending flag (`EXTI->PR = 1UL << 13`), and unmask `EXTI->IMR` bit `13 = 1` so `EXTI` asserts interrupt wire **`IRQ 40`**.
   * Inside the Arm Cortex-M4 core, we enable `IRQ 40` in `NVIC->ISER[1]` bit `8` (`40 - 32 = 8`) and start the `1 ms` `SysTick` timer (`SysTick->LOAD = 84000 - 1 = 83999`, `SysTick->CTRL = 0x07`).

6. **Step 6 — Interrupt Handoff and `20 ms` Debounce in `main()`:**
   * When you press or release `B1`, `EXTI15_10_IRQHandler()` clears `EXTI->PR = (1UL << 13)` (Write-1-to-Clear), records the millisecond timestamp (`b1_irq_tick_ms = app_ms`), and sets `b1_irq_pending = 1`.
   * `main()` checks that `PC13` stays stable for `20 ms` with no new pending edge (`(EXTI->PR & (1UL << 13)) == 0`) and toggles `LD2`'s blink interval between `500 ms` and `150 ms` on each accepted press.

```text
+-----------------------------------------------------------------------------------+
| 1. POWER-ON RESET & STARTUP (Reset_Handler)                                       |
|    Hardware loads MSP = 0x20020000, PC = Reset_Handler (from 0x08000000/04)       |
|    Reset_Handler enables FPU (CPACR), copies .data to SRAM, zeros .bss in SRAM    |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
| 2. CLOCK, GPIO, AND INTERRUPT ROUTING IN main()                                   |
|    - clock_init_84mhz(): 16 MHz HSI -> PLL (M=16, N=336, P=/4) -> 84 MHz (2 WS)   |
|    - gpio_init():        RCC->AHB1ENR bits 0,2 = 1 -> PA5 Output, PC13 Input+PU   |
|    - exti_nvic_systick_init():                                                    |
|        [PC13 Pin] -> [SYSCFG EXTICR4 = 2] -> [EXTI13 RTSR/FTSR/IMR]               |
|                   -> [Arm NVIC ISER[1] bit 8 (IRQ 40)] -> [VTOR + 0xE0]           |
|        [SysTick]  -> LOAD = 83999 (1 ms tick at 84 MHz) -> increments `app_ms`    |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
| 3. INTERRUPT HANDOFF & NON-BLOCKING LOOP                                          |
|    - EXTI15_10_IRQHandler: Clears EXTI->PR = (1<<13), sets b1_irq_pending = 1     |
|    - main() loop:          Waits 20 settled ms -> toggles LD2 speed (500 <-> 150) |
+-----------------------------------------------------------------------------------+
```

---

## Scene 1: Power-On Reset (`POR`) And The Vector Table (`g_pfnVectors`)

1. **Map Main Flash (`0x08000000`) to the boot address (`0x00000000`) using `BOOT0 = 0`:**
   * On `NUCLEO-F446RE`, the `BOOT0` pin is tied low (`0`), which aliases Main Flash (`0x08000000`) at `0x00000000`.
   * **Otherwise:** The Arm core would fetch its reset vectors from system ROM or SRAM instead of your Flash program.
2. **Place the initial Main Stack Pointer (`&_estack = 0x20020000`) in `g_pfnVectors[0]` (`0x08000000`):**
   * At reset, hardware reads word `0` (`0x00000000` -> `0x08000000`) and loads it into `MSP`.
   * **Otherwise:** `MSP` holds an invalid address and the first stack push faults immediately.
3. **Place `Reset_Handler` in `g_pfnVectors[1]` (`0x08000004`):**
   * Hardware reads word `1` (`0x00000004` -> `0x08000004`) into `PC`. Taking the address of `Reset_Handler` in `-mthumb` mode automatically sets bit `0 = 1` (`Reset_Handler | 1`), which puts the Cortex-M4 into **Thumb state (`EPSR.T = 1`)**.
   * **Otherwise:** If bit `0` is `0`, the CPU faults with `UsageFault` (`INVSTATE`).
4. **Place `HardFault_Handler` in slot `3`, `SysTick_Handler` in slot `15` (`0x0800003C`), and `EXTI15_10_IRQHandler` in slot `56` (`16 + 40`, at `0x080000E0`):**
   * Slots `0..15` are Cortex-M4 core exceptions; STM32 peripheral interrupts start at slot `16` (`IRQ 0`). Because `EXTI15_10` is `IRQ 40` (`RM0390` Table 38), its vector slot is `16 + 40 = 56` (byte offset `56 * 4 = 0xE0`).
   * **Otherwise:** When `SysTick` or `IRQ 40` fires, the CPU fetches `0` from the table and faults.

```text
          (2)                 (3)                        (4)
           v                   v                   +------------------+       +----------+
RESET ---> [ 0x20020000 ] ---> [ Reset_Handler|1 ] | First instruction| ----> |  main()  |
 (1)       0x00000000          0x00000004          +------------------+       +----------+
           (Flash 0x08000000)  (Flash 0x08000004)     Reset_Handler()
           MSP = 0x20020000    PC  = Reset_Handler
```

#### Linker Script Excerpt ([STM32F446RETX_FLASH.ld](../../../p1_f446re_baseline/STM32F446RETX_FLASH.ld))

```ld
ENTRY(Reset_Handler)
_estack = ORIGIN(RAM) + LENGTH(RAM); /* 0x20000000 + 128K = 0x20020000 */

MEMORY
{
  RAM    (xrw) : ORIGIN = 0x20000000, LENGTH = 128K
  FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
}

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector)) /* Places g_pfnVectors[] first in FLASH at 0x08000000 */
    . = ALIGN(4);
  } >FLASH
```

#### C Snippet: Vector Table `g_pfnVectors[113]`

```c
extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;

typedef void (*isr_vector_fn_t)(void);

__attribute__((section(".isr_vector"), used))
const isr_vector_fn_t g_pfnVectors[113] = {
    [0]       = (isr_vector_fn_t)&_estack, /* 0x08000000: Initial MSP = 0x20020000 */
    [1]       = Reset_Handler,             /* 0x08000004: Reset handler (#1)       */
    [2]       = Default_Handler,           /* 0x08000008: NMI (#2)                 */
    [3]       = HardFault_Handler,         /* 0x0800000C: HardFault (#3)           */
    [4]       = Default_Handler,           /* 0x08000010: MemManage (#4)           */
    [5]       = Default_Handler,           /* 0x08000014: BusFault (#5)            */
    [6]       = Default_Handler,           /* 0x08000018: UsageFault (#6)          */
    [11]      = Default_Handler,           /* 0x0800002C: SVCall (#11)             */
    [12]      = Default_Handler,           /* 0x08000030: DebugMon (#12)           */
    [14]      = Default_Handler,           /* 0x08000038: PendSV (#14)             */
    [15]      = SysTick_Handler,           /* 0x0800003C: SysTick (#15)            */
    [16 + 40] = EXTI15_10_IRQHandler,      /* 0x080000E0: IRQ 40 (EXTI15_10, #56)  */
};
```

---

## Scene 2: Startup Memory Initialization In `Reset_Handler()`

1. **Disable configurable interrupts (`__disable_irq()` -> `cpsid i`) during startup:**
   * Set `PRIMASK = 1` before initializing memory, clocks, and peripherals.
   * **Otherwise:** An interrupt firing mid-setup could run while C globals or hardware registers are only half-configured.
2. **Enable the Cortex-M4 Floating-Point Unit (`FPU`) in `SCB->CPACR` (`0xE000ED88`):**
   * Set bits `[21:20] = 0b11` (`CP10`) and bits `[23:22] = 0b11` (`CP11`) in `SCB->CPACR`, followed by `__DSB()` and `__ISB()`.
   * **Otherwise:** The FPU is disabled at reset, and any floating-point instruction generated by `-mfpu=fpv4-sp-d16 -mfloat-abi=hard` faults with `UsageFault` (`NOCP`).
3. **Copy `.data` initial values from Flash (`&_sidata`) to SRAM (`&_sdata` to `&_edata`):**
   * Copy each 32-bit word from Flash into SRAM before calling `main()`.
   * **Otherwise:** Initialized global variables (like `uint32_t SystemCoreClock = 16000000U;`) hold random garbage in SRAM at boot.
4. **Zero-fill `.bss` variables in SRAM (`&_sbss` to `&_ebss`) and call `main()`:**
   * Write `0U` to every word from `&_sbss` to `&_ebss`, then call `main()`. *(Note: `0x20020000` is the initial `MSP` boundary loaded at reset; inside `Reset_Handler()` and `main()`, live `MSP` is lower by the compiled C stack frames.)*
   * **Otherwise:** Uninitialized/zero-initialized globals (`app_ms`, `b1_irq_pending`, `fault_code`) start with random non-zero values in SRAM.

```text
FLASH (0x08000000)                           SRAM (0x20000000 .. 0x20020000)
+---------------------------+                +-----------------------------------+
| .isr_vector (0x08000000)  |                | .data (_sdata .. _edata)          |
| .text / .rodata           |   (1) Copy     |   SystemCoreClock = 16000000U     |
| _sidata (Initial .data) --+--------------->|-----------------------------------|
+---------------------------+                | .bss  (_sbss .. _ebss)            |
                                (2) Zero 0U  |   app_ms = 0, b1_irq_pending = 0  |
                                ------------>|-----------------------------------|
                                             | Stack grows down from _estack     |
                                             |   (0x20020000 initial MSP top)    |
                                             +-----------------------------------+
```

```c
void Reset_Handler(void)
{
    /* 1. Mask configurable interrupts during startup (PRIMASK = 1): */
    __disable_irq();

    /* 2. Enable FPU coprocessors CP10 (bits [21:20]=11) & CP11 (bits [23:22]=11): */
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __DSB();
    __ISB();

    /* 3. Copy .data initializers from Flash (&_sidata) to SRAM (&_sdata..&_edata): */
    const uint32_t *src = &_sidata;
    volatile uint32_t *dst = &_sdata;
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    /* 4. Zero-fill .bss in SRAM (&_sbss..&_ebss): */
    volatile uint32_t *bss = &_sbss;
    while (bss < &_ebss) {
        *bss++ = 0U;
    }

    __DSB();
    __ISB();

    /* 5. Enter main(): */
    (void)main();
    fault_stop(0xFFU);
}
```

---

## Scene 3: Clock Tree And `84 MHz` PLL Configuration (`clock_init_84mhz`)

We raise the system clock (`SYSCLK`) from the `16 MHz` `HSI` reset oscillator to `84 MHz` using the Main PLL in 8 ordered steps:

1. **Check that `HSI` is the active clock (`RCC->CFGR` bits `[3:2]` `SWS == 0`) and all PLLs are OFF (`RCC->CR` bits `24, 26, 28 == 0`).**
   * **Otherwise:** Hardware ignores writes to `RCC->PLLCFGR` if any PLL is already enabled.
2. **Turn `HSI` ON (`RCC->CR` bit `0` `HSION = 1`) and wait until `HSI` is ready (`RCC->CR` bit `1` `HSIRDY == 1`).**
   * **Otherwise:** The PLL has no stable `16 MHz` input clock.
3. **Turn the Main PLL OFF (`RCC->CR` bit `24` `PLLON = 0`) and wait until `PLLRDY` (bit `25`) is `0`.**
   * **Otherwise:** `PWR->CR` and `RCC->PLLCFGR` cannot be updated safely.
4. **Enable the Power Controller (`PWR`) clock (`RCC->APB1ENR` bit `28 = 1`) and set Voltage Scale `3` (`PWR->CR` bits `[15:14] = 0b01`, valid up to `120 MHz`).**
   * **Otherwise:** `PWR` is unclocked at reset, so writes to `PWR->CR` are ignored.
5. **Set Flash memory latency to `2 Wait States` (`FLASH->ACR` bits `[3:0] = 2`) and read `FLASH->ACR` back to confirm `LATENCY == 2` (`RM0390` Table 5 for `60 < HCLK <= 90 MHz` at `3.3 V`).**
   * **Otherwise:** At `84 MHz`, the CPU reads Flash faster than the Flash memory array can respond and crashes immediately on corrupted instructions!
6. **Set bus dividers in `RCC->CFGR`: `AHB / 1` (`HPRE` bits `[7:4] = 0` -> `84 MHz`), `APB1 / 2` (`PPRE1` bits `[12:10] = 4` -> `42 MHz`), and `APB2 / 1` (`PPRE2` bits `[15:13] = 0` -> `84 MHz`).**
   * **Otherwise:** `APB1` has a hardware maximum of `45 MHz`; leaving `APB1` at `/1` overclocks `APB1` to `84 MHz`.
7. **Program `RCC->PLLCFGR` (`PLLM = 16` in bits `[5:0]`, `PLLN = 336` in bits `[14:6]`, `PLLP = /4` [code `0b01 = 1UL << 16`] in bits `[17:16]`, `PLLSRC = 0` [`HSI`] in bit `22`, `PLLQ = 7` in bits `[27:24]`, `PLLR = 2` in bits `[30:28]`), then set `RCC->CR` bit `24` (`PLLON = 1`) and wait for `PLLRDY` (`bit 25 == 1`) and `PWR->CSR` `VOSRDY` (`bit 14 == 1`).**
   * **Otherwise:** The PLL stays at its reset multipliers or is selected before locking.
8. **Switch `SYSCLK` to the Main PLL (`RCC->CFGR` bits `[1:0]` `SW = 2`) and wait until hardware confirms `SWS` (`bits [3:2]`) `== (2UL << 2)`.**
   * **Otherwise:** The CPU keeps running at `16 MHz` on `HSI` even though the PLL is running.

```text
             (1)                  (2)                   (3)                  (4)
+---------+  / 16   +----------+ * 336   +-----------+  / 4    +------------+ /1  AHB/HCLK = 84 MHz
| 16 MHz  | ------> |  1 MHz   | ------> |  336 MHz  | ------> |   84 MHz   | --> APB1 (/2)= 42 MHz
|   HSI   | (PLLM)  | VCO In   | (PLLN)  |  VCO Out  | (PLLP)  |   SYSCLK   | --> APB2 (/1)= 84 MHz
+---------+         +----------+         +-----------+         +------------+
```

```c
static void clock_init_84mhz(void)
{
    /* 1. Require HSI active (SWS == 00) and all PLLs stopped: */
    if (((RCC->CFGR & 0x0CUL) != 0U) ||
        ((RCC->CR & ((1UL << 24) | (1UL << 26) | (1UL << 28))) != 0U)) {
        fault_stop(1U);
    }

    /* 2. Enable HSI (bit 0) and wait for HSIRDY (bit 1): */
    RCC->CR |= 1UL;
    wait_field(&RCC->CR, 1UL << 1, 1UL << 1, 2U);

    /* 3. Clear PLLON (bit 24) and wait for PLLRDY (bit 25) == 0: */
    RCC->CR &= ~(1UL << 24);
    wait_field(&RCC->CR, 1UL << 25, 0U, 3U);

    /* 4. Enable PWR clock (APB1ENR bit 28) and set VOS Scale 3 (bits [15:14] = 01): */
    RCC->APB1ENR |= 1UL << 28;
    (void)RCC->APB1ENR;
    __DSB();
    PWR->CR = (PWR->CR & ~(3UL << 14)) | (1UL << 14);

    /* 5. Set FLASH->ACR LATENCY (bits [3:0]) = 2 WS and verify readback: */
    FLASH->ACR = (FLASH->ACR & ~15UL) | 2UL;
    wait_field(&FLASH->ACR, 15UL, 2UL, 4U);

    /* 6. Set AHB /1, APB1 /2 (PPRE1 bits [12:10] = 4UL), APB2 /1: */
    RCC->CFGR = (RCC->CFGR & ~((15UL << 4) | (7UL << 10) | (7UL << 13)))
              | (4UL << 10);

    /* 7. Configure PLL (M=16, N=336, P=01 [/4], HSI=0, Q=7, R=2), enable PLLON, wait ready: */
    const uint32_t pll_mask  = 0x7F437FFFUL;
    const uint32_t pll_value = 16UL | (336UL << 6) | (1UL << 16)
                             | (7UL << 24) | (2UL << 28);
    RCC->PLLCFGR = (RCC->PLLCFGR & ~pll_mask) | pll_value;
    RCC->CR |= 1UL << 24;
    wait_field(&RCC->CR, 1UL << 25, 1UL << 25, 5U);
    wait_field(&PWR->CSR, 1UL << 14, 1UL << 14, 6U);

    /* 8. Switch SYSCLK to PLL_P (SW = 2UL) and wait for SWS == 2UL << 2: */
    RCC->CFGR = (RCC->CFGR & ~3UL) | 2UL;
    wait_field(&RCC->CFGR, 3UL << 2, 2UL << 2, 7U);
    __DSB();
    __ISB();
    SystemCoreClock = 84000000U;
}
```

---

## Scene 4: Enabling GPIO Clocks And Configuring `PA5` (`LD2`) & `PC13` (`B1`) (`gpio_init`)

1. **Enable the `GPIOA` and `GPIOC` peripheral clocks in `RCC->AHB1ENR` (`0x40023830`) by setting bit `0` (`GPIOAEN = 1`) and bit `2` (`GPIOCEN = 1`):**
   * **Otherwise:** Both `GPIOA` and `GPIOC` remain powered down (disabled) at reset, and any write to their registers is ignored.
2. **Pre-clear `PA5` (`LD2`) output latch LOW by setting bit `21` (`5 + 16`) in `GPIOA->BSRR` (`0x40020018`):**
   * **Otherwise:** If `GPIOA->ODR` bit `5` held `1`, `LD2` would briefly glitch ON when `PA5` is switched to Output mode.
3. **Configure `PA5` (`n = 5`, 2-bit field at `[11:10]`) as a Push-Pull Output by setting `GPIOA->MODER` bits `[11:10] = 0b01` (`1UL << 10`):**
   * **Otherwise:** `PA5` stays in its reset Input mode (`0b00`) and cannot drive `LD2`.
4. **Configure `PC13` (`n = 13`, 2-bit field at `[27:26]`) as an Input with Pull-Up by clearing `GPIOC->MODER` bits `[27:26] = 0b00` and setting `GPIOC->PUPDR` bits `[27:26] = 0b01` (`1UL << 26`):**
   * **Otherwise:** `PC13` input mode and internal pull-up are not explicitly guaranteed.

```text
RCC->AHB1ENR (0x40023830)
  +-- bit 0 = 1 ---> Enables GPIOA (0x40020000) ---> PA5 (LD2 Green LED):
  |                    - BSRR bit 21 = 1 (Preload ODR5 = 0 so LED starts OFF)
  |                    - MODER bits [11:10] = 01 (General-Purpose Output)
  |
  +-- bit 2 = 1 ---> Enables GPIOC (0x40020800) ---> PC13 (B1 Pushbutton):
                       - MODER bits [27:26] = 00 (Input mode)
                       - PUPDR bits [27:26] = 01 (Internal Pull-Up enabled)
                       - IDR   bit  13      = 0 when pressed, 1 when released
```

```c
static bool button_pressed(void)
{
    return (GPIOC->IDR & B1_MASK) == 0U; /* Active-LOW: IDR bit 13 == 0 when B1 is pressed */
}

static void gpio_init(void)
{
    /* 1. Enable GPIOA (bit 0) and GPIOC (bit 2) peripheral clocks on AHB1: */
    RCC->AHB1ENR |= (1UL << 0) | (1UL << 2);
    (void)RCC->AHB1ENR;
    __DSB();

    /* 2. Preload PA5 output latch LOW (BSRR bit 21) so LD2 starts OFF: */
    GPIOA->BSRR = LED_MASK << 16;

    /* 3. Configure PA5 as Push-Pull, Low-Speed, No-Pull, Output (MODER [11:10] = 01): */
    GPIOA->OTYPER  &= ~LED_MASK;
    GPIOA->OSPEEDR &= ~(3UL << 10);
    GPIOA->PUPDR   &= ~(3UL << 10);
    GPIOA->MODER    = (GPIOA->MODER & ~(3UL << 10)) | (1UL << 10);

    /* 4. Configure PC13 as Input (MODER [27:26] = 00) with Pull-Up (PUPDR [27:26] = 01): */
    GPIOC->MODER &= ~(3UL << 26);
    GPIOC->PUPDR  = (GPIOC->PUPDR & ~(3UL << 26)) | (1UL << 26);
}
```

---

## Scene 5: Interrupt Routing From STM32 Pin (`PC13`) To The Arm Cortex-M4 Core (`exti_nvic_systick_init`)

### A. How the Interrupt Signal Travels From `PC13` (STM32 Peripheral) to the Arm CPU Core

`GPIOC` is an **STM32 peripheral** on the `AHB1` bus outside the CPU—it cannot interrupt the **Arm Cortex-M4 core** directly. The interrupt signal travels through **5 hardware blocks in series**:

1. **`GPIOC` (`0x40020800`, STM32 `AHB1` peripheral):** Reads the voltage on pin `PC13` (`0` when pressed, `1` when released).
2. **`SYSCFG` (`0x40013800`, STM32 `APB2` peripheral):** The **Port Multiplexer**. Because `GPIOA..GPIOH` all have a pin `13` (`PA13, PB13, PC13...`) but the chip has only one `EXTI Line 13`, `SYSCFG->EXTICR[3]` bits `[7:4] = 2` selects **Port C (`PC13`)** to feed `EXTI Line 13`.
3. **`EXTI` (`0x40013C00`, STM32 `APB2` peripheral):** The **Edge Detector and Gate**. It watches `EXTI Line 13` for rising (`RTSR`) and falling (`FTSR`) edges, latches bit `13 = 1` in `EXTI->PR` (Pending Register), and when `EXTI->IMR` bit `13 = 1`, asserts interrupt wire **`IRQ 40` (`EXTI15_10`)** into the Arm core.
4. **`NVIC` (`0xE000E100`, Arm Cortex-M4 Core controller):** Receives wire `IRQ 40`, checks that `IRQ 40` is enabled in `NVIC->ISER[1]` bit `8` (`40 - 32 = 8`), and compares its priority (`NVIC->IP[40]`) against the running code.
5. **Arm Cortex-M4 CPU Core (`PRIMASK`, `SCB->VTOR`, Vector Slot `56`):** When `PRIMASK == 0`, the CPU pauses `main()`, pushes 8 registers onto `MSP`, reads vector slot `16 + 40 = 56` (`SCB->VTOR + 0xE0`), and runs `EXTI15_10_IRQHandler()`.

```text
+-----------------------------------------------------------------------+     +-------------------------------------------------+
|                   STM32F446 MCU PERIPHERALS (Outside CPU)             |     |         ARM CORTEX-M4 PROCESSOR CORE            |
|                                                                       |     |                                                 |
|  [1. GPIOC Pin]      [2. SYSCFG Mux]          [3. EXTI Line 13]       |     |  [4. NVIC Controller]      [5. CPU Core & VTOR] |
|  +------------+      +-----------------+      +--------------------+  |     |  +--------------------+    +-----------------+  |
|  | Pin PC13   |----->| SYSCFG->EXTICR  |----->| Edge Detector:     |--+---->|  | IRQ 40 (EXTI15_10) |--->| PRIMASK = 0     |  |
|  | Input mode |      | Connects Port C |      | RTSR=1, FTSR=1     |  | IRQ |  | ISER[1] bit 8 = 1  |    | Jumps via       |  |
|  | + Pull-Up  |      | pin 13 -> Line13|      | Sets PR bit 13 = 1 |  | 40  |  | Priority IP[40]    |    | [VTOR + 0xE0]   |  |
|  +------------+      +-----------------+      | IMR bit 13 = 1     |  |wire |  +--------------------+    +-----------------+  |
|   (AHB1 bus)          (APB2 bus)              +--------------------+  |     |   (0xE000E100 PPB)          (Handler mode MSP)  |
+-----------------------------------------------------------------------+     +-------------------------------------------------+
```

---

### B. Step-by-Step Register Configuration in `exti_nvic_systick_init()`

1. **Disable `IRQ 40` in `NVIC` (`NVIC->ICER[1] = 1UL << 8`) and mask `EXTI` Line `13` (`EXTI->IMR &= ~(1UL << 13)`) while changing routing.**
   * **Otherwise:** Switching the port multiplexer while unmasked can trigger a false interrupt mid-setup.
2. **Enable the `SYSCFG` peripheral clock by setting bit `14` (`SYSCFGEN = 1`) in `RCC->APB2ENR` (`0x40023844`).**
   * **Otherwise:** `SYSCFG` is unclocked at reset, so writing `SYSCFG->EXTICR[3]` does nothing and `EXTI Line 13` stays connected to `PA13` (`code 0`).
3. **Connect `PC13` (Port C = `2`) to `EXTI Line 13` by setting bits `[7:4] = 2` (`2UL << 4`) in `SYSCFG->EXTICR[3]` (`0x40013814`).**
   * **Otherwise:** `EXTI Line 13` never sees button `B1` on `PC13`.
4. **Enable rising and falling edge triggers on `EXTI Line 13` by setting bit `13 = 1` in `EXTI->RTSR` (`0x40013C08`) and `EXTI->FTSR` (`0x40013C0C`).**
   * **Otherwise:** `EXTI` will not latch bit `13` in `EXTI->PR` when you press or release `B1`.
5. **Set 4 preemption-priority bits (`SCB->AIRCR` `PRIGROUP = 3` with key `0x5FA`), point `SCB->VTOR` to `g_pfnVectors`, and set `SysTick` to Priority `1` (`SCB->SHP[11] = 1U << 4`) and `IRQ 40` to Priority `5` (`NVIC->IP[40] = 5U << 4`).**
   * **Otherwise:** Priority bytes are not interpreted as 4 preemption bits, or `SysTick` (`1 ms` timebase) cannot preempt a longer button ISR.
6. **Clear stale pending flags by writing `EXTI->PR = (1UL << 13)` (Write-1-to-Clear—use `=` not `|=`) and `NVIC->ICPR[1] = (1UL << 8)`, then unmask `EXTI->IMR |= (1UL << 13)` and enable `NVIC->ISER[1] = (1UL << 8)`.**
   * **Otherwise:** A stale pending edge fires immediately on unmasking, or button edges never reach the CPU.
7. **Start the `1 ms` `SysTick` timer at `84 MHz` by writing `SysTick->LOAD = 84000U - 1U` (`83999`), `SysTick->VAL = 0U`, and `SysTick->CTRL = 0x07U` (`CLKSOURCE | TICKINT | ENABLE`).**
   * **Otherwise:** `app_ms` stays at `0` and neither the `20 ms` debounce nor the `LD2` blink timer advances. *(Debugger note: bit `16` `COUNTFLAG` sets to `1` when the timer wraps and clears on read, so check `(SysTick->CTRL & 0x7U) == 0x7U`.)*
8. **Inside `EXTI15_10_IRQHandler()`, clear `EXTI->PR = (1UL << 13)` (`W1C`) and record `b1_irq_tick_ms = app_ms` and `b1_irq_pending = 1U`.**
   * **Otherwise:** If `EXTI->PR` bit `13` is not cleared by writing `1`, the interrupt stays pending and re-enters `EXTI15_10_IRQHandler` endlessly!

```c
static void exti_nvic_systick_init(void)
{
    /* 1. Disable IRQ 40 in NVIC and mask EXTI Line 13 while changing routing: */
    NVIC->ICER[1] = IRQ40_MASK;
    EXTI->IMR    &= ~B1_MASK;

    /* 2. Enable SYSCFG peripheral clock on APB2 (bit 14 = SYSCFGEN): */
    RCC->APB2ENR |= 1UL << 14;
    (void)RCC->APB2ENR;
    __DSB();

    /* 3. Route PC13 (Port C = code 2) to EXTI Line 13 in SYSCFG->EXTICR[3] bits [7:4]: */
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~(15UL << 4)) | (2UL << 4);

    /* 4. Disable Event mode and enable both rising and falling edges on Line 13: */
    EXTI->EMR  &= ~B1_MASK;
    EXTI->RTSR |= B1_MASK;
    EXTI->FTSR |= B1_MASK;

    /* 5. Set PRIGROUP = 3 (4 preemption bits), VTOR, STKALIGN, and priorities: */
    uint32_t aircr = SCB->AIRCR & ~((0xFFFFUL << 16) | (7UL << 8));
    SCB->AIRCR   = aircr | (0x5FAUL << 16) | (3UL << 8);
    SCB->VTOR    = (uint32_t)g_pfnVectors;
    SCB->CCR    |= 1UL << 9;
    NVIC->IP[40] = (uint8_t)(5U << 4);      /* EXTI15_10 priority = 5 (0x50) */
    SCB->SHP[11] = (uint8_t)(1U << 4);      /* SysTick   priority = 1 (0x10) */

    /* 6. Clear stale EXTI13 (W1C) & NVIC IRQ 40 pending flags, then unmask: */
    EXTI->PR      = B1_MASK;
    NVIC->ICPR[1] = IRQ40_MASK;
    EXTI->IMR    |= B1_MASK;
    NVIC->ISER[1] = IRQ40_MASK;

    /* 7. Start 1 ms SysTick at 84 MHz (84,000 ticks -> LOAD = 83999): */
    app_ms        = 0U;
    SysTick->LOAD = 84000U - 1U;
    SysTick->VAL  = 0U;
    SysTick->CTRL = (1UL << 2) | (1UL << 1) | 1UL;
    __DSB();
    __ISB();
}

void SysTick_Handler(void)
{
    ++app_ms;
}

void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & B1_MASK) != 0U) {
        EXTI->PR       = B1_MASK;           /* 8. Clear bit 13 (Write-1-to-Clear) */
        b1_irq_tick_ms = app_ms;            /* Record ISR-service millisecond     */
        b1_irq_pending = 1U;                /* Notify main()                      */
        ++b1_irq_count;
        __DSB();
    }
}
```

---

## Scene 6: `main()` — Release-Before-Arm Debounce And `LD2` Blink Loop

1. **Initialize clocks, GPIO, and interrupts, and start with `stable_pressed = true` and `armed = false` (Release-Before-Arm):**
   * Require button `B1` to be seen released (`HIGH`) for `20` ticks before arming the first press.
   * **Otherwise:** If `B1` is held down during reset, startup would immediately count a false button press.
2. **Take a short `PRIMASK`-saved snapshot in the `for (;;)` loop and require `20` settled ticks with `(EXTI->PR & B1_MASK) == 0U`:**
   * Save `PRIMASK`, disable interrupts briefly while checking `app_ms`, `b1_irq_pending`, and `(EXTI->PR & B1_MASK) == 0U`, and restore `PRIMASK`. When `PC13` stays unchanged for `20` ticks after a release (`!stable_pressed`), set `armed = true`; when it stays pressed for `20` ticks while armed (`stable_pressed && armed`), set `armed = false` and toggle `blink_period_ms` between `500 ms` and `150 ms`.
   * **Otherwise:** Contact bounce (`< 20` ticks) would toggle the blink speed multiple times on a single press. *(Note: `PRIMASK = 1` blocks ISR execution, not hardware edge detection, so an edge arriving after the `EXTI->PR` read sets `EXTI->PR = 1` for the next pass; and `20` ticks is a chosen filter threshold.)*
3. **Toggle `LD2` (`PA5`) every `blink_period_ms` ticks using `GPIOA->BSRR`:**
   * Write `LED_MASK` (`1UL << 5`) to turn `LD2` ON, or `LED_MASK << 16` (`1UL << 21`) to turn `LD2` OFF.
   * **Otherwise:** Using a blocking delay would freeze button debouncing while waiting for the LED toggle.

```text
      (1) Boot                (2) Released >= 20 ms          (3) Pressed >= 20 ms
+-------------------+       +-----------------------+       +-------------------------+
| stable = true     | ----> | stable = false        | ----> | stable = true           |
| armed  = false    |       | armed  = true (ARMED) |       | armed  = false          |
| (Ignores held B1) |       | (Ready for 1st press) |       | Toggles 500 <-> 150 ms! |
+-------------------+       +-----------------------+       +-------------------------+
```

```c
        /* Inside main() for(;;) loop: */
        uint32_t saved_mask = __get_PRIMASK();
        __disable_irq();
        uint32_t now_ms = app_ms;

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
                        ++accepted_press_count;
                        blink_period_ms = (blink_period_ms == 500U) ? 150U : 500U;
                    }
                }
            }
        }
        __set_PRIMASK(saved_mask);

        if ((now_ms - last_toggle_ms) >= blink_period_ms) {
            last_toggle_ms = now_ms;
            led_on         = !led_on;
            GPIOA->BSRR    = led_on ? LED_MASK : (LED_MASK << 16);
        }
```

---

## Complete Standalone Code: `Core/Src/register_app.c`

Link with `STM32F446RETX_FLASH.ld` and `-nostartfiles` in `RegistersDebug`:

```c
#include <stdbool.h>
#include <stdint.h>

#define LED_MASK   (1UL << 5)    /* PA5  (LD2) */
#define B1_MASK    (1UL << 13)   /* PC13 (B1)  */
#define IRQ40_MASK (1UL << 8)    /* IRQ 40 in NVIC word 1: bit (40 - 32) = 8 */
#define POLL_LIMIT 1000000U

/* ============================================================================
 * 1. CORTEX-M4 INLINE ASSEMBLY HELPERS (DUI0553B Sec 2.1.3 & Sec 3.12)
 * ============================================================================ */
static inline void __disable_irq(void) { __asm volatile ("cpsid i" : : : "memory"); }
static inline void __enable_irq(void)  { __asm volatile ("cpsie i" : : : "memory"); }
static inline void __NOP(void)         { __asm volatile ("nop"); }
static inline void __DSB(void)         { __asm volatile ("dsb 0xF" : : : "memory"); }
static inline void __ISB(void)         { __asm volatile ("isb 0xF" : : : "memory"); }

static inline uint32_t __get_PRIMASK(void)
{
    uint32_t result;
    __asm volatile ("mrs %0, primask" : "=r" (result));
    return result;
}

static inline void __set_PRIMASK(uint32_t priMask)
{
    __asm volatile ("msr primask, %0" : : "r" (priMask) : "memory");
}

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

/* ============================================================================
 * 2. MEMORY-MAPPED PERIPHERAL & CORE REGISTER STRUCTS (RM0390 & DUI0553B)
 * ============================================================================ */
typedef struct {
    volatile uint32_t ACR, KEYR, OPTKEYR, SR, CR, OPTCR;
} FLASH_TypeDef;

typedef struct {
    volatile uint32_t CR, CSR;
} PWR_TypeDef;

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

typedef struct {
    volatile uint32_t MEMRMP, PMC, EXTICR[4];
} SYSCFG_TypeDef;

typedef struct {
    volatile uint32_t IMR, EMR, RTSR, FTSR, SWIER, PR;
} EXTI_TypeDef;

typedef struct {
    volatile uint32_t CTRL, LOAD, VAL, CALIB;
} SysTick_Type;

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

#define PWR     ((PWR_TypeDef *)   0x40007000UL)
#define SYSCFG  ((SYSCFG_TypeDef *)0x40013800UL)
#define EXTI    ((EXTI_TypeDef *)  0x40013C00UL)
#define GPIOA   ((GPIO_TypeDef *)  0x40020000UL)
#define GPIOC   ((GPIO_TypeDef *)  0x40020800UL)
#define RCC     ((RCC_TypeDef *)   0x40023800UL)
#define FLASH   ((FLASH_TypeDef *) 0x40023C00UL)
#define SysTick ((SysTick_Type *)  0xE000E010UL)
#define NVIC    ((NVIC_Type *)     0xE000E100UL)
#define SCB     ((SCB_Type *)      0xE000ED00UL)

/* ============================================================================
 * 3. VECTOR TABLE & STARTUP (STM32F446RETX_FLASH.ld symbols)
 * ============================================================================ */
extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;

void Reset_Handler(void);
void Default_Handler(void);
void HardFault_Handler(void);
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
    [11]      = Default_Handler,
    [12]      = Default_Handler,
    [14]      = Default_Handler,
    [15]      = SysTick_Handler,
    [16 + 40] = EXTI15_10_IRQHandler,
};

uint32_t SystemCoreClock = 16000000U;

static volatile uint32_t app_ms;
static volatile uint32_t b1_irq_pending;
static volatile uint32_t b1_irq_tick_ms;
static volatile uint32_t b1_irq_count;
static volatile uint32_t fault_code;
static volatile uint32_t accepted_press_count;

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

/* ============================================================================
 * 4. CLOCK (84 MHz), GPIO (PA5/PC13), AND INTERRUPT ROUTING (EXTI13 -> IRQ 40)
 * ============================================================================ */
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

static bool button_pressed(void)
{
    return (GPIOC->IDR & B1_MASK) == 0U;
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
    GPIOA->MODER    = (GPIOA->MODER & ~(3UL << 10)) | (1UL << 10);

    GPIOC->MODER   &= ~(3UL << 26);
    GPIOC->PUPDR    = (GPIOC->PUPDR & ~(3UL << 26)) | (1UL << 26);
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
    SCB->AIRCR   = aircr | (0x5FAUL << 16) | (3UL << 8);
    SCB->VTOR    = (uint32_t)g_pfnVectors;
    SCB->CCR    |= 1UL << 9;
    NVIC->IP[40] = (uint8_t)(5U << 4);
    SCB->SHP[11] = (uint8_t)(1U << 4);

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

void SysTick_Handler(void)
{
    ++app_ms;
}

void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & B1_MASK) != 0U) {
        EXTI->PR       = B1_MASK;
        b1_irq_tick_ms = app_ms;
        b1_irq_pending = 1U;
        ++b1_irq_count;
        __DSB();
    }
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

    uint32_t blink_period_ms    = 500U;
    uint32_t last_toggle_ms     = 0U;
    uint32_t candidate_since_ms = 0U;
    bool     candidate_pressed  = button_pressed();
    bool     stable_pressed     = true;
    bool     armed              = false;
    bool     settling           = true;
    bool     led_on             = false;

    __set_BASEPRI(0U);
    __DSB();
    __ISB();
    __enable_irq();

    for (;;) {
        uint32_t saved_mask = __get_PRIMASK();
        __disable_irq();
        uint32_t now_ms = app_ms;

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
                        ++accepted_press_count;
                        blink_period_ms = (blink_period_ms == 500U) ? 150U : 500U;
                    }
                }
            }
        }
        __set_PRIMASK(saved_mask);

        if ((now_ms - last_toggle_ms) >= blink_period_ms) {
            last_toggle_ms = now_ms;
            led_on         = !led_on;
            GPIOA->BSRR    = led_on ? LED_MASK : (LED_MASK << 16);
        }
    }
}
```

---

## Document Lookup Cheat-Sheet And `RegistersDebug` Checklist

### A. Which Document Owns Which Detail (And How To Search It)

* **2-Step Register Address Formula:**
  $$\text{Register Address} = \text{Peripheral Base Address (`RM0390` Table 1)} + \text{Register Offset}$$
  * Example: `GPIOA` base (`0x4002 0000`) + `GPIOx_BSRR` offset (`0x18`) = `0x4002 0018`.
  * *(Note: In `RM0390` Table 1, `AHB1` starts at `0x4002 0000` (`GPIOA`), while `0x4004 0000` is `USB OTG HS` inside `AHB1`.)*

| Detail Needed | Document in `docs/stm32/` | Exact Section / Table | `Ctrl+F` Search Term |
| --- | --- | --- | --- |
| **`LD2` (`PA5`), `B1` (`PC13`), `R30 = 4.7 kOhm`, `R29 = 100 Ohm`, `C15 = 100 nF`** | [MB1136 C03 Schematic](../../docs/stm32/um-f446re/mb1136-default-c03_schematic.pdf) & [UM1724](../../docs/stm32/um-f446re/UM1724.pdf) | `UM1724` Sec 5 (Jumpers), Sec 6.4 (`LD2`), Sec 6.5 (`B1`); C03 Schematic Sheet 4 | `"User LD2"` or `"B1 USER"` |
| **`FLASH` (`0x08000000`, `512 KB`) & `SRAM` (`0x20000000`, `128 KB`), bus limits** | [Datasheet (`stm32f446mc.pdf`)](../../docs/stm32/um-f446re/stm32f446mc.pdf) | Sec 2 (Table 2), Sec 2.1 (Fig 1 *Block diagram*), Sec 3.6 (*Embedded SRAM*), Sec 5 (Fig 14) | `"Figure 14. Memory map"` |
| **Peripheral base addresses (`PWR`, `RCC`, `FLASH`, `GPIO`, `SYSCFG`, `EXTI`)** | [RM0390 Reference Manual](../../docs/stm32/um-f446re/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) | Sec 2.2.2, **Table 1 (*STM32F446xx register boundary addresses*)** | `"Table 1."` |
| **Flash `2 Wait States` at `84 MHz` (`2.7–3.6 V`), `PWR` Scale 3, PLL math** | [RM0390 Reference Manual](../../docs/stm32/um-f446re/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) | Sec 3.4.1 (**Table 5**), Sec 5.4.1 (`PWR_CR`), Sec 6.2 (Fig 16), Sec 6.3.2 (`RCC_PLLCFGR`) | `"Table 5. Number of wait states"` |
| **`EXTI15_10` = `IRQ 40` (Vector slot `56`, offset `VTOR + 0xE0`)** | [RM0390 Reference Manual](../../docs/stm32/um-f446re/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf) | Sec 10.1.2 (**Table 38 *Vector table for STM32F446xx***), Sec 8.2.6 (`SYSCFG_EXTICR4`), Sec 10.3 (`EXTI_PR`) | `"Table 38. Vector table"` |
| **Cortex-M4 `SysTick` (`0xE000E010`), `NVIC` (`0xE000E100`), `SCB` (`0xE000ED00`), `EXC_RETURN`** | [Arm Cortex-M4 User Guide (`DUI0553B`)](../../docs/stm32/um-cortexm4/DUI0553.pdf) | Sec 2.1.3 (*Core registers*), Sec 2.3.7 (**Table 2-17 `EXC_RETURN`**), Sec 4.2 (`NVIC`), Sec 4.3 (`SCB`), Sec 4.4 (`SysTick`) | `"0xE000E100"` or `"Table 2-17"` |

---

### B. `RegistersDebug` Build Setup And Debugger Verification Checks

1. **Create `RegistersDebug` in STM32CubeIDE (Preserving `Debug`):**
   * Duplicate the `Debug` configuration as **`RegistersDebug`**.
   * In `RegistersDebug`, compile **only** `Core/Src/register_app.c` (combine the C code blocks from Scenes 1–6 in order into `Core/Src/register_app.c`), exclude all other `.c`/`.s` files, enable **`-nostartfiles`** in *MCU GCC Linker -> General*, and link with [STM32F446RETX_FLASH.ld](../../../p1_f446re_baseline/STM32F446RETX_FLASH.ld).
   * In the original `Debug` configuration, exclude `Core/Src/register_app.c`.

2. **Expected Debugger & Hardware Checks:**

| Check | Expected Observation |
| --- | --- |
| **1. Clock (`84 MHz`)** | `fault_code == 0`, `RCC->CFGR` bits `[3:2] == 0b10` (`PLL_P`), `FLASH->ACR` bits `[3:0] == 2` |
| **2. `SysTick` (`1 ms`)** | `SysTick->LOAD == 83999`, `(SysTick->CTRL & 0x7U) == 0x7U`, `app_ms` advances while running |
| **3. Priorities** | `SCB->AIRCR` `PRIGROUP` bits `[10:8] == 3`, `SCB->SHP[11] == 0x10`, `NVIC->IP[40] == 0x50` |
| **4. Routing & Vector** | `SYSCFG->EXTICR[3]` bits `[7:4] == 2`; `EXTI->RTSR`, `FTSR`, `IMR` bit 13 set; `[VTOR + 0xE0]` points to `EXTI15_10_IRQHandler` |
| **5. Boot-Held Guard** | Holding `B1` across Reset keeps `accepted_press_count == 0` and `blink_period_ms == 500U` until `B1` is released for `>= 20` ticks |
| **6. Press / Release** | Each deliberate press (`< 20`-tick bounce) increments `accepted_press_count` by `1` and switches `LD2` between `500 ms` and `150 ms` |

