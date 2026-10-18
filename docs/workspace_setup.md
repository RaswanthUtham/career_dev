# Workspace Setup And Migration

Start here, then follow the [weekly curriculum](../plans/five_month_development_roadmap.md), [kernel milestones](../plans/scratch_riscv_os_plan.md) and [training tracker](../plans/training_tracker.md). Initial setup used a six-hour allocation inside historical week 1, not a measured study duration. Reuse C/C++ and xv6 readiness; the old Zephyr/Linux guest week numbers are historical targets pending the kernel-foundation rebaseline. Proposed commands are not passed verification gates.

## Hardware And Setup Policy

As of 2026-09-23 the learner reports STM32F446RE with onboard ST-LINK and BeagleBone Black. Confirm the STM32 board silkscreen/revision (expected NUCLEO-F446RE), power, debug connection and pin mapping before flashing or wiring. ESP32/Pico/Zero variants and external sensors remain unconfirmed and are not prerequisites. Use free official materials first, with no automatic purchases or paid enrollment.

From week 3, setup fits the confirmed 22-hour weekly budget. If physical setup stalls after one 2-hour block, switch to a supported Zephyr QEMU/native_sim target or disposable Linux QEMU guest, and log the hardware gate for later. native_sim requires Linux and is not STM32 peripheral emulation. Simulated logic does not verify bus timing, electrical safety, real DMA/cache behavior or power. Check container resources before heavier Linux builds; no source, remote or existing worktree replacement is required.

## Repository Boundaries

Use the selected parent `C:/workspace/career_ws` with **no parent Git repository**:

```text
career_ws/
  career_dev/                  original learning repository
    .github/skills/
    docs/
    plans/
    notes/
    career_dev.code-workspace
  xv6-riscv/                   independent fork clone
  linux/                       independent selected kernel fork clone
  my-linux/                    learner-created teaching kernel (not yet present)
  zephyr-workspace/            west topdir, no .git here
    .west/                     pending initialization
    zephyr/                    fork used as manifest repository
    modules/                   pending west update
```

Machine-specific migration origin: `C:/workspace/Career_Workspace/carreer_ws`. Teaching and workspace references are relative. Both the portable workspace and `C:/workspace/Career_Dev.code-workspace` now contain the four new roots above. Employer folders are excluded from this learning workspace but retained on disk. Do not put employer repositories, nested OS repositories or submodules in `career_dev`. Each source repository is committed/pushed separately; pushing `career_dev` does not upload sibling work.

The old `riscv-kernel` currently remains at `C:/workspace/career_ws/riscv-kernel`. The learner will create the new `my-linux` root, then preserve old source inside its excluded `reference/` before adding the new root to the VS Code workspace. This has not happened yet; do not treat a proposed rename or move as verified.

## Initial Inventory: 2026-09-22

Historical pre-migration observations; the verified current state is recorded below.

| Item | Observed status |
| --- | --- |
| Original learning folder | No Git repository detected |
| Original xv6 | Commit prefix `2534832`; modified Makefile and untracked utilities, container recipe and skill |
| Original Linux | Stable-kernel upstream; many staged deletions, left untouched, not used as a clean source |
| Original Zephyr | Clean worktree at inspection, commit prefix `468eb5`; build readiness not established |
| Host | Git 2.39.2.windows.1, Podman 5.8.1, MSYS2 UCRT64 GCC/G++ 14.2.0, CMake available; `gh`/`clang` not on PATH |
| Resources | About 31.7 GiB RAM and 107.3 GiB free on C: at inspection |
| Podman | Existing WSL-backed machine running; xv6-lab and xv6-dev images present |
| Public career_dev/xv6-riscv/zephyr lookups under RaswanthUtham | HTTP 404 without authentication: absent or private, not confirmed accessible |
| Public RaswanthUtham/linux | Exists, but parent is beagleboard/linux, NOT torvalds/linux |

The initial learning/xv6 recovery copy verified 326 files. Before the later disk cleanup, fresh full copies of all four old folders, including Git metadata, staged state, untracked files and generated outputs, were saved under `%LOCALAPPDATA%/CareerLearningBackups/cleanup-20260922-184607`. **114,383 files matched by SHA-256**, with per-folder CSV manifests; originals and copies were checked again immediately before deletion. Earlier backups remain intact. Keep these recovery copies private; a backup on the same disk does not protect against disk failure.

