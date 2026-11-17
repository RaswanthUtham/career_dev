# Board-Backed Projects And Failure Fallbacks

Research and recommendations, 2026-10-25. Based on the
[photo inventory](board_inventory.md); **no functional board tests yet**.
Keep the [two project contracts](../plans/project_portfolio.md) and
[21-week roadmap](../plans/five_month_development_roadmap.md).
This document allocates hardware and contingencies, not extra weeks or a third
large project. Hardware-dependent choices remain conditional on bring-up.

## 1. Recommended Allocation

| Product | Primary hardware | Why it fits | What remains optional |
| --- | --- | --- | --- |
| A: fault/event recorder | BBB + F446RE | Preserves the current Linux IIO/client-driver and timer/ADC/DMA plan with built-in MCU debugging | External I2C sensor, rich GUI, extra protocols |
| B: standalone Zephyr health/telemetry node | F767ZI | Independent MCU with more memory, M7 cache/MPU topics, user LEDs/button and onboard debug; possible Ethernet extension | TFT/LCD, physical CAN, Ethernet, networking protocols |
| Reusable test fixture, not another flagship | Pico first for simple digital stimulus; ESP32 for a documented SPI-target candidate | Known patterns and controlled timing/protocol faults without depending on an external sensor | Custom PIO program or additional SDK only if needed |
| Low-priority accessories | UNO-style board(s), ESP8266, displays | Useful for bounded experiments, not a reason to broaden the curriculum | Porting every example to every board |

There is enough identified hardware to **start the software and bring-up
stages**, but not evidence that every final interface can be wired safely today.
No sensor/transceiver purchase is needed just to learn a state machine.

## 2. Project A: Portable Low-Voltage Fault/Event Recorder

Keep the useful product problem: capture what happened before and after an
intermittent reset, latency event or low-voltage trigger on a bench device.

```text
Known low-voltage input / labelled synthetic source
       |
F446RE: timer -> ADC -> DMA block pool -> checked/versioned SPI protocol
       |
BBB: SPI core/controller -> original IIO client -> buffered userspace recorder
       |
timestamped capture file + fault/overrun metadata + Yocto release manifest
```

### Develop In Evidence-Producing Slices

1. **Host/model:** frame sizing/parsing, sequence/reset handling and bounded
   buffers. Exercise errors before attaching hardware.
2. **MCU:** identify/debug the F446, acquire a known input, demonstrate block
   ownership and count overruns. Confirm target memory and timings; no
   assumption that data width equals ADC accuracy.
3. **Transport:** prove a short known-pattern transaction before batching/DMA.
   Identify SPI controller/target support, chip-select, ready signaling and
   buffer-lifetime rules. Model delays and malformed/short frames.
4. **Linux:** bind the client, expose justified IIO channels, trace reads,
   errors, start/stop and unbind. The SPI client uses the SPI core's ownership
   contract; it does not become a DMA-controller driver.
5. **Product:** pre/post-trigger capture, bounded recording/storage behavior,
   reset-session identity and timestamp uncertainty; package through Yocto.
6. **Evidence:** compare latency/CPU/memory under one measured change and run
   the contract's lifecycle/fault tests. No universal zero-loss claim.

The existing 40,000 payload bytes/second and 20,000-byte example capture remain
provisional. A different MCU may require a smaller or host-owned capture window.
Record the new acceptance limit, not a silently weakened "PASS".

### Valuable Design Decisions

- MCU pre-trigger storage versus host storage; block size versus delivery delay.
- SPI split transactions versus full-duplex frames, based on readiness and
  protocol requirements rather than chasing a DMA threshold.
- Buffer-pool ownership, copy cost and teardown ordering.
- Sample timestamps versus Linux arrival timestamps; clock/reset behavior.
- IIO versus a custom interface based on actual sampled-data semantics.

These demonstrate embedded engineering depth. They do not establish production
qualification or guarantee INR 90 LPA/Staff/Principal placement.

## 3. Project B: Standalone Zephyr Equipment-Health Node

Give the existing RTOS product a concrete independent behavior:

> Monitor a bench input, maintain timely local status and periodic telemetry,
> and detect stale data or stalled tasks while a slow diagnostics/display
> consumer competes for resources.

Start with the F767ZI, onboard button/LEDs and UART, plus a synthetic sensor.
Add one verified ADC or external sensor input after basic task behavior works.
The node must remain useful without BBB, Ethernet, Wi-Fi or a display.

