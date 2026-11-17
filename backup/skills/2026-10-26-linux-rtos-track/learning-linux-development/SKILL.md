---
name: learning-linux-development
description: "Use for embedded Linux, Yocto/OpenEmbedded, BSP, QEMU, kernel source, device tree, platform/SPI/I2C drivers, IIO, IRQs, locking, DMA ownership, tracing, project design and career preparation."
argument-hint: "Linux/Yocto topic, selected function, driver failure or design decision"
---

# Linux Drivers, BSP And Systems Design

Apply the [shared coach](../learning-feynman-coach/SKILL.md).
Use the [roadmap](../../../plans/five_month_development_roadmap.md),
[A contract](../../../plans/project_portfolio.md) and
[tracker](../../../plans/training_tracker.md), not an independent schedule.
Pair with C or Arm only for real overlap; RISC-V only for architecture questions.

## Scope And Prerequisites

Teach processes/files/syscalls and device/kernel boundaries before module/driver
APIs, then matching/probe/resources, interrupts, synchronization/teardown,
buffering, tracing and power management. Answer requested later topics directly,
inlining prerequisites; do not force a whole teaching-kernel rebuild.

Project A is a low-voltage recorder, not a certified instrument. Preserve
existing MMU knowledge and teach its relevance to virtual/physical/DMA addresses.
Staff-oriented discussion includes requirements, alternatives, failure budgets,
maintainability and measured evidence, not promised job titles.
The target is embedded Linux/BSP/drivers and device-related userspace services,
not generic server, desktop or backend programming. Syscalls, files, scheduling
and memory remain supporting mechanisms for the embedded path.

## Source Trace

Confirm checkout revision, architecture, configuration, guest/board and relevant
version dependencies. Read contribution/coding instructions before changes.
Follow definition -> registration -> caller -> callbacks -> completion ->
failure/unwind -> removal. For each step identify execution context, caller and
callee ownership, locks, sleeping rules, state mutations and lifetime.

Trace one concrete operation end to end before line-level detail. Explain every
introduced API's inputs, outputs and errors. Label reduced models and upstream
deviations. Read the exact source before claiming register values or behaviors.

## Driver Accuracy

- A module is a loading unit; a driver binds a device. DT describes hardware,
  not executable driver logic. Bindings and available properties are versioned.
- SPI/I2C client drivers differ from controller drivers. Use existing bus APIs.
  Select IIO/hwmon/input/serdev/other interfaces from the device contract, not
  résumé keywords. A byte stream is not automatically an IIO sensor.
- IIO scans have channel ordering, endian/sign/width/shift and timestamp rules;
  readers must inspect metadata. Separate sampling time from delivery time.
- Managed resources do not solve asynchronous teardown. Stop producers,
  quiesce IRQ/work/transactions and manage users/open handles before freeing.
- Hard IRQ, threaded IRQ, softirq and workqueue have different context rules.
  Check PREEMPT_RT/configuration before universal locking claims.
- CPU virtual, physical and DMA/bus addresses are not interchangeable. Follow
  DMA direction, ownership and sync rules. Coherent memory still needs ordering.
  A SPI client must not bypass the controller/core's DMA ownership.
- Kernel release/provider/patches determine helper availability, not the Yocto
  codename alone. Do not mandate `__dma_from_device_group_begin/end` or assert
  their introduction version without evidence. Dedicated suitable allocations
  may simplify isolation; embedded fields and pool slices need both-boundary
  cache-sharing analysis. `GFP_DMA` is not an alignment/synchronization repair.
- SPI eligibility is per transfer and controller/configuration dependent.
  Inspect channel availability, mode and thresholds; do not equate transfer
  length with observed DMA. Compare split versus combined transactions using
  CS, turnaround/readiness, dummy bytes and latency requirements first.
- Distinguish compiler barriers, CPU barriers, locks, atomics, cache maintenance
  and reclamation. ISO C models are not replacements for kernel memory-model APIs.
- User access may fault; validate lengths and partial-copy/error semantics.
  No silent invalid-input returns or swallowed errors.
- Do not claim Linux supports a device in QEMU because an in-guest stub exists.
  `i2c-stub` covers documented SMBus operations; FIFO/IRQ behavior needs its own
  tested model. `dmatest` requires suitable advertised channel capabilities.

## Yocto And Hardware Workflow

Follow [environment gates](../../../docs/workspace_setup.md). Confirm a supported
Linux host, resources, pinned release/layers and BSP. Teach machine versus
distro versus image configuration, recipes, kernel fragments, SDK and devtool.
Start with a minimal QEMU image and one incremental change, then port recipes
to BBB. Do not promise identical binary images across targets.

The learner plans an HP x360 Ubuntu Server 26.04.1 builder with GitHub source
sync from the editing laptop; Remote-SSH is a suggested fast loop. Installation,
hardware resources and connectivity are not verified. Existing Windows/Podman
remains a fallback, not a mandatory permanent host. Pin the release first:
inspected Yocto 6.0 lists Ubuntu 26.04, while 5.0.14 lists 24.04. A compatible
Podman container on Linux is an option, not proof of build compatibility.
No public push, clone, login or install without the relevant permission.

Verify module ABI/configuration against the running kernel. A container shares
the host kernel: experimental modules belong in disposable guests/approved
learning boards. No installs, image writes, eMMC changes or host-resource changes
without permission. Preserve recovery, existing media and sibling repositories.

Keep build-system breadth bounded. Reproducible commands are not by themselves
bit-reproducible builds. Record source revisions, toolchain, configuration,
image version and test results, not merely successful compilation.

## Debug And Review

Supply a prediction in Teach mode. Use relevant logs, fault PC/cause, GDB,
ftrace, KUnit/selftests, lockdep and sanitizers where supported. Select tests
for the actual failure hypothesis; do not run all tools by default. Measure
baseline, change one thing, compare under identical workloads and state limits.
For PIO/DMA observations discover the running target's traceable functions/events
before promising output. Missing trace entries alone do not prove PIO. Do not
force module removal or deliberately corrupt memory as ordinary acceptance.
Use the [project decision/failure record contract](../../../plans/project_portfolio.md#incremental-design-and-failure-evidence)
incrementally, distinguishing proposed budgets, source traces and measurements.

Review learner code before edits; build/run only when requested. Preserve
historical notes and annotate their limitations in active guidance.

For career advice use dated official roles and actual evidence. Separate
software-development tenure from overall C/validation tenure, Senior/Staff/
Principal scope, learning work from workplace impact, and CTC from fixed pay.
Freelance briefs are examples of customer requirements, not salary data.
Never guarantee INR 90 LPA, offer dates or an upstream patch acceptance.
