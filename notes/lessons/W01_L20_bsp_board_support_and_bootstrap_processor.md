# W01-L20: BSP — Board Support Package Or Bootstrap Processor?

**Status:** Source-reviewed, 2026-11-01: ST's public STM32F4 Nucleo BSP
README/header/source, the local P1 HAL GPIO implementation, Zephyr's board
porting guide and EDK II's MP Services interface. Upstream `main`/`master`
content was inspected on this date; it is not a pinned project dependency.
This is a concept-only lesson. No firmware changes, installation, build,
flash or hardware measurement. Publication is not learner completion.

Previous: [Bare-metal `PendSV` context switching, official CMSIS headers, and FreeRTOS tasks (`P1` bridge)](W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md).

## Know This First

| Term | Simple meaning |
| --- | --- |
| Board / PCB | The physical circuit board connecting the processor, power, clocks, connectors and other components |
| MCU | Microcontroller chip containing a CPU, memory and peripherals |
| SoC | System-on-chip; a chip integrating processing and other system components |
| Driver | Software that operates a hardware controller or device |
| Pin multiplexing / pinmux | Selecting which function a physical chip pin performs |
| Bring-up | Making a new board start and proving its hardware features work step by step |
| Porting | Adapting existing software to a new processor, board or execution environment |
| Bootloader | Firmware that prepares for, selects or loads the next software image |
| Logical processor | A processor execution context; on some CPUs a core provides multiple hardware threads |

---

## Unified Example: How Everything in This Lesson Works Together

When engineers say **"BSP"** in embedded systems, they mean one of two completely different things depending on the context:

1. **Meaning 1 — Board Support Package (`BSP` in MCU / Embedded Board Work):**
   * The same `STM32F446` chip can be soldered onto two different circuit boards (`Board A` and `Board B`):
     * On `Board A` (`NUCLEO-F446RE`), the green status LED (`LD2`) is wired to pin **`PA5`** (active-HIGH), and the user button (`B1`) is wired to pin **`PC13`** (active-LOW).
     * On a custom `Board B`, the status LED might be wired to pin **`PB14`** (active-LOW).
   * The STM32 Reference Manual (`RM0390`) explains how the `GPIOA` and `GPIOB` hardware registers work, but it does **not** know which pin your circuit board wired to the LED.
   * A **Board Support Package (`BSP`)** is the board-specific software layer that reads the board schematic (`MB1136 C03`) and maps human board names (`LED2`, `BUTTON_USER`) to the exact MCU clocks (`RCC->AHB1ENR`), pins (`PA5`, `PC13`), interrupt lines (`SYSCFG->EXTICR[3]`, `EXTI13`, `NVIC IRQ 40`), and polarities so the application code does not have raw pin numbers scattered everywhere.

2. **Meaning 2 — Bootstrap Processor (`BSP` in Multiprocessor Startup):**
   * On a multi-core processor (such as x86 or a multi-core SoC), one CPU core is designated at reset as the **Bootstrap Processor (`BSP`)** to initialize the system and wake up the other cores, which are called **Application Processors (`APs`)**.
   * Because our `STM32F446RE` has a single Cortex-M4 core, "BSP" in our `P1` and `P2` projects always means **Board Support Package**.

```text
+-----------------------------------------------------------------------------------+
| APPLICATION LAYER (`main.c` / FreeRTOS Tasks)                                     |
|   Calls: `BSP_LED_On(LED2)` or `bsp_status_led_write(true)`                       |
|   (Does not hardcode `PA5` or active-HIGH polarity in application logic)          |
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
| BOARD SUPPORT PACKAGE (`BSP`: Knows NUCLEO-F446RE MB1136 C03 Schematic)           |
|   - Maps `LED2` -> Port `GPIOA`, Pin `5` (`PA5`), Active-HIGH, `AHB1ENR` bit `0`  |
|   - Maps `B1`   -> Port `GPIOC`, Pin `13` (`PC13`), `SYSCFG EXTICR[3] = 2`, IRQ 40|
+-----------------------------------------------------------------------------------+
                                         |
                                         v
+-----------------------------------------------------------------------------------+
| MCU DRIVER / CMSIS REGISTER LAYER (`HAL_GPIO_WritePin` or Direct Register Write)  |
|   - Sets `RCC->AHB1ENR |= (1UL << 0)` and `GPIOA->MODER` bits `[11:10] = 0b01`    |
|   - Writes `GPIOA->BSRR = (1UL << 5)` (`0x0020`) -> sets `PA5` output latch HIGH  |
+-----------------------------------------------------------------------------------+
```

