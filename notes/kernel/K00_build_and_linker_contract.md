# K0: Build, Toolchain and Linker Contract

> **Verified:** `riscv64-linux-gnu-gcc` 13.3.0, GNU ld 2.42, QEMU 8.2.2 (`virt`, OpenSBI v1.3 bundled), gdb-multiarch 15.1, upstream reference Linux 7.3-rc4 (`f0100363d`), 2026-10-21. The tutor ran the reference code below in a private scratch copy outside `my-linux`. Tutor-run results: clean `-Werror` build, `make check` 5/5 PASS, `K0 layout: PASS` with `-kernel Image` and `-kernel vmlinux`, PASS with `-smp 4` when OpenSBI picked hart 1, and a GDB stop at physical `0x80200000` with source lines. Your own run is the evidence that counts for the tracker.

| Item | Value |
| --- | --- |
| Milestone | K0 of [the kernel plan](../../plans/scratch_riscv_os_plan.md) |
| Repository | `C:\workspace\career_ws\my-linux`, branch `v2` (v1 is on branch `v1`, tag `v1-reference`) |
| You write | 14 files, listed in [section 5](#5-files-you-write) |
| Upstream to read | `linux/arch/riscv/kernel/head.S` (20-66, 210-328), `linux/arch/riscv/kernel/vmlinux.lds.S`, `linux/include/asm-generic/vmlinux.lds.h` (`SBSS` 759, `BSS` 775, `BSS_SECTION` 1183), `linux/arch/riscv/Makefile` (10-117), `linux/Makefile` (603-610, 1157, 1245-1262), `linux/arch/riscv/boot/Makefile` (17) |
| Done when | [Acceptance checklist](#9-acceptance-checklist) passes on your machine |

---

## 1. What K0 Is

K0 is the **contract between the build tools and the first instruction of the kernel**. It answers five questions before any real kernel feature exists:

1. Which **address is each byte linked for**, and which address is it **loaded at**?
2. Which **sections** can the compiler produce, and where does each one go?
3. Which compiler defaults (PIE, unwind tables, small data, FP) are **wrong for a kernel**?
4. What does a bootloader need to see at the start of the file (the **Image header**)?
5. How do we **prove** all of the above with tools, not by hoping?

K0 ends with a kernel that boots under OpenSBI and prints its own layout:

```text
my-linux v2 K0
boot_cpu_hartid    0x0000000000000000
_dtb_early_pa      0x0000000087e00000
_start (PC-rel)    0x0000000080200000
_end (PC-rel)      0x0000000080208000
&k0_small_zero     0x0000000080203010
k0_abs_ptr value   0xffffffff800010e8
K0 layout: PASS
```

The first five lines are **physical** addresses because the MMU is off. The `k0_abs_ptr` line is **virtual**. Understanding why those differ is the main idea of K0.

---

## 2. Why It Is Needed: the v1 Failure

**Simple version for a 12-year-old.** Imagine packing a school bag from a checklist. The checklist says "books, lunch box, pencil case". You also have small items like an eraser and a ruler, but they are not on the list. The packer puts them in a random pocket, sometimes outside the bag. Later you put a heavy box exactly where the eraser was and crush it.

**What actually happened in v1.** GCC for RISC-V places any global object of 8 bytes or less in special *small-data* sections: `.sdata` for initialised data and `.sbss` for zero-initialised data. The v1 linker script listed `.data` and `.bss` but not `.sdata` or `.sbss`. ld calls an input section that the script does not mention an **orphan** and places it wherever its heuristics choose. In v1 it went **after `_end`**:

```text
v1 layout (broken)                                 What the code assumed
------------------                                 ---------------------
0x8020_0000  .text                                 .text
             .rodata                               .rodata
             .data                                 .data
__bss_start  .bss   (cleared by head.S)            .bss   <- includes ALL zero data
__bss_stop                                         __bss_stop
_end ------------------------------------------    _end   <- free memory starts here
             .sbss  (orphan: mem_map, max_pfn,
                     va_pa_offset, ...)
                     ^ never cleared by head.S
                     ^ overwritten when mem_map[] was placed at PAGE_ALIGN(_end)
```

Two bugs came from one missing line: `.sbss` was **not zeroed** (because it was outside `[__bss_start, __bss_stop)`), and it was **overwritten** (because the allocator believed memory after `_end` was free). The result was a store fault inside `alloc_page()` with a garbage `stval`. It looked like an allocator bug, but it was a linker-script bug.

**The K0 fix is a process, not only a line:** name every section explicitly, make the linker **fail** on any orphan (`--orphan-handling=error`), and run an automatic audit (`make check`).

> **Tutor-run negative test.** Deleting `*(.sbss .sbss.*)` from the K0 script stops the link with
> `ld: error: unplaced orphan section '.sbss' from 'arch/riscv/kernel/setup.o'`.
> Without `--orphan-handling=error` the same broken script linked, and ld *happened* to place `.sbss` next to `.bss` this time. That is the danger: orphan placement is a heuristic, so the same mistake can work in one build and corrupt memory in the next.

---

## 3. Concepts, One at a Time

### 3.1 VMA and LMA: the address on the label and the shelf it sits on

- **VMA (virtual memory address)**, also called the *link address*: the address the code **believes** it lives at. All symbol values in `nm`, `System.map` and GDB are VMAs.
- **LMA (load memory address)**: where the bytes are **physically placed** when loaded.

Upstream Linux links the RV64 kernel at `KERNEL_LINK_ADDR = 0xffffffff80000000` (top 2 GB of the address space, `linux/arch/riscv/include/asm/pgtable.h:27`). OpenSBI loads the image into RAM at `0x80200000` (RAM base `0x80000000` plus the 2 MB `text_offset` from the Image header). The MMU is off when `_start` runs, so the CPU executes at the **physical** address.

```text
             VMA (what symbols say)          LMA (where the bytes are)
             ----------------------          -------------------------
_start       0xffffffff80000000   ------->   0x0000000080200000
_end         0xffffffff80008000   ------->   0x0000000080208000

             LOAD_OFFSET = VMA - LMA = 0xffffffff80000000 - 0x80200000
                                     = 0xfffffffeffe00000
```

In the linker script, `. = KERNEL_LINK_ADDR` sets VMAs, and `AT(ADDR(sec) - LOAD_OFFSET)` sets LMAs. `readelf -l` shows both: `VirtAddr` is VMA, `PhysAddr` is LMA.

### 3.2 How code runs at an address it was not linked for: PC-relative versus absolute

If every instruction used the absolute VMA `0xffffffff8000xxxx` while the MMU is off, the CPU would fetch from a physical address with no RAM behind it and fault. K0 works because **RISC-V `-mcmodel=medany` code computes addresses relative to the PC**:

```text
la t0, __bss_start     assembles to     auipc t0, 0x3          t0 = PC + 0x3000
                                         addi  t0, t0, -72      t0 = t0 - 72
```

`auipc` adds an offset to **the current PC**. The linker computes the offset as `VMA(target) - VMA(this instruction)`. Because VMA and LMA differ by the same `LOAD_OFFSET` for every byte of the image, the **distance** is identical in both worlds. At run time, `PC (physical) + distance = target (physical)`. That is why C and assembly run correctly before the MMU is enabled.

The rule has one important exception, and K0 prints it:

| Expression in `init/main.c` | How the compiler gets it | Value printed with the MMU off |
| --- | --- | --- |
| `(unsigned long)_start` | `auipc`: PC + distance | `0x80200000` (physical) |
| `k0_abs_ptr` (a pointer **stored** in `.sdata`, initialised to `"probe"`) | `ld` from memory; the **linker wrote the VMA** into the data at link time | `0xffffffff800010e8` (virtual) |

**Rule to remember:** *computed* addresses follow the PC, but *stored* addresses carry the link address. Dereferencing `k0_abs_ptr` now would access physical `0xffffffff800010e8`, where there is no RAM. K3 enables the MMU **before** any code depends on stored pointers. This is also why v1's statically initialised `LIST_HEAD` held the wrong half of the address space when the linking choices changed.

**Why `medany` and not `medlow`?** `medlow` builds addresses with `lui`, which gives an **absolute** address in the range from -2 GB to +2 GB around 0. `0xffffffff80000000` is exactly -2 GB, so `medlow` would link, but every address would be the VMA, which is wrong while the MMU is off. Upstream builds RV64 kernels with `-mcmodel=medany` (`linux/arch/riscv/Makefile:96`).

### 3.3 PIE and the GOT: a default that silently breaks kernels

Ubuntu's GCC builds **position-independent executables (PIE)** by default. In PIE code, a global's address is read from the **Global Offset Table (GOT)**, a table of absolute addresses that the **dynamic loader** (`ld.so`) fixes at program start. A kernel has no dynamic loader, so GOT entries would contain wrong values.

> **Tutor-run evidence (K0, first build).** With `-fno-PIE` in `CFLAGS` only, the link failed:
> `unplaced orphan section '.got' from 'arch/riscv/kernel/head.o'` (also `.rela.got`, `.got.plt`).
> In PIC mode, the assembler expands `la` into a **GOT load** (`lga`). Only `lla` stays PC-relative. Upstream passes `-fno-PIE` to **both** C and assembly (`linux/Makefile:603` for `KBUILD_AFLAGS`, `:609` for `KBUILD_CFLAGS`). After that fix, `readelf -r head.o` shows only `R_RISCV_PCREL_HI20/LO12_I` relocations.

Upstream's optional `CONFIG_RELOCATABLE` deliberately uses `-fPIE` and applies its own relocations from `.rela.dyn` at boot (`linux/arch/riscv/Makefile:10-12`). That is a later, separate design. K0 is linked at one fixed address.

**`.rela.dyn` finding.** After the PIE fix, ld still reported `.rela.head.text` and `.rela.text` as orphans. An ordinary link produced **no** relocation section in the output. ld only creates an empty `.rela.dyn` candidate, and `--orphan-handling=error` refuses to place it implicitly. Upstream names it explicitly (`.rela.dyn : { *(.rela .rela*) }` in `vmlinux.lds.S`). K0 does the same, and `make check` fails if it is ever non-empty.

### 3.4 Small-data sections and the global pointer

GCC for RISC-V uses `-msmall-data-limit=8` by default: globals of **8 bytes or less** go to `.sdata`, `.sbss` or `.srodata`. The goal is speed. If register `gp` points into the middle of the small-data area, the linker can **relax** a two-instruction `auipc`/`addi` into a single `gp`-relative instruction.

```text
upstream:  lds sets  __global_pointer$ = . + 0x800   (vmlinux.lds.S:133)
           head.S    load_global_pointer             (head.S:249, again at :111/:138 after the MMU switch)
```

`gp` holds a **stored** address, so upstream must reload it after the MMU switch (the rule from 3.2 again). K0 uses `-mno-relax` and never sets `gp`. Even so, **GCC still emits `.sdata` and `.sbss`**. The tutor-run build shows `boot_cpu_hartid`, `_dtb_early_pa` and `k0_small_zero` in `.sbss`. The linker script must therefore name them. Upstream puts `.sbss` **inside** the BSS range (`BSS_SECTION` -> `SBSS` + `BSS`), so the `head.S` clear loop covers it.

### 3.5 Orphans and `--orphan-handling=error`

An orphan is any input section that the script does not mention. Upstream builds with `--orphan-handling=$(CONFIG_LD_ORPHAN_WARN_LEVEL)` (`linux/Makefile:1262`) for the same reason. K0 uses `error`. Each orphan found while building K0 taught something:

| Orphan seen | Cause | Correct fix |
| --- | --- | --- |
| `.got`, `.rela.got`, `.got.plt` | assembly built as PIE | `-fno-PIE` in `AFLAGS` |
| `.riscv.attributes` | assembler records ISA attributes | `-mno-riscv-attribute -Wa,-mno-arch-attr`, plus a non-loaded `.riscv.attributes 0 :` rule as upstream has |
| `.rela.head.text`, `.rela.text` | empty dynamic-relocation candidate | explicit `.rela.dyn`, checked to be empty |
| `.sbss` (negative test) | section not named | `*(.sbss .sbss.*)` inside the BSS range |

**Do not fix an orphan by discarding it** unless you can explain why its contents are useless. Discarding `.sbss` would delete variables.

### 3.6 Unwind tables

`.eh_frame` describes how to unwind the stack for C++ exceptions and some debuggers. The kernel does not use it, so it only wastes space. Upstream disables it with `-fno-asynchronous-unwind-tables -fno-unwind-tables` (`linux/arch/riscv/Makefile:100`). K0 keeps **frame pointers** (`-fno-omit-frame-pointer`, upstream `linux/Makefile:968` under `CONFIG_FRAME_POINTER`) so K2's fatal-trap handler can walk the call chain through `s0`. `head.S` sets `s0 = 0` before the first C call to mark the end of that chain. `-g` still produces `.debug_frame`, which is not loaded.

### 3.7 No floating point in the kernel

`-march=rv64imac_zicsr_zifencei -mabi=lp64` has no `f` or `d` extension, so the compiler cannot emit FP instructions or pass arguments in FP registers. FP registers belong to **user** tasks. Saving them on every trap would be expensive, so kernels save them lazily or not at all. Upstream strips `fd` from the C ISA (`linux/arch/riscv/Makefile:78`) and clears `sstatus.FS/VS` in `head.S` so that accidental FP use traps. K0 does not clear `sstatus.FS` yet. That arrives in K2 together with `asm/csr.h` (named deviation).

### 3.8 Freestanding: no libc, no host headers

| Flag | Why |
| --- | --- |
| `-ffreestanding` | No hosted C library is assumed. GCC may **still** emit calls to `memcpy`/`memset` for large structure copies, so `lib/string.c` is added when the first such call appears. |
| `-nostdinc -isystem $(gcc -print-file-name=include)` | Do not search `/usr/include`, because glibc headers describe user space. Allow only compiler-provided headers such as `stddef.h` and `stdarg.h` (upstream `NOSTDINC_FLAGS`, `linux/Makefile:1157`). |
| `-nostdlib` (ld) | Do not link `crt0`, libc or libgcc. |
| `-fno-common` | `int x;` in two files is a link error, not a silent merge (`linux/Makefile:608`). |
| `-fno-strict-aliasing` | Kernels reinterpret memory through different pointer types (`linux/Makefile:610`). |
| `-fno-stack-protector` | A canary needs `__stack_chk_guard` set up first (upstream default unless `CONFIG_STACKPROTECTOR`, `linux/Makefile:961`). |
| `-mno-save-restore` | Otherwise GCC may call libgcc's `__riscv_save_N` helpers, which are not linked (`linux/arch/riscv/Makefile:90`). |
| `-mstrict-align` | A misaligned access may trap to firmware emulation (slow) or fault (`linux/arch/riscv/Makefile:117`). |
| `-std=gnu11` | Upstream C dialect (`linux/Makefile:813`). |
| `-z noexecstack --no-warn-rwx-segments` | Same link hygiene as upstream (`linux/Makefile:1245-1247`). |

### 3.9 `_AC()`: one constant for C, assembly and linker scripts

`0xffffffff80000000UL` is valid C but **invalid** in assembly and linker scripts because of the `UL` suffix. `_AC(0xffffffff80000000, UL)` expands to `0xffffffff80000000UL` in C and to plain `0xffffffff80000000` when `__ASSEMBLY__` is defined. The Makefile defines `__ASSEMBLY__` for `.S` files and for the preprocessed linker script. Upstream: `linux/include/uapi/linux/const.h:17-21`.

### 3.10 The Image header: what a bootloader reads first

`vmlinux` is an ELF file. Bootloaders such as U-Boot `booti`, and OpenSBI's payload mode, usually receive a **flat binary** called `Image` (`objcopy -O binary`, upstream `linux/arch/riscv/boot/Makefile:17`). A flat binary has no ELF metadata, so its first 64 bytes describe it:

```text
offset size field        K0 value (tutor-run od dump)    meaning
0x00   4    code0        6f 00 00 04  = j _start_kernel   first instruction: jump over the header
0x04   4    code1        00 00 00 00
0x08   8    text_offset  0x200000                         load 2 MB above RAM base
0x10   8    image_size   0x8000                           _end - _start, INCLUDING .bss
0x18   8    flags        0                                little endian
0x20   4    version      0x00000002                       header v0.2
0x24   4    res1         0
0x28   8    res2         0
0x30   8    magic        "RISCV\0\0\0"                    deprecated magic
0x38   4    magic2       "RSC\x05"                        current magic
0x3c   4    res3         0
```

`image_size` includes `.bss` (0x8000), but the `Image` file is only **8204 bytes**: it stops at `_edata`, because `.bss` contains only zeroes and is not stored. A loader reserves `image_size` bytes, **but nobody promises to zero the tail**. That is why `head.S` clears BSS itself (upstream comment: "Clear BSS for flat non-ELF images").

`.option norvc` around the `j` keeps it a 4-byte instruction so the bytes match the documented `code0` field. Upstream allows the assembler to choose, and its `.balign 8` keeps later fields at the same offsets. K0 picks the stricter form.

### 3.11 Build hygiene: depfiles, System.map, line endings

- `-MMD -MP` makes GCC write `*.d` files that list every header each object includes. `make` then rebuilds the right objects when a header changes. Tutor-run check: touching `asm/page.h` rebuilt only `vmlinux.lds` and `vmlinux`.
- `System.map` (`nm -n vmlinux`) is the sorted symbol list that upstream also ships. It is the first file to open when an address appears in a crash.
- **Line endings.** A shell script with Windows CRLF line endings fails with `set: Illegal option -`, because `sh` reads `set -eu\r`. `.gitattributes` with `* text=auto eol=lf` only controls what **Git** writes: it stores LF in commits and writes LF when it checks files out. It does **not** change a file you create in a Windows editor, so new files must be saved with LF. In VS Code, add `"files.eol": "\n"` to your settings (it applies to new files) and use the `CRLF`/`LF` switch in the status bar for existing files. Check with `grep -c $'\r' file`, which should print `0`. (Corrected 2026-10-22, see section 13.)

### 3.12 Sections Versus Segments (`readelf -lW`)

A **section** is the linker's unit: a named piece with one kind of content (`.text`, `.rodata`, `.sdata`, `.sbss`, `.debug_info`). The **section header table** lists them, and `objdump -h`/`readelf -S` print it. A **segment** is the loader's unit: one **program header** that says "copy these file bytes to this address, reserve this much memory, use these permissions". `readelf -l` prints the program headers. The linker builds segments by grouping adjacent sections with compatible permissions. A loader reads only the program headers and never looks at section names.

```text
Your readelf -lW vmlinux (K0):
Type  Offset   VirtAddr           PhysAddr           FileSiz  MemSiz   Flg
LOAD  0x001000 0xffffffff80000000 0x0000000080200000 0x001101 0x001101 R E   <- segment 00
LOAD  0x003000 0xffffffff80002000 0x0000000080202000 0x00000c 0x005120 RW    <- segment 01
GNU_STACK ...                                                          RW    <- segment 02 (marker)

Section to Segment mapping:
00  .head.text .text .rodata     file bytes 0x1000..0x2101 -> phys 0x80200000..0x80201101
01  .sdata .sbss .bss            copy 0xc bytes (.sdata), then MemSiz - FileSiz = 0x5114 bytes
                                 of zeroes (.sbss + .bss)
02  (none)                       no bytes; "stack is not executable" (from -z noexecstack)
.debug_*, .comment               in no segment: never loaded, used only by GDB and tools
```

Four details to notice:

1. **`FileSiz` versus `MemSiz`.** For segment 01, the file holds only 12 bytes (`.sdata`), but memory needs 0x5120 bytes. An ELF loader (QEMU's `-kernel vmlinux`) zeroes the difference. A flat `Image` has no program headers, so `head.S` must zero BSS itself.
2. **`.rodata` is in the `R E` segment.** ld merged it with `.text` because the script does not declare `PHDRS`, so read-only data is technically executable in the ELF file. Upstream does not rely on ELF permissions either: the kernel's own page tables enforce read-only, no-execute `.rodata` (K4).
3. **Alignment.** Each `Offset` is congruent to its `VirtAddr` modulo `Align` (0x1000), so a loader could map file pages directly. The gap between `.text`'s end and `.rodata` at the next page is padding.
4. **`Image` comes from segments, not sections.** `objcopy -O binary` writes the loadable bytes at their LMAs from the lowest to the highest address, filling gaps with zeroes. That gives `0x80202000 + 0xc - 0x80200000 = 0x200c = 8204` bytes, exactly your `Image` size.

Analogy: sections are the chapters of a manuscript (the editor's view). Segments are the labelled moving boxes (the movers' view: "put this box in room X, it is fragile/read-only"). Movers never open the box to read chapter titles.

---

## 4. Final Layout (tutor-run `objdump -h` and `System.map`)

```text
 VMA                 LMA          section      size    contents
 ffffffff80000000    80200000     .head.text   0x90    _start (Image header), _start_kernel
 ffffffff80000090    80200090     .text        0x1d4   k0_puts, k0_line, start_kernel
 ffffffff80001000    80201000     .rodata      0x101   strings, digits[] (.srodata merged here)
 ffffffff80002000    80202000     .sdata       0xc     k0_abs_ptr, k0_small_init      <- _sdata
                                                                                       <- _edata = ...200c
 ffffffff80003000    80203000     .sbss        0x14    boot_cpu_hartid, _dtb_early_pa,
                                                       k0_small_zero                  <- __bss_start
 ffffffff80003020    80203020     .bss         0x4100  init_stack (16 KB), k0_big_zero
 ffffffff80008000    80208000     (end)                                                <- __bss_stop = _end

 Image file  = [0x80200000, 0x8020200c)  8204 bytes   (everything up to _edata)
 image_size  = [0x80200000, 0x80208000)  0x8000 bytes (includes .bss)
 readelf -l: LOAD VirtAddr 0xffffffff80000000 PhysAddr 0x80200000 (R E)
             LOAD VirtAddr 0xffffffff80002000 PhysAddr 0x80202000 (RW, MemSiz > FileSiz = .bss)
```

`.data` is empty in K0, so it does not appear. `.rela.dyn` is empty, so ld drops it.

---

## 5. Files You Write

Type each file yourself on branch `v2`. Order matters only for understanding: headers, then linker script, then entry code, then C, then tooling.

```text
my-linux/
|-- .gitattributes
|-- .gitignore
|-- Makefile
|-- include/linux/const.h
|-- arch/riscv/include/asm/page.h
|-- arch/riscv/include/asm/pgtable.h
|-- arch/riscv/include/asm/image.h
|-- arch/riscv/kernel/vmlinux.lds.S
|-- arch/riscv/kernel/head.S
|-- arch/riscv/kernel/setup.c
|-- arch/riscv/mm/init.c
|-- init/main.c
`-- scripts/
    |-- check-vmlinux.sh
    `-- gdb-early.gdb
```

`reference/` (your untracked v1 copies) stays untracked through `.gitignore`.

### 5.1 `.gitattributes` and `.gitignore`

```gitattributes
# Build files are read by Linux tools: keep LF on every OS.
* text=auto eol=lf
```

```gitignore
*.o
*.d
/vmlinux
/vmlinux.map
/Image
/System.map
/arch/riscv/kernel/vmlinux.lds
/reference/
```

### 5.2 `include/linux/const.h`

```c
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_CONST_H
#define _LINUX_CONST_H

/*
 * _AC(X, Y) lets one header serve C, assembly and linker scripts.
 * In C, _AC(0x10, UL) becomes 0x10UL. In assembly and linker scripts the
 * UL suffix is invalid, so it becomes plain 0x10.
 */
#ifdef __ASSEMBLY__
#define _AC(X, Y)	X
#else
#define __AC(X, Y)	(X##Y)
#define _AC(X, Y)	__AC(X, Y)
#endif

#define UL(x)		(_AC(x, UL))

#endif /* _LINUX_CONST_H */
```

Why two levels (`_AC` calls `__AC`)? The extra level makes the preprocessor expand `X` **before** `##` pastes it. Otherwise `_AC(PAGE_SHIFT, UL)` would produce the token `PAGE_SHIFTUL`.

### 5.3 `arch/riscv/include/asm/page.h`, `pgtable.h`, `image.h`

```c
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_RISCV_PAGE_H
#define _ASM_RISCV_PAGE_H

#include <linux/const.h>

#define PAGE_SHIFT	12
#define PAGE_SIZE	(_AC(1, UL) << PAGE_SHIFT)

#endif /* _ASM_RISCV_PAGE_H */
```

```c
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_RISCV_PGTABLE_H
#define _ASM_RISCV_PGTABLE_H

#include <linux/const.h>

/*
 * Virtual base of the kernel image: the top 2 GB of the 64-bit address
 * space (upstream: ADDRESS_SPACE_END - SZ_2G + 1).
 */
#define KERNEL_LINK_ADDR	_AC(0xffffffff80000000, UL)

#endif /* _ASM_RISCV_PGTABLE_H */
```

```c
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_RISCV_IMAGE_H
#define _ASM_RISCV_IMAGE_H

/* Values checked by bootloaders such as U-Boot "booti" (upstream asm/image.h). */
#define RISCV_IMAGE_MAGIC		"RISCV\0\0\0"
#define RISCV_IMAGE_MAGIC2		"RSC\x05"

#define RISCV_IMAGE_FLAG_LE		0

#define RISCV_HEADER_VERSION_MAJOR	0
#define RISCV_HEADER_VERSION_MINOR	2
#define RISCV_HEADER_VERSION		(RISCV_HEADER_VERSION_MAJOR << 16 | \
					 RISCV_HEADER_VERSION_MINOR)

#endif /* _ASM_RISCV_IMAGE_H */
```

Upstream derives `KERNEL_LINK_ADDR` from `ADDRESS_SPACE_END - SZ_2G + 1` (`pgtable.h:27`). For 64-bit that is the same constant. The top 2 GB is chosen because any address there fits in a sign-extended 32-bit offset, and because it leaves the rest of the upper half for the linear map (K3/K4).

### 5.4 `arch/riscv/kernel/vmlinux.lds.S`

```c
/* SPDX-License-Identifier: GPL-2.0-only */
#include <asm/page.h>
#include <asm/pgtable.h>

/*
 * VMA (link address) = KERNEL_LINK_ADDR.  LMA (load address) = VMA - LOAD_OFFSET.
 * Deviation from upstream: upstream learns its physical base at run time and
 * relies on the Image loader. We also pin LMA to RAM base + 2 MB (0x80200000)
 * so QEMU and GDB can load vmlinux as an ELF.
 */
#define KERNEL_PHYS_ADDR	0x80200000
#define LOAD_OFFSET		(KERNEL_LINK_ADDR - KERNEL_PHYS_ADDR)

OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
	. = KERNEL_LINK_ADDR;

	.head.text : AT(ADDR(.head.text) - LOAD_OFFSET) {
		KEEP(*(.head.text))
	}

	.text : AT(ADDR(.text) - LOAD_OFFSET) {
		_stext = .;
		*(.text .text.*)
		_etext = .;
	}

	. = ALIGN(PAGE_SIZE);
	.rodata : AT(ADDR(.rodata) - LOAD_OFFSET) {
		__start_rodata = .;
		*(.rodata .rodata.*)
		*(.srodata .srodata.*)
		__end_rodata = .;
	}

	/*
	 * Upstream uses this for its relocatable kernel. Ours is linked at a fixed
	 * address, so it must stay empty (ld then drops it); make check enforces that.
	 */
	.rela.dyn : AT(ADDR(.rela.dyn) - LOAD_OFFSET) ALIGN(8) {
		*(.rela .rela*)
	}

	. = ALIGN(PAGE_SIZE);
	_sdata = .;
	.data : AT(ADDR(.data) - LOAD_OFFSET) {
		*(.data .data.*)
	}
	.sdata : AT(ADDR(.sdata) - LOAD_OFFSET) {
		*(.sdata .sdata.*)
	}
	_edata = .;

	/* Same shape as upstream BSS_SECTION(): .sbss first, then .bss. */
	. = ALIGN(PAGE_SIZE);
	__bss_start = .;
	.sbss : AT(ADDR(.sbss) - LOAD_OFFSET) {
		*(.sbss .sbss.*)
		*(.scommon)
	}
	.bss : AT(ADDR(.bss) - LOAD_OFFSET) {
		*(.bss .bss.*)
		*(COMMON)
	}
	. = ALIGN(PAGE_SIZE);
	__bss_stop = .;
	_end = .;

	/* Not loaded: debug information and tool metadata (upstream DWARF_DEBUG, ELF_DETAILS). */
	.comment		0 : { *(.comment) }
	.riscv.attributes	0 : { *(.riscv.attributes) }
	.debug_aranges		0 : { *(.debug_aranges) }
	.debug_info		0 : { *(.debug_info) }
	.debug_abbrev		0 : { *(.debug_abbrev) }
	.debug_line		0 : { *(.debug_line) }
	.debug_frame		0 : { *(.debug_frame) }
	.debug_str		0 : { *(.debug_str) }
	.debug_line_str		0 : { *(.debug_line_str) }
	.debug_loclists		0 : { *(.debug_loclists) }
	.debug_rnglists		0 : { *(.debug_rnglists) }

	/DISCARD/ : {
		*(.eh_frame)
		*(.note.GNU-stack)
	}
}
```

**Walkthrough.**

- The file is `.lds.S` so the C preprocessor can include `page.h`/`pgtable.h`. The Makefile runs `gcc -E -P -D__ASSEMBLY__` to produce `vmlinux.lds`. This is upstream's mechanism.
- `ENTRY(_start)` only sets the ELF entry field. `_start` itself is defined in `head.S`, so do **not** also assign `_start = .` here.
- `KEEP(*(.head.text))` guarantees the header is first and is never garbage-collected.
- `.text .text.*` also collects `-ffunction-sections`-style names, so later flags do not create orphans.
- Page alignment between text, read-only data and data is needed in K4, when `.text` becomes read-only+execute and data becomes read-write+no-execute with page granularity.
- The BSS range is `.sbss` **then** `.bss`, both between `__bss_start` and `__bss_stop`, like upstream `BSS_SECTION`. This is the v1 fix.
- `_end` is page aligned because later milestones start handing out memory from there.
- The `0 :` sections have VMA 0 and are not loaded. They keep debug information for GDB.
- `/DISCARD/` removes only sections whose content we can prove is useless (3.5).
- `.debug_*` names cover what GCC 13 emits at `-g`. If a different `-g` level adds a section (for example `.debug_loc`), the link stops with an orphan error. Add one more `0 :` line rather than switching to `warn`.

### 5.5 `arch/riscv/kernel/head.S`

```asm
/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * K0 entry: Image header, then the smallest correct path into C.
 * Runs with the MMU off, so the PC is a physical address (0x80200000...)
 * while every symbol is linked at a virtual address (0xffffffff80000000...).
 * Only PC-relative instructions (la, call, tail) are used, so that works.
 */
#include <asm/image.h>

	.section .head.text, "ax"
	.globl	_start
_start:
	/* Image header (asm/image.h). code0 must be one 4-byte jump. */
	.option	push
	.option	norvc
	j	_start_kernel
	.option	pop
	.word	0				/* code1 */
	.balign	8
	.dword	0x200000			/* text_offset: load 2 MB above RAM base */
	.dword	_end - _start			/* image_size, including .bss */
	.dword	RISCV_IMAGE_FLAG_LE		/* flags */
	.word	RISCV_HEADER_VERSION		/* version */
	.word	0				/* res1 */
	.dword	0				/* res2 */
	.ascii	RISCV_IMAGE_MAGIC		/* magic (deprecated) */
	.balign	4
	.ascii	RISCV_IMAGE_MAGIC2		/* magic2 */
	.word	0				/* res3 */

	.balign	4
_start_kernel:
	/* No interrupts until a trap vector exists (K2). */
	csrw	sie, zero
	csrw	sip, zero

	/*
	 * No hartid check: with SBI HSM firmware (OpenSBI, and ours in K1) only
	 * the boot hart enters here, and it need not be hart 0. Other harts stay
	 * in firmware until the kernel starts them (K8).
	 */

	/* Clear [__bss_start, __bss_stop): this range includes .sbss. */
	la	t0, __bss_start
	la	t1, __bss_stop
	bgeu	t0, t1, .Lclear_bss_done
.Lclear_bss:
	sd	zero, 0(t0)
	addi	t0, t0, 8
	bltu	t0, t1, .Lclear_bss
.Lclear_bss_done:

	/* Save boot arguments only after .bss is clear, or they would be wiped. */
	la	t0, boot_cpu_hartid
	sd	a0, 0(t0)
	la	t0, _dtb_early_pa
	sd	a1, 0(t0)

	/* Boot stack, frame pointer marks the end of the call chain. */
	la	sp, init_stack_top
	li	s0, 0
	tail	start_kernel

	.section .bss..init_stack, "aw", @nobits
	.balign	16
init_stack:
	.space	16384
init_stack_top:
```

**Walkthrough with the boot contract.** OpenSBI enters `_start` in **S-mode**, with `a0 = hartid`, `a1 = DTB physical address`, `satp = 0` (MMU off) and interrupts not yet enabled for S-mode.

| Step | Why | Upstream equivalent |
| --- | --- | --- |
| Header `j _start_kernel` | The CPU executes byte 0 first, so it must jump over 64 bytes of data | `head.S:20-61` |
| `csrw sie/sip, zero` | No interrupt may arrive before `stvec` exists | `head.S:212-213` (`CSR_IE`/`CSR_IP`) |
| **No** hartid check | With SBI HSM, only the boot hart arrives, and OpenSBI's cold-boot lottery may choose **any** hart | Upstream checks only in `CONFIG_RISCV_BOOT_SPINWAIT` mode (`head.S:258-270`) |
| Clear BSS with `sd`, 8 bytes per loop | The Image file does not contain `.bss`. Both bounds are page-aligned, so 8-byte stores are safe | `head.S:273-280` |
| Store `a0`/`a1` **after** clearing | Storing first would put them in `.sbss`, and the clear would wipe them | `head.S:281-282` stores the hartid after the clear |
| `la sp`, `li s0, 0` | C needs a stack. `s0 = 0` terminates frame-pointer walks | `head.S:286` (`init_thread_union + THREAD_SIZE`) |
| `tail start_kernel` | Jump without saving a return address; `start_kernel` never returns | `head.S:328` |

> **Tutor-run evidence for removing the hartid check.** An earlier draft had `bnez a0, park`. Running with `-smp 4` five times, OpenSBI printed `Boot HART ID : 1` once. With the check, that boot would have parked the **only** hart that entered the kernel and hung silently. Without it, all five runs printed `K0 layout: PASS`.

**Why the stack is in a `.bss..init_stack` section.** The `.bss.*` pattern in the script collects it into BSS, so it costs nothing in the `Image` file and the clear loop zeroes it. Upstream uses `init_thread_union` in a similar data section. Its size, `THREAD_SIZE`, is 16 KB on RV64.

### 5.6 `arch/riscv/kernel/setup.c` and `arch/riscv/mm/init.c`

```c
// SPDX-License-Identifier: GPL-2.0-only
/* Grows into setup_arch() in K4. */

/* Hart that ran the boot path; written by head.S. */
unsigned long boot_cpu_hartid;
```

```c
// SPDX-License-Identifier: GPL-2.0-only
/* Grows into setup_vm() and paging_init() in K3 and K4. */

/* Physical address of the device tree; written by head.S (by setup_vm() from K3). */
unsigned long _dtb_early_pa;
```

They are separate files now because that is where the variables live upstream (`boot_cpu_hartid` in `arch/riscv/kernel/setup.c:50`, the DTB address in `arch/riscv/mm/init.c:75`). Both are 8 bytes, so GCC puts both in `.sbss`. They are the exact kind of variable that broke v1.

### 5.7 `init/main.c` (K0 probe version)

```c
// SPDX-License-Identifier: GPL-2.0-only
/*
 * K0 start_kernel(): prove the build and layout contract.
 * The k0_* output helpers are temporary and are deleted in K1, when the
 * SBI console replaces direct UART access.
 */

extern unsigned long boot_cpu_hartid;
extern unsigned long _dtb_early_pa;
extern char _start[], _end[], __bss_start[], __bss_stop[];

#define K0_UART_BASE	0x10000000UL	/* QEMU virt NS16550A, physical */
#define K0_UART_THR	0		/* transmit holding register */
#define K0_UART_LSR	5		/* line status register */
#define K0_UART_LSR_THRE 0x20		/* transmitter can accept a byte */

static void k0_putc(char c)
{
	volatile unsigned char *uart = (volatile unsigned char *)K0_UART_BASE;

	while (!(uart[K0_UART_LSR] & K0_UART_LSR_THRE))
		;
	uart[K0_UART_THR] = c;
}

static void k0_puts(const char *s)
{
	for (; *s; s++) {
		if (*s == '\n')
			k0_putc('\r');
		k0_putc(*s);
	}
}

static void k0_puthex(unsigned long v)
{
	static const char digits[] = "0123456789abcdef";
	int shift;

	k0_puts("0x");
	for (shift = 60; shift >= 0; shift -= 4)
		k0_putc(digits[(v >> shift) & 0xf]);
}

static void k0_line(const char *label, unsigned long v)
{
	k0_puts(label);
	k0_puthex(v);
	k0_puts("\n");
}

/* Layout probes: one object for each data section kind. */
int k0_small_zero;			/* .sbss */
int k0_small_init = 42;			/* .sdata */
char k0_big_zero[256];			/* .bss */
const char *k0_abs_ptr = "probe";	/* .sdata; holds a link-time (virtual) address */

void start_kernel(void)
{
	k0_puts("\nmy-linux v2 K0\n");
	k0_line("boot_cpu_hartid    ", boot_cpu_hartid);
	k0_line("_dtb_early_pa      ", _dtb_early_pa);
	k0_line("_start (PC-rel)    ", (unsigned long)_start);
	k0_line("_end (PC-rel)      ", (unsigned long)_end);
	k0_line("&k0_small_zero     ", (unsigned long)&k0_small_zero);
	k0_line("k0_abs_ptr value   ", (unsigned long)k0_abs_ptr);

	if (k0_small_zero == 0 && k0_small_init == 42 && k0_big_zero[255] == 0 &&
	    (unsigned long)&k0_small_zero >= (unsigned long)__bss_start &&
	    (unsigned long)&k0_small_zero < (unsigned long)__bss_stop)
		k0_puts("K0 layout: PASS\n");
	else
		k0_puts("K0 layout: FAIL\n");

	for (;;)
		__asm__ volatile("wfi");
}
```

**Walkthrough.**

- `extern char _start[]` declares linker symbols as arrays. The symbol **is** the address, and there is no object to read. Writing `extern unsigned long _start;` and then reading `_start` would load the first 8 bytes of the header instead.
- `volatile` on the UART pointer tells the compiler that every access is a real device access. Without it, the busy-wait loop could be optimised into one read.
- The UART is reached through a constant **physical** address, which works only because the MMU is off. K1 removes this file-local driver and uses the SBI console. K7 adds the real interrupt-driven 8250 driver after `ioremap` exists. This is a named temporary deviation (section 7).
- The PASS condition checks the run-time result of the v1 bug: `.sbss` is zero, `.sdata` holds its initial value, `.bss` is zero, and the `.sbss` object lies inside the range that `head.S` cleared.
- `wfi` waits for an interrupt in a loop. Interrupts are masked, so this simply idles the hart.

### 5.8 `Makefile`

```make
# SPDX-License-Identifier: GPL-2.0
# my-linux v2 top-level Makefile (K0). Flags mirror upstream Linux where they matter for early boot.

CROSS_COMPILE ?= riscv64-linux-gnu-
CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
NM      := $(CROSS_COMPILE)nm
QEMU    ?= qemu-system-riscv64
GDB     ?= gdb-multiarch

# Integer-only ISA and ABI: the kernel never touches FP registers.
ARCH_FLAGS := -march=rv64imac_zicsr_zifencei -mabi=lp64 -mcmodel=medany

CPPFLAGS := -nostdinc -isystem $(shell $(CC) -print-file-name=include) \
	    -Iarch/riscv/include -Iinclude -D__KERNEL__

CFLAGS := $(ARCH_FLAGS) -std=gnu11 -O2 -g -Wall -Werror \
	  -ffreestanding -fno-common -fno-PIE -fno-pic \
	  -fno-strict-aliasing -fno-stack-protector -fno-omit-frame-pointer \
	  -fno-asynchronous-unwind-tables -fno-unwind-tables \
	  -mno-save-restore -mstrict-align -mno-relax \
	  -mno-riscv-attribute -Wa,-mno-arch-attr

# Assembly needs -fno-PIE too: under default PIE, "la" becomes a GOT load.
AFLAGS := $(ARCH_FLAGS) -D__ASSEMBLY__ -fno-PIE -g -mno-relax \
	  -mno-riscv-attribute -Wa,-mno-arch-attr

LDFLAGS := -nostdlib --orphan-handling=error -z noexecstack --no-warn-rwx-segments

DEPFLAGS = -MMD -MP

objs := arch/riscv/kernel/head.o \
	arch/riscv/kernel/setup.o \
	arch/riscv/mm/init.o \
	init/main.o

lds := arch/riscv/kernel/vmlinux.lds

QEMU_FLAGS := -machine virt -cpu rv64 -m 128M -smp 1 -nographic

all: Image System.map

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CPPFLAGS) $(AFLAGS) $(DEPFLAGS) -c $< -o $@

$(lds): $(lds).S
	$(CC) $(CPPFLAGS) -D__ASSEMBLY__ -E -P -x c $(DEPFLAGS) -MT $@ -MF $@.d $< -o $@

vmlinux: $(objs) $(lds)
	$(LD) $(LDFLAGS) -T $(lds) -Map=vmlinux.map $(objs) -o $@

Image: vmlinux
	$(OBJCOPY) -O binary -R .note -R .note.gnu.build-id -R .comment -S $< $@

System.map: vmlinux
	$(NM) -n $< > $@

check: vmlinux
	CROSS_COMPILE=$(CROSS_COMPILE) sh scripts/check-vmlinux.sh vmlinux

qemu: Image
	$(QEMU) $(QEMU_FLAGS) -bios default -kernel Image

qemu-gdb: Image
	$(QEMU) $(QEMU_FLAGS) -bios default -kernel Image -s -S

gdb: vmlinux
	$(GDB) -x scripts/gdb-early.gdb

clean:
	rm -f $(objs) $(objs:.o=.d) $(lds) $(lds).d vmlinux vmlinux.map Image System.map

-include $(objs:.o=.d) $(lds).d

.PHONY: all check qemu qemu-gdb gdb clean
```

**Walkthrough.** Recipe lines must begin with a **tab** character. `qemu`, `qemu-gdb` and `check` depend on what they use, so `make qemu` rebuilds first. That was the dependency question you raised about v1's `qemu-baremetal` target. `-x c` tells GCC to preprocess the `.lds.S` file as C, and `-P` removes `#line` markers that ld would reject. `-include` loads depfiles if they exist, and the leading `-` means "do not fail on the first build, when none exist yet". `qemu-fw` is added in K1 together with the firmware.

### 5.9 `scripts/check-vmlinux.sh`

```sh
#!/bin/sh
# SPDX-License-Identifier: GPL-2.0
# K0 layout contract checks for vmlinux. Prints PASS/FAIL lines; exit 1 on any failure.
set -eu

V=${1:-vmlinux}
X=${CROSS_COMPILE:-riscv64-linux-gnu-}
fail=0

ok()  { echo "PASS: $1"; }
bad() { echo "FAIL: $1"; fail=1; }

addr() { "${X}nm" "$V" | awk -v s="$1" '$3 == s { print $1; exit }'; }

# 1. Link address and load address of the first loadable segment.
seg=$("${X}readelf" -lW "$V" | awk '$1 == "LOAD" { print $3, $4; exit }')
[ "$seg" = "0xffffffff80000000 0x0000000080200000" ] \
	&& ok "first LOAD segment VMA/LMA = $seg" \
	|| bad "first LOAD segment VMA/LMA = $seg (expected 0xffffffff80000000 0x0000000080200000)"

[ "$(addr _start)" = "ffffffff80000000" ] && ok "_start at KERNEL_LINK_ADDR" || bad "_start = $(addr _start)"

# 2. Every zero-initialised object (b, B, s, S) lies inside [__bss_start, __bss_stop).
#    __bss_stop and _end are end markers, so they are equal to the upper bound.
lo=$(addr __bss_start); hi=$(addr __bss_stop)
out=$("${X}nm" "$V" | awk -v lo="x$lo" -v hi="x$hi" '
	$2 ~ /^[bBsS]$/ && $3 != "__bss_stop" && $3 != "_end" {
		a = "x" $1; if (a < lo || a >= hi) print $3 }')
[ -z "$out" ] && ok "all .bss/.sbss symbols inside [__bss_start, __bss_stop)" \
	|| bad "symbols outside bss range: $out"

# 3. No dynamic-linking, run-time relocation or unwind sections.
secs=$("${X}readelf" -SW "$V" | grep -E '\.(got|plt|dynamic|interp|eh_frame|rela)' || true)
[ -z "$secs" ] && ok "no GOT/PLT/dynamic/rela/eh_frame sections" || bad "unexpected sections: $secs"

# 4. _end is page aligned (later milestones hand out memory from there).
case "$(addr _end)" in
	*000) ok "_end page aligned ($(addr _end))" ;;
	*) bad "_end not page aligned ($(addr _end))" ;;
esac

exit $fail
```

**Why the `"x"` prefix in awk?** `nm` prints 16 hex digits. awk would convert a value such as `ffffffff80003000` to a floating-point number and lose precision. Prefixing both sides with `x` forces a **string** comparison. Fixed-width lowercase hex strings compare in the same order as the numbers. Check 2 is the automatic guard against the v1 bug, even if someone later switches orphan handling to `warn`.

### 5.10 `scripts/gdb-early.gdb`

```gdb
# K0: stop at the first kernel instruction while the MMU is still off.
set architecture riscv:rv64
target remote :1234
# Symbols are linked at 0xffffffff80000000 but run at 0x80200000 with the MMU off.
# Load them shifted by (0x80200000 - 0xffffffff80000000) mod 2^64 = 0x100200000.
symbol-file -o 0x100200000 vmlinux
hbreak *0x80200000
continue
info registers pc a0 a1
x/2i $pc
```

GDB knows symbols only at their VMAs. While the MMU is off, the PC is physical, so `break _start` would never hit. `symbol-file -o OFFSET` shifts every symbol by `0x80200000 - 0xffffffff80000000`, which wraps to `0x100200000` in 64-bit arithmetic. `hbreak` uses a hardware breakpoint, so GDB never writes into memory before OpenSBI has loaded the kernel there. After K3 enables the MMU, you load symbols **without** an offset (two symbol views, one per address space).

---

## 6. Build, Check, Run, Debug

All commands run in the container from the `my-linux` root:

```powershell
podman run --rm -it -v "C:\workspace\career_ws\my-linux:/ws:Z" -w /ws localhost/xv6-lab bash
```

Inside the container:

```sh
make                 # vmlinux, Image, System.map, vmlinux.map
make check           # five PASS lines expected
make qemu            # OpenSBI banner, then the K0 block; quit with Ctrl-A then x
readelf -lW vmlinux  # (riscv64-linux-gnu-readelf) VirtAddr vs PhysAddr
objdump -h vmlinux   # (riscv64-linux-gnu-objdump) section VMA/LMA table from section 4
od -A x -t x1 -N 64 Image   # Image header bytes from 3.10
```

Expected tail of `make qemu`:

```text
Boot HART MEDELEG         : 0x0000000000f0b509

my-linux v2 K0
boot_cpu_hartid    0x0000000000000000
_dtb_early_pa      0x0000000087e00000
_start (PC-rel)    0x0000000080200000
_end (PC-rel)      0x0000000080208000
&k0_small_zero     0x0000000080203010
k0_abs_ptr value   0xffffffff800010e8
K0 layout: PASS
```

Exact text addresses such as `0x800010e8` can change if you format strings differently. The **pattern** must match: physical `0x8020xxxx` for computed addresses and `0xffffffff8xxxxxxx` for the stored pointer.

GDB needs QEMU running at the same time. The simplest method is one container shell, with QEMU in the background and its console sent to a log file. With `-nographic`, QEMU would otherwise compete with GDB for the terminal:

```sh
make qemu-gdb < /dev/null > /tmp/qemu.log 2>&1 &   # QEMU waits for GDB at reset
make gdb             # stops at _start, physical 0x80200000, head.S line 16
# in gdb: continue, then Ctrl-C, then quit; afterwards: cat /tmp/qemu.log; kill %1
```

Tutor-run GDB result:

```text
Breakpoint 1, _start () at arch/riscv/kernel/head.S:16
16		j	_start_kernel
pc             0x80200000	0x80200000 <_start>
a0             0x0	0
a1             0x87e00000	2279604224
```

Optional extra checks that the tutor ran:

```sh
qemu-system-riscv64 -machine virt -cpu rv64 -m 128M -smp 1 -nographic -bios default -kernel vmlinux   # ELF loads at PhysAddr: PASS
qemu-system-riscv64 -machine virt -cpu rv64 -m 128M -smp 4 -nographic -bios default -kernel Image     # run several times; hartid may be non-zero: PASS
```

---

## 7. Deviations From Upstream (named, temporary or permanent)

| Deviation | Upstream | Why K0 differs | Until |
| --- | --- | --- | --- |
| LMA pinned to `0x80200000` with `AT()` | No `AT()`; physical base found at run time (`load_pa = auipc`) | Lets QEMU/GDB load `vmlinux` as ELF and makes VMA/LMA visible in `readelf` | K3 computes `load_pa` at run time anyway; `AT()` may stay as a debug aid |
| `-mno-relax`, no `gp` | Relaxation on; `__global_pointer$` and `load_global_pointer` | Removes one stored-address register that must be reloaded around the MMU switch | Permanent for v2 (named in interviews as a trade-off) |
| DTB address stored by `head.S` into `_dtb_early_pa` | `head.S` passes `a1` to `setup_vm()`, which stores `dtb_early_pa` | No `setup_vm()` exists yet | K3 |
| Polled NS16550 at physical `0x10000000` in `init/main.c` | `earlycon` / SBI console, then the 8250 driver | Nothing else exists to print with | K1 (SBI console) |
| `sstatus.FS/VS` not cleared | `csrc CSR_STATUS, SR_FS_VS` in `head.S` | Needs `asm/csr.h` | K2 |
| 16 KB `init_stack` symbol | `init_thread_union` + `init_task` in `tp` | No `task_struct` yet | K8 |
| `.option norvc` for `code0` | Assembler may compress `j` | Byte-exact match with the documented `u32 code0` | Permanent, harmless |

---

## 8. Worked Trace: Power-On to `K0 layout: PASS`

```text
QEMU reset vector 0x1000 (M-mode, tiny ROM)
  a0 = mhartid, a1 = DTB 0x87e00000, a2 = &fw_dynamic_info
  jump 0x80000000
        |
OpenSBI fw_dynamic (M-mode, 0x80000000)
  cold-boot lottery picks ONE hart (may be hart 1 on -smp 4)
  sets PMP, medeleg=0xf0b509, mideleg=0x1666, menvcfg.STCE (Sstc)
  next_addr = 0x80200000, next_mode = S   (QEMU loaded Image there)
  mret
        |
_start  PC = 0x80200000 (physical), satp = 0, S-mode
  j _start_kernel                       (jump over 64-byte header)
_start_kernel  PC = 0x80200040
  sie = sip = 0
  t0 = auipc-based &__bss_start = 0x80203000 (physical!)
  t1 = auipc-based &__bss_stop  = 0x80208000
  zero 0x5000 bytes: .sbss (boot_cpu_hartid, _dtb_early_pa, k0_small_zero) + init_stack + k0_big_zero
  boot_cpu_hartid = a0, _dtb_early_pa = a1
  sp = 0x80207020 (init_stack_top), s0 = 0
  tail start_kernel
        |
start_kernel  PC = 0x802001xx
  prints computed addresses -> physical
  prints stored pointer      -> virtual 0xffffffff800010e8
  checks .sbss == 0, .sdata == 42, .bss == 0, &k0_small_zero in [__bss_start, __bss_stop)
  "K0 layout: PASS"; wfi loop
```

---

## 9. Acceptance Checklist

- [ ] Branch `v2`, all 14 files typed by you; `git status` shows no build outputs (`.gitignore` works) and `reference/` stays untracked.
- [ ] `make` succeeds with `-Werror` and `--orphan-handling=error`.
- [ ] `make check` prints five `PASS` lines.
- [ ] `readelf -lW vmlinux`: first `LOAD` has VirtAddr `0xffffffff80000000`, PhysAddr `0x0000000080200000`.
- [ ] `System.map`: `boot_cpu_hartid`, `_dtb_early_pa` and `k0_small_zero` lie between `__bss_start` and `__bss_stop`.
- [ ] `od` of `Image`: bytes `6f 00 00 04` at 0, `RISCV` at 0x30, `RSC\x05` at 0x38.
- [ ] `make qemu` prints `K0 layout: PASS`.
- [ ] `make qemu-gdb` + `make gdb` stops at `head.S:16`, PC `0x80200000`, `a1 = 0x87e00000`.
- [ ] **Negative test (on a throwaway copy or a commit you revert):** delete the `.sbss` line from the script and confirm the link fails with an orphan error.
- [ ] Commit on `v2` with a message that you write.

Report the results as a short paste: the `make check` output and the last eight QEMU lines. The tutor records them in the tracker as learner-run evidence.

---

## 10. Arm64 Comparison (for interviews)

| Concept | RISC-V (this lesson) | Arm64 upstream |
| --- | --- | --- |
| Link address | `KERNEL_LINK_ADDR` top 2 GB | `KIMAGE_VADDR` in the upper (`TTBR1`) half |
| PC-relative addressing | `auipc` + `addi`/`ld` (`medany`) | `adrp` + `add`/`ldr` (`-mcmodel=small`, +/-4 GB) |
| Image header | 64 bytes, `RISCV`/`RSC\x05` magic, `text_offset` | 64 bytes, `ARM\x64` magic at 0x38, `text_offset`, `image_size`, flags |
| Entry state | S-mode, `a0` hartid, `a1` DTB, MMU off | EL1 (or EL2), `x0` DTB, MMU off, D-cache off |
| BSS | Cleared by `head.S` | Cleared in C before the MMU switch: `memset(__bss_start, ...)` in `arch/arm64/kernel/pi/map_kernel.c:252` |
| Firmware below the kernel | OpenSBI (M-mode) | TF-A BL31 (EL3), PSCI |

The same lesson applies on Arm: computed addresses follow the PC, stored addresses carry the link address, and the early code must use only the first kind until the MMU is on.

---

## 11. Interview Answers

**Q1. What is the difference between VMA and LMA, and where do you see both?**
VMA is the address the code is linked to run at; LMA is where the loader places the bytes. In a kernel they differ because the image is linked high (Linux RISC-V: `0xffffffff80000000`) but loaded into physical RAM (`0x80200000`) with the MMU off. `readelf -l` shows them as `VirtAddr` and `PhysAddr`, and `AT()` in a linker script sets the LMA. Early boot code must work at the LMA until the MMU maps the VMA.

**Q2. Why can a kernel run C code before the MMU is enabled if it is linked at a high virtual address?**
Because RISC-V `medany` code (and Arm64 `adrp` code) computes addresses PC-relatively, and the VMA-LMA difference is the same for every byte, so PC-relative distances are identical in both views. The exception is any address **stored** in memory, such as an initialised pointer, a GOT entry or `gp`, because it holds the link-time VMA. Upstream therefore avoids stored addresses before `relocate_enable_mmu` and reloads `gp` afterwards.

**Q3. What are PIC/PIE and the GOT, and why does the kernel build with `-fno-PIE`?**
PIE code finds globals through the Global Offset Table, which a dynamic loader fills with absolute addresses at start-up. A kernel has no dynamic loader, so GOT entries would be wrong. Upstream passes `-fno-PIE` to both C and assembly. In my K0 build, missing it in `AFLAGS` turned `la` into GOT loads and the link stopped on `.got` orphans. Linux's optional `CONFIG_RELOCATABLE` uses PIE deliberately and applies its own `.rela.dyn` relocations at boot.

**Q4. What are RISC-V code models?**
`medlow` uses `lui` for absolute addresses within +/-2 GB of address 0. `medany` uses `auipc` for addresses within +/-2 GB of the PC. A kernel linked high but started at a physical address needs `medany`, because `medlow` would produce the link-time addresses. Upstream RV64 Linux uses `medany`.

**Q5. What are small-data sections, and how did they break my first kernel?**
GCC puts globals of 8 bytes or less in `.sdata`/`.sbss` so they can be reached through `gp` with one instruction after linker relaxation. My linker script did not name `.sbss`, so ld placed it after `_end`. It was never cleared, and the page allocator later overwrote it with `mem_map`, which caused a store fault with a garbage address. The fix was to name `.sbss` inside the BSS range as upstream `BSS_SECTION` does, link with `--orphan-handling=error`, and audit with `nm`.

**Q6. Why do kernels disable unwind tables and floating point?**
`.eh_frame` exists for C++ exceptions and generic unwinders. The kernel uses frame pointers (or ORC on x86) for backtraces, so unwind tables only cost space. FP registers hold user state that the kernel would otherwise have to save on every entry. Building with an integer ABI and disabling `sstatus.FS` turns accidental FP use into a trap instead of silent corruption.

**Q7. Why does the kernel clear its own BSS?**
The flat `Image` contains only bytes up to `_edata`. The header's `image_size` tells the loader how much memory to reserve, but no boot protocol promises that the reserved tail is zero. `head.S` clears `[__bss_start, __bss_stop)` before any C code runs, and it saves boot arguments only after the clear.

**Q8. Which hart boots the kernel?**
Under SBI HSM firmware, exactly one hart enters the kernel, chosen by firmware (OpenSBI uses a cold-boot lottery), and it need not be hart 0. The kernel records it as `boot_cpu_hartid` and later starts other harts with `sbi_hart_start`. Older spin-wait booting sends all harts in, and then the kernel runs its own lottery.

---

## 12. Optional Private Reconstruction

No answers need to be submitted. If you want to test your understanding, close this note and, on paper, explain to an imagined 12-year-old:

1. Draw the VMA/LMA picture from 3.1 and mark where the PC is when `_start` runs.
2. Explain why `(unsigned long)_start` printed a physical address but `k0_abs_ptr` printed a virtual one.
3. Write the BSS range from memory, and say which two small variables v1 lost and why.

Where the explanation gets vague, reread that subsection. That gap is your next study target.

---

## 13. Corrections

- **2026-10-22, line endings (3.11).** The note said `.gitattributes` "prevents this permanently". Wrong: it affects only files that Git writes or stages. The learner's typed files were CRLF, and `make check` failed with `set: Illegal option -`. The tutor confirmed that all 14 files matched the reference except for CR bytes, converted them to LF, and the learner's tree then passed `make check` 5/5 and printed `K0 layout: PASS`. 3.11 now describes the editor setting and a check command.
- **2026-10-22, section 4 sizes.** The table showed `.head.text` as 0x98 and `.text` at `...098`. Those numbers came from a draft `head.S` that still had an 8-byte park loop. The published `head.S` gives `.head.text` 0x90 and `.text` at `ffffffff80000090`, as the learner's `readelf -SW` shows.
