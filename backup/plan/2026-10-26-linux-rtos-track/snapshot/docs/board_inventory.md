# Hardware Inventory From Photographs

Reviewed 2026-10-25. Source: all 11 images in [board_images](board_images/).
**Photo identification is not an electrical test, authenticity check or proof
that a board boots.** No board was powered, flashed or probed by the tutor.

Use the [project allocation and fallback plan](board_project_plan.md) alongside
this inventory. The [roadmap](../plans/five_month_development_roadmap.md) remains
the only weekly schedule.

## Evidence Labels

- **Visible:** label, connector or feature readable in the photograph.
- **Identified:** model/family supported by visible markings and layout.
- **Probable:** exact controller, chip variant or board revision not established.
- **Reference capability:** from documentation, not measured on your unit.
- **Working status:** unknown for every photographed item until tested.

## 1. Inventory Summary

| ID | Item | Photo evidence | Confidence / unresolved detail | Recommended use |
| --- | --- | --- | --- | --- |
| B01 | ST NUCLEO-F446RE | P10: `NUCLEO-F446RE`, `MB1136 rev C`, onboard ST-LINK section | High model/PCB identification; silicon revision and condition unverified | Primary acquisition MCU for A |
| B02 | ST NUCLEO-F767ZI | P11: `NUCLEO-F767ZI`, `MB1137 Rev B`, Ethernet and ST-LINK | High model/PCB identification; MCU silicon cut not established | Primary standalone Zephyr board for B; conditional A substitute |
| B03 | BeagleBone Black family | P09: `beagleboard.org`, `AM335x`, Ethernet, eMMC, USB, microSD/HDMI layout | High family identification; exact revision/storage capacity not certified | Linux target for A |
| B04 | Original Raspberry Pi Pico, not Pi Zero | P06: Pico layout, Raspberry Pi branding/2020, BOOTSEL, micro-USB, SWD pads; no wireless module visible | High original-Pico identification; header pins not fitted in the photo | MCU spare, deterministic stimulus or later protocol test fixture |
| B05 | ESP32 development board, HW-463 | P07: `HW-463`, classic ESP32-style pin labels/module, CP2102 USB-UART | High ESP32-family identification; module suffix, flash size and exact compatible board target unresolved | Optional SPI protocol emulator or later RTOS backup |
| B06 | ESP8266 NodeMCU-style board | P08: `ESP8266MOD`, ESP-12-style module, D0-D8/A0 labels, FLASH/RST | High ESP8266-family identification; exact ESP-12 suffix/board variant unresolved | Optional simple telemetry peer; not a core DMA/Zephyr replacement |
| B07 | UNO R3-style AVR board(s) | P04/P05: UNO layout, socketed 28-pin AVR, USB-B, analog/digital headers; P05 says UNO | High UNO-style identification; exact AVR/USB bridge markings and board count need confirmation | Small GPIO/UART/I2C exercises or slow stimulus only |
| D01 | 1.8-inch SPI TFT module with microSD socket | P01 rear says `1.8 TFT SPI 128*160 V1.1`; P02 consistent front view | Dimensions/interface printed; controller unknown, ST7735-family only a candidate | Optional B status/waveform display, not a core dependency |
| D02 | Parallel character LCD, probably 16x2 | P03: 16-position header labelled VSS/VDD/VO/RS/RW/E/D0-D7/backlight | Geometry/controller probable; rear label not supplied; no I2C backpack visible | Optional low-rate status display after voltage/controller checks |
| T01 | Saleae logic analyser | Previously learner-reported, not in these 11 photos | Model, channels, probe limits and condition unresolved | Planned timing/logic evidence, after verification |

P04 and P05 may show two UNO-style units, but separate photos do not establish
the count. P01/P02 are treated as two views of one TFT candidate, not two
confirmed displays. Do not infer spare-board quantities from photo count alone.

## 2. Item Descriptions

### B01: NUCLEO-F446RE

The onboard debug section and accessible headers make this the least disruptive
starting point for the existing STM32 acquisition plan. The
[local Zephyr board documentation](../../zephyr-workspace/zephyr/boards/st/nucleo_f446re/doc/index.rst)
describes Cortex-M4 with FPU, 512 KB flash, 128 KB SRAM, timers, ADC, SPI, I2C,
UART and CAN, plus ST-LINK/V2-1.

These are chip/board reference capabilities, not proof that every pin/peripheral
is enabled in our selected build. Maximum CPU frequency is not the actual clock.
Do not infer exact sampling accuracy or DMA accessibility from the model name.
Use this board for timer -> ADC -> DMA-block ownership before transport.

### B02: NUCLEO-F767ZI

