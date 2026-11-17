# Two Product-Oriented Project Contracts

Proposed scope, 2026-10-22; review refinements applied 2026-10-23.
**Not implemented or verified.**
The [roadmap](five_month_development_roadmap.md) alone owns weeks/hours.
[Tracker](training_tracker.md) owns gates and observed evidence.

## Hardware Allocation Update: 2026-10-25

All 11 supplied photos are described in the
[inventory](../docs/board_inventory.md), with a
[per-board fallback plan](../docs/board_project_plan.md).
Recommend **A on BBB + F446RE** and **B on F767ZI**, subject to independent
bring-up. The second STM32 is identified as NUCLEO-F767ZI; the Raspberry Pi
board is an original Pico, not a Linux-capable Pi Zero. Other MCU boards and
displays are optional resources, not additional flagship projects.

No hardware gate is closed by identification. F767 Ethernet/PA7 conflicts,
silicon errata, display controllers, electrical limits and CAN accessories
remain gates. Existing project targets and calendar are unchanged; substitutions
require explicit port/revalidation and must state which evidence is lost.

## Why These Two

**A solves a Linux-connected measurement/debugging problem. B solves a
standalone MCU scheduling/reliability problem.** They must run independently.
Reuse justified low-level helpers only after teaching their contracts; do not
make B depend on A's Linux host, or count one demonstration as two projects.

The goal is production-oriented engineering: useful behavior, bounded resource
use, maintainable interfaces, diagnostics, reproducible releases and tested
failure paths. Production qualification additionally requires device-specific
electrical, calibration, environmental and deployment evidence. Do not label
a learning prototype production-ready solely because it passes a bench test.

## A. Embedded Fault/Event Recorder

### User And Problem

An embedded developer needs to see low-voltage signals immediately before and
after an intermittent reset, slow response or trigger. Serial logs alone miss
that history. Build a recorder that exports timestamped captures and explicitly
reports loss or invalid data. Not a certified oscilloscope, medical instrument
or industrial protection/control device.

```text
Safe bench input -> STM32 timer/ADC -> DMA blocks -> bounded firmware protocol
                                                           |
                                                          SPI
                                                           |
                         BBB Linux client driver -> IIO buffer
                                                           |
                         C recorder -> pre/post-trigger record -> CLI/export
```

Firmware may start bare-metal; A must not require completing B first. An RTOS
is optional here, not a second scheduler implementation exercise. Pin and test
SPI target support early. On failure, document the cause and obtain approval
for a transport alternative; do not claim the SPI/IIO hardware gate passed.

### Scope And Initial Targets

- Two ADC channels; provisional 10,000 samples/second/channel, stored in 16-bit
  containers: `2 * 10000 * 2 = 40000` payload bytes/second before framing.
  Container width is not ADC precision. Validate analog settling, clocking,
  protection, transport and available memory before committing to the rate.
- Example capture: 250 ms before + 250 ms after trigger gives 20,000 payload
  bytes at that rate. Choose explicitly whether pre-trigger storage is MCU or
  host owned; account for all buffers, metadata, stacks and driver allocations.
- ADC/DMA producer -> buffer handoff -> transport consumer with explicit states.
  No allocation in the time-critical steady-state MCU path.
- Versioned configuration/data protocol: lengths, channel format, sequence,
  session/reset identity, errors and rate negotiation. Choose checksums and
  their protected scope based on failure requirements, not by habit.
- Linux SPI client and suitable IIO channel/scan/buffer interface; user tool
  reads the actual scan metadata rather than assuming packed layouts.
- Sampling time and Linux receipt time are distinct. Define MCU clock rate,
  wraparound, reset epochs and synchronization assumptions; report uncertainty.
- Stop, transport timeout, MCU reset and driver removal have bounded completion
  and explicit data-validity outcomes.
- Yocto layer packages kernel/config/DT, driver and userspace tool. Record
  source/build identities in the running system and every capture.
- CLI and simple export first. A desktop GUI, cloud and fancy visualization
  are not core requirements.