## Current Clones And Cleanup

Local Git configuration and revisions verified on 2026-09-22:

| Root | Origin under RaswanthUtham | Upstream | Revision/status |
| --- | --- | --- | --- |
| career_dev | `career_dev.git` | None | `main`, no commits; SSH origin configured |
| xv6-riscv | `xv6-riscv.git` | `mit-pdos/xv6-riscv.git` | `9e3161a`, branch `riscv`; migrated sources and Makefile changes |
| linux | `linux-mainline.git` | `torvalds/linux.git` | `f0100363d`, branch `master`; 13 pre-existing modified files preserved |
| zephyr-workspace/zephyr | `zephyr.git` | `zephyrproject-rtos/zephyr.git` | `55f132b58`, branch `main`; clean at inspection |

All configured remotes use GitHub SSH URLs. The separate BeagleBoard fork `RaswanthUtham/linux` was not changed. Local configuration is not a fresh authentication/push test.

The old Zephyr folder was deleted. All contents of the approved old xv6, Linux and learning folders were removed after recovery verification, but Windows process locks left these **empty directory shells**: `C:/workspace/xv6-riscv`, `C:/workspace/Career_Workspace/linux`, and `C:/workspace/Career_Workspace/carreer_ws`. Close terminals/windows holding those paths, then remove only those empty folders. Both workspace configurations already exclude them. Employer directories `tle9018` and `ipd_ref_cdd`, recovery copies and user-profile skills remain untouched.

Six xv6 sources (`sleep`, `pingpong`, `find`, `xargs`, `fdtest`, `linktest`) and the container recipe were migrated. The only source adaptation removed three unused static helpers from `xargs.c` to satisfy `-Werror`; six program registrations were merged into the new Makefile. Incomplete `find_u.c`/`pingpon.c` variants and the old repository-local teaching skill remain in private recovery, not active source. The five portable `learning-*` skills remain canonical.

## GitHub Prerequisites

