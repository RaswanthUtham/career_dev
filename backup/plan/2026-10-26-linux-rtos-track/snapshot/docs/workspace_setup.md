# Workspace Readiness

Updated 2026-10-25. This is an environment gate, not a second schedule.
Follow the [roadmap](../plans/five_month_development_roadmap.md),
[project contracts](../plans/project_portfolio.md) and
[tracker](../plans/training_tracker.md).

## Source And Ownership Boundaries

```text
career_ws/
  career_dev/                plans, teaching skills, notes and learning history
  my-linux/                  learner teaching kernel; existing v2 stays untouched
  linux/                     upstream reference checkout
  xv6-riscv/                 architecture reference checkout
  zephyr-workspace/zephyr/   RTOS reference checkout
```

The saved [workspace](../career_dev.code-workspace) includes these five roots.
Do not vendor source clones into the learning repository. Choose locations for
new project repositories and Yocto build trees with the learner before creating
them; do not create implementation scaffolds merely because a plan names them.
Preserve dirty worktrees, remotes and independent repository histories.

Current inspected reference revisions: Linux `f0100363d`, xv6 `9e3161a`;
teaching kernel `v2` at `3fddd0d`, with `main`/`v1` and `v1-reference` at
`6610119`. Recheck before version-specific teaching. Pin Zephyr, west modules,
SDK, Yocto release/layers, kernel, QEMU and compiler before execution.

## Hardware Inventory

The [photo inventory](board_inventory.md) covers all 11 supplied photographs;
[project allocation and fallbacks](board_project_plan.md) describe how to use
them. Labels/layouts were inspected on 2026-10-25; no board functionality tested.

| Item | Evidence and intended use |
| --- | --- |
| NUCLEO-F446RE | Photo label MB1136 rev C; recommended A acquisition board; debug/power/clock/condition unverified |
| NUCLEO-F767ZI | Photo label MB1137 Rev B; recommended B board or conditional A substitute; silicon cut, cache/DMA setup and operation unverified |
| BeagleBone Black family | Photo-identified; Linux target for A; exact revision, boot media, serial recovery and functionality pending |
| Raspberry Pi Pico | Original Pico identified, not Pi Zero; MCU spare/stimulus; headers absent in photo, no onboard wireless module visible |
| ESP32 HW-463 / ESP8266 NodeMCU-style | Family/layout identified; exact variants/toolchain compatibility and functionality pending; optional fixtures |
| UNO R3-style board(s) | Two photos; exact count/chip markings unconfirmed. Treat 5 V logic compatibility as a gate, not a STM32-equivalent spare |
| SPI TFT and character LCD | 1.8-inch SPI 128x160 V1.1 module with SD socket; probable parallel 16x2 LCD. Controllers/voltage/working status unknown |
| Saleae logic analyser | Learner-reported, not photographed; model, limits, sample rate and probes still pending |
| Sensors, CAN transceivers, wiring and spare SD | Not established by photos; BOM/budget deferred, no purchases authorized |

F767's default SPI/Ethernet mapping conflicts on PA7; its local board docs also
flag an Ethernet issue for silicon cut A. PCB Rev B does not identify that cut.
Do not enable Ethernet and a display on assumed default SPI pins without
checking the selected source/configuration and schematic.

Do not purchase or wire hardware from this table. Confirm schematics, pinmux,
logic levels, grounding and protections for the exact board. Work on isolated,
low-voltage bench equipment, not vehicles, mains, battery power systems or
safety-critical equipment. CAN controllers need suitable physical transceivers
and termination; a logic analyser is not automatically a CAN electrical probe.

## Emulator-First, Not Emulator-Only

| Target | Useful evidence | Not established |
| --- | --- | --- |
| Disposable Linux QEMU guest | Kernel boot, modules, KUnit/model tests, lifetimes, tracing and userspace interfaces | BBB electrical timing or physical DMA/cache behavior |
| Linux `i2c-stub` inside guest | Supported SMBus register interactions | Full device FIFO/IRQ behavior, arbitrary I2C transfers or a physical controller |
| Zephyr supported QEMU target | Scheduling, queues, state machines and simulated faults | F446 pinmux, actual ADC/DMA/CAN or MCU timing measurements |
| Physical boards | Target-specific transfers and measured behavior | Production qualification beyond the tested conditions |

Select emulator machine and architecture explicitly. `qemu_cortex_m3` is a
documented Zephyr example, not an F446 emulator. A QEMU Arm `virt` machine is not
a BeagleBone peripheral model. Never load experimental modules into the Podman
host kernel: a container shares its host kernel.