---

## Scene 1: The Two Meanings Of `BSP` (`Board Support Package` vs. `Bootstrap Processor`)

### Step-by-Step Explanation: Board Support Package (`BSP`) vs. Bootstrap Processor (`BSP`)

1. **Use a Board Support Package (`BSP` in embedded board/driver work) to collect the board-specific pin mappings, clock sources, and peripheral initializations for a specific PCB (`NUCLEO-F446RE` `MB1136 C03`).**
   * **Otherwise:** If board-specific pin numbers (`PA5`, `PC13`), active polarities, and clock crystals are scattered across application files, moving the application to a new PCB revision requires hunting down and editing raw pin numbers throughout the codebase. (Note that ST's `Drivers/BSP` folder is a narrow board-component layer, whereas a company's full BSP deliverable may also span startup, linker script, and RTOS hooks.)
   * **Layer / Mapping:** `Define board wiring in one board header/module (e.g., LD2 -> GPIOA pin 5, AHB1ENR bit 0; B1 -> GPIOC pin 13, EXTI13, IRQ 40).`

2. **Distinguish `Bootstrap Processor` (`BSP` in multiprocessor startup, such as x86 or multicore SoCs) as the CPU core assigned to lead initial boot before waking the `Application Processors` (`APs`).**
   * **Otherwise:** Confusing the two meanings of `BSP` in an interview or architecture document mixes up a **software board-support layer** with a **hardware multiprocessor boot-leader role**. (Our single-core `STM32F446RE` has only one Cortex-M4 core, so "BSP" in `P1`/`P2` means **Board Support Package**.)
   * **Architecture Role:** `Multicore boot: 1 Bootstrap Processor (BSP) initializes platform resources and dispatches Application Processors (APs); Single-core STM32F446RE: 1 Cortex-M4 core runs Reset_Handler() directly.`

```text
Illustrative multiprocessor startup:

Initial state                 Later state
one processor leads boot  ->  additional processors are initialized/dispatched
       BSP                               BSP + APs
```

In the current MCU/firmware course, "BSP development" normally means
**Board Support Package development**.

### Code for Scene 1: Board-Specific Wiring Definitions in a `NUCLEO-F446RE` BSP Header

```c
#include <stdint.h>

/* Board-specific mapping for NUCLEO-F446RE (MB1136 Rev C03 schematic):
 * Keeps PCB wiring facts in one place instead of scattering raw numbers: */
#define BSP_LD2_GPIO_BASE      0x40020000UL  /* GPIOA base address (RM0390 Table 1) */
#define BSP_LD2_RCC_AHB1_BIT   (1UL << 0)    /* RCC->AHB1ENR bit 0 (GPIOAEN)        */
#define BSP_LD2_PIN            5U            /* PA5 drives Green LED LD2 (UM1724)   */

#define BSP_B1_GPIO_BASE       0x40020800UL  /* GPIOC base address (RM0390 Table 1) */
#define BSP_B1_RCC_AHB1_BIT    (1UL << 2)    /* RCC->AHB1ENR bit 2 (GPIOCEN)        */
#define BSP_B1_PIN             13U           /* PC13 reads Blue Button B1 (UM1724)  */
#define BSP_B1_EXTI_PORT_CODE  2UL           /* SYSCFG_EXTICR4 bits [7:4] = 0b0010  */
#define BSP_B1_IRQn            40U           /* EXTI15_10 = IRQ 40 in NVIC          */
```

## Scene 2: Why Board Support Is Needed

```text
Same MCU on two boards
       |
       +-> Board A: status LED connected to PA5 (active HIGH)
       +-> Board B: status LED connected to PB14 (active LOW)
```

### Step-by-Step Explanation: Connecting Application Intent to Physical Board Wiring

1. **Read the board schematic (`MB1136 C03`) to find which MCU pin and polarity connect to each board device (`LD2` on `PA5` active-HIGH, `B1` on `PC13` active-LOW), and translate that into MCU register settings (`RM0390`).**
   * **Otherwise:** The MCU Reference Manual (`RM0390`) explains how `GPIOA` and `GPIOC` registers work, but it cannot tell you which pin the PCB designer wired to the status LED or whether writing `1` turns the LED ON or OFF.
   * **Register / Bit:** `Board A (NUCLEO-F446RE): LD2 ON sets GPIOA->BSRR bit 5 = 1 (active-HIGH on PA5); if a custom Board B wired an active-LOW LED on PB14, the BSP would set GPIOB->BSRR bit (14 + 16) = 1 without changing the application call.`

