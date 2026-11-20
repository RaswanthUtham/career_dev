# W01-L08: Creating Our Clean Windows CubeIDE Project (`p1_f446re_baseline`)

> **Source-reviewed:** Generated project in
> [p1_f446re_baseline](../../../p1_f446re_baseline), STM32CubeIDE `1.16.1`,
> CubeMX `6.12.1`, and `STM32Cube FW_F4 V1.28.3`, 2026-10-27.

Previous: [W01-L07: From C Source To Running Firmware On The F446RE](W01_L07_f446re_build_flash_and_debug_path.md).

## 0. Know This First

Before writing driver code, we set up a clean workshop on Windows using
**STM32CubeIDE** so our project files stay organized and our handwritten C code
stays safe whenever we update pin or clock settings.

| Term | What it does in our setup |
| --- | --- |
| **STM32CubeIDE (`1.16.1`)** | All-in-one Windows tool combining the Eclipse editor, STM32CubeMX pin/clock configurator, `arm-none-eabi-gcc` (`12.3.rel1`) compiler, and GDB debugger. |
| **IDE Workspace (`mcu_ide_ws`)** | Folder holding IDE window layout and preferences (`.metadata`). |
| **Project Folder (`p1_f446re_baseline`)** | Standalone folder holding all source code, linker scripts, and build settings for our F446RE firmware. |
| **`.ioc` File (`p1_f446re_baseline.ioc`)** | Hardware blueprint file read by CubeMX (stores chip model `STM32F446RETx`, `84 MHz` clock tree, `PA5` LED pin, and `USART2` pin choices). |
| **STM32CubeF4 Package (`V1.28.3`)** | ST's library package providing CMSIS headers (`stm32f446xx.h`) and HAL peripheral drivers (`stm32f4xx_hal_*.c`). |
| **`USER CODE BEGIN / END` Markers** | Protected comment blocks inside generated files (`main.c`, `stm32f4xx_it.c`). CubeMX preserves everything you type inside these markers when regenerating code. |

---

## 1. Goal

Understand how our Windows workshop is organized, what each generated folder in
`p1_f446re_baseline` does, and how `/* USER CODE BEGIN ... */` zones protect
your handwritten C code.

---

## 2. The Story: Building the Workshop Blueprint

### Scene 1: Separating the IDE Workspace From the Project Folder
We keep three sibling folders under `C:\workspace\career_ws\`:

```text
C:\workspace\career_ws\
  |-- career_dev\            (Lessons, plans, and notes)
  |-- mcu_ide_ws\            (CubeIDE workspace settings: .metadata\)
  `-- p1_f446re_baseline\    (Our STM32F446RE firmware project)
```

Keeping `p1_f446re_baseline` in its own sibling folder means your firmware
source code stays clean and independent of IDE window settings.

### Scene 2: How CubeMX Turns `.ioc` Choices Into C Code

```text
BEFORE Code Generation:                  AFTER Code Generation (`p1_f446re_baseline`):
+------------------------------------+   +------------------------------------------------+
| Wizard choices:                    |   | p1_f446re_baseline\                            |
| - Board: NUCLEO-F446RE             |   |   |-- p1_f446re_baseline.ioc (CubeMX blueprint)|
| - MCU:   STM32F446RETx             |   |   |-- STM32F446RETX_FLASH.ld (512K Flash/128K) |
| - Pkg:   STM32Cube FW_F4 V1.28.3   |   |   |-- Core\Inc\main.h        (LD2=PA5, B1=PC13)|
| - Defaults: LD2 (PA5), B1 (PC13),  |   |   |-- Core\Src\main.c        (init calls+loop) |
|             USART2 (PA2/PA3),      |   |   |-- Core\Src\stm32f4xx_it.c(ISR handlers)    |
|             HSI+PLL -> 84 MHz      |   |   |-- Core\Startup\startup_stm32f446retx.s     |
|                                    |   |   `-- Drivers\ (CMSIS + STM32F4xx_HAL_Driver)  |
+------------------------------------+   +------------------------------------------------+
```

---

## 3. Tour of the Generated Files in `p1_f446re_baseline`

| File / Folder | What it does |
| --- | --- |
| [p1_f446re_baseline.ioc](../../../p1_f446re_baseline/p1_f446re_baseline.ioc) | Stores your pinout, clock tree (`84 MHz`), and peripheral settings for CubeMX. |
| [Core/Inc/main.h](../../../p1_f446re_baseline/Core/Inc/main.h) | Defines board pin names: `LD2_Pin` (`GPIO_PIN_5` on `GPIOA`), `B1_Pin` (`GPIO_PIN_13` on `GPIOC`), and `USART_TX_Pin`/`RX_Pin` (`PA2`/`PA3`). |
| [Core/Src/main.c](../../../p1_f446re_baseline/Core/Src/main.c) | Runs `HAL_Init()`, `SystemClock_Config()`, `MX_GPIO_Init()`, `MX_USART2_UART_Init()`, and the main `while (1)` loop. |
| [Core/Src/stm32f4xx_hal_msp.c](../../../p1_f446re_baseline/Core/Src/stm32f4xx_hal_msp.c) | Low-level MCU Support Package callbacks (`HAL_MspInit`, `HAL_UART_MspInit`) that enable peripheral clocks and pin alternate functions (`AF7` for `USART2`). |
| [Core/Src/stm32f4xx_it.c](../../../p1_f446re_baseline/Core/Src/stm32f4xx_it.c) | Exception and interrupt handlers, including `SysTick_Handler()` calling `HAL_IncTick()`. |
| [Core/Startup/startup_stm32f446retx.s](../../../p1_f446re_baseline/Core/Startup/startup_stm32f446retx.s) | Vector table (`g_pfnVectors`) and `Reset_Handler` assembly startup code. |
| [STM32F446RETX_FLASH.ld](../../../p1_f446re_baseline/STM32F446RETX_FLASH.ld) | Linker script mapping `FLASH` (`0x08000000`, `512K`) and `RAM` (`0x20000000`, `128K`). |

---

## 4. How `USER CODE` Blocks Protect Your Handwritten Code

Inside [Core/Src/main.c](../../../p1_f446re_baseline/Core/Src/main.c#L96), CubeMX
places comment pairs like this:

```c
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* <--- Any code you write here is preserved when CubeMX regenerates! */
  }
  /* USER CODE END 3 */