I2C can add a verified low-cost temperature/reference sensor as metadata later.
Do not add a sensor before choosing a compatible module and approved budget.
Reuse existing sensor drivers where appropriate; originality belongs in the
custom MCU interface, lifecycle behavior, recorder and test harness.

### DMA And Memory Boundaries

Direct ADC/DMA work happens on STM32. On Linux, the SPI controller driver owns
its transfer implementation; a client must follow SPI API buffer/lifetime rules
rather than independently programming DMA or bypassing core ownership.
Do not claim Linux DMA-provider authorship, zero-copy, cache-coherency
verification or performance benefits without inspecting and measuring the path.

Study reference ADC/DMA/IIO implementations without assuming they run on the
F446RE or that a generic QEMU machine has the same hardware. Choose memory pools,
copying and batch sizes for correctness first, then measure bottlenecks.

#### Version And Buffer Ownership Rules

Record the actual target kernel provider, version, source commit, configuration
and relevant patches. A Yocto release name does not uniquely specify the kernel
or available helpers. The researched Scarthgap BBB default points toward 6.6;
the workspace reference at `f0100363d` is not automatically that target.

The reference [DMA header](../../linux/include/linux/dma-mapping.h) contains
`__dma_from_device_group_begin/end`. Check availability in the selected build
before use; no exact introduction release is asserted here. These helpers are
not a mandatory project feature or a reason to backport unrelated code.

- A suitable dedicated `kmalloc`/`devm_kmalloc` allocation can simplify DMA-buffer
  isolation. Follow the target's DMA and SPI APIs; do not claim a particular
  allocation alignment universally from a macro name alone.
- `devm_*` frees managed resources eventually; it does not stop producers,
  cancel DMA or drain callbacks. Quiesce users before resource release.
- Do not use stack storage as a general DMA buffer. Synchronous API completion
  does not make an otherwise unsuitable buffer DMA-safe.
- Buffers embedded in structures or suballocated from a pool can share cache
  lines with CPU-written data on noncoherent systems. Protect both boundaries
  using the correct target/subsystem alignment and allocation rules. Merely
  aligning the enclosing structure or appending a cache-line annotation is
  not a universal repair.
- `GFP_DMA` concerns address-zone constraints, not cache-line isolation or a
  synchronization barrier. Mapping direction, ownership and lifetime remain
  necessary even when alignment is correct.
- Consult the [DMA guide](../../linux/Documentation/core-api/dma-api-howto.rst)
  and [IIO alignment definitions](../../linux/include/linux/iio/iio.h) matching
  the target. Model unsafe sharing first; do not promise visible corruption
  on every board or deliberately corrupt memory in ordinary acceptance tests.

### SPI Protocol And DMA Observation

Use this as a bounded design/source trace in the existing Week 9 allocation and
an observational experiment in the existing Week 18 work, not an extra project.
Follow buffer allocation -> DMA eligibility -> SPI-core mapping -> controller
submission -> completion/synchronization -> unmapping and final release.

In the inspected [SPI core](../../linux/drivers/spi/spi.c), eligibility is
evaluated per `spi_transfer`. The inspected
[McSPI driver](../../linux/drivers/spi/spi-omap2-mcspi.c) first requires both DMA
channels, treats controller target mode separately, and otherwise applies
`DMA_MIN_BYTES` (160 bytes in this checkout). This is not a universal SPI
threshold, a per-message rule, or proof of DMA execution on the future board.

Compare these designs only if both satisfy the MCU protocol:

| Candidate | Source-level expectation under the stated conditions | Decision questions |
| --- | --- | --- |
| One message with a 4-byte command transfer and a 256-byte data transfer | Header is below the inspected threshold; data transfer is a DMA candidate | Must CS stay asserted? Is a gap/delay or ready signal needed before the response? |
| One 260-byte full-duplex transfer | One transfer is a DMA candidate; non-null TX/RX buffers require the appropriate handling | Can the MCU prepare data in time? Which RX bytes are invalid during the header, and which TX bytes are dummy clocks? |