```text
Application intent: "turn on status LED"
                    |
                    v
Board-specific mapping and electrical settings (BSP)
                    |
                    v
MCU driver or direct register access (GPIOA->BSRR)
                    |
                    v
Correct pin (PA5) -> physical board device (LD2)
```

The application should not need scattered pin-number changes for every board
revision. Board-specific configuration belongs in an identifiable place.

### Code for Scene 2: Isolating Board Pin & Polarity Inside a BSP Function

```c
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, IDR, ODR, BSRR;
} BSP_GPIO_TypeDef;

#define BSP_GPIOA ((BSP_GPIO_TypeDef *)0x40020000UL)

/* Application calls `bsp_status_led_write(true)` without knowing PA5 or active-HIGH: */
static inline void bsp_status_led_write(bool on)
{
    /* On NUCLEO-F446RE (MB1136), PA5 is active-HIGH (BSRR bit 5 = ON, bit 21 = OFF): */
    BSP_GPIOA->BSRR = on ? (1UL << 5) : (1UL << (5U + 16U));
}
```

## Scene 3: One Concrete Nucleo BSP Example (`BSP_LED_Init` And `BSP_LED_On`)

```text
LED2 identifier (0) -> BSP mapping (GPIOA, PA5) -> RCC->AHB1ENR bit 0 + GPIOA->MODER[11:10] -> GPIOA->BSRR bit 5
```

The inspected public ST Nucleo BSP maps its `LED2` identifier to GPIOA pin 5.
That is a real board-support mapping, not a feature of all STM32 chips.

### Step-by-Step Explanation: What `BSP_LED_Init(LED2)` and `BSP_LED_On(LED2)` Do in Hardware

1. **Enable the `GPIOA` peripheral clock (`RCC->AHB1ENR` bit `0`) inside `BSP_LED_Init(LED2)` before configuring `PA5`.**
   * **Otherwise:** `GPIOA` is unclocked at reset (`RCC->AHB1ENR` bit `0 == 0`), so any write to `GPIOA->MODER` or `GPIOA->BSRR` is ignored by the peripheral and `PA5` cannot drive `LD2`.
   * **Register / Bit:** `Set RCC->AHB1ENR bit 0 (GPIOAEN = 1UL << 0) and read back RCC->AHB1ENR.`

2. **Configure `PA5` as push-pull output (`GPIOA->MODER` bits `[11:10] = 0b01`) and drive its initial output latch LOW (`GPIOA->BSRR` bit `21 = 1`) in `BSP_LED_Init(LED2)`.**
   * **Otherwise:** At reset `GPIOA->MODER` bits `[11:10]` are `0b00` (Input mode), so setting `ODR` bit `5` does not connect the output driver to the `PA5` pin.
   * **Register / Bit:** `Set GPIOA->MODER bits [11:10] = 0b01 (Output), clear GPIOA->OTYPER bit 5 = 0 (Push-Pull), and write GPIOA->BSRR = (1UL << 21) (initial LOW).`

3. **Set `PA5` output latch bit `5` HIGH (`GPIOA->BSRR = 0x0020`) when `BSP_LED_On(LED2)` calls `HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET)`.**
   * **Otherwise:** Using a non-atomic `GPIOA->ODR |= (1UL << 5)` in Thread mode can race if an ISR modifies another `GPIOA` pin between the load and store; writing `GPIOA->BSRR = (1UL << 5)` (`0x0020`) sets bit `5` atomically in one 32-bit write without touching bits `0..4` or `6..15`.
   * **Register / Bit:** `Write GPIOA->BSRR = (1UL << 5) (0x00000020UL) to set ODR bit 5 = 1.`

### Illustrative Caller Using The Public ST API

Preconditions: a compatible board, matching BSP/HAL/CMSIS versions, valid
system setup, and no competing use of the LED pin. This excerpt explains
the API; it does not add that BSP dependency to the existing project.

```c
/* Configure the board's LED pin and set its output latch low. */
BSP_LED_Init(LED2);

/* Use the BSP's mapping to set the LED pin's output latch high. */
BSP_LED_On(LED2);
```

**Syntax and contract:**

- `LED2` is an enumeration identifier with value 0 in this BSP, not pin 2.
- The BSP uses that identifier to select a port and pin from its board mapping.
- These two inspected functions return `void`: there is no success status to
  check from these calls.
- Their return does not measure the pin voltage or verify that the LED lit.
- The pin's configured output mode and the board's electrical connection
  determine the physical result.

### Follow The Work Through The Layers

