# W04-L25: I2C, SPI, ADC And DMA — Simple Foundations And Continuous 4-Mode Use-Case Scenarios

> **Status:** Document/source-reviewed, 2026-11-18 (rev 5). Based on NXP UM10204
> Rev 7.0, STM32F446 RM0390 Rev 6, DS10693 Rev 10, and Arm DUI0553B, with
> cross-MCU architecture notes for STM32 (Arm Cortex-M), Infineon AURIX
> (TriCore), and RISC-V. Concrete register examples target `STM32F446RE` via
> CMSIS (`stm32f446xx.h`, assuming `SYSCLK = 84 MHz`, `APB1 = 42 MHz`,
> `APB2 = 84 MHz` from [W01-L17](W01_L17_b1_exti_interrupt_handoff_and_debounce.md)).
> Sensor addresses, voltages, and traces are synthetic; verify target datasheets,
> board pins, voltage rails, and errata before hardware wiring.

Read in order:
- **Part I — Hardware Foundations (`Scenes 1–5`):** `Scene 1: I2C & pull-ups` $\rightarrow$ `Scene 2: SPI & resistors` $\rightarrow$ `Scene 3: ADC basics` $\rightarrow$ `Scene 4: Generic MCU DMA & Stream vs. Channel` $\rightarrow$ `Scene 5: Step-by-step Interrupt Paths (Pin, SPI/I2C, ADC, DMA)`.
- **Part II — Polling, Interrupt, DMA Polling & DMA Interrupt (`Scenes 6–9`):** `Scene 6: SPI (6 bytes 0xAA..0xFF)` $\rightarrow$ `Scene 7: I2C (6 bytes 0xAA..0xFF)` $\rightarrow$ `Scene 8: ADC (3 channels / 6 bytes)` $\rightarrow$ `Scene 9: Production DMA ownership, debugging & interview Q&As`.

---

## Know This First

| Term | Simple meaning |
| --- | --- |
| Protocol | Rules for sending bits on wires and interpreting them |
| Bus | Shared communication wires (`I2C`, `SPI`, or internal MCU `AHB`/`APB` buses) |
| Controller / target | Device initiating the transfer / device responding (manuals also say master/slave) |
| Clock (`SYSCLK`, `PCLK`, `SCK`, `SCL`, `ADCCLK`) | Repeating pulse telling hardware how fast to run or when to sample a bit |
| Open-drain vs. Push-pull | Open-drain only pulls LOW or releases (needs pull-up resistor for HIGH); push-pull actively drives both HIGH and LOW |
| GPIO Mode (`MODER`) | Selects pin role: `00` = Input, `01` = Output, `10` = Alternate Function (`SPI`/`I2C`), `11` = Analog (`ADC`) |
| Sample-and-Hold (`S&H`) | Tiny internal switch + capacitor inside the `ADC` that captures pin voltage so it stays steady during conversion |
| `SAR` (`Successive Approximation`) | Binary-search comparator inside the `ADC` that narrows a voltage down to a `12-bit` number (`0..4095`) one bit per `ADCCLK` |
| `DMA` (`Direct Memory Access`) | Hardware copy engine on the MCU bus matrix that moves data between `PERIPH->DR` and `SRAM` without running CPU instructions |
| Polling vs. Interrupt | **Polling:** CPU loops reading a status bit; **Interrupt:** hardware sets a flag that makes `NVIC` vector the CPU into an `ISR` |
| `W1C` vs. `rc_w0` | **Write-`1`-to-Clear** (`DMA->LIFCR`, `EXTI->PR`: write `1` to clear) vs. **Write-`0`-to-Clear** (`ADC1->SR`, `I2C1->SR1` errors: write `0` to clear, `1` leaves other bits unchanged!) |
| Quiescent | Neither `DMA`, peripheral shift registers, nor `ISR`s can touch the buffer anymore |

Prerequisites: [bare-metal `84 MHz` clock & `EXTI`](W01_L17_b1_exti_interrupt_handoff_and_debounce.md),
[buffer ownership (`C03`)](W03_L23_c03_producer_consumer_payload_ownership.md), and
[FreeRTOS synchronization](W04_L24_freertos_synchronization_and_priority_inversion.md).

---

## Continuous Use-Case Overview (`STM32F446RE` At `84 MHz`)

Across the entire lesson, one `STM32F446RE` MCU (`SYSCLK = 84 MHz`, `APB1 = 42 MHz`, `APB2 = 84 MHz`) handles **6 bytes of data** on each peripheral through **four progressive transfer methods**:

1. **`SPI1` (`PA4=CS, PA5=SCK, PA6=MISO, PA7=MOSI`, `APB2 = 84 MHz`):**
   * Sends **6 bytes** `tx_6[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }` on `MOSI` while simultaneously receiving **6 reply bytes** `rx_6[6] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 }` on `MISO`.
2. **`I2C1` (`PB8=SCL, PB9=SDA`, `APB1 = 42 MHz`, Target `0x48`, Register `0x10`):**
   * Addresses target `0x48` (`0x90`), selects register `0x10`, and writes the **same 6 payload bytes** `tx_6[6] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF }` (and we also trace reading bytes back with Repeated `START`).
3. **`ADC1` (`PA0=IN0`, `PA1=IN1`, `PB0=IN8`, `APB2 = 84 MHz -> ADCCLK = 21 MHz`):**
   * Converts **3 analog sensor voltages** (`1.650 V`, `0.825 V`, `2.475 V`) into **3 x 16-bit halfwords = 6 bytes** in SRAM: `adc_3[3] = { 0x0800, 0x0400, 0x0C00 }`.

For **each** of `SPI1`, `I2C1`, and `ADC1`, we walk the exact same 5-stage ladder:
```text
[Step 0: 84 MHz Clock + GPIO + Peripheral Setup]
        |
        +--> Mode 1: CPU Polling    (CPU loops on peripheral SR flags & copies each item)
        +--> Mode 2: CPU Interrupt  (Peripheral IRQ fires on EVERY byte/channel; CPU ISR copies item)
        +--> Mode 3: DMA Polling    (DMA copies all 6 bytes in HW; CPU polls DMA TCIF flag, 0 IRQs)
        +--> Mode 4: DMA Interrupt  (DMA copies all 6 bytes in HW; CPU runs tasks & gets 1 IRQ at end!)
```

---

# Part I — Simple Hardware Ideas (`I2C`, `SPI`, `ADC`, And `DMA`)

## Scene 1: Simple Idea Of `I2C` And Its Pull-Up Resistors

### 1. Two Shared Open-Drain Wires (`SCL` And `SDA`)

`I2C` (*Inter-Integrated Circuit*) connects one controller to multiple targets using just two signal wires (plus common `GND` and power):

```text
           3.3 V          3.3 V
             |              |
          R_pull         R_pull      (e.g., 2.2 kohm each)
             |              |
Controller --+---- SCL -----+------> Target 0x48 (Sensor)
             |              |        Target 0x68 (RTC)
Controller <-+---- SDA -----+------>
```

* **`SCL` (Serial Clock):** Driven by the controller (`100 kHz` Standard-mode or `400 kHz` Fast-mode). A target can hold `SCL` low (*clock stretching*) if it needs a moment.
* **`SDA` (Serial Data):** Bidirectional! Controller and target take turns pulling `SDA` low.
* **Why Open-Drain Needs Pull-Up Resistors:** Neither chip actively drives `3.3 V` onto `SCL` or `SDA`—each pin only has a low-side transistor switch to `GND`.
  * Switch **closed** $\rightarrow$ pulls wire **LOW (`~0 V`)**.
  * **All** switches **open (released)** $\rightarrow$ external `R_pull` charges wire capacitance up to **HIGH (`3.3 V`)**.
  * This prevents a short-circuit (`3.3 V` fighting `GND`) when `SDA` reverses direction for `ACK`!

### 2. Choosing Pull-Up Resistor Values (`R_min` To `R_max`)

```text
R_min = (V_pullup - V_OLmax) / I_OL      (Too small R -> too much sink current, LOW rises above V_OLmax)
R_max = t_r_max / (0.8473 * C_bus)       (Too large R -> RC rise too slow, HIGH arrives too late!)
```

**Where:**
* `R_pull`: External pull-up resistor value (in ohms, $\Omega$) wired from `SCL` or `SDA` to `V_pullup`.
* `R_min`: Minimum allowed pull-up resistance ($\Omega$) so a device pulling the line LOW does not exceed its sink-current rating.
* `R_max`: Maximum allowed pull-up resistance ($\Omega$) so wire capacitance charges to HIGH within the protocol's rise-time limit.
* `V_pullup`: Pull-up supply voltage (in volts, `V` — e.g., `3.3 V`).
* `V_OLmax`: Maximum Low-level Output Voltage (`V` — `0.4 V` in `UM10204` Table 10), the highest wire voltage still guaranteed to be recognized as logic `0` while sinking `I_OL`.
* `I_OL`: Low-level Output sink Current (in amperes, `A` — `3 mA = 0.003 A` for Standard/Fast-mode), the guaranteed current the weakest pin on the bus can sink to `GND` while holding the line at or below `V_OLmax`.
* `t_r_max`: Maximum permitted bus rise time (in seconds, `s` — `1000 ns = 1000e-9 s` for `100 kHz` Standard-mode; `300 ns = 300e-9 s` for `400 kHz` Fast-mode), measured from `30%` to `70%` of `V_pullup`.
* `C_bus`: Total capacitance on one bus wire (in farads, `F` — e.g., `100 pF = 100e-12 F`), equal to PCB trace capacitance + all attached device pin capacitances + scope/analyzer probe capacitance.
* `0.8473`: Exact RC charging constant $\ln(0.7 / 0.3) \approx 0.8473$ required for an RC network to rise from `30%` to `70%` of `V_pullup`.

* **Concrete Example (`V_pullup = 3.3 V`, `V_OLmax = 0.4 V` at `I_OL = 3 mA`, `C_bus = 100 pF`, `400 kHz` Fast-mode `t_r_max = 300 ns`):**
  * `R_min = (3.3 - 0.4) / 0.003 = 967 ohms`
  * `R_max = 300e-9 / (0.8473 * 100e-12) = 3541 ohms`
  * Trying `R_pull = 2.2 kohm` (`2200 ohms`):
    * Predicted rise time `t_r = 0.8473 * 2200 * 100e-12 = 186 ns` (`<= 300 ns`, valid!)
    * Sink current when LOW `I_low = (3.3 - 0.4) / 2200 = 1.32 mA` (`<= 3 mA`, valid!)
  * Trying `R_pull = 4.7 kohm`: `t_r = 398 ns` (exceeds `300 ns` Fast-mode limit at `100 pF`, though valid for `100 kHz` Standard-mode where `t_r_max = 1000 ns`).
  * *(Note: STM32 internal pull-ups `R_PU` are `~40 kohm`—far above `R_max`, so external pull-up resistors are mandatory for `I2C`!)*

### 3. Wire Framing: `START`, `7-Bit Address + R/W`, `ACK/NACK`, And `STOP`