Specify CS transitions, turnaround timing, byte offsets, dummy values,
readiness, word size, lengths, versioning and reset behavior. Combining phases
just to cross a DMA threshold is not a design justification. Larger batches
can reduce overhead while increasing sample-delivery latency or memory use.

For the board experiment:

1. Verify the running kernel/controller, configuration and DMA-channel binding;
   discover available tracepoints/functions and tracing support before choosing
   `ftrace` filters. No pre-promised function names in the trace output.
2. Select protocol-valid transfer lengths below, at and above the target's
   applicable threshold, respecting word width and buffer constraints.
3. Observe eligibility, actual submission and completion. If channels/tracing
   are unavailable, record the blocker; absence from a trace is not by itself
   proof of PIO.
4. Compare known-pattern integrity, CPU cost and latency under the same workload.
   Record raw evidence, instrument overhead and limitations, not just a PASS.
5. Keep client buffers valid until their API-defined completion; the SPI
   client must not independently map/unmap buffers owned by the SPI core.

Source walkthroughs and models remain useful if the hardware path is blocked,
but cannot be labelled a measured DMA/cache-coherency demonstration.

### A Acceptance Matrix

| Check | Required evidence |
| --- | --- |
| Known input | Expected pattern/signal, captured values, conditions and error bounds; synthetic and physical results labelled separately |
| Capture semantics | Pre/post-trigger boundaries, channel ordering and timestamps verified for declared trigger rules |
| Throughput | Declared workload and duration; sequence accounting; no silent loss within tested limits |
| Overload | Every dropped/invalid block accounted for; bounded queues and documented policy |
| Failure | Stall, malformed/truncated packet, MCU reset and stop during transfer have explicit outcomes |
| Lifecycle | Partial-probe unwind and repeated start/stop/remove tests; no outstanding resource ownership after quiescence |
| SPI/DMA trace | Target-version/eligibility/ownership trace recorded; live PIO/DMA observations or an explicit hardware/tracing blocker; no assumed threshold result |
| Persistence | Incomplete/corrupt records detected; storage-full behavior bounded; no power-cut tests on valuable media |
| Release | Clean documented build/deploy, running version match, test commands with timeout and failure status, known limitations |
| Design/failure records | Incremental decisions and reproducible fault evidence as specified below; estimates and measurements separated |

Set exact stress counts, soak duration, latency percentiles and resource budgets
before each experiment, using a measured baseline. No vague "fast" or
unconditional "all tests passed." Logic-analyser timing needs sample rate,
probe placement, clock/compiler configuration and instrumentation overhead.

## B. Standalone Zephyr Telemetry And Health-Monitoring Node

### User And Problem

A small equipment-monitoring node must keep periodic sensing and local status
responsive when diagnostics or communications are busy. A blocked peripheral
or stalled task must be detected, not hidden by another task feeding a watchdog.
It runs on an MCU without a Linux host; its outputs are bench indicators and
telemetry, not safety-critical actuators.

**Recommend Zephyr as the implementation RTOS.** The workspace already has its
source; official QEMU, synchronization, watchdog and CAN material supports the
learning path. Recommend F767ZI for the standalone node, with F446RE/Pico
fallbacks as documented above. Pin a compatible release/SDK and verify the
selected board's drivers and memory layout. Compare
FreeRTOS scheduling/queues/priority inheritance conceptually after one working
system; implementing both would dilute the five-month scope.

```text
timer/sensor event -> acquisition task -> bounded message queue -> telemetry
                           |                      |
                           +--> local status <---+
                           |
                      progress counters -> health supervisor -> recovery

diagnostic/command task runs independently; no Linux host required
optional second MCU <---- physical CAN after hardware gate ----> this node
```

### Core Behavior

- Separate acquisition, local-status, telemetry/diagnostics and supervision
  responsibilities. Choose priorities from deadlines and blocking bounds,
  not from names or the assumption that more threads are better.
- Begin with a synthetic source in QEMU, then one real input on the selected Nucleo.
  Expand peripherals only when each adds a useful behavior.