## Planned HP Linux Build Host

Latest environment direction, 2026-10-23: the learner plans to install
**Ubuntu Server 26.04.1 on an HP x360**, retain the present laptop for editing,
push original project source to GitHub and pull it on the HP for building.
Installation, SSH/network reachability, CPU architecture, RAM and available SSD
space on the HP are **not verified**. The Windows/Podman measurements below must
not be copied onto the HP's inventory.

Recommend the HP as the primary builder **after readiness is demonstrated**.
Keep the existing Windows/Podman setup as a usable fallback while that happens;
do not delete or migrate any current source/worktree/container automatically.
Changing the build host does not change the embedded targets: STM32 firmware
and BBB Linux. Ubuntu Server is a development host, not a new server-programming
course or the required operating system image for BBB.

### Ubuntu And Yocto Compatibility

Official documentation checked 2026-10-23:

| Choice | Evidence and decision gate |
| --- | --- |
| Ubuntu Server 26.04.1 | [Ubuntu release directory](https://releases.ubuntu.com/26.04/) lists the server installer; verify architecture, checksum, recovery and disk backup before installing |
| Yocto 6.0 (Wrynose) natively on Ubuntu 26.04 | [6.0 requirements](https://docs.yoctoproject.org/6.0/ref-manual/system-requirements.html) list Ubuntu 26.04; their `core-image-sato`/`qemux86-64` example calls for 140 GB disk and 32 GB RAM. Verify HP resources and BBB BSP/layer support before selecting it |
| Earlier Scarthgap 5.0.14 candidate | [5.0.14 requirements](https://docs.yoctoproject.org/5.0.14/ref-manual/system-requirements.html) list Ubuntu 24.04, not 26.04; the documented example uses 90 GB disk and 8 GB RAM. A 24.04-based Podman build container is an option on the HP, subject to validation |

Do not apply one release's resource estimates or supported-distribution list to
another. A minimal image can differ from the documented example; do not promise
it fits before measurement. An unlisted host may work but is not established as
a tested combination by that document. Select release, layer revisions, machine
and kernel provider together; no release upgrade is performed by this plan.

Podman and a physical Linux builder are compatible choices, not opposites.
On Linux, Podman containers share the native Linux kernel and can isolate
user-space tool dependencies without a Windows WSL backend. Use persistent
Linux-filesystem source/build/cache storage; container image existence alone is
not a verified reproducible build.

### Edit, Synchronize And Build

**Learner-proposed workflow (valid, not yet configured):**

```text
Current laptop: edit original code -> commit/push approved source to GitHub
HP Linux host: pull the selected commit -> build/test -> record results
Embedded target: QEMU guest or approved STM32/BBB hardware
```

For a clean checkpoint build, record the branch, commit, configuration and
toolchain; ensure the HP builds the intended revision. Do not overwrite dirty
worktrees or force synchronization. Keep independently owned source forks and
project repositories separate on both hosts. No repository creation, clone,
credentials, commit or push has been performed or authorized as an immediate
assistant action by this workflow description.

**Recommended daily alternative:** use
[VS Code Remote - SSH](https://code.visualstudio.com/docs/remote/ssh).
The editor UI stays on the current laptop; files, terminals, builds and debug
tools operate on the HP. Save -> build/test avoids a GitHub round trip for each
small edit. GitHub remains version control/checkpoint backup and collaboration,
not a mandatory transport for every experimental change. This is a recommendation,
not a replacement of the learner's local-edit preference without agreement.

Keep one authoritative working copy per experiment. If alternating local editing
and remote editing, synchronize committed work deliberately instead of editing
two copies independently. Keep build outputs, Yocto `tmp`, downloads, sstate
caches, toolchains, secrets and private/employer artifacts out of GitHub.
Repository visibility and content must be reviewed before publication; private
hosting is not permission to upload employer code.

Use SSH on a trusted network, with appropriate credentials and host-key checks.
Do not expose the laptop to the public Internet as a routine setup step.
Remote-SSH installs a VS Code server component; installs/access remain subject
to permission. Ubuntu partitioning, disk cleanup, sleep/power settings and
USB/debug permissions also require deliberate setup, not automatic changes.

### HP Readiness Checklist

Start with the [SSH-first setup and package review](hp_linux_ssh_setup.md).
The latest supplied proposal describes 16 GB RAM and a 220 GB SSD; these are
reported/proposed specifications, not tool-verified values or 220 GB free space.
No HP package or SSH command has been executed by the tutor.

- Confirm exact CPU architecture, RAM, free SSD space and Linux filesystem.
- Confirm installed OS/version, network/SSH access and an ordinary build user.
- Pin the chosen native toolchain or container distribution and required packages.
- Select a compatible Yocto release/BSP and verify its actual resource needs.
- Verify a small build, a disposable QEMU guest and source-level debugging.
- Confirm hardware recovery and USB/serial probe access before flashing.

Do not load experimental kernel modules into the HP's own host kernel. Use a
disposable QEMU guest or approved learning board; a build container is not an
independent kernel-testing machine.

## Approved Podman Workflow

This remains the existing-laptop fallback while the planned HP host is not ready.
Earlier on 2026-10-23 the learner clarified: **use Podman commands; the existing WSL2
backend is acceptable, but do not create a separate WSL distribution, shell
workflow or independent WSL tool installation.** Use PowerShell to manage
Podman and run the selected Linux tools/QEMU inside approved containers.

```text
PowerShell -> Podman -> existing Linux machine (WSL2 backend)
                         -> pinned development container -> QEMU guest
                         -> supported Yocto build container + Linux filesystem
```

For this fallback, the Windows backend is an implementation detail, not a second learning
environment. No backend migration or resource change is authorized. A suitable
container image, module toolchain and QEMU/Zephyr target still need validation.
The presence of `localhost/xv6-lab:latest` and Ubuntu 24.04 images does not prove
they contain the required Arm/Yocto tools or release-compatible packages.

Native Windows Arm QEMU is already installed, but it is not the default workflow.
Do not download another copy just because a board uses Arm. QEMU's executable,
CPU model, board/machine model and guest image are separate choices:

- `qemu-system-arm`: candidate for 32-bit Arm Linux and Cortex-M models.
- `qemu-system-aarch64`: candidate for Arm64 comparison guests, not an F446 or
  BBB board emulator.
- The inspected native build offers `virt`, `mps2-an386` (Cortex-M4), and other
  STM32-related machines. Sharing a processor family does not make their
  peripherals or memory maps identical to Nucleo-F446RE or BBB.
- Choose a machine supported by the pinned Zephyr/Yocto configuration. Do not
  assume the native Windows machine list matches a container's QEMU build.

## Resource Observation: 2026-10-23

Read-only tool inspection; available memory and disk are point-in-time values.

| Surface | Observed value | Interpretation |
| --- | --- | --- |
| Windows host | Windows 11 Enterprise; 31.69 GiB physical RAM; 8 logical processors | Consistent with learner-reported 32 GB |
| Windows available RAM | 7.97 GiB at inspection | Current workload matters; not all installed RAM is free |
| C: free space | 102.13 GiB | Consistent with approximately 100 GB reported; limited build headroom |
| Podman machine metadata | Running; provider `wsl`; 4 CPUs; memory field 2 GiB; disk field 100 GiB | Configuration metadata is not proof of effective guest limits |
| Running Podman guest (`free -b`) | 15.47 GiB total RAM; 14.85 GiB available; 4 GiB swap | Corrects the assumption that the metadata's 2 GiB is the running memory ceiling |
| Guest root filesystem (`df -B1 /`) | Approximately 1.08 TB virtual capacity; 1.016 TB reported available | Virtual capacity is not physical host free space; check backing storage |
| Native Arm/AArch64 QEMU | Both executables respond with version 9.1.1 | Availability/version only; no guest boot or container-tool check performed |

Do not add Windows and guest available RAM together: they are not independent
physical pools. No per-container memory limit or build-volume backing location
has been established. Verify the selected container and filesystem before a
large build, and monitor host memory pressure.

## Planned Storage And Build-Host Choice

On 2026-10-23 the learner reported that free disk space can be increased to
approximately **200 GB** by personally removing unwanted files. This is a
planned capacity, not a new measurement. The last observed C: free space
remains 102.13 GiB. No cleanup, deletion or storage move is authorized here.
Recheck physical free space and the build volume's backing location afterward.

With the observed 32 GB-class host, roughly 15.47 GiB visible to the Podman
guest and approximately 200 GB genuinely available to its build storage,
the existing laptop is a reasonable candidate for a minimal Yocto QEMU/BBB
workflow. This is a feasibility assessment, not a passed build gate.
Start with one image and conservative parallelism; multiple machine builds,
SDKs and retained caches can exceed that storage allowance. Preserve host
headroom and monitor usage rather than treating 200 GB as an unlimited budget.

**Fallback assessment, before HP readiness:** keep the existing Windows/Podman
workflow available. The HP plan above is now the proposed primary builder.
Validate the
supported Linux container, permissions, dependencies and persistent
Linux-filesystem build volume before starting. Do not put the main build tree
on a Windows shared/bind-mounted filesystem merely because it has free space.
Its actual filesystem semantics and performance matter.

A MacBook Air M3 running macOS is **not a supported native Yocto build host**.
Unix ancestry does not supply Linux kernel behavior or the supported Linux
distribution/toolchain environment. The
[versioned system requirements](https://docs.yoctoproject.org/5.0.14/ref-manual/system-requirements.html)
list supported Linux distributions; the quick-build guide directs non-Linux
hosts toward a Linux container environment.

The Mac could instead host a Linux VM, with builds running directly inside
that VM (a container inside it is optional), or act as a client to a remote
Linux builder. On Apple Silicon, verify the chosen release/layers/native-tool
support for an AArch64 Linux build host; x86-only tools may add emulation or
compatibility work. Mac RAM, disk and VM readiness have not been inspected.
Do not switch machines merely because macOS is Unix-based, and do not install
a VM or change the approved workflow without agreement.

## Linux And Yocto Gates

1. Start with a pinned, documented QEMU Linux image/toolchain and inspect its
   kernel/configuration. Establish serial boot, recovery, module compatibility
   and source-level debugging before a driver lesson.
2. Check resources on the selected builder: HP inventory is pending; the
   observation above belongs to the existing Windows/Podman host. Recheck
   container limits if used and the actual Linux build volume. Do not infer
   a 2 GB running limit from machine metadata alone.
3. Use a supported Linux build host. The versioned
   [Yocto 5.0.14 quick build](https://docs.yoctoproject.org/5.0.14/brief-yoctoprojectqs/index.html)
   specifies at least 90 GB free disk and 8 GB RAM. The observed guest RAM is
   above that minimum. The last observed approximately 100 GB host free space
   offers modest headroom; the learner's planned approximately 200 GB would
   improve feasibility after rechecking it. Neither figure guarantees space
   for multiple images, SDKs and retained build trees.
   These are 5.0.14-specific figures for the existing-host assessment, not a
   claim about the HP or Yocto 6.0. Plan one minimal image first, conservative
   parallelism and monitored disk growth. Verify the selected release's supported
   Linux distribution natively or inside Podman, and Linux-filesystem build
   storage. Do not start a separate WSL
   workflow, assume Podman's virtual disk is physically free, or move/delete
   existing data to make room without approval.
4. Select a supported release and pin all layers together. Scarthgap remains a
   researched candidate; 6.0 is an alternative for native Ubuntu 26.04, pending
   BSP/layer/resource checks. Neither is an installed or verified build here. The Scarthgap
   [BBB reference machine](https://github.com/yoctoproject/poky/blob/scarthgap/meta-yocto-bsp/conf/machine/beaglebone-yocto.conf)
   provides a starting point; validate needed driver support and boot media.
   Do not mix rolling documentation commands with an older release blindly.
5. Build a minimal QEMU image, a small custom layer and SDK/module workflow.
   Record kernel source/config, toolchain and image identifiers. Verify that a
   small source change reaches the running image.
6. Port the configuration/recipes to BBB; do not assume the same binary image
   works unchanged. First boot from approved removable media; do not overwrite
   eMMC, bootloaders or existing files without explicit permission.

Use incremental builds and SDK/devtool iteration. Reproducible instructions are
not proof of byte-for-byte reproducibility: record what was actually compared.
If setup stalls for one two-hour block, record the blocker and continue with an
already working disposable guest/model. Do not declare the Yocto gate passed.

## Before Any Experiment

Record exact command, target, revision, configuration, expected output and a
bounded timeout/stop procedure. Ask before builds/runs of learner work, installs,
flashes, host-resource changes or destructive experiments. All scripts must
report failures and propagate nonzero status; no unconditional PASS banners.
Use LF for shell/Make/assembly files created on Windows; `.gitattributes` alone
does not correct newly typed CRLF files.

Original migration records and earlier toolchain smoke evidence remain in the
[foundation snapshot](../backup/plan/2026-10-22-foundation/snapshot/docs/workspace_setup.md).
Those records do not verify current Zephyr, Yocto or BBB readiness.