* **Normal bit rule:** `SDA` changes only while `SCL` is **LOW**, and stays steady while `SCL` is **HIGH**.
* **`START` (`S`):** `SDA` falls `HIGH -> LOW` while `SCL` is **HIGH**.
* **`STOP` (`P`):** `SDA` rises `LOW -> HIGH` while `SCL` is **HIGH**.
* **Every byte = `8` data clocks (`MSB` first) + `9th` `ACK/NACK` clock:** Receiver pulls `SDA` LOW on clock 9 for **`ACK`**, or leaves `SDA` released HIGH for **`NACK`**.
* **Address byte = `(7-bit address << 1) | R/W_bit`:**
  * **Where:** `MSB` = *Most Significant Bit* (bit `7`, transmitted first); `<< 1` shifts the 7-bit device address into bits `[7:1]`; `R/W_bit` (bit `0`) selects transfer direction (`0` = Controller **Writes** to Target, `1` = Controller **Reads** from Target).
  * For target `0x48` (`0b1001000`): **Write byte** = `(0x48 << 1) | 0 = 0x90`; **Read byte** = `(0x48 << 1) | 1 = 0x91`.

#### Wire Trace A — Writing Our 6 Bytes `{0xAA..0xFF}` To Target `0x48`, Register `0x10` (`8 bytes * 9 = 72 SCL clocks`)

```text
START -> 0x90 (0x48+W) [ACK] -> 0x10 (Reg) [ACK]
      -> 0xAA [ACK] -> 0xBB [ACK] -> 0xCC [ACK] -> 0xDD [ACK] -> 0xEE [ACK] -> 0xFF [ACK] -> STOP
```

#### Wire Trace B — Reading Bytes Back From Target `0x48`, Register `0x10` (Repeated `START`)

```text
START -> 0x90 (0x48+W) [ACK] -> 0x10 (Reg) [ACK]
  -> REPEATED START -> 0x91 (0x48+R) [ACK] -> rx[0] [Controller ACK] -> ... -> last rx [Controller NACK] -> STOP
```
*(Notice: when reading, the **controller** deliberately **NACKs** the final byte so the target releases `SDA`, allowing the controller to generate `STOP`!)*

---

## Scene 2: Simple Idea Of `SPI` And Its Resistors

### 1. Four Push-Pull Signals And Two Shift Registers Connected In A Circle

`SPI` (*Serial Peripheral Interface*) uses active **push-pull** drivers (no open-drain bus pull-ups needed for highs) and **shifts bits in both directions at the exact same time** (full-duplex):

```text
   STM32F446 Controller                              External SPI Target
  +--------------------+                            +--------------------+
  |  8-bit Shift Reg   | ------- MOSI (PA7) ------> |  8-bit Shift Reg   |
  |  [MSB ...... LSB]  | <------ MISO (PA6) ------- |  [MSB ...... LSB]  |
  |                    | ------- SCK  (PA5) ------> |  (Clocked by SCK)  |
  |  GPIO PA4 (CS_n)   | ------- CS   (PA4) ------> |  Active-Low Select |
  +--------------------+                            +--------------------+
```

**Where:**
* `MOSI`: *Master Out, Slave In* — serial data wire driven by the Controller into the Target.
* `MISO`: *Master In, Slave Out* — serial data wire driven by the selected Target back to the Controller (released to high-impedance when `CS` is HIGH).
* `SCK`: *Serial Clock* — clock pulses generated by the Controller (`1` pulse per bit = `8` pulses per byte).
* `CS` / `CS_n` (also called `NSS` = *Negative Slave Select* in STM32 manuals): Active-low *Chip Select* (`_n` denotes active-low: `0 V` selects the target; `3.3 V` deselects it).
* `MSB` / `LSB`: *Most Significant Bit* (bit `7`, shifted out first by default when `LSBFIRST = 0`) and *Least Significant Bit* (bit `0`).

* **Every byte is an exchange:** To **send** `1` byte (`0xAA`), `SCK` pulses `8` times and `1` byte simultaneously shifts in from `MISO`. To **read** `1` byte from a sensor, the controller **must still write `1` dummy byte** (`0x00` or `0xFF`) so hardware generates `8` `SCK` pulses!
* **Where Resistors Still Matter on SPI:**
  1. **`10k–47k` pull-up on active-low `CS`:** Keeps the target deselected (`HIGH`) during MCU reset while `PA4` floats as an input.
  2. **Optional `22–47 ohm` series resistor near the driving pin** (`PA5`/`PA7` at MCU, `MISO` at target): Damps fast-edge ringing on longer traces/wires.

### 2. `CPOL` And `CPHA` (`SPI` Modes `0..3`)

Controller and target **must** agree on idle clock level (`CPOL`) and sampling edge (`CPHA`):
* **Where:**
  * `CPOL` (*Clock Polarity*, `SPI1->CR1` bit `1`): Sets the voltage level of `SCK` when idle (`0` = `SCK` rests LOW; `1` = `SCK` rests HIGH).
  * `CPHA` (*Clock Phase*, `SPI1->CR1` bit `0`): Selects which `SCK` edge samples each bit (`0` = sample on the **1st** edge and shift on the 2nd; `1` = shift on the 1st edge and sample on the **2nd**).
  * `BSY` (*Busy Flag*, `SPI1->SR` bit `7`): Hardware status bit that stays `1` while the SPI shift register is actively clocking bits on the wires, and drops to `0` only after the final clock edge completes.

| SPI Mode | `CPOL` (`CR1` bit `1`) | `CPHA` (`CR1` bit `0`) | Idle `SCK` | Data Sampled On | Data Shifted On |
| --- | --- | --- | --- | --- | --- |
| **Mode 0** *(used below)* | `0` | `0` | **LOW** | **1st (Rising) edge** | 2nd (Falling) edge |
| **Mode 1** | `0` | `1` | LOW | 2nd (Falling) edge | 1st (Rising) edge |
| **Mode 2** | `1` | `0` | HIGH | 1st (Falling) edge | 2nd (Rising) edge |
| **Mode 3** | `1` | `1` | **HIGH** | **2nd (Rising) edge** | 1st (Falling) edge |

### 3. Wire Trace — Exchanging Our 6 Bytes `{0xAA..0xFF}` (`6 * 8 = 48 SCK` Clocks)

```text
CS (PA4): HIGH ¯¯\_______________________________________________________________________/¯¯ HIGH
Byte index:          [0]        [1]        [2]        [3]        [4]        [5]
MOSI (Controller):   0xAA       0xBB       0xCC       0xDD       0xEE       0xFF
MISO (Target):       0x11       0x22       0x33       0x44       0x55       0x66
SCK pulses:          8 clk      8 clk      8 clk      8 clk      8 clk      8 clk   (48 clocks total)
```

```text
BEFORE 6-Byte SPI Burst                         AFTER 6-Byte SPI Burst
  tx_6 = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}     tx_6 unchanged
  rx_6 = { ?,    ?,    ?,    ?,    ?,    ?   } -> rx_6 = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66}
  CS = HIGH                                       CS raised HIGH only AFTER 48th SCK & BSY == 0!
```

---

## Scene 3: Simple Idea Of `ADC` (Analog-To-Digital Converter)

Just as `I2C` and `SPI` read **digital** chips over clocked wires, an **`ADC`** measures a **continuous analog voltage** on a pin (`0.000 V` to `3.300 V`, e.g., temperature thermistor, battery divider, or current shunt) and turns it into a **`12-bit` integer (`0 .. 4095`)**.

### 1. Inside `ADC1`: Multiplexer $\rightarrow$ Sample-And-Hold Capacitor $\rightarrow$ `12-Bit` Binary Search (`SAR`)

`STM32F446` `ADC1` (`0x40012000`) has **one** converter shared across **16 external input channels (`IN0..IN15`)** through an internal analog switch (*multiplexer*):

```text
Analog Pins            16:1 Analog     Sample Switch      Tiny Internal      12-Bit SAR        Single 16-Bit
(MODER = 11)           Multiplexer     (Closes SMP cyc)   Cap (C_ADC ~4pF)   Binary Search     Data Register
PA0 (IN0: 1.650 V) --->|         |        /               |                  +-----------+    +---------------+
PA1 (IN1: 0.825 V) --->|   MUX   |------o/  o-------------+----------------->| Compares  |--->|   ADC1->DR    |
PB0 (IN8: 2.475 V) --->|         |                        |                  | vs VREF+  |    | (0x0000..0FFF)|
                       +---------+                       === C_ADC           | (12 clks) |    +---------------+
                                                          |                  +-----------+     ONLY ONE DR!
                                                         VSSA (0 V)          (3.300 V ref)
```

Every single channel conversion happens in **two back-to-back phases** clocked by `ADCCLK` (`PCLK2 / 4 = 21 MHz`, `1 ADCCLK = 47.6 ns`):

1. **Phase A — Sampling Phase (`SMP` cycles, e.g., `84` `ADCCLK` cycles = `4.00 us`):**
   * The MUX selects one pin (say `PA0`), and the internal **Sample Switch closes**, letting your external sensor charge the tiny internal capacitor `C_ADC` up to the pin's exact voltage (`1.650 V`).
   * **Why `SMP` matters:** High-impedance voltage dividers charge `C_ADC` slowly (`R_source * C_ADC`). If the switch opens too early (`3` cycles), `C_ADC` still holds leftover charge from the *previous* channel!
2. **Phase B — `12-Bit` SAR Conversion Phase (Fixed `12` `ADCCLK` cycles = `0.57 us`):**
   * The Sample Switch **opens**, freezing `V_pin` on `C_ADC`.
   * A hardware binary-search comparator tests `V_pin` against fractions of `VREF+` (`3.300 V`) for `12` clock ticks (`Bit 11: >= 1.65 V?`, `Bit 10: >= ...?`, down to `Bit 0`), producing a `12-bit` count (`0..4095`):

```text
ADC_Code = floor( (V_pin / VREF+) * 4096 )   [clamped to 0 .. 4095]
t_conv   = (SMP + 12) / ADCCLK
```

**Where:**
* `MUX` (*Analog Multiplexer*): Internal `16:1` analog switch inside `ADC1` that connects one chosen pin (`IN0..IN15`) at a time to the converter.
* `V_pin`: Analog input voltage present on the selected MCU pin (must stay between `VSSA` and `VREF+`).
* `VSSA`: Analog ground reference voltage (`0.000 V`).
* `VREF+`: Positive analog reference voltage (`3.300 V` on `NUCLEO-F446RE`), representing full-scale input (`4096` ideal steps, so `1 LSB = 3.300 V / 4096 = 0.8057 mV`).
* `4096` (`2^12`): Total quantization levels of a `12-bit` `ADC` (producing integer codes `0` through `4095`, i.e., `0x000` to `0xFFF`).
* `C_ADC`: Internal Sample-and-Hold capacitor inside `ADC1` (`~4 pF` typical in `DS10693`) that stores `V_pin` while the binary search runs.
* `R_source`: Output impedance/resistance of the external sensor circuit driving the pin (higher `R_source` needs a larger `SMP` to charge `C_ADC` fully).
* `ADCCLK`: Internal `ADC` clock frequency (`PCLK2 / ADCPRE` = `84 MHz / 4 = 21 MHz`; `DS10693` requires `ADCCLK <= 36 MHz` at `VDD = 3.3 V`).
* `SMP`: Programmable sampling duration in `ADCCLK` cycles (`3, 15, 28, 56, 84, 112, 144, or 480` cycles set in `ADC1->SMPR1`/`SMPR2`).
* `12`: Fixed number of `ADCCLK` cycles required by the `12-bit` `SAR` comparator (`1` bit resolved per `ADCCLK` cycle).
* `t_conv`: Total single-channel conversion time (`(84 + 12) / 21 MHz = 96 / 21e6 = 4.57 us`).
* `ADC1->DR`: Single `16-bit` *Data Register* holding the finished `12-bit` reading.
* `OVR`: *Overrun Flag* (`ADC1->SR` bit `5`), set by hardware if a new channel finishes and overwrites `ADC1->DR` before software or `DMA` read the previous value.