```text
illustrative caller
  -> BSP_LED_On (upstream stm32f4xx_nucleo.c)
  -> HAL_GPIO_WritePin (stm32f4xx_hal_gpio.c)
  -> GPIOA->BSRR write
  -> GPIOA output latch bit 5 set
  -> pin/LED behavior depends on configuration and wiring
```

| # | Where (file : function) | Goal/state change | Concrete data in -> out | Context/mode |
| --- | --- | --- | --- | --- |
| 1 | Upstream `stm32f4xx_nucleo.c` : `BSP_LED_Init` | Enable GPIO clock, configure output, drive initial low latch | LED2 -> GPIOA pin 5 output, latch bit 5 = 0 | Assumed privileged Thread mode |
| 2 | Upstream `stm32f4xx_nucleo.c` : `BSP_LED_On` | Resolve board mapping and request high | LED2 = 0 -> GPIOA, mask `0x0020`, SET | Same synchronous call context |
| 3 | Local `stm32f4xx_hal_gpio.c` : `HAL_GPIO_WritePin` | Write the pin-set register | Mask `0x0020` -> BSRR write | Same synchronous call context |
| 4 | GPIO hardware | Update the selected output latch bit | Bit 5: 0 -> 1; other bits unchanged | Hardware response to register write |

The mask `0x0020` selects bit 5: binary `0010 0000`.
BSRR is the GPIO bit-set/reset register. Writing its lower bit 5 sets output
latch bit 5; it is not writing a whole new value to the output-data register.

```text
BEFORE BSP_LED_On                 AFTER BSP_LED_On
PA5 configured as output         PA5 remains configured as output
ODR bit 5 = 0             ->     ODR bit 5 = 1
other ODR bits unchanged         other ODR bits unchanged
```

