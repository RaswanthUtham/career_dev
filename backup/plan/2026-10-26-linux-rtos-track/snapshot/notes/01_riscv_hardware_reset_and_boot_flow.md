# RISC-V Hardware Reset, Mask ROM, and Boot Flow

## 1. The Hardware Reset Vector ($0\text{x}1000$ in QEMU vs Physical Silicon)

### Who Hardcodes the Reset Vector?
In computer architecture, the CPU reset vector is the physical memory address forced into the Program Counter ($\text{PC}$) upon electrical power-on or hardware reset pin toggle.

- **On Physical Silicon (ASIC / SoC)**: The chip design team and semiconductor foundry (e.g., SiFive, Allwinner, STMicroelectronics, TSMC) physically etch this into the silicon wafer during photolithography as **Mask ROM (MROM)**. It is metal-programmed and immutable; no software, JTAG command, or operating system instruction can overwrite it.
- **In QEMU Virt**: The QEMU developers implement this behavior in C within `hw/riscv/boot.c` (`riscv_setup_rom_reset_vec()`). QEMU creates a read-only memory region at base address $0\text{x}1000$ (`VIRT_MROM`) and writes an 8-instruction machine code sequence before vCPU execution begins.

### The 8-Instruction Stub at $0\text{x}1000$ in QEMU
```assembly
# Physical Memory Location: 0x1000 (QEMU Virt Mask ROM)
# Hardware State at Reset: Mode = Machine (M-mode), Interrupts Disabled, PC = 0x1000

0x1000:  auipc  t0, 0           # t0 = PC + 0 = 0x1000 (anchor base)
0x1004:  addi   a2, t0, 32      # a2 = pointer to dynamic info table for OpenSBI
0x1008:  csrr   a0, mhartid     # Read Hardware Thread ID into register a0 (e.g. 0)
0x100C:  ld     a1, 24(t0)      # a1 = Load 64-bit DTB physical address from 0x1018
0x1010:  ld     t0, 16(t0)      # t0 = Load 64-bit Kernel/Firmware start address from 0x1010
0x1014:  jr     t0              # Jump to target address in register t0 (0x80000000)

# Embedded Pointer Table (immediately following instructions):
0x1018:  .dword 0x80000000      # Target start address in DRAM
0x1020:  .dword 0x87e00000      # Target DTB location in DRAM (top of RAM)
```

### Reset Address Differences Across Silicon Platforms
The RISC-V Privileged Architecture Specification intentionally leaves the reset vector **implementation-defined**:
- **QEMU Virt**: $0\text{x}1000$
- **SiFive Freedom U540 / U740**: $0\text{x}10040$ / $0\text{x}10000$
- **Allwinner D1 (T-Head C906)**: $0\text{x}00000000$ / $0\text{x}00020000$
- **Kendryte K210**: $0\text{x}88000000$
- **STM32F446RE (Arm Cortex-M4)**: $0\text{x}00000000$ (aliased to flash $0\text{x}08000000$)
- **BeagleBone Black (Arm Cortex-A8)**: $0\text{x}40020000$

---

## 2. Real Silicon Reality: The "DRAM Dilemma" and Boot Stages

### Why Can't Real Silicon Jump Straight to DRAM?
In QEMU, virtual DRAM at $0\text{x}80000000$ is simulated via host memory allocation (`malloc()`) and is usable instantly.
On physical silicon:
1. **DDR4/LPDDR4 DRAM is dead at reset**: High-speed DRAM requires clock generation (Phase-Locked Loops - PLLs), voltage stabilization, calibration, and row/column refresh training. Not a single byte can be read or written to DRAM at power-on.
2. **External Flash is unconfigured**: The CPU does not inherently know whether code is stored on SPI NOR flash, an SD card, eMMC, or USB.
3. **Internal SRAM is tiny**: The silicon die contains only a small block of on-chip Static RAM (SRAM), typically $32\text{ KB}$ to $128\text{ KB}$, which is functional immediately without calibration.