* **Our 3-Channel Example (`VREF+ = 3.300 V`, Total time per channel = `84 + 12 = 96` cycles = `4.57 us`):**
  * `PA0` (`IN0 = 1.650 V`) $\rightarrow$ `(1.650 / 3.300) * 4096 = 2048` = **`0x0800`**
  * `PA1` (`IN1 = 0.825 V`) $\rightarrow$ `(0.825 / 3.300) * 4096 = 1024` = **`0x0400`**
  * `PB0` (`IN8 = 2.475 V`) $\rightarrow$ `(2.475 / 3.300) * 4096 = 3072` = **`0x0C00`**
  * Stored in `uint16_t adc_3[3]` (`3 x 2 bytes = 6 bytes` in SRAM): **`{ 0x0800, 0x0400, 0x0C00 }`**! *(Note: storing a 12-bit number in `uint16_t` gives 16-bit memory alignment, not 16-bit analog accuracy.)*

### 2. The Single-`DR` Bottleneck In Multi-Channel `SCAN` Mode

Look at the right side of the diagram above: **`ADC1` has NO FIFO—only ONE `16-bit` `ADC1->DR` register!**
* When `ADC1` scans `IN0 -> IN1 -> IN8` (`CR1.SCAN = 1`), `IN0` writes `0x0800` into `ADC1->DR`, and **`4.57 microseconds` later** `IN1` overwrites `ADC1->DR` with `0x0400`!
* If nobody read `0x0800` out of `ADC1->DR` during those `4.57 us`, hardware sets **`OVR = 1` (`ADC1->SR` bit `5`, Overrun)** and `0x0800` is lost! That single hardware fact explains why **`DMA` is the natural partner for multi-channel `ADC` scans**.

---

## Scene 4: Generic MCU `DMA` Architecture And `Stream` vs. `Channel`

### 1. Why `DMA` Works The Same Way On Arm (`STM32`), TriCore (`AURIX`), And `RISC-V`

A **`DMA` (*Direct Memory Access*) controller** is a **separate hardware peripheral** on the microcontroller's internal bus interconnect (`AHB` Bus Matrix on STM32, `SRI`/`SPB` Crossbar on Infineon AURIX, `AXI`/`TileLink` on RISC-V SoCs). Because `DMA` never executes CPU instructions, **it makes zero difference whether the CPU core is Arm Cortex-M, AURIX TriCore, or RISC-V**:

```text
  +----------------+         +---------------------+
  |   CPU Core     |         |   DMA Controller    |   <-- Two independent Bus Masters!
  | (Arm / AURIX / |         | (Pure HW copy state |       CPU runs C code & RTOS tasks;
  |    RISC-V)     |         |  machine, 0 code!)  |       DMA copies items on bus cycles.
  +-------+--------+         +----------+----------+
          |                             |
  ========+=============================+================================== On-Chip Bus Matrix
          |                             |                        |
          v                             v                        v
  +---------------+          +---------------------+    +------------------+
  |     SRAM      |          | SPI / I2C / ADC DR  |    |   Flash / ROM    |
  |  (tx[], rx[]) | <======> | (1-item Data Reg)   |    +------------------+
  +---------------+          +----------+----------+
                                        |
                                        +=== Private HW Trigger Wire ("Copy 1 item now!") ===> DMA
```

* **How `DMA` Works Alongside Any Peripheral:**
  1. Before starting a transfer, the CPU writes **four parameters** into one DMA engine: **Source Address**, **Destination Address**, **Item Width** (`8-bit` byte, `16-bit` halfword, or `32-bit` word), and **Item Count** (e.g., `6` bytes).
  2. Whenever `SPI`, `I2C`, or `ADC` needs `1` outgoing item or finishes `1` incoming item, the peripheral pulses a **private hardware DMA Request wire** directly to the `DMA` controller (**without** interrupting the CPU!).
  3. The `DMA` engine steals `1–2` bus cycles to read the source address, write the destination address, increment the RAM pointer, and decrement its **Remaining Item Counter** (`6 -> 5 -> 4 -> 3 -> 2 -> 1 -> 0`).
  4. Only when the counter hits **`0`** does `DMA` raise its **Transfer Complete flag**!
* **What `DMA` Never Does (On Any MCU):** `DMA` is purely a byte/word mover. It never toggles `SPI` Chip Select (`CS`), never generates `I2C` `START` / `Address` / `STOP`, and never configures `SPI` / `ADC` clocks.

---

### 2. `Stream` vs. `Channel` — Simple Concept And Real-World Collision Scenario

Why does a DMA controller need **both** *Streams* and *Channels*? Because a microcontroller has **dozens of peripheral request wires** (`40–100+` triggers across `SPI`, `I2C`, `UART`, `ADC`, `Timers`), while silicon area only provides a **fixed pool of active copy engines** (e.g., `8` engines per DMA block on STM32F4). Every MCU splits DMA hardware into **two distinct parts**:

1. **The Active Copy Engine (*called a `Stream` on STM32F4; called a `DMA Channel` on AURIX / NXP / STM32H7*):**
   * An actual hardware worker with its **own dedicated registers** (Source pointer, Destination pointer, Item counter, Control register, Status flags, and its own Interrupt line).
   * **Hard rule:** One Copy Engine (`Stream`) can run **only ONE active transfer at a time**. If an MCU has `8` Streams (`Stream 0 .. Stream 7`), at most `8` transfers can run simultaneously on that DMA controller.
2. **The Trigger Input Selector (*called a `Channel` [`CHSEL 0..7`] on STM32F4; called `DMAMUX` or `Service Request Router` on STM32G4/H7, AURIX, and RISC-V*):**
   * An input multiplexer (`8-to-1` switch on STM32F4) sitting in front of **each** Copy Engine (`Stream`) that selects **which peripheral's trigger wire** drives that engine.

```text
Peripheral Trigger Wires            Selector Switch              Active Copy Engine (1 Transfer at a Time)
========================            ===============              =========================================
ADC1_Ready   --(Input Ch 0)-----\   +-------------+              +---------------------------------------+
Timer_Update --(Input Ch 1)------>  | 8:1 MUX     |              |            DMA2_Stream0               |
SPI1_RX_Ready--(Input Ch 3)----->   | (CHSEL bits)|===Trigger===>| Own PAR, Own M0AR, Own NDTR,IRQ 56    |
UART1_RX     --(Input Ch 4)-----/   +-------------+              +---------------------------------------+
                                    ("Channel" on F4;            ("Stream" on STM32F4;
                                     "DMAMUX/SRN" elsewhere)      "DMA Channel" on AURIX / NXP)
```

#### Simple Use-Case Scenario Showing Why `Stream` $\neq$ `Channel`

Suppose on `STM32F446` (`RM0390` Table 29) our `P1` firmware must run **`ADC1`** (continuous 3-sensor analog scan) **at the exact same time** as **`SPI1_RX`** (reading an external sensor):
* Looking at `DMA2_Stream0`'s input selector: **`ADC1` is wired to `Channel 0`**, and **`SPI1_RX` is wired to `Channel 3`** of that **same `Stream 0`**.
* **What goes wrong if you assign both to `Stream 0`?** Even though their *Channel* numbers differ (`Ch 0` vs. `Ch 3`), **there is only ONE `DMA2_Stream0` copy engine** (only one `PAR` peripheral-address register, one `M0AR` memory-address register, and one `NDTR` counter)! Flipping `CHSEL` from `0` to `3` would destroy `ADC1`'s active transfer!
* **How the hardware mapping solves it:** ST also wired `SPI1_RX` (`Channel 3`) to **`DMA2_Stream2`**, and `SPI1_TX` (`Channel 3`) to **`DMA2_Stream3`**!
  * **`DMA2_Stream0` (Copy Engine `0`) + `Channel 0`** $\rightarrow$ dedicated to **`ADC1`**.
  * **`DMA2_Stream2` (Copy Engine `2`) + `Channel 3`** $\rightarrow$ dedicated to **`SPI1_RX`**.
  * **`DMA2_Stream3` (Copy Engine `3`) + `Channel 3`** $\rightarrow$ dedicated to **`SPI1_TX`**.
  * Three separate **Streams** (copy engines) $\rightarrow$ all three run **100% in parallel** with zero conflict!
* *(Bus Matrix Check on `STM32F446`: `DMA1` is wired only to `APB1` peripherals like `I2C1` [`RX = DMA1_Stream0 Ch 1`, `TX = DMA1_Stream6 Ch 1`], whereas **`DMA2` is the only DMA wired to `APB2` peripherals `SPI1` and `ADC1`**!)*

#### Generic MCU Pseudocode To Arm Any DMA Engine

```text
1. DMA_Engine.ENABLE = 0;  wait_until(DMA_Engine.ENABLE == 0); // Registers locked while active
2. DMA_Engine.CLEAR_FLAGS = ALL_COMPLETE_AND_ERROR_FLAGS;      // Old flags block re-arming
3. DMA_Engine.PERIPH_ADDR = &PERIPH.DATA_REG;                  // Fixed peripheral data register
4. DMA_Engine.MEM_ADDR    = &ram_buffer[0];                    // Start of RAM array
5. DMA_Engine.ITEM_COUNT  = N;                                 // Number of items (bytes or halfwords)
6. DMA_Engine.CONFIG      = TRIGGER_SRC | DIR | MEM_INC | ITEM_WIDTH | (IRQ_ON_COMPLETE if IRQ mode);
7. DMA_Engine.ENABLE      = 1;                                 // Engine armed, waiting for peripheral!
```

---

## Scene 5: Step-By-Step Interrupt Paths — From Enabling Peripherals To CPU Entry

Whether you program **STM32 (`Arm NVIC`)**, **AURIX (`Interrupt Router` $\rightarrow$ `TriCore`)**, or **RISC-V (`PLIC`/`CLIC`)**, an interrupt is simply a digital signal that must pass through **four sequential enable gates** from left to right before the CPU jumps to your `ISR`:

```text
  [Gate 1: Peripheral]      [Gate 2: Router/Mux]     [Gate 3: Interrupt Ctrl]    [Gate 4: CPU Core]
  Clock ON + Local IRQ  --> (Only for GPIO Pins: --> Checks Enable + Priority -> Checks Global/Threshold
  Enable bit == 1           SYSCFG+EXTI / EICR)      (Arm NVIC / AURIX IR)       Mask -> Vector -> ISR!
```

### Path 1 — External Pin Interrupt (`Button B1` On `PC13` Or Sensor `DRDY` Pin)