There is no ISR handoff or privilege transition in this example.
The [local HAL implementation](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c#L410)
shows the BSRR write; physical behavior is a separate test.

## Scene 4: What Work Does A BSP Engineer Do? (Including Board Interrupt Routing)

```text
Board Schematic (B1 on PC13) -> STM32 Peripherals (GPIOC -> SYSCFG -> EXTI13) -> Arm Core (NVIC IRQ 40 -> Vector Slot 56)
```

| Work area | Actual engineering work | Useful evidence |
| --- | --- | --- |
| Board review | Read schematic/BOM, identify MCU and revisions, power/reset/clock sources, pin assignments and conflicts | Reviewed board configuration and unresolved-issue list |
| Initial bring-up | Coordinate electrical checks, establish debug access, confirm reset path and a known execution point | Reproducible reset/debug record |
| Startup and memory | Integrate startup/vector table, stack, linker regions, `.data` copy and `.bss` clearing | Map-file review, known-memory checks and fault-free startup |
| Clocks and power | Configure sources/dividers, voltage settings and Flash timing; handle startup failures | Clock configuration, bounded failure path and suitable measurements |
| Peripheral support | Configure pinmux and bus instances; integrate controller and external-device drivers | Normal transactions and error/timeout tests |
| Interrupts and DMA | Route IRQs, choose priorities, define buffer ownership and completion/cancellation behavior | Event traces, no stale-buffer reuse, bounded overload behavior |
| RTOS integration | Select existing architecture port, reconcile tick/IRQ priorities, stacks and allocation policy | Correct scheduling and ISR-to-task handoff under load |
| Diagnostics/recovery | Capture reset/fault causes, expose useful errors and implement required recovery | Failure records and demonstrated recovery path |
| Build and release | Maintain board targets, toolchain/library compatibility, flashing/debug configuration and documentation | Reproducible image with exact inputs and known limits |
| Maintenance | Support PCB/silicon revisions, errata and dependency updates | Regression results and documented compatibility |

### Step-by-Step Explanation: How a BSP Engineer Routes a Board Button Interrupt (`B1` on `PC13`) to the Arm Core

1. **Enable the `GPIOC` peripheral clock (`RCC->AHB1ENR` bit `2`) and configure `PC13` as Input (`GPIOC->MODER` bits `[27:26] = 0b00`) with Pull-Up (`GPIOC->PUPDR` bits `[27:26] = 0b01`).**
   * **Otherwise:** `GPIOC` remains disabled (`AHB1ENR` bit `2 == 0`), so the `PC13` input buffer cannot read the board button `B1`.
   * **Register / Bit:** `Set RCC->AHB1ENR bit 2 = 1; clear GPIOC->MODER bits [27:26] = 0b00; set GPIOC->PUPDR bits [27:26] = 0b01.`

2. **Enable the `SYSCFG` peripheral clock (`RCC->APB2ENR` bit `14`) and route `EXTI` line `13` to Port C (`SYSCFG->EXTICR[3]` bits `[7:4] = 0b0010`).**
   * **Otherwise:** `GPIOC` cannot interrupt the Arm CPU directly—all 16 GPIO ports share `16` `EXTI` lines through the `SYSCFG` multiplexer, which defaults to Port A (`PA13`) at reset. And if `RCC->APB2ENR` bit `14` (`SYSCFGEN`) is `0`, writes to `SYSCFG->EXTICR[3]` are ignored!
   * **Register / Bit:** `Set RCC->APB2ENR bit 14 = 1 (SYSCFGEN); set SYSCFG->EXTICR[3] bits [7:4] = 0b0010 (Port C -> EXTI13).`

3. **Configure the `EXTI` edge detector (`EXTI->FTSR` / `RTSR` bit `13`), clear stale pending flags (`EXTI->PR = 1UL << 13`), and unmask `EXTI->IMR` bit `13`.**
   * **Otherwise:** Without `FTSR`/`RTSR` bit `13`, `EXTI13` never latches an edge into `EXTI->PR`; and without `IMR` bit `13 = 1`, `EXTI13` never forwards the interrupt request wire (`IRQ 40`) to the Arm core.
   * **Register / Bit:** `Set EXTI->FTSR bit 13 = 1 (and RTSR bit 13 = 1 if both edges are needed), write EXTI->PR = (1UL << 13) (W1C), and set EXTI->IMR bit 13 = 1.`

4. **Set the interrupt priority (`NVIC->IP[40]`) and enable `IRQ 40` (`NVIC->ISER[1]` bit `8`) inside the Arm Cortex-M4 `NVIC`.**
   * **Otherwise:** Even when `EXTI13` asserts `IRQ 40`, the Arm core ignores `IRQ 40` until `NVIC->ISER[1]` bit `8` (`40 - 32 = 8`) is `1` and `PRIMASK` / `BASEPRI` permit that priority.
   * **Register / Bit:** `Set NVIC->IP[40] = (5U << 4), write NVIC->ICPR[1] = (1UL << 8), and write NVIC->ISER[1] = (1UL << 8) so the CPU jumps to g_pfnVectors[16 + 40] (EXTI15_10_IRQHandler).`

### Code for Scene 4: BSP Button Interrupt Initialization (`bsp_b1_exti_init`)

```c
#include "stm32f446xx.h"

/* Complete BSP helper routing NUCLEO-F446RE B1 (PC13) -> SYSCFG -> EXTI13 -> NVIC IRQ 40: */
static inline void bsp_b1_exti_init(void)
{
    NVIC->ICER[1] = (1UL << 8);                          /* 1. Mask IRQ 40 during setup */
    EXTI->IMR    &= ~(1UL << 13);

    RCC->AHB1ENR |= (1UL << 2);                          /* 2. Enable GPIOC clock (bit 2) */
    (void)RCC->AHB1ENR;
    RCC->APB2ENR |= (1UL << 14);                         /* 3. Enable SYSCFG clock (bit 14) */
    (void)RCC->APB2ENR;

    GPIOC->MODER &= ~(3UL << 26);                        /* 4. PC13 = Input (00) + Pull-up (01) */
    GPIOC->PUPDR  = (GPIOC->PUPDR & ~(3UL << 26)) | (1UL << 26);

    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~(15UL << 4)) | (2UL << 4); /* 5. PC13 -> EXTI13 */
    EXTI->RTSR |= (1UL << 13);                           /* 6. Rising & falling edge triggers */
    EXTI->FTSR |= (1UL << 13);

    NVIC->IP[40]  = (uint8_t)(5U << 4);                  /* 7. Priority 5 in upper 4 bits */
    EXTI->PR      = (1UL << 13);                         /* 8. Clear stale EXTI13 pending (W1C) */
    NVIC->ICPR[1] = (1UL << 8);                          /* 9. Clear stale NVIC IRQ 40 pending */
    EXTI->IMR    |= (1UL << 13);                         /* 10. Unmask EXTI13 + NVIC IRQ 40 */
    NVIC->ISER[1] = (1UL << 8);
}
```

Most projects **reuse vendor and upstream code**. BSP work is not automatically
writing every driver, startup routine and RTOS port from scratch.

Scope depends on the product:

- A small MCU board may need clocks, GPIO, UART, a few buses and FreeRTOS.
- A more complex board may add external memory, displays, boot/update support
  and low-power sequencing.
- A Linux BSP role often adds bootloader, device-tree, kernel-driver and image
  integration work. That is a different specialization, not a new prerequisite
  for the active MCU course.

## Scene 5: `BSP`, `HAL`/`LL`, `CMSIS` And The `RTOS` Are Different

```text
Application:             what the product should do
Board support (BSP):     what this board is and how its hardware is connected
Drivers / HAL / LL:      how to operate an MCU controller or device
CMSIS:                   standard Arm core interfaces and device register definitions
Hardware:                CPU, peripherals, board wiring and external components

RTOS: scheduling and synchronization, integrated with this platform
```

### Step-by-Step Explanation: What Each Software Layer Owns

1. **Use `CMSIS` (`core_cm4.h` + `stm32f446xx.h`) to access standard Arm Cortex-M4 registers (`SCB`, `NVIC`, `SysTick`, `MPU`) and STM32 peripheral structs (`RCC`, `GPIOA`, `EXTI`).**
   * **Otherwise:** Without `CMSIS`, every file must redeclare raw hex addresses and inline assembly intrinsics by hand. (`CMSIS` is a register/core interface, not a board support package.)
   * **Header / Symbol:** `#include "stm32f446xx.h" -> provides GPIOA->BSRR, RCC->AHB1ENR, NVIC->ISER, __disable_irq().`

2. **Use `HAL` / `LL` (`stm32f4xx_hal_gpio.c`) when you want reusable MCU peripheral APIs, and `BSP` (`stm32f4xx_nucleo.c`) when you want to map board names (`LED2`, `BUTTON_USER`) to specific MCU pins (`PA5`, `PC13`).**
   * **Otherwise:** `HAL` knows how to set a bit in `GPIOx->BSRR`, but it does not know which pin on `NUCLEO-F446RE` is wired to `LD2`; the `BSP` bridges that board wiring to `HAL` or direct `CMSIS` register writes.
   * **Call Chain:** `BSP_LED_On(LED2) [BSP] -> HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET) [HAL] -> GPIOA->BSRR = (1UL << 5) [CMSIS/Hardware].`

3. **Integrate the `RTOS` (`FreeRTOS` on `P1`, `Zephyr` on `P2`) to schedule tasks on `PSP` via `SVC`/`PendSV`/`SysTick` while keeping board pin/clock facts in the `BSP` / devicetree.**
   * **Otherwise:** Mixing board pin numbers into scheduler code makes both the RTOS configuration and the board support harder to maintain. (In Zephyr, board support is expressed through `devicetree` for hardware description and `Kconfig` for software feature selection.)
   * **Layer Boundary:** `RTOS owns task scheduling (PendSV, SysTick, queues); BSP/devicetree owns board pinmux, clocks, and device wiring.`

### Code for Scene 5: Side-by-Side Comparison of `BSP` vs. `HAL` vs. `CMSIS` Register Write

```c
/* All three lines below turn LD2 (PA5) ON on NUCLEO-F446RE at different layers: */

/* 1. BSP Layer (knows board name `LED2` -> maps to GPIOA, GPIO_PIN_5): */
BSP_LED_On(LED2);

/* 2. HAL Driver Layer (caller must supply MCU port `GPIOA` and pin `GPIO_PIN_5`): */
HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET);

/* 3. CMSIS / Direct Register Layer (writes bit 5 of GPIOA->BSRR at 0x40020018): */
GPIOA->BSRR = (1UL << 5);
```

## Scene 6: Which Technical Concepts Should Be Strong?

| Priority | Concepts | What practical strength looks like |
| --- | --- | --- |
| 1 | Embedded C: pointers, bounds, alignment, integer operations, bit fields, `volatile`, lifetime | Safely access registers and buffers; explain why `volatile` is not synchronization |
| 2 | Schematics and electrical basics: rails, reset, clocks, pulls, active levels, pinmux, voltage compatibility | Trace a signal from chip pin to connector/device and identify conflicting uses |
| 3 | CPU startup and toolchain: vector table, stack, linker symbols, sections, ELF/map, ABI, basic assembly | Explain the reset-to-`main()` path and diagnose an invalid stack/vector |
| 4 | Clock tree and timing: source/dividers, peripheral clocks, timer calculations, Flash latency | Derive the actual selected clocks and distinguish configured timing from measured timing |
| 5 | Peripheral protocols: GPIO, UART, SPI, I2C, timers, ADC; CAN when applicable | Read a datasheet and diagnose a transaction rather than only call an API |
| 6 | Exceptions and concurrency: NVIC, priorities/masks, ISR/task boundaries, bounded critical sections | Explain who can preempt whom and transfer data without races |
| 7 | DMA and memory: transfer units, accessible regions, ownership, alignment, cache coherency where present | Explain when a buffer is safe to read, modify, reuse or free |
| 8 | RTOS fundamentals: states, scheduling, queues/notifications, mutexes, stack budgets | Choose a synchronization mechanism from timing and ownership requirements |
| 9 | Debugging and verification: SWD/JTAG, registers, disassembly, fault capture, scope/logic analyzer | Follow evidence across software and hardware instead of changing settings randomly |
| 10 | Integration discipline: versioning, builds, errata, tests and clear failure reporting | Produce a repeatable result someone else can reproduce and maintain |

These are connected skills, not ten separate courses to finish first.

### Step-by-Step Debugging Example: Diagnosing Garbled `USART2` (`PA2`/`PA3`) Output on `NUCLEO-F446RE`

```text
Symptom: UART terminal shows unreadable characters
        |
        +-> 1. Wrong source clock or APB1 divider (RCC->CFGR)?
        +-> 2. Wrong pinmux (GPIOA->MODER / GPIOA->AFR[0] for PA2/PA3)?
        +-> 3. Wrong baud-rate divisor (USART2->BRR) or frame settings?
        +-> 4. Incompatible electrical levels or missing common ground?
```

1. **Verify the active system clock (`RCC->CFGR` bits `[3:2]` `SWS`) and `APB1` peripheral clock divider (`RCC->CFGR` bits `[12:10]` `PPRE1`) that feeds `USART2`.**
   * **Otherwise:** `USART2` sits on `APB1`. If software calculates `USART2->BRR` assuming `APB1 = 42 MHz` (`SYSCLK = 84 MHz`, `PPRE1 = /2`), but the CPU is still running on `16 MHz HSI` (`SWS = 0b00`), the actual baud rate is off by `42 / 16 = 2.625x` and the terminal prints garbage!
   * **Register / Bit:** `Check (RCC->CFGR & (3UL << 2)) == (2UL << 2) (PLL active) and (RCC->CFGR & (7UL << 10)) == (4UL << 10) (APB1 = HCLK / 2 = 42 MHz).`

2. **Verify that `PA2` (`USART2_TX`) and `PA3` (`USART2_RX`) are configured for Alternate Function mode (`GPIOA->MODER` = `0b10`) and mapped to `AF7` (`GPIOA->AFR[0]` = `7`).**
   * **Otherwise:** Leaving `PA2`/`PA3` in Input (`0b00`) or GPIO Output (`0b01`) mode, or selecting the wrong alternate function number in `GPIOA->AFR[0]` (Datasheet Table 11 shows `USART2` is `AF7`), leaves the `USART2` peripheral disconnected from the ST-LINK Virtual COM Port pins (`PA2`/`PA3` per `UM1724` Sec 6.8).
   * **Register / Bit:** `Set RCC->AHB1ENR bit 0 (GPIOAEN = 1) and RCC->APB1ENR bit 17 (USART2EN = 1); set GPIOA->MODER bits [5:4] = 0b10 (PA2 AF) and bits [7:6] = 0b10 (PA3 AF); set GPIOA->AFR[0] bits [11:8] = 7 (PA2 = AF7) and bits [15:12] = 7 (PA3 = AF7).`

3. **Compare the selected pins with the board schematic (`MB1136 C03`) and, with approved compatible equipment, measure signal timing/levels and record the result.**
   * **Otherwise:** Guessing and changing baud rates randomly without checking the clock tree, pinmux registers, and schematic hides the root cause.
   * **Verification Rule:** `Join code, chip manual (RM0390 / Datasheet), board schematic (MB1136 C03), and measured signal behavior into one verified explanation.`

### Code for Scene 6: BSP Diagnostic Check for `USART2` Clock & `PA2`/`PA3` (`AF7`) Pinmux

```c
#include <stdbool.h>
#include <stdint.h>
#include "stm32f446xx.h"

/* Returns true if GPIOA + USART2 clocks are enabled and PA2/PA3 are muxed to AF7 (USART2): */
static inline bool bsp_check_usart2_pa2_pa3_mux(void)
{
    bool clocks_ok = ((RCC->AHB1ENR & (1UL << 0)) != 0U) &&   /* GPIOAEN (bit 0)   */
                     ((RCC->APB1ENR & (1UL << 17)) != 0U);    /* USART2EN (bit 17) */
    bool moder_af  = ((GPIOA->MODER & (0xFUL << 4)) == (0xAUL << 4)); /* PA2,PA3 = 0b10 (AF) */
    bool afr_af7   = ((GPIOA->AFR[0] & (0xFFUL << 8)) == (0x77UL << 8)); /* PA2,PA3 = AF7 */
    return clocks_ok && moder_af && afr_af7;
}
```

## How This Fits P1/P2

1. Your current reset, vector table, linker, stack and clock study is directly
   relevant to BSP work.
2. GPIO/button/interrupt work adds pin mapping, hardware events and handoff.
3. P1 then adds FreeRTOS, bounded drivers, diagnostics and timer/ADC/DMA ownership.
4. P2 transfers those foundations to Zephyr's board/device model and the
   F767's relevant memory/cache constraints.
5. These are private learning activities. Neither reading this note nor
   running an LED example establishes complete BSP ownership or production
   experience.

## Document Lookup Cheat-Sheet

| Need to know | Look here |
| --- | --- |
| What is connected to this MCU pin? | Exact board-revision schematic and board manual |
| Is the pin/clock/voltage choice allowed? | MCU datasheet and electrical specifications |
| Which register controls the feature? | MCU reference manual |
| How do reset, exceptions and CPU registers work? | Cortex-M architecture/core documentation |
| What changes for a silicon revision? | Exact device errata |
| What did the linker actually place? | Linker script plus generated map/ELF |
| How does this API reach the hardware? | Matching BSP, driver and configuration source |
| What is required for a Zephyr board? | Board-porting guide for the selected Zephyr release |

## Feynman Recap

A Board Support Package is the software that knows the details of a particular
board and connects those details to reusable drivers and applications. A
Bootstrap Processor is a different thing: the processor leading multiprocessor
startup. BSP development means making the board boot, configuring its hardware,
integrating drivers and an RTOS where needed, and proving normal and failure
behavior. Strong C, hardware-document reading, startup, clocks, interrupts,
memory ownership and debugging matter more than memorizing API names.

## Short Interview Answers

### What is a Board Support Package?

- **Simple answer:** Board-specific software and configuration needed to use
  a board with applications or an OS.
- **How:** Connects actual clocks, memory, pins and devices to reusable support.
- **Why / trade-off:** Keeps board differences controlled; excessive wrappers
  can add complexity, so use the platform's existing conventions.

### Is a BSP the same as a HAL?

- **Simple answer:** No. A HAL operates hardware; a BSP describes and supports
  a particular board using those interfaces or lower-level code.
- **How:** The BSP knows which GPIO belongs to the board LED.
- **Why / trade-off:** The same MCU driver can support several different boards.

### What proves a board-support feature works?

- **Simple answer:** Reproducible evidence for the feature and its failure cases.
- **How:** Check configuration, observe behavior, test faults and record inputs.
- **Why / trade-off:** A successful build or an API returning is not a physical
  measurement or a complete reliability test.

## Sources

- [ST STM32F4 Nucleo BSP overview](https://github.com/STMicroelectronics/stm32f4xx-nucleo-bsp):
  component scope and CMSIS/HAL/BSP compatibility.
- [ST board header](https://github.com/STMicroelectronics/stm32f4xx-nucleo-bsp/blob/main/stm32f4xx_nucleo.h)
  and [implementation](https://github.com/STMicroelectronics/stm32f4xx-nucleo-bsp/blob/main/stm32f4xx_nucleo.c):
  LED2 mapping and initialization/on functions.
- [Local HAL GPIO source](../../../p1_f446re_baseline/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_hal_gpio.c#L410):
  pin set/reset register writes.
- [Zephyr board-porting guide](https://docs.zephyrproject.org/latest/hardware/porting/board_porting.html):
  board support builds on supported SoC features; use release-matched guidance
  when P2 implementation begins.
- [EDK II MP Services interface](https://github.com/tianocore/edk2/blob/master/MdePkg/Include/Protocol/MpService.h):
  BSP/AP roles and boot-time multiprocessor services.
- [Board inventory](../../docs/board_inventory.md),
  [project contracts](../../plans/project_portfolio.md) and
  [active roadmap](../../plans/five_month_development_roadmap.md).

## Corrections

- Initial publication, 2026-10-29.
- 2026-10-29: Updated Scenes 1–6 with step-by-step `"What & Why -> Otherwise... -> Register / Bit"` explanations, STM32-peripheral-to-Arm-core interrupt routing (`bsp_b1_exti_init`), and corresponding C code snippets per learner request.