### The Five Boot Stages

```text
+-------------------------------------------------------------------------+
| STAGE 0: Mask ROM / ZSBL (Etched into Silicon Wafer at e.g. 0x1000)     |
|   - CPU Reset Pin goes HIGH -> PC jumps to Reset Vector in M-mode       |
|   - DRAM is completely dead / uncalibrated                              |
|   - Reads boot pins; copies SPL from SD Card / SPI Flash into SRAM      |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| STAGE 1: SPL / FSBL (Runs inside tiny 32-128 KB On-Chip Internal SRAM)  |
|   - Initializes CPU Clocks, PLLs, and DDR DRAM Controller               |
|   - External DDR DRAM (0x80000000) is now ALIVE!                        |
|   - Copies OpenSBI (0x80000000) and Kernel (0x80200000) into DRAM       |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| STAGE 2: OpenSBI Firmware (Runs in DRAM at 0x80000000 in M-mode)        |
|   - Configures Physical Memory Protection (PMP)                         |
|   - Delegates traps/interrupts to S-mode (medeleg, mideleg)             |
|   - Sets register a0 = hartid, register a1 = DTB physical address       |
|   - Executes 'mret' to drop privilege to S-mode at 0x80200000           |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| STAGE 3: my-linux Kernel (Runs in DRAM at 0x80200000 in S-mode)         |
|   - Masks interrupts, sets up stack, zeroes BSS                         |
|   - Initializes Page Allocator, Sv39 MMU, Traps, and Drivers            |
|   - Executes 'sret' to drop privilege to U-mode                         |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| STAGE 4: User Space Shell / Applications (Runs in U-mode)               |
|   - Interactive command shell (usr/bin/sh.c) talking via syscalls       |
+-------------------------------------------------------------------------+
```

---

## 3. What is SPL (Secondary Program Loader)?

### The Size Constraint
A full production bootloader like U-Boot is $1\text{ MB} - 4\text{ MB}$ because it includes filesystems (ext4, FAT), networking (DHCP, TFTP), USB stacks, and interactive command shells. A $2\text{ MB}$ U-Boot binary physically cannot fit into a $64\text{ KB}$ internal SRAM.

### The Solution: Two-Stage Bootloader
U-Boot is compiled into two separate binaries:
1. **`u-boot-spl.bin` (SPL / FSBL / MLO)**: Lightweight binary ($30\text{ KB} - 60\text{ KB}$) that fits inside internal SRAM. Its primary responsibilities:
   - Configure CPU clocks and PLLs.
   - Initialize and calibrate the DDR DRAM controller.
   - Read storage media (SD card, eMMC, SPI flash) and copy OpenSBI, U-Boot, or the Kernel into DRAM.
2. **`u-boot.bin` (Full U-Boot)**: Full feature-rich bootloader that executes from DRAM only after DRAM has been trained.

---

## 4. xv6 Direct Boot vs OpenSBI Standard S-Mode Boot Contract

| Attribute | xv6 Bare-Metal (`-bios none`) | OpenSBI + Linux ABI (`my-linux`) |
| :--- | :--- | :--- |
| **Initial Hardware Mode** | Machine Mode (M-mode) | Supervisor Mode (S-mode) |
| **How Hart ID is Obtained** | `csrr reg, mhartid` (Machine CSR) | Passed by firmware in register `a0` |
| **Hardware Discovery** | Hardcoded literals (`memlayout.h`) | Passed by firmware in register `a1` (DTB pointer) |
| **PMP & Trap Delegation** | Hand-rolled in kernel C code | Pre-configured by OpenSBI firmware |
| **Privilege Transition** | Explicit `mret` in C via inline assembly | Completed by firmware before reaching `_start` |
| **Communication Mechanism** | Direct CSR access | SBI runtime calls (`ecall` with extension ID in `a7`) |