1. **Setup Order (Software Opens the Gates):**
   * **Step 1 (`GPIO` + `SYSCFG` Clocks):** Turn on clocks for `GPIOC` (`RCC->AHB1ENR` bit `2`) and `SYSCFG` (*System Configuration Controller*, `RCC->APB2ENR` bit `14`), and leave `PC13` as digital Input (`GPIOC->MODER` bits `[27:26] = 00`).
   * **Step 2 (`SYSCFG` Pin Multiplexer):** Route pin `13` from **`Port C`** (instead of `PA13` or `PB13`) into external interrupt line `13` (`SYSCFG->EXTICR[3]` bits `[7:4] = 0x2`).
   * **Step 3 (`EXTI` Edge Detector & Local Mask):** Select falling/rising edge in `EXTI->FTSR`/`RTSR`, clear any stale latched edge in `EXTI->PR` (*Pending Register*, Write-`1`-to-Clear), and open the peripheral mask gate (`EXTI->IMR` *Interrupt Mask Register* bit `13 = 1`).
   * **Step 4 (`NVIC` Priority & Enable + CPU Mask):** Set priority (`NVIC_SetPriority(EXTI15_10_IRQn, 5U)`) and enable `IRQ 40` in the Arm `NVIC` (`NVIC_EnableIRQ(EXTI15_10_IRQn)`), with CPU `PRIMASK = 0` (and `BASEPRI` unmasked).
2. **What Happens Step-By-Step The Instant The Pin Voltage Changes:**
   ```text
   [PC13 Pin Edge] -> [GPIOC Input Buffer] -> [SYSCFG Mux selects Port C]
     -> [EXTI13 Edge Detector sets EXTI->PR bit 13 = 1; checks EXTI->IMR bit 13 == 1]
     -> [Asserts IRQ 40 wire into Arm NVIC]
     -> [NVIC sets Pending=1, checks Enable==1 & Priority 5 vs active ISR / CPU BASEPRI]
     -> [NVIC clears its own Pending->0, sets Active->1; CPU pushes {R0..R3,R12,LR,PC,xPSR}]
     -> [CPU reads vector slot 16+40 -> runs EXTI15_10_IRQHandler() -> SW clears EXTI->PR!]
   ```
   *(Remember from [W01-L19](W01_L19_pendsv_context_switch_cmsis_and_freertos_bridge.md): `NVIC` clears its own core `Pending` bit on `ISR` entry, while **peripheral** `EXTI->PR` stays `1` until your `ISR` writes `EXTI->PR = (1UL << 13)`!)*

```c
static void exti_pc13_pin_irq_init_and_route(void)
{
    RCC->AHB1ENR |= (1UL << 2);  /* 1. Enable GPIOC clock so PC13 input buffer works       */
    RCC->APB2ENR |= (1UL << 14); /* 2. Enable SYSCFG clock so port-mux registers work      */
    (void)RCC->APB2ENR;          /*    Dummy read ensures clock gate is open before write   */

    GPIOC->MODER &= ~(3UL << 26);/* 3. PC13 = 00 (Digital Input mode)                      */
    SYSCFG->EXTICR[3] = (SYSCFG->EXTICR[3] & ~(0xFUL << 4))
                      | (2UL << 4); /* 4. Route Port C (code 2) onto EXTI line 13          */

    EXTI->FTSR |= (1UL << 13);   /* 5. Detect falling edge on line 13                      */
    EXTI->PR    = (1UL << 13);   /* 6. W1C (=, never |=): clear any old latched edge first */
    EXTI->IMR  |= (1UL << 13);   /* 7. Open EXTI mask gate so line 13 drives NVIC IRQ 40   */

    NVIC_SetPriority(EXTI15_10_IRQn, 5U); /* 8. Priority 5 (safe for FreeRTOS FromISR)     */
    NVIC_EnableIRQ(EXTI15_10_IRQn);       /* 9. Open NVIC gate for IRQ 40                  */
}
```

---

### Paths 2, 3, And 4 — How `SPI`/`I2C`, `ADC`, And `DMA` Interrupts Route Differently

**Crucial distinction:** `SPI`, `I2C`, `ADC`, and `DMA` **NEVER go through `SYSCFG` or `EXTI`!** Each peripheral has its **own dedicated wire straight into the central Interrupt Controller (`NVIC`)**:

```text
PATH 2 — SPI / I2C Peripheral Interrupt (Used in Mode 2: CPU Interrupt WITHOUT DMA)
  [AF Pins PA5..7 / PB8..9] -> [SPI1 / I2C1 Shift Logic sets Status Flag: RXNE / TXE / SB / ADDR / BTF]
                            -> [ANDed with Peripheral IRQ Gate: SPI1->CR2 RXNEIE / I2C1->CR2 ITEVTEN]
                            -> [NVIC checks Enable + Priority (IRQ 35 / IRQ 31,32)] -> [CPU runs Periph ISR!]

PATH 3 — ADC Peripheral Interrupt (Used in Mode 2: CPU Interrupt WITHOUT DMA)
  [Analog Pin PA0/PA1/PB0 (MODER=11)] -> [ADC1 SAR finishes -> sets ADC1->SR EOC=1 (or OVR=1)]
                                      -> [ANDed with ADC1->CR1 EOCIE==1]
                                      -> [NVIC checks Enable + Priority (IRQ 18)] -> [CPU runs ADC_IRQHandler!]

PATH 4 — DMA Block-Complete Interrupt (Used in Mode 4: WITH DMA -> Offloads Paths 2 & 3!)
  [SPI1 / I2C1 / ADC1] --(Per-item CPU IRQ Gates CLOSED: RXNEIE=0, ITBUFEN=0, EOCIE=0!)
                       ==(DMA Request Gate OPEN: RXDMAEN=1 / DMAEN=1 / ADC_CR2_DMA=1)==>
  [DMA Stream copies items 1..N silently over Bus Matrix]
                       -->(Only when NDTR hits 0: sets LISR/HISR TCIF=1, ANDed with CR TCIE==1)
                       --> [NVIC checks Enable + Priority (DMA Stream IRQ)] -> [CPU runs DMA ISR ONCE!]
```

* **Why Closing Path 2/3 Gates During Path 4 Is Mandatory:** If you turn on `DMA` **and** leave `RXNEIE = 1` or `EOCIE = 1`, **both** the CPU `ISR` and the `DMA` engine race to read `DR`, stealing bytes from each other!

---

# Part II — Polling, Interrupt, DMA Polling & DMA Interrupt (`SPI`, `I2C`, `ADC`)

Before looking at each peripheral, compare the **four universal transfer modes** on any microcontroller:

| Mode | Who Moves Each Byte/Sample? | Who Watches Completion? | `NVIC` IRQs For `N` Items | Key Advantage / When Used In Production |
| --- | --- | --- | --- | --- |
| **1. CPU Polling** | CPU (`while` loop reads/writes `DR`) | CPU spins on peripheral `SR` flag | **`0`** | Simplest; used for `1–2` byte boot init or crash-dump logging where IRQs are off. Wastes `100%` CPU while waiting. |
| **2. CPU Interrupt** | CPU inside peripheral `ISR` | Peripheral `IRQ` fires **every `1` item** | **`N` (`N+` on `I2C`)** | Frees CPU between **slow** items, but at high speed (`10.5 MHz` `SPI` / `21 MHz` `ADC`) `N` context switches burn CPU and risk **`OVR` (Overrun)** if IRQs are masked briefly! |
| **3. DMA Polling** | **`DMA` hardware engine** | CPU polls `DMA` `TCIF` flag (`TCIE = 0`) | **`0`** | Zero `ISR` jitter and **zero `OVR` risk** even with CPU interrupts disabled! Great for bootloaders, Flash/CRC bursts, and deterministic hardware bring-up tests. |
| **4. DMA Interrupt** | **`DMA` hardware engine** | `DMA` `IRQ` fires **ONCE** when `NDTR == 0` (`TCIE = 1`) | **`1`** *(+ framing on `I2C`)* | **Gold-standard RTOS driver mode:** `0%` CPU copy cost, immune to brief critical-section latency, and wakes the waiting FreeRTOS task once when the whole buffer is ready! |

---

## Scene 6: `SPI` (`6` Bytes `{0xAA..0xFF}`) — All 4 Modes & Why `DMA` Wins

### 1. How `DMA` Works Alongside `SPI` & Why `DMA` Offloads CPU So Strongly

* **Full-Duplex Coupling:** In `SPI`, shifting `1` byte out on `MOSI` **always** clocks `1` byte in on `MISO` (`8` `SCK` clocks). Even if you only want to read sensor bytes, `SPI` Master hardware generates clocks **only when a byte is written into `TX`**. Therefore, **production `SPI` `DMA` uses TWO DMA engines together**: one **TX DMA** feeding `tx_6[]` (`{0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}`) into `SPI->DR` whenever `TXE == 1`, and one **RX DMA** draining `SPI->DR` into `rx_6[]` whenever `RXNE == 1`!
* **Exact CPU Offload Advantage:** At `SCK = 10.5 MHz` (`84 MHz / 8`), `1` byte finishes every **`762 ns` = `64` CPU clock cycles**:
  * **Mode 2 (CPU Interrupt):** Entering + running + exiting `SPI1_IRQHandler` takes `~44` cycles out of every `64` cycles (**`~68%` of the CPU burned on ISR overhead**!), and a `2 us` critical section causes `SPI` **`OVR` (*Overrun*)**.
  * **Mode 4 (DMA Interrupt):** `DMA2` copies each byte in `1–2` bus cycles with **`0` CPU interrupts during the burst**, firing **`1` single interrupt on the RX DMA stream** when byte `6` (or byte `512`!) lands in SRAM!
* **Production `CS` Rule (`RM0390` Sec 26.3.11):** Always trigger completion on **RX DMA `TCIF`** (never TX DMA `TCIF`, which fires while the last byte `0xFF` is still shifting out on `MOSI`!), and confirm `TXE == 1` and `BSY == 0` before raising `CS` HIGH.

#### Generic `SPI` Pseudocode Across All 4 Modes

```text
// Mode 1 (CPU Poll):  CS_LOW(); for i in 0..5 { wait(TXE); DR=tx[i]; wait(RXNE); rx[i]=DR; } wait(!BSY); CS_HIGH();
// Mode 2 (CPU IRQ):   CS_LOW(); enable_irq(SPI_RXNE); DR=tx[0];  --> ISR runs 6 times (reads rx[i], loads tx[i+1])!
// Mode 3 (DMA Poll):  arm_DMA_RX(rx,6,irq=0); arm_DMA_TX(tx,6,irq=0); CS_LOW(); enable_SPI_DMA(); wait(DMA_RX.TCIF); wait(!BSY); CS_HIGH();
// Mode 4 (DMA IRQ):   arm_DMA_RX(rx,6,irq=1); arm_DMA_TX(tx,6,irq=0); CS_LOW(); enable_SPI_DMA(); --> CPU runs tasks; 1 RX_DMA ISR raises CS!
```

---

### 2. Simple Production `STM32F446RE` `SPI1` Register Code (`{0xAA..0xFF}`)

*(Assumes `84 MHz` `SYSCLK` / `PCLK2 = 84 MHz` from `W01-L17`. Note: `PA5` is shared with `LD2` on `NUCLEO-F446RE`.)*