This resolves the previously unknown second STM32. The
[local board documentation](../../zephyr-workspace/zephyr/boards/st/nucleo_f767zi/doc/index.rst)
describes Cortex-M7 with FPU, 2 MB flash, 512 KB SRAM, ADC/DMA, serial peripherals,
CAN and onboard ST-LINK/V2-1. Ethernet connector/PHY hardware is present.

Good additions for B are cache-aware DMA reasoning, memory placement, multi-rate
tasks and optional Ethernet telemetry. Not all SRAM regions are necessarily
accessible to each DMA engine. Inspect the actual memory map/driver/configuration.
Do not assume disabling cache or using `volatile` solves ownership/coherency.

Two important gates:

1. The [board DTS](../../zephyr-workspace/zephyr/boards/st/nucleo_f767zi/nucleo_f767zi.dts)
   explicitly warns that default SPI1 MOSI and Ethernet conflict on **PA7**.
   Select a verified alternate mapping or defer one peripheral; no wiring
   prescription is being approved by this inventory.
2. The local board documentation warns about Ethernet instability on a specific
   **silicon cut A**. `MB1137 Rev B` is a PCB revision, not the silicon cut.
   Inspect chip marking/device identity and the applicable ST errata before
   attributing any fault to it. Do not assume this photographed board is affected.

The local defconfig enables MPU/hardware stack-protection options, but only a
real generated configuration and test establishes what our application uses.

### B03: BeagleBone Black