1. Sign in through GitHub's UI or credential manager. Never put secrets in commands, remote URLs, notes or chat.
2. Create an empty original repository **RaswanthUtham/career_dev**, preferably private during review. If it exists privately, confirm access instead. Do not overwrite an initialized remote's history.
3. Fork [MIT xv6](https://github.com/mit-pdos/xv6-riscv/fork) and [Zephyr](https://github.com/zephyrproject-rtos/zephyr/fork) to your account; confirm existing private forks before duplicating.
4. Resolve the Linux choice: preserve the existing BeagleBoard fork for compatible board work, or create a separate [mainline fork](https://github.com/torvalds/linux/fork), for example `linux-mainline` if GitHub permits it. Do not rename/delete the existing fork. Confirm the board/guest and actual branch before choosing.
5. Record the actual parent, clone URL and selected revision. Pin compiler/container/SDK versions with the source. The default branch is not automatically a suitable learning version.

The user subsequently supplied the clones listed above; these prerequisite steps are retained for future setup, not current cloning blockers. No remote creation, commit or push was performed by the assistant. A public 404 is not proof that a private repository does not exist. Never store credentials in scripts.

## Cloning And West

After the remotes exist, run these PowerShell commands from the `career_ws` parent, only when the target directory does not exist:

```powershell
git clone https://github.com/RaswanthUtham/xv6-riscv.git xv6-riscv
git -C xv6-riscv remote add upstream https://github.com/mit-pdos/xv6-riscv.git
git -C xv6-riscv remote -v
```

For Linux, clone the selected fork/branch into local `linux`, using partial/single-branch history when suitable. Set `upstream` to its actual parent. Mainline does not automatically include stable patch-series or board-vendor history. Never overwrite dirty old worktrees. Use `origin` for user-fork pushes; keep original remotes unchanged.

For the existing Zephyr clone, select a supported revision and its compatible Python environment, `west`, dependencies and SDK first. From the parent `career_ws`, the pending local-manifest initialization is:

```text
cd zephyr-workspace
west init -l zephyr
west update
west zephyr-export
```

Use the selected release's installation instructions; newer setup commands may not exist in older versions. Fetch needed module groups where practical. `west` controls module `manifest-rev` branches; do not put personal work there. Keep original application code in `career_dev`, outside the Zephyr source. The west topdir itself must not be a Git repository.

The xv6 learning changes have been migrated and smoke-tested as recorded below. The private backup retains their original worktree and history.

## Build Gates

| When | Required evidence |
| --- | --- |
| Week 1 | C17 and C++17 assertion probes compile/link/run with warnings; finding compilers alone is insufficient |
| Before week 3 | New xv6 clone builds, reaches a QEMU shell and passes relevant utility/regression checks |
| During week 6, before week 7 labs | Matching Zephyr release/modules/SDK builds and runs a supported simulator/native probe; flash only a confirmed board |
| Weeks 10-11 | Disposable Linux guest/spare BBB boots; image, running kernel/config and recovery recorded |
| Before week 12 module execution | Module headers/config/toolchain match the guest/board running kernel; build/load/unload demonstrated there |

Read the chosen xv6 Makefile/test harness before selecting targets. Public xv6 is not automatically a MIT lab supporting `make grade`. Reuse the existing Ubuntu 24.04/RISC-V GCC/QEMU recipe only after checking the new checkout's requirements.

The existing Podman machine reported four CPUs and about 2 GiB configured memory, despite the host's larger RAM. Check actual guest resources before heavy builds; use conservative parallelism. Choose a known kernel/rootfs guest recipe and record its configuration. A container shares its host kernel: do not load experimental modules into it as if it were an independent guest. No guessed board IDs, wiring or voltage assumptions; confirm recovery before flashing.

## Content And Retirement

- Preserve unique progress in the learning log before deleting old schedules. Historical completion is not current mastery verification.
- Employer code, protocols, test vectors and detailed review findings remain outside public learning material. Use original invented formats or public device specifications in labs.
- Downloaded book pages and machine-specific command notes remain only in private recovery until license/portability review. Prefer the [official xv6 book](https://mit-pdos.github.io/xv6-riscv-book/) link. Review original notes and references before publishing too.
- Ignore generated/private files, but remember `.gitignore` does not sanitize existing history. Inspect staged/tracked files and all history before public publication.
- Both workspace files now select the new roots. Fresh-chat skill discovery still requires an editor check; reload the window if roots/customizations have not refreshed. Retain employer directories and verified recovery copies; old source retirement was explicitly authorized.
- After content/visibility approval, commit and push each repository separately. No commits or pushes have been performed by this migration preparation.

## Verification Record

On 2026-09-22 the curated learning content was copied into `C:/workspace/career_ws/career_dev`, with 16 files verified against the updated original using SHA-256. A separate local Git repository was initialized on `main`, initially without a remote; the user later configured the SSH origin above. The parent has no `.git`. Continue edits in `career_dev`; old content is preserved in private recovery, not a second active curriculum.

C17 atomic-operation and C++17 RAII assertion probes compiled, linked and exited successfully using GCC/G++ 14.2.0 with `-Wall -Wextra -Werror -pedantic`. Generated outputs were placed in a unique temporary directory, outside publication trees. These are single-thread toolchain checks, not proof of concurrent correctness or sanitizer availability. No learner exercise completion is inferred.

The new xv6 kernel and filesystem built successfully using `localhost/xv6-lab:latest`. A bounded QEMU run with one CPU reached the shell and passed smoke checks for `sleep 1`, `pingpong`, `find . README`, `echo alpha beta | xargs echo migrated`, `fdtest`, and `linktest`. The linker reported RWX-segment warnings for the kernel and upstream `forktest`; compilation succeeded. These are migration smoke checks, not exhaustive correctness tests or new learner-mastery claims.

Both workspace JSON files parse and resolve to the four existing new roots. The 13 pre-existing new-Linux file contents were hash-checked unchanged before cleanup. Zephyr west/modules/SDK setup, Linux guest/module build and execution, and fresh-chat skill discovery remain unverified. Nothing was committed or pushed by the assistant.

## References

- [GitHub fork/clone workflow](https://docs.github.com/en/get-started/quickstart/fork-a-repo)
- [Zephyr workspaces](https://docs.zephyrproject.org/latest/develop/west/workspaces.html) and [getting started](https://docs.zephyrproject.org/latest/develop/getting_started/index.html), matched to the selected release
- [Linux build documentation](https://docs.kernel.org/kbuild/) and [Bootlin kernel labs](https://bootlin.com/training/kernel/)