```c
#include <stdbool.h>
#include <stdint.h>
#include "stm32f446xx.h"

#define SPI1_CS_LOW()   (GPIOA->BSRR = (1UL << (4U + 16U))) /* BSRR[20]=1: atomically pull PA4 (CS) LOW  */
#define SPI1_CS_HIGH()  (GPIOA->BSRR = (1UL << 4U))         /* BSRR[4]=1:  atomically drive PA4 (CS) HIGH*/

static const uint8_t g_tx6[6] = { 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU };
static uint8_t       g_rx6[6];

/* Step 0 — Enable GPIOA + DMA2 + SPI1 clocks, mux PA5..PA7 to AF5, configure SPI1 Master: */
void spi1_hw_init(void)
{
    RCC->AHB1ENR |= (1UL << 0) | (1UL << 22); /* Bit 0 = GPIOA clock; Bit 22 = DMA2 clock (APB2 needs DMA2!) */
    RCC->APB2ENR |= (1UL << 12);              /* Bit 12 = SPI1 clock on APB2 (84 MHz)                        */
    (void)RCC->APB2ENR;

    SPI1_CS_HIGH();                           /* Preload PA4 HIGH *before* enabling output -> avoids CS glitch */
    GPIOA->MODER  = (GPIOA->MODER  & ~((3UL << 8) | (0x3FUL << 10)))
                  | (1UL << 8) | (0x2AUL << 10); /* PA4 = 01 (Output); PA5/PA6/PA7 = 10 (Alternate Function) */
    GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFFFUL << 20))
                  | (0x555UL << 20);          /* Route PA5(SCK), PA6(MISO), PA7(MOSI) to AF5 (SPI1)          */

    /* SPI1->CR1 (SPE bit 6 kept 0 while configuring):
     *   Bit 9 (SSM=1) + Bit 8 (SSI=1): Software CS management; SSI=1 prevents Mode-Fault (MODF) reset!
     *   Bits [5:3] (BR=010): PCLK2 / 8 = 10.5 MHz SCK; Bit 2 (MSTR=1): Master; CPOL=0, CPHA=0 (Mode 0). */
    SPI1->CR1 = (1UL << 9) | (1UL << 8) | (2UL << 3) | (1UL << 2);
    SPI1->CR2 = 0U;
}

/* MODE 1 — SPI1 CPU Polling (0 IRQs; CPU loops waiting on TXE & RXNE for all 6 bytes): */
void spi1_6bytes_cpu_poll(void)
{
    SPI1->CR1 |= (1UL << 6);                  /* Bit 6 (SPE=1): Enable SPI1 shift hardware                   */
    (void)SPI1->DR; (void)SPI1->SR;           /* Dummy read DR then SR clears any stale RXNE / OVR flag      */

    SPI1_CS_LOW();
    for (uint32_t i = 0U; i < 6U; ++i) {
        while ((SPI1->SR & (1UL << 1)) == 0U) {} /* Wait SR bit 1 (TXE == 1): TX holding register empty      */
        SPI1->DR = g_tx6[i];                     /* Write 0xAA..0xFF -> starts 8 SCK pulses                  */
        while ((SPI1->SR & (1UL << 0)) == 0U) {} /* Wait SR bit 0 (RXNE == 1): 8 bits finished shifting in   */
        g_rx6[i] = (uint8_t)SPI1->DR;            /* Reading DR saves reply byte AND clears RXNE to 0         */
    }
    while ((SPI1->SR & (1UL << 7)) != 0U) {}  /* Wait SR bit 7 (BSY == 0): wire idle before raising CS       */
    SPI1_CS_HIGH();
}

/* MODE 2 — SPI1 CPU Interrupt (Fires SPI1_IRQHandler 6 separate times — once per byte!): */
static volatile uint32_t g_spi_idx;

void spi1_6bytes_cpu_irq_start(void)
{
    g_spi_idx = 0U;
    NVIC_SetPriority(SPI1_IRQn, 5U);          /* Priority 5 allows safe FreeRTOS ...FromISR() calls          */
    NVIC_EnableIRQ(SPI1_IRQn);                /* Open NVIC gate for SPI1 (IRQ 35)                            */

    SPI1->CR1 |= (1UL << 6);                  /* SPE = 1                                                     */
    (void)SPI1->DR; (void)SPI1->SR;
    SPI1_CS_LOW();
    SPI1->CR2 = (1UL << 6) | (1UL << 5);      /* Bit 6 (RXNEIE=1): IRQ on each RX byte; Bit 5 (ERRIE=1)      */
    SPI1->DR  = g_tx6[0];                     /* Seed 1st byte (0xAA); ISR feeds remaining 5 bytes!          */
}

void SPI1_IRQHandler(void)                    /* Enters 6 times for 6 bytes!                                 */
{
    uint32_t sr = SPI1->SR;
    if ((sr & (1UL << 6)) != 0U) {            /* SR bit 6 (OVR == 1): clear by reading DR+SR, abort cleanly  */
        (void)SPI1->DR; (void)SPI1->SR;
        SPI1->CR2 = 0U;
        SPI1_CS_HIGH();
        return;
    }
    if ((sr & (1UL << 0)) != 0U) {            /* RXNE == 1: byte `g_spi_idx` just finished shifting          */
        g_rx6[g_spi_idx++] = (uint8_t)SPI1->DR; /* Read clears RXNE (g_spi_idx advances 0->1..->6)           */
        if (g_spi_idx < 6U) {
            SPI1->DR = g_tx6[g_spi_idx];      /* Load next byte (0xBB .. 0xFF)                               */
        } else {
            SPI1->CR2 = 0U;                   /* Clear RXNEIE & ERRIE so no more SPI1 IRQs fire              */
            while ((SPI1->SR & (1UL << 7)) != 0U) {} /* Confirm BSY == 0                                     */
            SPI1_CS_HIGH();                   /* Notify RTOS task here via vTaskNotifyGiveFromISR()          */
        }
    }
}

/* Shared helper for Modes 3 & 4 — Arms DMA2_Stream2 (RX, Ch 3) + DMA2_Stream3 (TX, Ch 3): */
static void spi1_arm_dma2(bool irq_on_complete)
{
    SPI1->CR1 &= ~(1UL << 6);                 /* SPE = 0 while reconfiguring DMA request wires               */
    SPI1->CR2  = 0U;                          /* Keep CPU RXNEIE/TXEIE = 0 so CPU never races DMA on DR!     */
    (void)SPI1->DR; (void)SPI1->SR;           /* Flush any stale RXNE so RX DMA starts cleanly at byte 0     */

    DMA2_Stream2->CR &= ~1UL;                 /* Clear bit 0 (EN=0) to unlock Stream 2 & Stream 3 registers  */
    DMA2_Stream3->CR &= ~1UL;
    while (((DMA2_Stream2->CR & 1UL) != 0U) || ((DMA2_Stream3->CR & 1UL) != 0U)) {}

    /* LIFCR W1C (=, not |=): 0x3D clears all 5 flags (TCIF,HTIF,TEIF,DMEIF,FEIF) for Stream 2 & Stream 3 */
    DMA2->LIFCR = (0x3DUL << 16) | (0x3DUL << 22);

    DMA2_Stream2->PAR  = (uint32_t)&SPI1->DR; /* RX source: SPI1->DR; M0AR: g_rx6; NDTR: 6 bytes             */
    DMA2_Stream2->M0AR = (uint32_t)g_rx6;
    DMA2_Stream2->NDTR = 6U;
    DMA2_Stream2->CR   = (3UL << 25)          /* Bits [27:25] CHSEL=3: select SPI1_RX trigger wire           */
                       | (1UL << 10)          /* Bit 10 (MINC=1): increment g_rx6 pointer; DIR=00 (P->M)     */
                       | (irq_on_complete ? (1UL << 4) : 0U) /* Bit 4 (TCIE): 0 = Mode 3 Poll, 1 = Mode 4 IRQ*/
                       | 1UL;                 /* Bit 0 (EN=1): RX Stream armed first so byte 0 isn't missed  */

    DMA2_Stream3->PAR  = (uint32_t)&SPI1->DR; /* TX dest: SPI1->DR; M0AR: g_tx6 ({0xAA..0xFF}); NDTR: 6      */
    DMA2_Stream3->M0AR = (uint32_t)g_tx6;
    DMA2_Stream3->NDTR = 6U;
    DMA2_Stream3->CR   = (3UL << 25) | (1UL << 10) | (1UL << 6) | 1UL; /* CHSEL=3 (SPI1_TX), DIR=01 (M->P)   */

    SPI1_CS_LOW();
    SPI1->CR2  = (1UL << 1) | (1UL << 0);     /* Bit 1 (TXDMAEN=1) + Bit 0 (RXDMAEN=1): route triggers->DMA2 */
    SPI1->CR1 |= (1UL << 6);                  /* SPE = 1 -> DMA2 shifts all 6 bytes with 0 per-byte IRQs!    */
}

/* MODE 3 — SPI1 DMA Polling (DMA moves all 6 bytes; CPU polls RX TCIF2 with 0 NVIC interrupts): */
void spi1_6bytes_dma_poll(void)
{
    spi1_arm_dma2(false);                     /* TCIE = 0: no NVIC interrupt                                 */
    while ((DMA2->LISR & (1UL << 21)) == 0U) {} /* Poll LISR bit 21 (TCIF2): 6th RX byte written to g_rx6[5] */
    DMA2->LIFCR = (0x3DUL << 16) | (0x3DUL << 22);
    SPI1->CR2   = 0U;                         /* Disconnect DMA request wires                                */
    while (((SPI1->SR & (1UL << 1)) == 0U) || ((SPI1->SR & (1UL << 7)) != 0U)) {} /* Confirm TXE==1 & BSY==0 */
    SPI1_CS_HIGH();
}

/* MODE 4 — SPI1 DMA Interrupt (CPU runs RTOS tasks freely; DMA2_Stream2_IRQHandler fires ONCE at end!): */
void spi1_6bytes_dma_irq_start(void)
{
    NVIC_SetPriority(DMA2_Stream2_IRQn, 5U);
    NVIC_EnableIRQ(DMA2_Stream2_IRQn);        /* Open NVIC gate for RX Stream 2 (IRQ 58)                     */
    spi1_arm_dma2(true);                      /* TCIE = 1: fire 1 interrupt when NDTR reaches 0              */
}

void DMA2_Stream2_IRQHandler(void)            /* Fires ONLY ONCE after all 6 RX bytes land in g_rx6[]!       */
{
    if ((DMA2->LISR & (1UL << 21)) != 0U) {   /* LISR bit 21 (TCIF2 == 1)                                    */
        DMA2->LIFCR = (0x3DUL << 16) | (0x3DUL << 22); /* W1C clear Stream 2 & Stream 3 flags                */
        SPI1->CR2   = 0U;                     /* Disable TXDMAEN & RXDMAEN                                   */
        while (((SPI1->SR & (1UL << 1)) == 0U) || ((SPI1->SR & (1UL << 7)) != 0U)) {} /* TXE==1 && BSY==0    */
        SPI1_CS_HIGH();                       /* Wake blocked RTOS task via vTaskNotifyGiveFromISR()         */
    }
}
```
*(Production note: in real flight/automotive code, give every short hardware wait loop a bounded iteration counter so a broken peripheral clock fails cleanly instead of hanging.)*

---

## Scene 7: `I2C` (`6` Bytes `{0xAA..0xFF}` To Target `0x48`, Reg `0x10`) — All 4 Modes & How `I2C` DMA Differs

### 1. How `DMA` Works Alongside `I2C` & Where `DMA` Helps