```

```text
When you change a pin in .ioc and click "Generate Code":
  - Code OUTSIDE `USER CODE BEGIN / END` -> refreshed by CubeMX from the .ioc blueprint
  - Code INSIDE  `USER CODE BEGIN / END` -> kept intact!
  - Your own separate driver files (`my_driver.c` / `my_driver.h`) -> kept intact!
```

---

## 5. Control Flow: From `.ioc` Blueprint to Generated Project

```text
Select Board (NUCLEO-F446RE) -> CubeMX writes p1_f446re_baseline.ioc
  -> Code Generator writes Core/ + Drivers/ + STM32F446RETX_FLASH.ld + .cproject
  -> Ready for Debug Build (Lesson 9)
```

| # | Step | Input -> Output |
| --- | --- | --- |
| 1 | Launch STM32CubeIDE `1.16.1` | Opens workspace `C:\workspace\career_ws\mcu_ide_ws` |
| 2 | Board Selector: `NUCLEO-F446RE` | Selects `STM32F446RETx` and board pin defaults (`LD2`, `B1`, `USART2`) |
| 3 | Code Generation (`FW_F4 V1.28.3`) | Populates `Core/`, `Drivers/`, and `STM32F446RETX_FLASH.ld` in `p1_f446re_baseline` |
| 4 | Handwritten edits | Placed inside `/* USER CODE BEGIN ... */` or dedicated driver `.c`/`.h` files |

---

## 6. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
Think of CubeMX and the `.ioc` file as an architect who draws the house walls
(clocks, pins, and startup files), and the `/* USER CODE BEGIN */` boxes as your
own rooms inside the house. Whenever you ask the architect to add a new window
(enable a new timer or SPI pin in `.ioc`), the architect redraws the walls while
leaving everything inside your `USER CODE` rooms and your separate driver files
untouched!

---

### Interview Q1: What is the difference between an STM32CubeIDE workspace and a project?
* **Simple answer:** The **workspace** (`mcu_ide_ws`) holds local IDE preferences and window state (`.metadata`), while the **project** (`p1_f446re_baseline`) holds the actual firmware source files, linker script, `.ioc` hardware configuration, and compiler/linker settings (`.project`, `.cproject`).

### Interview Q2: How do you prevent STM32CubeMX code regeneration from overwriting your driver code?
* **Simple answer:**
  1. Keep all hooks inside generated files strictly between `/* USER CODE BEGIN ... */` and `/* USER CODE END ... */` comments.
  2. Put your core driver state machines and application modules in your own separate `.c` and `.h` files, and call them from the `USER CODE` blocks in `main.c` or `stm32f4xx_it.c`.

### Interview Q3: What is the role of `CMSIS` vs. the `STM32 HAL` in a CubeIDE project?
* **Simple answer:**
  * **CMSIS (`Drivers/CMSIS/`):** Arm's standard interface for the Cortex-M4 core itself (core registers like `SysTick` and `NVIC`, plus ST's register address map `stm32f446xx.h` and `SystemInit()`).
  * **STM32 HAL (`Drivers/STM32F4xx_HAL_Driver/`):** ST's peripheral driver library (`HAL_Init`, `HAL_RCC_*`, `HAL_GPIO_*`, `HAL_UART_*`) that configures STM32 peripherals using those CMSIS register definitions.

## 7. Corrections

- 2026-10-26: Initial publication.
- 2026-10-27 (revision 2): Updated to reflect the generated `p1_f446re_baseline` project (`STM32Cube FW_F4 V1.28.3`) in direct story mode.