- UART diagnostics are the initial transport. B remains useful and testable
  on one board even if the other board or CAN accessories fail. Display/CAN/
  Ethernet extensions must not block core acceptance.
- Provisional workload: acquisition every 10 ms, status every 100 ms and
  telemetry every 1 s. Derive CPU/stack/queue budgets, stale-data thresholds and
  response deadlines before hardware tests; these periods are targets, not
  measured guarantees.
- Bounded queues with explicit drop-oldest, drop-newest or backpressure choice;
  explain which consumer may block and why. Use sequence and age information.
- Demonstrate priority inversion with a controlled shared-resource scenario;
  trace priority inheritance and limitations, then shorten critical sections
  and compare with an ownership/message-passing design.
- Track useful task progress, not just task execution. A blocked acquisition
  path must not be reported healthy by a looping supervisor.
- Use bounded retries and a documented recovery state machine. Hardware
  watchdog/reset testing needs an approved recovery/debug procedure.
- Fixed pools or bounded allocation policy; stack-watermark/overflow checks
  where supported. Logging must not determine real-time correctness.
- Saleae GPIO markers measure wakeup-to-work and recovery latency under load.
  A sampled maximum is not a proof of worst-case execution time.

### Optional Two-Board CAN Gate

The photos identify F767ZI and F446RE as the two STM32 models. Confirm both
boards work, CAN controller support in the pinned release,
transceiver availability, voltage, termination, pinmux and bench wiring.
F446 capabilities must not be confused with CAN FD. Internal loopback, software
models and a real two-node bus are different evidence.

After the gate, add telemetry, heartbeat, missed-peer detection, overload and
bus-off recovery where supported. Use a small documented protocol first;
ISO-TP is an optional extension, not a requirement to implement UDS/CANopen or
connect to a vehicle. If unavailable, retain UART/core completion and mark the
physical CAN extension deferred, never simulated-as-verified.

### B Acceptance Matrix

| Check | Required evidence |
| --- | --- |
| Standalone | MCU starts, senses/models input, displays status and reports diagnostics without BBB |
| Scheduling | Task priorities/deadlines justified; execution traces under normal and overloaded workload |
| Inversion | Reproducible blocked-high-priority scenario and measured/traceable repair |
| Memory | Queue saturation policy exercised; bounded memory and stack budget documented |
| Health | Stalled task, stale sensor and slow consumer detected; recovery/failure recorded |
| Timing | Board timing conditions and workload stated; simulation results labelled separately |
| Release | Pinned build/configuration, test procedure, startup/recovery behavior and limits |
| Design/failure records | Incremental scheduling/ownership decisions and fault evidence as specified below; no automotive/safety qualification claim |
| Optional CAN | Separate controller/physical-bus test record after hardware confirmation |

## Incremental Design And Failure Evidence

Each project maintains its own `DESIGN_DECISIONS.md` and
`FAILURE_AND_TIMING_EVIDENCE.md` inside its eventual project repository.
These are required release deliverables, not files created by this plan update.
Begin records with the first implementation decision, update them as evidence
arrives, and assemble them for portfolio review. Do not invent measurements to
fill a template or wait until the final week to reconstruct decisions.

Use existing concept/implementation/career hours. Keep records short and link
raw evidence rather than duplicating it. Four substantive decisions per project
are the initial review target; a split or combined decision is acceptable if
the important trade-offs are still covered.

### Design Decision Record

For each decision record:

`Requirement -> constraints -> alternatives -> choice -> assumptions/budget -> measured result -> limits -> reconsideration trigger`

| Project | Initial decisions |
| --- | --- |
| A | SPI versus UART/I2C and split versus combined transactions; MCU versus host pre-trigger storage; IIO versus a custom userspace interface; MCU sample-time relation to a documented Linux clock |
| B | Priorities from periods/deadlines/blocking; queue overload policy; progress-aware supervision versus a timer-fed watchdog; priority-inheritance mutex versus message-passing ownership |

Put units and conditions beside numbers. A proposed sample rate or memory
budget is not a measurement. Explain MCU clock drift, reset epochs and clock
correlation uncertainty; Linux receipt time is not automatically sample time.
Do not describe a project decision as production deployment or workplace impact.