Unlike `SPI` (where every byte is just 8 raw clocks), **`I2C` has bus framing (`START`, `7-bit Address 0x90`, `ACK/NACK`, `STOP`) that `DMA` does NOT generate!**
* **Division of Labor (`I2C` + `DMA`):**
  1. **Software / Event `ISR` starts the frame:** Issues `START`, sends wire address `0x90` (`(0x48 << 1) | 0`), and clears the address-matched flag (`ADDR`).
  2. **`DMA` streams the entire byte payload:** As soon as `ADDR` clears, `TXE` (*Transmit Data Empty*) becomes `1` and pulses the `I2C_TX` DMA trigger wire! `DMA` copies `{0x10, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}` (`7` bytes, or `256` EEPROM/IMU FIFO bytes!) with **zero per-byte CPU interrupts** (per-byte buffer interrupt `ITBUFEN` stays **`0`**!).
  3. **Wire finish (`BTF`) before `STOP`:** When TX `DMA` hits `TCIF`, the last byte `0xFF` was just loaded into `I2C->DR` and still takes `~90 us` (at `100 kHz`) to shift out! Software waits for **`BTF` (*Byte Transfer Finished*, confirming both `DR` and shift register are empty after the `9th` `ACK` clock)**—in Mode 4 via the `I2C` Event `ISR` so we **never busy-wait `90 us` inside a DMA `ISR`**—and then sets `STOP = 1`!
  *(Master RX `DMA` note: when reading `>= 2` bytes via `DMA1_Stream0` Ch `1`, setting `I2C1->CR2` bit `12` [`LAST = 1`] makes hardware automatically send the mandatory **`NACK`** on the final received byte!)*

#### Generic `I2C` Pseudocode Across All 4 Modes

```text
// Mode 1 (CPU Poll):  START(); send_addr(0x90); clear_ADDR(); for b in {0x10,0xAA..0xFF} { wait(TXE); DR=b; } wait(BTF); STOP();
// Mode 2 (CPU IRQ):   enable_irq(EVT|BUF|ERR); START(); --> Event ISR runs on SB -> ADDR -> 7x TXE (clear BUF on last!) -> BTF(STOP)!
// Mode 3 (DMA Poll):  arm_DMA_TX({0x10,0xAA..0xFF},7,irq=0); enable_I2C_DMA(); START(); send_addr(0x90); clear_ADDR(); wait(DMA.TCIF); wait(BTF); STOP();
// Mode 4 (DMA IRQ):   arm_DMA_TX(...,7,irq=1); enable_I2C_DMA(); enable_irq(EVT|ERR); START(); --> EVT handles SB/ADDR, DMA streams bytes, EVT catches BTF->STOP!
```

---

### 2. Simple Production `STM32F446RE` `I2C1` Register Code (`{0xAA..0xFF}`)

*(Assumes `PCLK1 = 42 MHz`, `PB8 = SCL` & `PB9 = SDA` with external `2.2 kohm` pull-ups to `3.3 V`.)*

```c
static uint8_t g_i2c_buf[7] = { 0x10U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU }; /* Reg 0x10 + 6 bytes */

/* Step 0 — Enable GPIOB + DMA1 + I2C1 clocks, PB8/PB9 Open-Drain AF4, 100 kHz Standard-mode: */
void i2c1_hw_init(void)
{
    RCC->AHB1ENR |= (1UL << 1) | (1UL << 21); /* Bit 1 = GPIOB clock; Bit 21 = DMA1 clock (APB1 uses DMA1)   */
    RCC->APB1ENR |= (1UL << 21);              /* Bit 21 = I2C1 clock on APB1 (42 MHz)                        */
    (void)RCC->APB1ENR;

    GPIOB->OTYPER |= (1UL << 8) | (1UL << 9); /* Bits 8,9 = 1: Open-Drain (mandatory so ACK never shorts!)   */
    GPIOB->PUPDR  &= ~((3UL << 16) | (3UL << 18)); /* 00: No internal pull; external 2.2k resistors pull HIGH */
    GPIOB->AFR[1]  = (GPIOB->AFR[1] & ~0xFFUL) | 0x44UL; /* Route PB8(SCL) & PB9(SDA) to AF4 (I2C1)          */
    GPIOB->MODER   = (GPIOB->MODER  & ~((3UL << 16) | (3UL << 18))) | (2UL << 16) | (2UL << 18); /* 10 = AF */

    I2C1->CR1   = (1UL << 15);                /* Bit 15 (SWRST=1): reset I2C1 internal state machine         */
    I2C1->CR1   = 0U;                         /* Release SWRST=0 while configuring timing                    */
    I2C1->CR2   = 42U;                        /* Bits [5:0] FREQ = 42 (tell I2C1 that PCLK1 is 42 MHz)       */
    I2C1->CCR   = 210U;                       /* 100 kHz SCL: CCR = 42 MHz / (2 * 100 kHz) = 210             */
    I2C1->TRISE = 43U;                        /* Max 1000 ns rise: (1000 ns * 42 MHz) + 1 = 43               */
    I2C1->CR1   = (1UL << 0);                 /* Bit 0 (PE=1): Enable I2C1 peripheral                        */
}

/* MODE 1 — I2C1 CPU Polling (CPU steps through START -> 0x90 -> ADDR -> 7 bytes -> BTF -> STOP): */
void i2c1_6bytes_cpu_poll(void)
{
    while ((I2C1->SR2 & (1UL << 1)) != 0U) {} /* Wait SR2 bit 1 (BUSY == 0): bus idle                        */
    I2C1->CR1 |= (1UL << 8);                  /* Bit 8 (START=1): generate START condition                   */
    while ((I2C1->SR1 & (1UL << 0)) == 0U) {} /* Wait SR1 bit 0 (SB == 1): START sent                        */
    I2C1->DR = (0x48U << 1);                  /* Write 0x90 (0x48 + Write); reading SR1 + writing DR clears SB*/

    while ((I2C1->SR1 & (1UL << 1)) == 0U) {} /* Wait SR1 bit 1 (ADDR == 1): target ACKed 0x90               */
    (void)I2C1->SR1; (void)I2C1->SR2;         /* Reading SR1 followed by SR2 is required to clear ADDR!      */

    for (uint32_t i = 0U; i < 7U; ++i) {
        while ((I2C1->SR1 & (1UL << 7)) == 0U) {} /* Wait SR1 bit 7 (TXE == 1): DR empty                     */
        I2C1->DR = g_i2c_buf[i];              /* Send reg 0x10, then 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF      */
    }
    while ((I2C1->SR1 & (1UL << 2)) == 0U) {} /* Wait SR1 bit 2 (BTF == 1): 0xFF shifted out & ACKed!        */
    I2C1->CR1 |= (1UL << 9);                  /* Bit 9 (STOP=1): release I2C bus                             */
}

/* MODE 2 — I2C1 CPU Interrupt (Fires I2C1_EV_IRQHandler 9 times: SB + ADDR + 7 TXE + BTF!): */
static volatile uint32_t g_i2c_idx;

void i2c1_6bytes_cpu_irq_start(void)
{
    g_i2c_idx = 0U;
    NVIC_SetPriority(I2C1_EV_IRQn, 5U);
    NVIC_EnableIRQ(I2C1_EV_IRQn);             /* Open NVIC gate for I2C1 Event IRQ (IRQ 31)                  */
    /* CR2: keep FREQ=42 (0x3F), set Bit 10 (ITBUFEN: TXE IRQ) | Bit 9 (ITEVTEN: SB/ADDR/BTF) | Bit 8 (ERR) */
    I2C1->CR2  = (I2C1->CR2 & 0x3FUL) | (1UL << 10) | (1UL << 9) | (1UL << 8);
    I2C1->CR1 |= (1UL << 8);                  /* START = 1                                                   */
}

void I2C1_EV_IRQHandler(void)
{
    uint32_t sr1 = I2C1->SR1;
    if ((sr1 & (1UL << 0)) != 0U) {           /* SB == 1 -> send address byte 0x90                           */
        I2C1->DR = (0x48U << 1);
    } else if ((sr1 & (1UL << 1)) != 0U) {    /* ADDR == 1 -> read SR2 to clear ADDR                         */
        (void)I2C1->SR2;
    } else if (((sr1 & (1UL << 7)) != 0U) && ((I2C1->CR2 & (1UL << 10)) != 0U)) { /* TXE == 1                */
        I2C1->DR = g_i2c_buf[g_i2c_idx++];
        if (g_i2c_idx == 7U) {
            I2C1->CR2 &= ~(1UL << 10);        /* Last byte (0xFF) loaded: clear ITBUFEN so TXE stops firing! */
        }
    } else if (((sr1 & (1UL << 2)) != 0U) && (g_i2c_idx == 7U)) { /* BTF == 1 after 0xFF ACKed               */
        I2C1->CR2 &= ~((1UL << 9) | (1UL << 8)); /* Disable ITEVTEN & ITERREN                                */
        I2C1->CR1 |= (1UL << 9);              /* STOP = 1 (and notify RTOS task)                             */
    }
}

/* Shared helper for Modes 3 & 4 — Arms DMA1_Stream6, Channel 1 (I2C1_TX): */
static void i2c1_arm_dma1_tx(bool irq_on_complete)
{
    DMA1_Stream6->CR &= ~1UL;                 /* EN = 0: unlock Stream 6 registers                           */
    while ((DMA1_Stream6->CR & 1UL) != 0U) {}
    DMA1->HIFCR = (0x3DUL << 16);             /* Stream 6 flags live in HISR/HIFCR bits [21:16] (W1C)        */

    DMA1_Stream6->PAR  = (uint32_t)&I2C1->DR;
    DMA1_Stream6->M0AR = (uint32_t)g_i2c_buf; /* {0x10, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}                  */
    DMA1_Stream6->NDTR = 7U;
    DMA1_Stream6->CR   = (1UL << 25)          /* CHSEL = 1 (I2C1_TX trigger wire)                            */
                       | (1UL << 10)          /* MINC = 1 (advance RAM pointer)                              */
                       | (1UL << 6)           /* DIR = 01 (Memory -> Peripheral)                             */
                       | (irq_on_complete ? (1UL << 4) : 0U) /* TCIE = 0 (Poll) or 1 (IRQ)                   */
                       | 1UL;                 /* EN = 1                                                      */
}

/* MODE 3 — I2C1 DMA Polling (CPU sends START+0x90; DMA1 sends all 7 bytes; CPU polls TCIF6 + BTF): */
void i2c1_6bytes_dma_poll(void)
{
    i2c1_arm_dma1_tx(false);
    I2C1->CR2  = (I2C1->CR2 & 0x3FUL) | (1UL << 11); /* Bit 11 (DMAEN=1): TXE triggers DMA1_Stream6!         */
    I2C1->CR1 |= (1UL << 8);                         /* START = 1                                            */
    while ((I2C1->SR1 & (1UL << 0)) == 0U) {}
    I2C1->DR = (0x48U << 1);
    while ((I2C1->SR1 & (1UL << 1)) == 0U) {}
    (void)I2C1->SR1; (void)I2C1->SR2;         /* Clearing ADDR unleashes DMA1_Stream6 for all 7 bytes!       */

    while ((DMA1->HISR & (1UL << 21)) == 0U) {} /* Poll HISR bit 21 (TCIF6): 0xFF written to I2C1->DR        */
    DMA1->HIFCR = (0x3DUL << 16);
    I2C1->CR2  &= ~(1UL << 11);               /* Clear DMAEN                                                 */
    while ((I2C1->SR1 & (1UL << 2)) == 0U) {} /* Wait BTF == 1 so 0xFF finishes shifting before STOP         */
    I2C1->CR1  |= (1UL << 9);                 /* STOP = 1                                                    */
}

/* MODE 4 — I2C1 DMA Interrupt (0 per-byte IRQs; DMA ISR re-enables ITEVTEN to catch BTF without spinning!): */
static volatile bool g_i2c_dma_done;

void i2c1_6bytes_dma_irq_start(void)
{
    g_i2c_dma_done = false;
    NVIC_SetPriority(DMA1_Stream6_IRQn, 5U);
    NVIC_SetPriority(I2C1_EV_IRQn, 5U);
    NVIC_EnableIRQ(DMA1_Stream6_IRQn);        /* IRQ 17 (I2C1_TX DMA)                                        */
    NVIC_EnableIRQ(I2C1_EV_IRQn);             /* IRQ 31 (Framing: SB, ADDR, and final BTF only!)             */

    i2c1_arm_dma1_tx(true);
    I2C1->CR2  = (I2C1->CR2 & 0x3FUL) | (1UL << 11) | (1UL << 9); /* DMAEN=1 | ITEVTEN=1 (ITBUFEN stays 0!)  */
    I2C1->CR1 |= (1UL << 8);                  /* START = 1                                                   */
}

void DMA1_Stream6_IRQHandler(void)            /* Fires ONCE when 7th byte (0xFF) enters I2C1->DR             */
{
    DMA1->HIFCR    = (0x3DUL << 16);          /* W1C clear Stream 6 flags                                    */
    I2C1->CR2     &= ~(1UL << 11);            /* Disable DMAEN                                               */
    g_i2c_dma_done = true;
    I2C1->CR2     |= (1UL << 9);              /* Re-enable ITEVTEN so BTF (~90 us later) issues STOP cleanly!*/
}

void I2C1_EV_DMA_Handler(void)                /* Runs ONLY for SB, ADDR, and final BTF (0 byte-copy IRQs!)   */
{
    uint32_t sr1 = I2C1->SR1;
    if ((sr1 & (1UL << 0)) != 0U) {
        I2C1->DR = (0x48U << 1);
    } else if ((sr1 & (1UL << 1)) != 0U) {
        (void)I2C1->SR2;
        I2C1->CR2 &= ~(1UL << 9);             /* Mute ITEVTEN while DMA1_Stream6 transfers payload silently  */
    } else if (((sr1 & (1UL << 2)) != 0U) && g_i2c_dma_done) {
        I2C1->CR2 &= ~(1UL << 9);
        I2C1->CR1 |= (1UL << 9);              /* BTF == 1 -> issue STOP = 1 & wake RTOS task!                */
    }
}
```
*(Note: if `I2C1->SR1` bit `10` [`AF` = Acknowledge Failure / `NACK`] ever sets, clear it with Write-`0`-to-Clear `I2C1->SR1 = ~(1UL << 10)` and issue `STOP`.)*