```text
input/timer -> acquisition task -> bounded queue -> telemetry task
                    |                 |
                    +--> local status +--> optional display task
                    |
              useful-progress counters -> health supervisor -> recovery
```

### Core And One Selectable Extension

**Core:** priorities from deadlines/blocking, bounded memory, stale-data and
overload policies, controlled priority inversion and recovery; use existing
Zephyr APIs and trace their implementation. Measure GPIO timing with the
verified analyser when available. A working model is not physical timing proof.

After core gates, select **one** extension within existing hours:

- **Display contention:** use the SPI TFT as a deliberately slow, lower-priority
  consumer. Compare a 40 KiB full frame with a 4 KiB tile; count missed deadlines,
  dropped display updates and memory use. Confirm controller/voltage first.
  Do not build a large GUI or make display refresh part of the health guarantee.
- **CAN peer:** connect F767 and F446 only after transceivers, termination,
  driver/pin support and safe wiring are confirmed. Demonstrate heartbeat,
  backlog, peer loss and supported recovery behavior on an isolated bench.
  While F446 is the peer, it is not simultaneously A's independent acquisition
  firmware unless that combined design is explicitly developed.
- **Ethernet diagnostics:** use the F767's existing Ethernet hardware for
  optional status transport. First check the silicon errata and pin conflicts.
  Keep networking work bounded; it is not a new TCP/IP implementation project.

Do not add all three merely because the board can support them. M7 cache/DMA
and memory-region analysis is useful depth where the selected peripheral
actually uses DMA; enabling a feature or disabling cache is not proof of
correct ownership.

### Why Zephyr Remains The First Choice

There is a local board definition for F767 and F446, a supported Pico fallback,
and documented queue/mutex/watchdog mechanisms. Use one pinned version deeply.
FreeRTOS/ESP-IDF remains a possible fallback after an identified support blocker,
not a second full implementation to maintain. A firmware port is not evidence
that the same timing limits still hold.

## 4. Fallback Matrix

| Failure/blocker | First response | Continue learning with | Evidence that remains pending |
| --- | --- | --- | --- |
| BBB does not boot | Check known-good data cable/power, removable media and serial logs; preserve eMMC | Disposable Linux QEMU guest for driver/model/lifecycle tests | Physical McSPI/DMA/pinmux and board timing; no pictured MCU replaces a Linux SBC |
| BBB unrecoverable | Bound diagnosis, then evaluate repair or an approved Linux SBC replacement/BSP | Same guest-based interfaces and tests | Replacement board resources/DT/driver paths must be ported and measured |
| F446 fails | Verify ST-LINK, supply and recovery without overwriting unknown existing firmware | F767 as acquisition candidate; B can use Pico or time-share F767 with separate images | Revalidated ADC/DMA/SPI memory/clock/latency contract; not a drop-in port |
| F767 fails | Check debug/power/reset and preserve a recoverable image | B on F446; A and B may time-share it, or use a validated Pico port for B | M7/cache/Ethernet-specific evidence cannot be claimed on M4/M0+ |
| Both STM32s fail | Do not spend the whole schedule repairing hardware | Zephyr QEMU for B logic; Pico for a bounded MCU port; ESP32 for synthetic SPI device work | Physical STM32 ADC/DMA/CAN and timing; revise rates only explicitly |
| STM32 SPI target path unsupported/unreliable | Test the exact peripheral/framework path in isolation | ESP32 SPI-target fixture for A's Linux protocol tests after IDF validation; or approved UART transport change | Synthetic data is not real acquisition; UART does not complete the SPI-driver gate |
| Pico cannot be connected/programmed | Inspect cable/BOOTSEL/probe/header requirements; no assumed soldered contacts | Available Nucleo or host-generated test patterns | Physical stimulus timing/PIO work |
| ESP32 variant/toolchain blocks | Confirm module and pinned SDK; bound setup | Nucleo or Pico fixture, or guest model | ESP-specific transport/Wi-Fi behavior |
| UNO or ESP8266 fails | Omit auxiliary use | Host model or another confirmed MCU | No core milestone depends on these boards |
| TFT controller/voltage unknown or display fails | Stop display wiring; identify module before retry | UART/LED status; character LCD only after its own gate | Display-driver/bus-contention evidence, not RTOS core completion |
| LCD fails or needs unavailable level conversion | Omit it | UART/LED status | LCD-specific behavior only |
| F767 Ethernet unstable | Check silicon identity/errata, link/PHY and pin conflicts | UART telemetry; retain standalone sensing/supervision | Ethernet acceptance; do not assume PCB Rev B identifies silicon cut |
| CAN accessories missing or bus fails | Confirm controller/driver/transceiver/termination before blaming MCU | UART peer or software heartbeat/queue model | Physical CAN timing, arbitration and error recovery |
| Saleae unavailable or model unsuitable | Verify USB/software/probe compatibility; do not exceed limits | Software counters/timestamps and host/guest traces | Electrical timing, waveform validation and externally measured latency |
| HP builder unavailable | Preserve source/revisions and use working build artifacts | Existing approved Windows/Podman fallback | New host/toolchain parity and any unrun tests |