### Fault-Injection And Timing Evidence

For each test record:

`Requirement/decision -> fault and injection point -> safe setup/recovery -> expected behavior and timeout -> observed result -> source/config/command/runner -> trace -> limitation/repair`

- A: malformed or truncated frames; checksum faults if a checksum is selected;
  stalled transfers, MCU reset, slow userspace, storage full, partial probe
  failure and stop/unbind with activity outstanding.
- B: stale sensor, stalled useful progress, slow telemetry, queue saturation,
  bounded retry and priority-inversion scenarios; physical CAN errors only
  after its gate is satisfied.
- Define workload, test duration, sample count, memory/latency budgets and pass/
  fail rules before execution. Record distributions and sampled maxima with
  measurement conditions, not unproved worst-case timing.
- Use ordinary supported unbind/unload behavior on disposable guests or approved
  bench targets. Do not force module removal; refusal while referenced can be
  correct. Never free in-use resources merely because a timeout expires.
- State who ran each test and whether it was a model, emulator or real board.
  No automatic mastery, "automotive-grade", ASIL or production certification.

## Optional Genuine Upstream Contribution

During the existing late career/project allocation, consider a small upstream
documentation, test, binding/configuration or reproducible bug fix encountered
in the work. Core functionality and the March deadline take priority; no
submission or acceptance is a hiring/project gate.

Read the chosen project's contribution rules. For Linux, learn applicable
`checkpatch.pl`, `get_maintainer.pl`, patch preparation and review etiquette;
for Zephyr use its own process and tests. Do not manufacture cosmetic churn
or send patches solely to collect a contribution count. A local prepared patch
can teach the workflow without publication. Obtain explicit approval before
committing, emailing, pushing or submitting any public contribution.

## References And What They Establish

Inspected 2026-10-22. Use the pinned checkout/version before code-level teaching.

- [Bootlin kernel labs](https://bootlin.com/training/kernel/): complete driver
  mechanisms, I2C and serial examples, including BBB lab options.
- [Bootlin Yocto labs](https://bootlin.com/training/yocto/): BSP/layers/recipes/SDK.
- [IIO buffers](https://docs.kernel.org/iio/iio_devbuf.html): standard buffer and
  scan metadata; [I2C stub](https://docs.kernel.org/i2c/i2c-stub.html): limited
  in-guest register model, not physical peripherals.
- [DMA test guide](https://docs.kernel.org/driver-api/dmaengine/dmatest.html):
  suitable channel capabilities are required; not a generic peripheral-DMA test.
- [Zephyr QEMU](https://docs.zephyrproject.org/latest/boards/qemu/cortex_m3/doc/index.html),
  [mutexes/inheritance](https://docs.zephyrproject.org/latest/kernel/services/synchronization/mutexes.html),
  [task watchdog](https://docs.zephyrproject.org/latest/services/task_wdt/index.html),
  [CAN controller](https://docs.zephyrproject.org/latest/hardware/peripherals/can/controller.html),
  [two-board ISO-TP example](https://docs.zephyrproject.org/latest/samples/subsys/canbus/isotp/README.html):
  concrete mechanisms for B, not evidence of our board's readiness.
- [Client waveform/STM32 brief 40778250](https://www.freelancer.com/projects/embedded-systems/arduino-stm-gui-firmware):
  requests DMA/peripheral integration, waveform capture, calibration, debug
  hooks and reproducible tests.
- [Client firmware brief 40777791](https://www.freelancer.com/projects/stm32/stm-snmpv-control-firmware):
  requests bounded resource growth, diagnostics, integration and acceptance
  tests. Its F767/Ethernet setup is not our F446 platform.

Freelance briefs establish example customer needs, not technical feasibility,
delivery success, market-wide demand rankings or Staff compensation. Upwork
access returned 403; no Upwork listings were verified. No client was contacted
and no proprietary design was copied. Vendor pages that timed out were not
used to certify hardware compatibility or select a purchase.