---

## Scene 8: `ADC` (`3` Channels = `6` Bytes `{0x0800, 0x0400, 0x0C00}`) — All 4 Modes & Why `ADC` Needs `DMA`

### 1. How `DMA` Works Alongside `ADC` & Why Multi-Channel Scan Needs `DMA`

On `STM32F446` (and many SAR `ADC` blocks across MCUs), **all `16` analog channels share ONE `16-bit` result register (`ADC1->DR`) with zero FIFO**:
* At `ADCCLK = 21 MHz` (`84 + 12 = 96` cycles), each channel finishes in **`4.57 microseconds`**.
* **Without `DMA`:**
  * **Mode 1 (CPU Polling):** Cannot safely use multi-channel `SCAN` without risking overrun, so software triggers `1` channel at a time (`IN0`, then `IN1`, then `IN8`), spinning the CPU for `3 * 4.57 = 13.7 us`.
  * **Mode 2 (CPU Interrupt):** Must set `EOCS = 1` (*End-of-Conversion Selection* = per-channel flag) so `ADC_IRQHandler` fires **every `4.57 us` (`3` ISRs per scan)**—any `> 4.57 us` critical section causes **`OVR` (*Overrun*)** and loses sensor data!
* **With `DMA` (Modes 3 & 4):** Setting `SCAN = 1` + `DMA = 1` + `DDS = 1` (*DMA Disable Selection* = keep DMA active after `NDTR == 0`) lets `DMA2_Stream0` copy each `16-bit` halfword (`PSIZE = 16-bit, MSIZE = 16-bit`, `NDTR = 3` items = `6` bytes) into `g_adc3[0..2]` within nanoseconds of each conversion—even while CPU interrupts are masked!—and fires **only `1` interrupt after all `3` channels finish**!

#### Generic `ADC` Pseudocode Across All 4 Modes

```text
// Mode 1 (CPU Poll):  for ch in {0,1,8} { select_ch(ch); start_adc(); wait(EOC); out[i] = ADC.DR; }
// Mode 2 (CPU IRQ):   set_scan({0,1,8}); enable_irq(EOC_EACH_CH); start_adc(); --> ISR runs 3 times (4.57 us apart)!
// Mode 3 (DMA Poll):  arm_DMA_16bit(out,3,irq=0); set_scan_dma({0,1,8}); start_adc(); wait(DMA.TCIF);
// Mode 4 (DMA IRQ):   arm_DMA_16bit(out,3,irq=1); set_scan_dma({0,1,8}); start_adc(); --> CPU free; 1 DMA ISR at end!
```

---

### 2. Simple Production `STM32F446RE` `ADC1` Register Code (`PA0=IN0`, `PA1=IN1`, `PB0=IN8`)

```c
static uint16_t      g_adc3[3];               /* Stores {0x0800, 0x0400, 0x0C00} = 3 halfwords (6 bytes)     */
static const uint8_t g_chs[3] = { 0U, 1U, 8U };

/* Step 0 — Analog pins (MODER=11), ADCCLK = 84 MHz / 4 = 21 MHz (<= 36 MHz max), 84-cycle sample time: */
void adc1_hw_init(void)
{
    RCC->AHB1ENR |= (1UL << 0) | (1UL << 1) | (1UL << 22); /* GPIOA + GPIOB + DMA2 clocks                   */
    RCC->APB2ENR |= (1UL << 8);                            /* Bit 8 = ADC1 clock on APB2 (84 MHz)           */
    (void)RCC->APB2ENR;

    GPIOA->MODER |= 0xFUL;                    /* PA0, PA1 = 11 (Analog mode: disconnects Schmitt trigger!)   */
    GPIOB->MODER |= 0x3UL;                    /* PB0      = 11 (Analog mode)                                 */
    ADC->CCR = (ADC->CCR & ~(3UL << 16)) | (1UL << 16); /* Bits [17:16] ADCPRE=01: PCLK2/4 = 21 MHz ADCCLK   */
    ADC1->SMPR2 = (4UL << 24) | (4UL << 3) | (4UL << 0);/* Code 4 = 84 sample cycles for IN8, IN1, and IN0   */
    ADC1->CR2   = (1UL << 0);                 /* Bit 0 (ADON=1): power up ADC1 (~3 us stabilization follow)  */
    for (volatile uint32_t d = 0U; d < 300U; ++d) {}
}

/* MODE 1 — ADC1 CPU Polling (Converts 1 channel per trigger so single DR never overruns): */
void adc1_3ch_cpu_poll(void)
{
    ADC1->CR1  = 0U;                          /* SCAN = 0, EOCIE = 0                                         */
    ADC1->CR2  = (1UL << 10) | (1UL << 0);    /* Bit 10 (EOCS=1): set EOC after each single conversion; ADON */
    ADC1->SQR1 = 0U;                          /* Bits [23:20] L = 0 -> (0 + 1) = 1 channel per SWSTART       */

    for (uint32_t i = 0U; i < 3U; ++i) {
        ADC1->SQR3 = g_chs[i];                /* Select channel 0, then 1, then 8 in Rank 1                  */
        ADC1->CR2 |= (1UL << 30);             /* Bit 30 (SWSTART=1): start conversion                        */
        while ((ADC1->SR & (1UL << 1)) == 0U) {} /* Wait SR bit 1 (EOC == 1)                                 */
        g_adc3[i] = (uint16_t)ADC1->DR;       /* Reading DR saves 12-bit code AND clears EOC                 */
    }
}

/* MODE 2 — ADC1 CPU Interrupt (SCAN=1 + EOCS=1 -> fires ADC_IRQHandler 3 times, 4.57 us apart!): */
static volatile uint32_t g_adc_idx;

void adc1_3ch_cpu_irq_start(void)
{
    g_adc_idx  = 0U;
    ADC1->SQR1 = (2UL << 20);                 /* L = 2 -> (2 + 1) = 3 channels in scan                       */
    ADC1->SQR3 = (8UL << 10) | (1UL << 5) | 0UL; /* SQ1=IN0, SQ2=IN1, SQ3=IN8 (5 bits per rank)              */
    ADC1->SR   = 0U;                          /* ADC1->SR is rc_w0: writing 0 clears all stale flags         */
    ADC1->CR1  = (1UL << 8) | (1UL << 5);     /* Bit 8 (SCAN=1); Bit 5 (EOCIE=1: enable EOC interrupt)       */
    ADC1->CR2  = (1UL << 10) | (1UL << 0);    /* Bit 10 (EOCS=1): MUST fire IRQ after *each* channel!        */

    NVIC_SetPriority(ADC_IRQn, 5U);
    NVIC_EnableIRQ(ADC_IRQn);                 /* Open NVIC gate for shared ADC IRQ 18                        */
    ADC1->CR2 |= (1UL << 30);                 /* SWSTART = 1                                                 */
}

void ADC_IRQHandler(void)                     /* Fires 3 times per scan!                                     */
{
    if ((ADC1->SR & (1UL << 1)) != 0U) {      /* EOC == 1                                                    */
        g_adc3[g_adc_idx++] = (uint16_t)ADC1->DR;
        if (g_adc_idx == 3U) {
            ADC1->CR1 &= ~(1UL << 5);         /* Disable EOCIE after 3rd channel (or notify RTOS task)       */
        }
    }
}

/* Shared helper for Modes 3 & 4 — Arms DMA2_Stream0, Channel 0 (ADC1, 16-bit halfwords): */
static void adc1_arm_dma2(bool irq_on_complete)
{
    DMA2_Stream0->CR &= ~1UL;                 /* EN = 0: unlock Stream 0 registers                           */
    while ((DMA2_Stream0->CR & 1UL) != 0U) {}
    DMA2->LIFCR = 0x3DUL;                     /* W1C clear all 5 Stream 0 flags in bits [5:0]                */

    DMA2_Stream0->PAR  = (uint32_t)&ADC1->DR;
    DMA2_Stream0->M0AR = (uint32_t)g_adc3;
    DMA2_Stream0->NDTR = 3U;                  /* 3 halfword items = 6 bytes total!                           */
    DMA2_Stream0->CR   = (0UL << 25)          /* CHSEL = 0 (ADC1 trigger wire)                               */
                       | (1UL << 13) | (1UL << 11) /* MSIZE=01 & PSIZE=01: 16-bit halfword transfers!        */
                       | (1UL << 10)          /* MINC = 1: advance g_adc3[0..2]; DIR = 00 (Periph -> Mem)    */
                       | (irq_on_complete ? (1UL << 4) : 0U) /* TCIE = 0 (Poll) or 1 (IRQ)                   */
                       | 1UL;                 /* EN = 1                                                      */

    ADC1->SQR1 = (2UL << 20);                 /* 3 conversions: IN0 -> IN1 -> IN8                            */
    ADC1->SQR3 = (8UL << 10) | (1UL << 5) | 0UL;
    ADC1->SR   = 0U;                          /* rc_w0 clear any prior OVR (if OVR=1, ADC stops DMA!)        */
    ADC1->CR1  = (1UL << 8);                  /* Bit 8 (SCAN=1); keep EOCIE = 0 so CPU never races DMA!      */
    ADC1->CR2  = (1UL << 9) | (1UL << 8) | (1UL << 0); /* Bit 9 (DDS=1) + Bit 8 (DMA=1) + Bit 0 (ADON=1)     */
}

/* MODE 3 — ADC1 DMA Polling (DMA copies all 3 channels into g_adc3[]; CPU polls TCIF0, 0 IRQs): */
void adc1_3ch_dma_poll(void)
{
    adc1_arm_dma2(false);
    ADC1->CR2 |= (1UL << 30);                 /* SWSTART = 1                                                 */
    while ((DMA2->LISR & (1UL << 5)) == 0U) {}/* Poll LISR bit 5 (TCIF0): all 3 channels stored in g_adc3[]  */
    DMA2->LIFCR = 0x3DUL;
}

/* MODE 4 — ADC1 DMA Interrupt (DMA2_Stream0_IRQHandler fires ONCE when all 3 channels are in SRAM!): */
void adc1_3ch_dma_irq_start(void)
{
    NVIC_SetPriority(DMA2_Stream0_IRQn, 5U);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);        /* Open NVIC gate for DMA2_Stream0 (IRQ 56)                    */
    adc1_arm_dma2(true);                      /* TCIE = 1                                                    */
    ADC1->CR2 |= (1UL << 30);                 /* SWSTART = 1                                                 */
}

void DMA2_Stream0_IRQHandler(void)            /* Fires ONCE per complete 3-channel scan!                     */
{
    if ((DMA2->LISR & (1UL << 5)) != 0U) {    /* TCIF0 == 1: {0x0800, 0x0400, 0x0C00} ready in g_adc3[]!     */
        DMA2->LIFCR = 0x3DUL;                 /* W1C clear Stream 0 flags; wake RTOS task here               */
    }
}
```
*(Reminder: `ADC1->SR` flags like `OVR` [bit `5`] are **Write-`0`-to-Clear [`rc_w0`]** — write `ADC1->SR = ~(1UL << 5);` to clear `OVR`, whereas `DMA->LIFCR` is **Write-`1`-to-Clear [`W1C`]**!)*

