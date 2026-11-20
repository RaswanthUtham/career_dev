# Master Technical Deep Dive: Virtual Memory & Page Tables in ARM32 (ARMv7-A) and ARM64 (AArch64)

---

## Table of Contents
1. [Executive Comparison: RISC-V SV39 vs. ARM32 (VMSAv7) vs. ARM64 (VMSAv8-A)](#1-executive-comparison-risc-v-sv39-vs-arm32-vmsav7-vs-arm64-vmsav8-a)
2. [ARM32 (ARMv7-A / BeagleBone Black AM335x): 2-Level Paging](#2-arm32-armv7-a--beaglebone-black-am335x-2-level-paging)
3. [ARM64 (AArch64 / NVIDIA Tegra & Qualcomm Snapdragon): 4-Level Paging & Split TTBR](#3-arm64-aarch64--nvidia-tegra--qualcomm-snapdragon-4-level-paging--split-ttbr)
4. [Standalone C Simulation for ARM32 & ARM64 Address Translation](#4-standalone-c-simulation-for-arm32--arm64-address-translation)
5. [The Split TTBR Architecture: Kernel Space (`TTBR1`) vs. User Space (`TTBR0`)](#5-the-split-ttbr-architecture-kernel-space-ttbr1-vs-user-space-ttbr0)
6. [Line-by-Line Mapping to Linux Kernel Source (`arch/arm` and `arch/arm64`)](#6-line-by-line-mapping-to-linux-kernel-source-archarm-and-archarm64)
7. [Staff-Level Interview Deep-Dive Q&A (ARM Memory Subsystem)](#7-staff-level-interview-deep-dive-qa-arm-memory-subsystem)

---

## 1. Executive Comparison: RISC-V SV39 vs. ARM32 vs. ARM64

| Architecture | Virtual Address Width | Standard Page Size | Levels of Paging | Root Register | Page Table Entry (PTE) Size | Key Silicon Feature |
| :--- | :---: | :---: | :---: | :---: | :---: | :--- |
| **RISC-V (SV39)** | 39 bits | 4 KB | 3 Levels (L2 $\rightarrow$ L1 $\rightarrow$ L0) | `satp` | 8 bytes (64-bit) | Single root pointer for both user & kernel (trampoline needed). |
| **ARM32 (ARMv7-A Short-descriptor)** | 32 bits | 4 KB | 2 Levels (L1 $\rightarrow$ L2) | `TTBR0` / `TTBR1` (via `TTBCR`) | 4 bytes (32-bit) | Hardware supports Section Mappings (1 MB / 16 MB supersections without L2 tables). |
| **ARM64 (AArch64 4KB Granule)** | 48 bits (or 52 bits) | 4 KB (or 16KB / 64KB) | 4 Levels (L0 $\rightarrow$ L1 $\rightarrow$ L2 $\rightarrow$ L3) | `TTBR0_EL1` (User) & `TTBR1_EL1` (Kernel) | 8 bytes (64-bit) | **Hardware split address space**: Zero TLB flushes on user/kernel context switches. |

---

## 2. ARM32 (ARMv7-A / BeagleBone Black AM335x): 2-Level Paging

In 32-bit ARM Cortex-A processors (such as the TI Sitara AM3358 on BeagleBone Black), the MMU implements the **Virtual Memory System Architecture version 7 (VMSAv7)**.

```
                              32-bit Virtual Address (VA)
            ┌────────────────────────────┬───────────────────┬───────────────────┐
            │   Level 1 Index (12 bits)  │ Level 2 (8 bits)  │ Offset (12 bits)  │
            │   Bits 31 .. 20            │ Bits 19 .. 12     │ Bits 11 .. 0      │
            └────────────────────────────┴───────────────────┴───────────────────┘
                     4096 entries             256 entries         4096 bytes
```

### The Slicing Breakdown:
- **Level 1 (First-Level Table / Page Global Directory - PGD):**
  - Uses top 12 bits (`va >> 20 & 0xFFF`) $\rightarrow$ $2^{12} = \mathbf{4096 \text{ entries}}$.
  - Each entry is 4 bytes (32-bit).
  - Total Level 1 Table Size = $4096 \times 4 \text{ bytes} = \mathbf{16 \text{ KB}}$.
- **Level 2 (Second-Level Table / Page Table Entry - PTE):**
  - Uses middle 8 bits (`va >> 12 & 0xFF`) $\rightarrow$ $2^8 = \mathbf{256 \text{ entries}}$.
  - Each entry is 4 bytes (32-bit).
  - Total Level 2 Table Size = $256 \times 4 \text{ bytes} = \mathbf{1 \text{ KB}}$.
- **Page Offset:**
  - Lowest 12 bits (`va & 0xFFF`) $\rightarrow$ $2^{12} = \mathbf{4096 \text{ bytes}}$ (4 KB).

```
                ARM32 2-LEVEL TRANSLATION FLOW (4KB Pages)
                
  TTBR0 / TTBR1 (Points to 16KB Level-1 Table in Physical RAM)
        │
        │ Uses Bits 31..20 (Index 0..4095)
        ▼
  ┌────────────────────────────────────────────────────────┐
  │ Level-1 Table (16 KB in Physical RAM)                  │
  │   - Entry [0]    : Points to Level-2 Table A           │───┐
  │   - Entry [1]    : Points to Level-2 Table B           │   │
  │   - Entry [4095] : ...                                 │   │
  └────────────────────────────────────────────────────────┘   │
                                                               │ Uses Bits 19..12 (Index 0..255)
                                                               ▼
  ┌────────────────────────────────────────────────────────┐
  │ Level-2 Table (1 KB in Physical RAM)                   │
  │   - Entry [0]   : Points to Physical RAM Page          │───┐
  │   - Entry [255] : ...                                  │   │
  └────────────────────────────────────────────────────────┘   │
                                                               │ Attach 12-bit Offset
                                                               ▼
  ┌────────────────────────────────────────────────────────┐
  │ Physical RAM Page (4 KB)                               │
  │   - Byte at [Offset] holds actual data                 │
  └────────────────────────────────────────────────────────┘
```

---

## 3. ARM64 (AArch64 / NVIDIA Tegra & Qualcomm Snapdragon): 4-Level Paging & Split TTBR

Modern 64-bit ARM processors (ARM Cortex-A53, A72, A78, X-series) use **48-bit Virtual Addressing** with 4 levels of translation tables when configured for a 4 KB granule.

```
                              48-bit Virtual Address (VA)
 47         39 38         30 29         21 20         12 11            0
┌─────────────┬─────────────┬─────────────┬─────────────┬───────────────┐
│ Level 0 (L0)│ Level 1 (L1)│ Level 2 (L2)│ Level 3 (L3)│  Page Offset  │
│  (9 bits)   │  (9 bits)   │  (9 bits)   │  (9 bits)   │   (12 bits)   │
└─────────────┴─────────────┴─────────────┴─────────────┴───────────────┘
  512 entries   512 entries   512 entries   512 entries    4096 bytes
```

### The Slicing Breakdown (4KB Granule):
- **Level 0 (PGD - Page Global Directory):** Bits 47..39 (9 bits) $\rightarrow$ 512 entries ($512 \times 8 \text{ bytes} = 4 \text{ KB}$).
- **Level 1 (PUD - Page Upper Directory):** Bits 38..30 (9 bits) $\rightarrow$ 512 entries (4 KB).
- **Level 2 (PMD - Page Middle Directory):** Bits 29..21 (9 bits) $\rightarrow$ 512 entries (4 KB).
- **Level 3 (PTE - Page Table Entry / Leaf):** Bits 20..12 (9 bits) $\rightarrow$ 512 entries (4 KB).
- **Page Offset:** Bits 11..0 (12 bits) $\rightarrow$ 4096 bytes.

```
                     ARM64 4-LEVEL TRANSLATION TREE
                     
  TTBR0_EL1 (User VA: 0x0000_xxxx) OR TTBR1_EL1 (Kernel VA: 0xFFFF_xxxx)
        │
        ▼
  ┌─────────────┐ (Level 0 Table: 512 entries * 8B = 4KB)
  │ Entry [idx0]│ ──► Physical Address of Level 1 Table
  └─────────────┘
        │
        ▼
  ┌─────────────┐ (Level 1 Table: 512 entries * 8B = 4KB)
  │ Entry [idx1]│ ──► Physical Address of Level 2 Table (or 1GB Block!)
  └─────────────┘
        │
        ▼
  ┌─────────────┐ (Level 2 Table: 512 entries * 8B = 4KB)
  │ Entry [idx2]│ ──► Physical Address of Level 3 Table (or 2MB Block!)
  └─────────────┘
        │
        ▼
  ┌─────────────┐ (Level 3 Leaf Table: 512 entries * 8B = 4KB)
  │ Entry [idx3]│ ──► Physical Address of 4KB Data Page in RAM
  └─────────────┘
        │
        ▼ + 12-bit Offset
  [ TARGET PHYSICAL BYTE IN DDR RAM ]
```

---

## 4. Standalone C Simulation for ARM32 & ARM64 Address Translation

Here is a pure, self-contained C simulation demonstrating both ARM32 and ARM64 bit-slicing, table walking, and physical resolution without external dependencies:

```c
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// ============================================================================
// PART A: ARM32 (VMSAv7) 2-LEVEL PAGING SIMULATION
// ============================================================================

#define ARM32_L1_ENTRIES 4096  // 12-bit index -> 16 KB Table
#define ARM32_L2_ENTRIES 256   // 8-bit index  -> 1 KB Table

typedef struct {
    uint32_t *l1_table; // 16 KB table (4096 * uint32_t)
} arm32_mmu_t;

arm32_mmu_t create_arm32_mmu(void) {
    arm32_mmu_t mmu;
    mmu.l1_table = (uint32_t*)calloc(ARM32_L1_ENTRIES, sizeof(uint32_t));
    return mmu;
}

void arm32_map_page(arm32_mmu_t *mmu, uint32_t va, uint32_t pa) {
    uint32_t l1_idx = (va >> 20) & 0xFFF; // Top 12 bits
    uint32_t l2_idx = (va >> 12) & 0xFF;  // Middle 8 bits

    // Check if Level-2 Table exists in L1 entry (Bit 0 indicates Page Table descriptor)
    if ((mmu->l1_table[l1_idx] & 0x1) == 0) {
        uint32_t *l2_table = (uint32_t*)calloc(ARM32_L2_ENTRIES, sizeof(uint32_t));
        // Store physical address of L2 table with descriptor type bits
        mmu->l1_table[l1_idx] = ((uint32_t)(uintptr_t)l2_table) | 0x1;
        printf("[ARM32 MAP] Allocated L2 Table at %p for L1 Index %u\n", (void*)l2_table, l1_idx);
    }

    uint32_t *l2_table = (uint32_t*)(uintptr_t)(mmu->l1_table[l1_idx] & ~0x3FF);
    // Write Physical Page Base (top 20 bits) + Valid Small Page Descriptor (bit 1)
    l2_table[l2_idx] = (pa & 0xFFFFF000) | 0x2;
    printf("[ARM32 MAP] Mapped VA 0x%08X -> PA 0x%08X (L1_idx=%u, L2_idx=%u)\n", 
           va, pa, l1_idx, l2_idx);
}

uint32_t arm32_translate(arm32_mmu_t *mmu, uint32_t va) {
    uint32_t l1_idx = (va >> 20) & 0xFFF;
    uint32_t l2_idx = (va >> 12) & 0xFF;
    uint32_t offset = va & 0xFFF;

    if ((mmu->l1_table[l1_idx] & 0x1) == 0) return 0; // Fault

    uint32_t *l2_table = (uint32_t*)(uintptr_t)(mmu->l1_table[l1_idx] & ~0x3FF);
    if ((l2_table[l2_idx] & 0x2) == 0) return 0; // Fault

    uint32_t physical_page = l2_table[l2_idx] & 0xFFFFF000;
    return physical_page | offset;
}

// ============================================================================
// PART B: ARM64 (AArch64) 4-LEVEL PAGING SIMULATION
// ============================================================================

#define ARM64_ENTRIES 512 // 9 bits per level -> 4 KB Table (512 * 8 bytes)

uint64_t arm64_extract_idx(uint64_t va, int level) {
    // Level 0: 47..39 (shift 39), L1: 38..30 (shift 30), L2: 29..21 (shift 21), L3: 20..12 (shift 12)
    int shift = 12 + (9 * (3 - level));
    return (va >> shift) & 0x1FF;
}

uint64_t* arm64_alloc_table(void) {
    return (uint64_t*)calloc(ARM64_ENTRIES, sizeof(uint64_t));
}

void arm64_map_page(uint64_t *root_l0, uint64_t va, uint64_t pa) {
    uint64_t *curr = root_l0;
    for (int lvl = 0; lvl < 3; lvl++) {
        uint64_t idx = arm64_extract_idx(va, lvl);
        if ((curr[idx] & 0x1) == 0) { // Table descriptor bit 0
            uint64_t *next = arm64_alloc_table();
            curr[idx] = ((uint64_t)(uintptr_t)next) | 0x3; // Valid Table Descriptor
            printf("[ARM64 MAP] L%d Table created at %p (Index %llu)\n", lvl + 1, (void*)next, idx);
        }
        curr = (uint64_t*)(uintptr_t)(curr[idx] & 0x0000FFFFFFFFF000ULL);
    }
    uint64_t l3_idx = arm64_extract_idx(va, 3);
    curr[l3_idx] = (pa & 0x0000FFFFFFFFF000ULL) | 0x3; // Page descriptor (Valid)
    printf("[ARM64 MAP] Leaf L3 Mapped: VA 0x%016llX -> PA 0x%016llX\n\n", va, pa);
}

int main() {
    printf("=== ARM32 (ARMv7-A) 2-Level Translation ===\n");
    arm32_mmu_t mmu32 = create_arm32_mmu();
    arm32_map_page(&mmu32, 0x80004560, 0x48004000);
    uint32_t resolved_pa32 = arm32_translate(&mmu32, 0x80004560);
    printf("Resolved Physical Address: 0x%08X (Expected: 0x48004560)\n\n", resolved_pa32);

    printf("=== ARM64 (AArch64) 4-Level Translation ===\n");
    uint64_t *root_l0 = arm64_alloc_table();
    arm64_map_page(root_l0, 0x00007FFF12345678ULL, 0x0000000088005000ULL);
    return 0;
}
```

---

## 5. The Split TTBR Architecture: Kernel Space (`TTBR1`) vs. User Space (`TTBR0`)

One of the most critical architectural innovations in ARM (ARMv7-A and ARMv8/v9-A) that distinguishes it from basic RISC-V SV39 is the **Split Translation Table Base Register (TTBR)** design:

```
                            ARM64 VIRTUAL ADDRESS SPACE
                            
  0xFFFF_FFFF_FFFF_FFFF ┌────────────────────────────────────────┐
                        │                                        │
                        │        KERNEL VIRTUAL MEMORY           │  ===> Translated by TTBR1_EL1
                        │     (Modules, vmalloc, direct map)     │
  0xFFFF_8000_0000_0000 ├────────────────────────────────────────┤
                        │         NON-CANONICAL HOLE             │  ===> Access triggers immediate fault!
  0x0000_7FFF_FFFF_FFFF ├────────────────────────────────────────┤
                        │                                        │
                        │         USER VIRTUAL MEMORY            │  ===> Translated by TTBR0_EL1
                        │     (Stack, Heap, Code, Libraries)     │
  0x0000_0000_0000_0000 └────────────────────────────────────────┘
```

### Why does ARM use two different hardware base registers?
1. **Zero Kernel Page Table Copying:**
   - In single-root architectures (like standard xv6 on RISC-V), every process page table must duplicate the entire top-level kernel page table entries so that traps and interrupts can execute kernel code.
   - In ARM, **`TTBR1_EL1` points to the global Kernel Page Table once**, and **never changes during a process context switch**.
2. **Fast Context Switches (No TLB Invalidation for Kernel):**
   - When switching between Process A and Process B, the Linux kernel only changes **`TTBR0_EL1`** (the User Page Table).
   - All cached TLB entries for kernel memory (`TTBR1_EL1`) remain valid across context switches, drastically reducing cache miss penalties.

---

## 6. Line-by-Line Mapping to Linux Kernel Source (`arch/arm` and `arch/arm64`)

Source references below are pinned to the original [Linux checkout revision](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/?id=8b73de7da85fde281a385e0b26eda9bffd3ca477). Recheck implementation details against the version selected for each lab:

### 1. ARM32 Linux Page Table Definitions
- [Arm two-level page-table definitions](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/arch/arm/include/asm/pgtable-2level.h?id=8b73de7da85fde281a385e0b26eda9bffd3ca477):
  - `PGDIR_SHIFT` = 20 (Bits 31..20 for 1 MB sections or L1 index).
  - `PTRS_PER_PGD` = 2048 / 4096.
  - `PTRS_PER_PTE` = 512 / 256.

### 2. ARM64 Linux 4-Level Page Table Definitions
- [Arm64 page-table definitions](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/arch/arm64/include/asm/pgtable.h?id=8b73de7da85fde281a385e0b26eda9bffd3ca477) & [hardware definitions](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/arch/arm64/include/asm/pgtable-hwdef.h?id=8b73de7da85fde281a385e0b26eda9bffd3ca477):
  - `PAGE_SHIFT` = 12 (4 KB page size).
  - `ARM64_HW_PGTABLE_LEVEL_SHIFT(n)`: Calculates shifts for L0 (39), L1 (30), L2 (21), L3 (12).
  - `PTE_VALID` = `(1UL << 0)` (Valid bit).
  - `PTE_USER` = `(1UL << 6)` (AP[1] User accessible).
  - `PTE_RDONLY` = `(1UL << 7)` (AP[2] Read-only).

### 3. TTBR Switching Assembly in ARM64 Linux
- [Arm64 processor setup assembly](https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/arch/arm64/mm/proc.S?id=8b73de7da85fde281a385e0b26eda9bffd3ca477):
  ```assembly
  // Setting User Page Table in TTBR0_EL1 during context switch
  msr ttbr0_el1, x0    // Load new user root page table physical address
  isb                  // Instruction Synchronization Barrier (flush CPU pipeline)
  ```

---

## 7. Staff-Level Interview Deep-Dive Q&A (ARM Memory Subsystem)

### Q1: What is the difference between ARM's `DMB`, `DSB`, and `ISB` when modifying Page Tables?
- **`DSB` (Data Synchronization Barrier):** When the kernel writes a new PTE to RAM, it must execute `DSB` to ensure that the memory write has completed and is visible to the MMU table walker before any subsequent instructions attempt to access that virtual address.
- **`ISB` (Instruction Synchronization Barrier):** Required after writing to control registers (like `TTBR0_EL1` or `SCTLR_EL1` to enable the MMU) to flush the CPU's speculative prefetch pipeline and force it to re-fetch instructions using the new translation rules.
- **`DMB` (Data Memory Barrier):** Ensures ordering of data accesses between CPU cores or DMA masters, but does not wait for execution completion like `DSB`.

### Q2: What is an ASID (Address Space Identifier) in ARM, and how does it prevent TLB flushes?
- **Answer:** In ARM64, each `TTBR0_EL1` entry includes an 8-bit or 16-bit **ASID** tagged to the current process.
- When the MMU caches a virtual-to-physical translation in the TLB, it tags the entry with the process's ASID (e.g., `VA 0x1000 [ASID 4] -> PA 0x88000`).
- When switching to another process (ASID 5), the OS does **not** need to flush the TLB (`TLBI`). If the CPU later switches back to ASID 4, the TLB entries are still warm in the cache!

### Q3: What are Block / Section Mappings (Huge Pages) in ARM?
- In ARM64, Level 1 and Level 2 descriptors can be marked as **Block Descriptors** instead of Table Descriptors.
- **Level 1 Block:** Maps a contiguous **1 GB** physical block directly with a single TLB entry.
- **Level 2 Block:** Maps a contiguous **2 MB** physical block directly (Transparent Huge Pages).
- **Benefit:** Greatly reduces TLB misses for large buffers, such as video framebuffers, GPU shared memory, and network packet rings.