The [official board overview](https://docs.beagleboard.org/boards/beaglebone/black/ch04.html)
describes an AM335x/Cortex-A8 Linux SBC with Ethernet, expansion headers and
boot storage. It is **32-bit Armv7-A**, not Arm64. Exact board revision and eMMC
capacity should come from the board identity/boot inspection, not this photo.

This is the only Linux SBC identified in the image set. The Pico, ESP boards
and Nucleos are not drop-in replacements for it. Use BBB for IIO/client-driver,
device-tree, lifecycle and deployment evidence, not as the Yocto build host.
Check header pinmux conflicts, removable-media recovery and serial access.
BBB digital interfaces and its analog inputs have different electrical limits;
do not apply a generic 3.3 V assumption to ADC inputs.

### B04: Raspberry Pi Pico

This is not a Pi Zero. The [local Pico documentation](../../zephyr-workspace/zephyr/boards/raspberrypi/rpi_pico/doc/index.rst)
describes RP2040, Cortex-M0+ cores, 264 KB SRAM, 2 MB onboard flash, ADC,
SPI/I2C/UART, PWM and programmable I/O. The non-W Pico has no onboard Wi-Fi.

The inspected Zephyr revision documents single-core operation and specific PIO
tooling limits; physical dual cores do not prove a selected RTOS configuration
uses both. Recheck the pinned version before proposing multicore work.
BOOTSEL/UF2 provides a documented programming path; SWD requires a suitable
probe and connections. The unpopulated edge headers may require approved
soldering or another reliable connection method before peripheral use.

Best near-term role: digital test stimulus or small protocol/state-machine
fixture. ADC/DMA replacement work needs a fresh port and validation, not merely
rebuilding the STM32 image.

### B05: ESP32 HW-463 Board

The visible layout is consistent with a classic ESP32 development module,
not an ESP32-S3/C3 identification. CP2102 is the USB-UART bridge, not the MCU.
Do not assign flash/PSRAM capacity or a module suffix from the glare-obscured
shield. Confirm chip identity through an approved read-only discovery step.

Espressif documents an
[ESP32 SPI target driver](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_slave.html)
with queued transfers, readiness coordination and DMA-buffer restrictions.
That makes a useful **candidate** for a synthetic Project A device if the
STM32 transport blocks. Pin ESP-IDF and verify the actual module before use.
This is not proof of compatible ADC accuracy or automatic Zephyr portability.

### B06: ESP8266 NodeMCU-Style Board

The ESP8266 module and NodeMCU-style headers indicate an MCU/Wi-Fi development
board, not a Linux computer. The
[vendor RTOS SDK guide](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/index.html)
describes its own toolchain/framework.

Use only if a simple auxiliary task adds value. It is a lower-priority spare
than the F767/F446/Pico for this course; do not add another SDK just because a
board is available. Board A0 divider/range and USB-UART details need confirmation.
No claim that it replaces STM32 CAN, DMA or the Zephyr application unchanged.

### B07: UNO R3-Style Board(s)

The visible socketed AVR/USB-B/header layout matches the
[UNO Rev3 family](https://docs.arduino.cc/hardware/uno-rev3).
The normal reference design uses ATmega328P and has much smaller RAM than the
STM32 boards. Authenticity, exact chip marking and count remain unverified.

Treat it as a potential **5 V logic board until confirmed**. Arduino-compatible
header placement on a Nucleo does not imply identical electrical tolerance.
Use appropriate level compatibility checks before connecting to 3.3 V devices.
Do not use it as the fallback for the recorder's 20 KB sample window or a
Cortex-M/Zephyr/cache experiment. Slow stimulus or simple UART/I2C behavior is
a more realistic role.

### D01: SPI TFT + microSD Module

Rear labels show display VCC/GND/CS/RESET/A0/SDA/SCK/LED and separate
SD_CS/SD_MOSI/SD_MISO/SD_SCK pads. Here the printed `SDA` beside CS/SCK is
consistent with **SPI serial data**, not evidence that the display is I2C.
An SD socket is not a fitted/tested card.

ST7735-family hardware is a plausible candidate, **not an identification**.
Confirm the controller, initialization variant, operating voltage, I/O tolerance
and backlight current from a seller record/rear details before selecting a driver.
No touch controller is established by the photos.

At a proposed 16-bit color format, a full frame is
`128 * 160 * 2 = 40960 bytes` (40 KiB). A 16-row tile is 4096 bytes.
This creates a useful optional bounded-buffer/display-refresh decision in B,
but a framebuffer is not mandatory and display failure must not stop sensing.

### D02: Parallel Character LCD

Visible RS/RW/E/D0-D7 pins indicate the familiar parallel character-module
interface. A 16x2, HD44780-compatible module is likely but the exact model,
controller and voltage requirements are not readable from this front photo.
Do not call it an I2C LCD without an identified backpack.

A low-rate status display is a possible B extension. Verify signal thresholds,
contrast/backlight requirements and any return-data voltage before wiring.
Use LEDs/UART as the primary status output until the module is known.

## 3. Accessories Not Established By These Photos

- Saleae model and probe set; supported digital input voltage/sample rate.
- USB data cables, regulated supplies, microSD card/capacity and USB-TTL serial
  adapter for BBB recovery.
- CAN transceivers, termination, wiring and a tested two-node physical bus.
- Level shifting, breadboard, secure connectors, low-voltage input conditioning,
  protection and reference test signals for ADC acquisition.
- Any external temperature, acceleration or other measurement sensor.

Not pictured does not mean not owned. Do not buy duplicates before inventory
confirmation. No exact shopping list or budget is approved by this document.
Never connect these prototypes to mains, vehicles, high-energy battery systems
or safety-critical equipment.

## 4. Complete Photo Index

Photos were not renamed, modified or uploaded to external services.

| Photo | Item/view |
| --- | --- |
| [P01](<board_images/WhatsApp Image 2026-10-25 at 11.21.50 AM.jpeg>) | TFT rear: interface labels and microSD socket |
| [P02](<board_images/WhatsApp Image 2026-10-25 at 11.21.51 AM.jpeg>) | TFT front, apparently the same module |
| [P03](<board_images/WhatsApp Image 2026-10-25 at 11.21.51 AM (1).jpeg>) | Parallel character LCD front/header |
| [P04](<board_images/WhatsApp Image 2026-10-25 at 11.21.51 AM (2).jpeg>) | UNO-style AVR board |
| [P05](<board_images/WhatsApp Image 2026-10-25 at 11.21.52 AM.jpeg>) | UNO-labelled board; count relative to P04 unconfirmed |
| [P06](<board_images/WhatsApp Image 2026-10-25 at 11.21.52 AM (1).jpeg>) | Original Pico |
| [P07](<board_images/WhatsApp Image 2026-10-25 at 11.21.52 AM (2).jpeg>) | ESP32-family HW-463 |
| [P08](<board_images/WhatsApp Image 2026-10-25 at 11.21.52 AM (3).jpeg>) | ESP8266 NodeMCU-style board |
| [P09](<board_images/WhatsApp Image 2026-10-25 at 11.21.53 AM.jpeg>) | BeagleBone Black family |
| [P10](<board_images/WhatsApp Image 2026-10-25 at 11.21.53 AM (1).jpeg>) | NUCLEO-F446RE, MB1136 rev C |
| [P11](<board_images/WhatsApp Image 2026-10-25 at 11.21.53 AM (2).jpeg>) | NUCLEO-F767ZI, MB1137 Rev B |

Local Zephyr reference revision: `55f132b58`. Online `latest` documentation is
research context, not a pinned toolchain or a successful build. Some online
source-text URLs were unavailable; the local board documentation was inspected
instead. Remaining controller/variant uncertainties are intentionally explicit.