---

## Scene 9: Production `DMA` Ownership, Outside-In Debugging & Interview Q&As

### 1. Three Production Rules Every Firmware Architect Enforces With `DMA`

```text
FREE -> CPU_PREPARES -> DMA_ACTIVE (EN == 1) -> TCIF + WIRE_IDLE -> TASK_READS -> FREE
                              |
                              +--(RTOS Timeout / Error)--> QUIESCE_DMA_FIRST -> FREE
```

1. **Buffer Lifetime:** Never pass a local stack array to `DMA` if the function can return while `DMA` is active—use static/global or `C03` pool buffers in DMA-reachable RAM. *(On MCUs with CPU data cache such as `STM32F767` Cortex-M7 or AURIX cached `DSPR`/`LMU`, use non-cacheable `MPU` regions or clean/invalidate cache lines—`volatile` never flushes CPU cache!)*
2. **A Task Timeout Does NOT Stop `DMA` Hardware (`Quiescence`):** If `ulTaskNotifyTake()` times out, `DMA` is **still armed (`EN == 1`)**! Before reusing or freeing the buffer, disable the peripheral's DMA request bit (`RXDMAEN`/`DMAEN`/`ADC_CR2_DMA`), clear `Stream->CR &= ~1UL`, **wait until `(Stream->CR & 1UL) == 0U`**, and clear `LIFCR`/`HIFCR`.
3. **Outside-In Hardware Debug Order:** Check **wires/pull-ups/pinmux first** $\rightarrow$ **peripheral `SR` flags (`AF`, `MODF`, `OVR`)** $\rightarrow$ **`DMA` `NDTR` & `LISR`/`HISR`** $\rightarrow$ **`NVIC` enable & `BASEPRI`**. *(Never live-watch `SPI1->DR`, `ADC1->DR`, or `I2C1->SR1/SR2` in a running debugger—SWD reads consume bytes and clear status flags!)*

---

## Feynman Recap

`DMA` is a CPU-independent hardware copy engine on the MCU bus matrix: a **Copy Engine (`Stream` on STM32F4 / `DMA Channel` on AURIX)** owns one active transfer's address/count registers, while a **Selector Switch (`Channel` `CHSEL` / `DMAMUX` / `SRN`)** picks which peripheral trigger wire drives it. Interrupts travel through four sequential gates (**Peripheral Enable $\rightarrow$ Optional Pin Mux/`EXTI` $\rightarrow$ Central Interrupt Controller [`NVIC`] $\rightarrow$ CPU Mask**), with `Pin`, `SPI`/`I2C`, `ADC`, and `DMA` using **four separate interrupt lines**. Across `SPI`, `I2C`, and `ADC`, **CPU Polling** spins the CPU, **CPU Interrupt** interrupts the CPU on *every single item* (risking `OVR` at high rates), **DMA Polling** moves items in hardware with `0` IRQs, and **DMA Interrupt** lets RTOS tasks run freely and interrupts the CPU **once when the whole block is in RAM**—while software still owns `SPI` `CS`/`BSY`, `I2C` `START`/`ADDR`/`BTF`/`STOP`, and timeout quiescence.

---

## Spoken-English Interview Answers

### 1. What is the difference between a DMA Stream and a DMA Channel?
* **Simple answer:** A **Stream** (called a *DMA Channel* on AURIX/NXP) is the physical copy engine with its own address/count registers that runs **one** active transfer at a time, while a **Channel** (`CHSEL` on STM32F4, or `DMAMUX`/`SRN`) is the multiplexer input selecting **which peripheral's trigger wire** feeds that engine.
* **Example:** On `STM32F446`, `ADC1` (`Ch 0`) and `SPI1_RX` (`Ch 3`) both connect to `DMA2_Stream0`, so to run both concurrently we put `ADC1` on `DMA2_Stream0` (`Ch 0`) and move `SPI1_RX` to `DMA2_Stream2` (`Ch 3`).

### 2. Trace the hardware interrupt path for a GPIO pin versus a peripheral with and without DMA.
* **Simple answer:** A GPIO pin goes `Pin -> GPIO -> Port Mux (SYSCFG) -> Edge Latch (EXTI) -> NVIC -> CPU`, whereas `SPI`/`I2C`/`ADC` bypass `EXTI` and wire directly into `NVIC` (without DMA) or trigger `DMA` over a private request wire so **only `DMA`** interrupts `NVIC` once at `NDTR == 0` (with DMA).
* **Rule:** When `DMA` owns `DR`, disable per-item peripheral CPU interrupts (`RXNEIE = 0`, `ITBUFEN = 0`, `EOCIE = 0`) so CPU and `DMA` never race on `DR`.

### 3. Compare DMA's advantage across `SPI`, `I2C`, and multi-channel `ADC`.
* **`SPI`:** Full-duplex requires TX + RX service every `8` `SCK` clocks (`762 ns` at `10.5 MHz`); TX+RX `DMA` eliminates `~68%` per-byte `ISR` overhead and prevents `OVR`, raising `CS` only after RX `TCIF` + `TXE == 1 && BSY == 0`.
* **`I2C`:** `DMA` cannot send `START`, `Address`, or `STOP`, so software/event-IRQ frames the transfer while `DMA` offloads all payload bytes, waiting for `BTF == 1` before `STOP`.
* **`ADC`:** Single `ADC1->DR` has no FIFO and is overwritten every `4.57 us` in `SCAN` mode; `DMA` (`16-bit`) drains each channel immediately with zero `OVR` risk and wakes the CPU once per full scan.

---

## Document Lookup And Source Provenance

| Question | Authority / location | Search term |
| --- | --- | --- |
| I2C framing, ACK/NACK, pull-up formulas | [NXP UM10204 Rev 7.0](https://www.nxp.com/docs/en/user-guide/UM10204.pdf), sections 3.1.1-3.1.16, Table 10 | `SDA and SCL signals` |
| `STM32F446` pin `AF4`/`AF5`, `ADC12_IN0/1/8`, `f_ADC <= 36 MHz` | [DS10693 Rev 10](../../docs/stm32/um-f446re/stm32f446mc.pdf), Tables 10/11 & ADC characteristics | `SPI1_MISO`, `ADC12_IN8`, `fADC` |
| `DMA1`/`DMA2` bus matrix, Streams vs. Channels, `LISR`/`HISR` | [RM0390 Rev 6](../../docs/stm32/um-f446re/rm0390-stm32f446xx-advanced-armbased-32bit-mcus-stmicroelectronics.pdf), Fig 27, Tables 28/29, Sec 9.5 | `Table 28`, `Table 29`, `DMA_LISR` |
| `SYSCFG`, `EXTI`, and `NVIC` vector table (`Table 38`) | Same RM, Sec 8.2 (`SYSCFG_EXTICR`), Sec 10.2-10.3 (`EXTI`), **Table 38** | `EXTICR`, `EXTI_IMR`, `Table 38` |
| `ADC1` `SCAN`, `EOCS`, `OVR` (`rc_w0`), `DMA`, `DDS` | Same RM, Sec 13.3.6, 13.8, 13.13 | `ADC_SR`, `DDS`, `EOCS` |
| `I2C1` `CCR`/`TRISE`, `SB`/`ADDR`/`BTF`/`AF` (`rc_w0`), `DMAEN`, `LAST` | Same RM, Sec 24.3.3, 24.3.8, 24.6 | `I2C_CCR`, `ITBUFEN`, `LAST` |
| `SPI1` `SSM`/`SSI` (`MODF`), `TXE`/`RXNE`/`BSY`, `SPI` DMA sequence | Same RM, Sec 26.3.3-26.3.11, 26.7 | `Communication using DMA`, `SSI` |

## Corrections

- Initial publication, 2026-11-17 (revisions 2–4 on 2026-11-17 added direct CMSIS registers, `ADC`/`DMA` foundations, `6`-byte traces, and symbol definitions).
- 2026-11-18 (revision 5): Preserved Scenes 1–3 intact and rewrote Scene 4 onward per learner feedback: explained generic cross-MCU `DMA` (`STM32` / `AURIX` / `RISC-V`) and `Stream` vs. `Channel` with the `ADC1` + `SPI1_RX` collision scenario, traced the 4 step-by-step interrupt paths (`Pin/SYSCFG/EXTI`, `SPI/I2C`, `ADC`, `DMA` $\rightarrow$ `NVIC` $\rightarrow$ `CPU`), removed redundant clock/boilerplate code, folded symbol meanings inline + into C bit comments, and paired generic pseudocode with concise production-style `STM32F446RE` register code across all 4 modes for `SPI1`, `I2C1`, and `ADC1`.