Apply the existing **one two-hour diagnostic block** fallback rule: collect a
small evidence set, then continue an independent model/software slice. This is
not permission to silently declare a broken hardware gate complete. Some faults
need later diagnosis or replacement; document them without extending weekly
hours. One physical board can run separate project images sequentially, not two
independent demonstrations simultaneously.

## 5. Readiness Before Any Connection

1. Label physical units and confirm uncertain models/counts using this inventory.
2. Inspect connector condition, jumpers, cables and power arrangements on an
   appropriate nonconductive/ESD-aware work surface.
3. With permission, identify/debug one board at a time and preserve existing
   source/firmware where recovery matters. Record board and silicon revisions.
4. Establish a minimal serial/debug/LED check before ADC/SPI/CAN/display work.
5. Confirm common reference/ground, logic voltage, input ranges and signal
   direction before wiring. A 5 V UNO and 3.3 V MCU are not plug-compatible;
   BBB ADC inputs need their own lower-voltage limit checks.
6. Approve a small BOM only after missing accessories are confirmed. Do not
   buy a replacement board until an existing failure has been isolated.

For every fallback record: failed component, exact evidence, replacement,
behavior/API changes, tests repeated, target limits retained/revised and gaps.
No board here is marked working from appearance alone.

## 6. Research Basis And Limits

Inspected 2026-10-25:

| Reference | What it supports | What it does not prove |
| --- | --- | --- |
| [Bootlin kernel labs](https://bootlin.com/training/kernel/) | Complete driver workflows with I2C/serial and BBB lab options | Our custom protocol or salary outcome |
| [Zephyr F446](https://docs.zephyrproject.org/latest/boards/st/nucleo_f446re/doc/index.html) and [F767](https://docs.zephyrproject.org/latest/boards/st/nucleo_f767zi/doc/index.html) | Board mappings and starting documentation | Every peripheral combination works together |
| [Local F767 docs](../../zephyr-workspace/zephyr/boards/st/nucleo_f767zi/doc/index.rst) and [DTS](../../zephyr-workspace/zephyr/boards/st/nucleo_f767zi/nucleo_f767zi.dts) at `55f132b58` | Cortex-M7 board support, PA7 conflict, specific Ethernet erratum warning | Current board's silicon cut, clock or generated config |
| [Zephyr Pico](https://docs.zephyrproject.org/latest/boards/raspberrypi/rpi_pico/doc/index.html) | MCU alternative, UF2/SWD workflows | Pi Zero/Linux capability or an automatically equivalent acquisition port |
| [ESP32 SPI target](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/peripherals/spi_slave.html) | Queued target transfers, readiness and DMA buffer constraints | Compatibility with an unidentified module/Zephyr target |
| [Zephyr mutexes](https://docs.zephyrproject.org/latest/kernel/services/synchronization/mutexes.html) and [task watchdog](https://docs.zephyrproject.org/latest/services/task_wdt/index.html) | Priority inheritance and per-task health mechanisms | Application deadline or recovery correctness without tests |
| [CAN controller API](https://docs.zephyrproject.org/latest/hardware/peripherals/can/controller.html) | Controller setup/bit timing concepts | Physical transceivers or working CAN wiring in the photos |
| [Linux IIO buffers](https://docs.kernel.org/iio/iio_devbuf.html) and [I2C stub](https://docs.kernel.org/i2c/i2c-stub.html) | Standard interfaces and bounded software-model fallback | Electrical fidelity or generic SPI emulation |

The existing project briefs contain the previously researched public client
requirements and role mapping. These board choices preserve that practical
focus; no new claim that a peripheral or project guarantees Staff/Principal
placement or INR 90 LPA. A strong portfolio explains constraints, alternatives,
measured failure behavior and original contributions, not just board count.
