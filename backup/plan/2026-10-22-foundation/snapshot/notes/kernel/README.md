# my-linux v2 Lesson Notes

Lessons for the [kernel milestone guide](../../plans/scratch_riscv_os_plan.md). Each note starts with its verification status, gives complete reference code with target paths, and ends with deviations, interview answers and corrections. The learner types all code in `my-linux` on branch `v2`.

## Kernel Series

| Milestone | Note | Status |
| --- | --- | --- |
| K0 | [Build, toolchain and linker contract](K00_build_and_linker_contract.md) | Published; learner tree passes (2026-10-22) |
| K1a | [Firmware and boot contract (my-sbi, dual boot)](K01a_firmware_and_boot_contract.md) | Published; learner tree done and committed |
| K1b-1 | [Talking to the firmware (`sbi_ecall`)](K01b1_talk_to_firmware_sbi_ecall.md) | Published, tutor-verified 2026-10-22 |
| K1b-2 | printk, `vsnprintf` and the early console | Written after K1b-1 |
| K1b-3 | SBI discovery, power off and `make test` | Written after K1b-2 |
| K2 | Traps and fatal diagnostics | Not written |
| K3 | Early MMU and virtual entry | Not written |
| K4 | `setup_arch`: device tree, memblock, final page tables | Not written |
| K5 | Buddy allocator and SLUB | Not written |
| K6 | Time, timer interrupt and softirq | Not written |
| K7 | IRQ core, PLIC and UART | Not written |
| K8 | Tasks, scheduling and locking | Not written |
| K9 | User space and shell (optional) | Not written |
| K10 | Arm64 transfer | Not written |

## Background Notes Still Valid

These describe hardware and concepts independent of the v1 code layout:

- [00 RISC-V assembly](../00_riscV_asm.md)
- [01 Hardware reset and boot flow](../01_riscv_hardware_reset_and_boot_flow.md)
- [02 S-mode boot and early console](../02_riscv_smode_boot_and_early_console.md)
- [03 Device tree discovery](../03_devicetree_discovery_and_role_boundaries.md)
- [04 Supervisor trap architecture](../04_supervisor_trap_architecture_and_frame.md)
- [Sv39 virtual and physical addresses](../sv39_virtual_and_physical_addresses.md)
- [Virtual memory and page tables deep dive](../virtual_memory_and_page_tables_deep_dive.md)
- [Arm32 and Arm64 virtual memory deep dive](../arm32_and_arm64_virtual_memory_deep_dive.md)

## Superseded v1 Notes (kept unchanged)

These were written for v1 and are replaced by the K series. Keep them for history only. Their code and layout advice does not apply to v2.

- [05 Physical memory allocator](../05_physical_memory_allocator_linux_way.md): replaced by K4/K5.
- [06 Slab, kmalloc and high-half relocation](../06_slab_kmalloc_and_high_half_relocation.md): replaced by K3/K5.
- [07 Timekeeping, clocks and timers](../07_timekeeping_clocks_and_timers_linux_way.md): replaced by K6.
