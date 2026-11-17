# K1b-1: Talking to the Firmware (`sbi_ecall`)

> **Verified:** `riscv64-linux-gnu-gcc` 13.3.0, GNU ld 2.42, QEMU 8.2.2, gdb-multiarch 15.1, upstream Linux 7.3-rc4 (`f0100363d`), 2026-10-22. Tutor-run in a private copy (K0 + K1a + this lesson):
> - `make` has no warnings; `make check` passes 5/5.
> - `K0 layout: PASS` printed through the firmware, under both OpenSBI (`make qemu`) and my-sbi (`make qemu-fw`).
> - `qemu -d int` counted **280** `supervisor_ecall` traps for **280** printed characters: one firmware call per character.
> - GDB stopped inside my-sbi's `sbi_ecall_handler` with `a7 = 1`, `mepc = __sbi_ecall + 6`, and showed the full backtrace down to the UART driver.

| Item | Value |
| --- | --- |
| Milestone | K1b, part 1 of 3. Before: [K1a firmware](K01a_firmware_and_boot_contract.md). Next: K1b-2 (printk) |
| Repository | `my-linux`, branch `v2` (K0 + K1a committed as `3fddd0d`) |
| You write | 4 new files, 2 changed files (section 3) |
| Upstream | `arch/riscv/kernel/sbi_ecall.c` (20-48), `arch/riscv/kernel/sbi.c` (58-62), `arch/riscv/include/asm/sbi.h` (15, 561-576), `include/linux/types.h` (41) |

---

## 0. Know This First

Read [K1a section 0](K01a_firmware_and_boot_contract.md#0-know-this-first) first: registers, CSRs, modes, traps and assembly. Then the following five points.

**0.1 How C passes arguments.** The compiler puts arguments 1 to 8 in registers `a0` to `a7`. The return value comes back in `a0`, plus `a1` for a second word.

```text
f(10, 20, 30)            ->  a0 = 10, a1 = 20, a2 = 30, then jump to f
struct {long x; long y;} ->  returned as  a0 = x, a1 = y
```

**0.2 What `ecall` does when the kernel runs it.** `ecall` has no operands. It means "trap to the more privileged level". From S-mode it is cause 9, which the firmware did **not** delegate (K1a section 5.1), so the CPU enters the firmware:

```text
mepc = address of ecall, mcause = 9, mode = M, pc = firmware's trap handler
```

The firmware reads the request from the registers, does the job, puts its answer in `a0`/`a1`, and returns with `mret` to the instruction after `ecall`.

**0.3 SBI call rules** (the agreement between kernel and firmware):

```text
a7 = extension ID (which service)    a6 = function ID    a0..a5 = arguments
answer: a0 = error (0 = OK)          a1 = value
```

**0.4 Inline assembly.** C has no `ecall` statement, so GCC lets C code contain one instruction:

```c
asm volatile ("instruction" : outputs : inputs : clobbers);
```

The details are explained in unit 3, where it is used.

**0.5 Legacy putchar.** The oldest SBI service, "console putchar", is extension `0x1`: "print the character in `a0`". Both OpenSBI v1.3 and my-sbi support it.

---

## 1. Goal

**Problem.** In K0 the kernel printed by writing the UART chip's registers itself (`k0_putc` wrote `0x10000000`). That has three problems:
- The UART address is board-specific. A different board needs different kernel code.
- The firmware also drives the same UART (its banner), so two drivers own one chip.
- Upstream Linux does not do this. It uses `earlycon=sbi`: early messages go **through the firmware**.

**Solution.** Ask the firmware to print, with an SBI call:

```text
K0:     kernel k0_putc  ---------------------------------------->  UART (0x10000000)

K1b-1:  kernel k0_putc -> sbi_console_putchar -> __sbi_ecall
                           ecall ===> firmware -> sbi_putc -> uart8250_putc -> UART
```

The kernel no longer knows where the UART is. Only the firmware does.

**Expected output** (`make qemu` and `make qemu-fw`):

```text
my-linux v2 K1b-1 (output via SBI)
boot_cpu_hartid    0x0000000000000000
_dtb_early_pa      0x0000000087e00000
_start (PC-rel)    0x0000000080200000
_end (PC-rel)      0x0000000080208000
&k0_small_zero     0x0000000080203010
k0_abs_ptr value   0xffffffff800010f8
K0 layout: PASS
```

It is the same report as K0. Only the path to the screen changed.

---

## 2. The Files in This Lesson

```text
my-linux/
|-- include/linux/types.h            NEW   uintptr_t
|-- arch/riscv/include/asm/sbi.h     NEW   SBI numbers, struct sbiret, prototypes
|-- arch/riscv/kernel/sbi_ecall.c    NEW   __sbi_ecall(): the only ecall in the kernel
|-- arch/riscv/kernel/sbi.c          NEW   sbi_console_putchar()
|-- init/main.c                      CHANGED  k0_putc() calls the firmware
`-- Makefile                         CHANGED  2 objects, qemu-fw target
```

Type them in this order: each file only uses files above it. Save with **LF** line endings.

---

## 3. The Units

### 3.1 `include/linux/types.h` (new)

**Why.** Unit 3.3 needs an integer type exactly as wide as a pointer (a register is 64 bits). We use `-nostdinc`, so `<stdint.h>` is not available (K0 section 3.8).

**What.** One typedef, copied from upstream `include/linux/types.h` line 41. More types are added when a later lesson needs them.

**Code** (`include/linux/types.h`):

```c
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TYPES_H
#define _LINUX_TYPES_H

/* An integer wide enough to hold a pointer (upstream include/linux/types.h). */
typedef unsigned long		uintptr_t;

#endif /* _LINUX_TYPES_H */
```

**How it works.** `uintptr_t x = (uintptr_t)p;` stores the pointer `p` as a number. Example: `p = (void *)0x80200000` gives `x = 0x80200000`.

### 3.2 `arch/riscv/include/asm/sbi.h` (new)

**Why.** Kernel and firmware must agree on numbers (`0x1` = putchar) and on the shape of the answer. One header holds that agreement.

**What.** A reduced copy of upstream `arch/riscv/include/asm/sbi.h`, with the same names:
- `enum sbi_ext_id`: service numbers (line 15). Only putchar for now.
- `struct sbiret`: the answer, `error` and `value` (line 561).
- the `__sbi_ecall()` prototype and the `sbi_ecall()` macro (lines 568-574).
- the `sbi_console_putchar()` prototype (line 576).

**Technical.** The SBI rules (0.3) list the request as "extension, function, arguments". But `__sbi_ecall()` takes its parameters as `arg0..arg5, fid, ext`. Why? By the calling convention (0.1), parameter 1 lands in `a0` and parameter 8 in `a7`. With this order, every value is **already in the register `ecall` needs**, so no copying is needed. The macro lets callers keep writing the friendly order:

```text
caller writes:   sbi_ecall(EXT,  FID,  A0, A1, A2, A3, A4, A5)
macro produces: __sbi_ecall(A0, A1, A2, A3, A4, A5, FID, EXT)
registers:                  a0  a1  a2  a3  a4  a5  a6   a7
```

**Code** (`arch/riscv/include/asm/sbi.h`):

```c
/* SPDX-License-Identifier: GPL-2.0-only */
/* Reduced arch/riscv/include/asm/sbi.h: names and values match upstream. */
#ifndef _ASM_RISCV_SBI_H
#define _ASM_RISCV_SBI_H

#include <linux/types.h>

/* Extension IDs: the value the kernel puts in a7 to choose a firmware service. */
enum sbi_ext_id {
	SBI_EXT_0_1_CONSOLE_PUTCHAR = 0x1,
};

/* What every SBI call returns: a0 = error, a1 = value. */
struct sbiret {
	long error;
	long value;
};

struct sbiret __sbi_ecall(unsigned long arg0, unsigned long arg1,
			  unsigned long arg2, unsigned long arg3,
			  unsigned long arg4, unsigned long arg5,
			  int fid, int ext);

/* Callers write (extension, function, args); the function wants args first. */
#define sbi_ecall(e, f, a0, a1, a2, a3, a4, a5)	\
		__sbi_ecall(a0, a1, a2, a3, a4, a5, f, e)

void sbi_console_putchar(int ch);

#endif /* _ASM_RISCV_SBI_H */
```

**How it works (example).**

```text
sbi_ecall(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, 'A', 0, 0, 0, 0, 0)
  preprocessor  ->  __sbi_ecall('A', 0, 0, 0, 0, 0, 0, 0x1)
  at the call   ->  a0 = 0x41, a1..a6 = 0, a7 = 0x1
```

**Syntax.**
- `enum sbi_ext_id { SBI_EXT_0_1_CONSOLE_PUTCHAR = 0x1, };` gives the number `0x1` a name. Upstream lists all services here; we add each one when a lesson uses it.
- In `#define sbi_ecall(e, f, a0, ...)`, the names `a0`, `a1`... are only macro parameter names. They are not registers yet.

### 3.3 `arch/riscv/kernel/sbi_ecall.c` (new)

**Why.** Something must load the registers and execute `ecall`. Upstream keeps this in **one** function, so every SBI call in the kernel goes through one place (useful for tracing).

**What.** `__sbi_ecall()`: put 8 values in `a0`-`a7`, run `ecall`, return `a0`/`a1` as a `struct sbiret`. Copied from upstream `arch/riscv/kernel/sbi_ecall.c` lines 20-48, without its two tracepoint lines.

**Technical.** C cannot say "put this in register `a7`", so two GCC features are used:

```text
register uintptr_t a7 asm ("a7") = ext;    1. pin: variable a7 lives in register a7
asm volatile ("ecall" : ... );             2. run one instruction, telling GCC which
                                              registers it reads and changes
```

**Code** (`arch/riscv/kernel/sbi_ecall.c`):

```c
// SPDX-License-Identifier: GPL-2.0
/* The one place that executes ecall (upstream arch/riscv/kernel/sbi_ecall.c). */
#include <asm/sbi.h>

struct sbiret __sbi_ecall(unsigned long arg0, unsigned long arg1,
			  unsigned long arg2, unsigned long arg3,
			  unsigned long arg4, unsigned long arg5,
			  int fid, int ext)
{
	struct sbiret ret;

	register uintptr_t a0 asm ("a0") = (uintptr_t)(arg0);
	register uintptr_t a1 asm ("a1") = (uintptr_t)(arg1);
	register uintptr_t a2 asm ("a2") = (uintptr_t)(arg2);
	register uintptr_t a3 asm ("a3") = (uintptr_t)(arg3);
	register uintptr_t a4 asm ("a4") = (uintptr_t)(arg4);
	register uintptr_t a5 asm ("a5") = (uintptr_t)(arg5);
	register uintptr_t a6 asm ("a6") = (uintptr_t)(fid);
	register uintptr_t a7 asm ("a7") = (uintptr_t)(ext);
	asm volatile ("ecall"
		      : "+r" (a0), "+r" (a1)
		      : "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
		      : "memory");
	ret.error = a0;
	ret.value = a1;

	return ret;
}
```

**How it works (example).** `__sbi_ecall(0x41, 0, 0, 0, 0, 0, 0, 1)`:

```text
1. arguments arrive (calling convention): a0 = 0x41 ... a6 = 0, a7 = 1
2. register variables: already in the right registers, no copy
3. ecall: firmware prints 'A', sets a0 = 0 (OK), leaves a1 = 0
4. ret.error = a0 = 0, ret.value = a1 = 0
5. return {0, 0}
```

Tutor-run disassembly shows exactly that: no register moves, just `ecall`.

```text
<__sbi_ecall>:   addi sp,sp,-32 ; sd s0,24(sp) ; addi s0,sp,32    (frame setup)
                 ecall
                 ld s0,24(sp) ; addi sp,sp,32 ; ret
```

**Syntax** (the inline assembly, one part at a time):

| Part | Meaning |
| --- | --- |
| `register uintptr_t a0 asm ("a0") = ...;` | this local variable must live in register `a0` |
| `asm volatile ("ecall" ...)` | emit this instruction; `volatile`: never delete or move it |
| `: "+r" (a0), "+r" (a1)` | outputs: `a0`/`a1` go in **and** come back changed (`+`) |
| `: "r" (a2), ... "r" (a7)` | inputs only: the firmware must not change them (SBI rule) |
| `: "memory"` | the firmware may read or write memory: finish all stores before `ecall` |

### 3.4 `arch/riscv/kernel/sbi.c` (new)

**Why.** Callers should say "print this character", not "extension 1, function 0". `sbi.c` holds one small, named function per SBI service.

**What.** `sbi_console_putchar()`, copied from upstream `arch/riscv/kernel/sbi.c` lines 58-62. K1b-3 adds more functions here (version, probe, power off).

**Code** (`arch/riscv/kernel/sbi.c`):

```c
// SPDX-License-Identifier: GPL-2.0-only
/* SBI helper functions (upstream arch/riscv/kernel/sbi.c). K1b-1: console only. */
#include <asm/sbi.h>

/*
 * Legacy SBI v0.1 putchar: print one character through the firmware.
 * Deprecated in newer SBI versions, but every firmware we use supports it.
 */
void sbi_console_putchar(int ch)
{
	sbi_ecall(SBI_EXT_0_1_CONSOLE_PUTCHAR, 0, ch, 0, 0, 0, 0, 0);
}
```

**How it works (example).** `sbi_console_putchar('A')`:

```text
input:  ch = 0x41 (already in a0)
calls:  sbi_ecall(0x1, 0, 0x41, 0, 0, 0, 0, 0) -> __sbi_ecall(...)
output: 'A' on the screen; the answer {0, 0} is ignored
```

Tutor-run disassembly: the function sets `a7 = 1` and `a1`-`a6 = 0`, then **jumps** (does not call) to `__sbi_ecall`. `ch` is never moved, because it is already in `a0`.

### 3.5 `init/main.c` (changed)

**Why.** The K0 report must now print through the firmware.

**What changed (only these):**
1. Added `#include <asm/sbi.h>`.
2. Deleted the four `K0_UART_*` defines.
3. `k0_putc()` body: the UART busy-wait and register write became `sbi_console_putchar(c);`.
4. `k0_puts()`: no longer sends `'\r'` before `'\n'`. Both firmwares do that now (in my-sbi: `sbi_putc()` in `firmware/sbi_console.c`).
5. Banner text: `my-linux v2 K1b-1 (output via SBI)`.

The `k0_*` helpers are still temporary. K1b-2 replaces them with `printk()`.

**Code** (`init/main.c`, complete file):

```c
// SPDX-License-Identifier: GPL-2.0-only
/*
 * K0 start_kernel(): prove the build and layout contract.
 * K1b-1: output now goes through the firmware (SBI) instead of the UART.
 * The k0_* helpers are temporary; K1b-2 replaces them with printk().
 */
#include <asm/sbi.h>

extern unsigned long boot_cpu_hartid;
extern unsigned long _dtb_early_pa;
extern char _start[], _end[], __bss_start[], __bss_stop[];

/* The firmware owns the UART now: one SBI call per character. */
static void k0_putc(char c)
{
	sbi_console_putchar(c);
}

/* No '\r' here: both firmwares turn '\n' into "\r\n" themselves. */
static void k0_puts(const char *s)
{
	for (; *s; s++)
		k0_putc(*s);
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
	k0_puts("\nmy-linux v2 K1b-1 (output via SBI)\n");
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

**How it works (example).** `k0_puts("hi\n")`:

```text
'h' -> k0_putc('h') -> sbi_console_putchar('h') -> ecall #1 -> firmware -> 'h'
'i' -> ... ecall #2 -> 'i'
'\n'-> ... ecall #3 -> firmware sends '\r' then '\n'
output on screen: "hi" + new line; 3 traps into the firmware
```

`k0_puthex(0x2a)` prints `0x000000000000002a`: 2 + 16 = 18 characters, so 18 traps.

### 3.6 `Makefile` (changed)

**Why.** The two new `.c` files must be compiled and linked. A `qemu-fw` target makes "boot with my-sbi" one command.

**What changed (only these):**
1. `objs`: added `arch/riscv/kernel/sbi_ecall.o` and `arch/riscv/kernel/sbi.o`.
2. New `FW_BIN`, `BIOS ?= default` and targets `fw` and `qemu-fw`. `qemu` and `qemu-gdb` use `-bios $(BIOS)`.
3. `clean` also cleans `firmware/`.
4. `.PHONY` lists `fw` and `qemu-fw`.

**Code** (`Makefile`, complete file):

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
	arch/riscv/kernel/sbi_ecall.o \
	arch/riscv/kernel/sbi.o \
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

# Firmware choice: BIOS=default is QEMU's OpenSBI; qemu-fw uses our my-sbi.
FW_BIN := firmware/fw.bin
BIOS ?= default

fw:
	$(MAKE) -C firmware

qemu: Image
	$(QEMU) $(QEMU_FLAGS) -bios $(BIOS) -kernel Image

qemu-fw: Image fw
	$(MAKE) qemu BIOS=$(FW_BIN)

qemu-gdb: Image
	$(QEMU) $(QEMU_FLAGS) -bios $(BIOS) -kernel Image -s -S

gdb: vmlinux
	$(GDB) -x scripts/gdb-early.gdb

clean:
	rm -f $(objs) $(objs:.o=.d) $(lds) $(lds).d vmlinux vmlinux.map Image System.map
	$(MAKE) -C firmware clean

-include $(objs:.o=.d) $(lds).d

.PHONY: all check fw qemu qemu-fw qemu-gdb gdb clean
```

**How it works (example).**

```text
make qemu       BIOS = default         -> -bios default         (QEMU's OpenSBI)
make qemu-fw    builds firmware/fw.bin, then runs: make qemu BIOS=firmware/fw.bin
```

**Syntax.**
- `BIOS ?= default` sets `BIOS` only if it is not already set, for example from the command line.
- `$(MAKE) -C firmware` runs `make` inside `firmware/`, using its own Makefile.
- `$(MAKE) qemu BIOS=$(FW_BIN)` runs this Makefile again with `BIOS` overridden.

---

## 4. How It Fits Together: Every Piece That Prints

Many files take part in printing. Each one does **one** job:

| # | Piece | File | Mode | Its one job | Added in |
| --- | --- | --- | --- | --- | --- |
| 1 | `k0_puts`, `k0_line`, `k0_puthex` | `init/main.c` | S | turn the report into characters | K0 (temporary) |
| 2 | `k0_putc` | `init/main.c` | S | hand one character to the SBI layer | K0, changed now |
| 3 | `sbi_console_putchar` | `arch/riscv/kernel/sbi.c` | S | name the service: "putchar" = extension 1 | **K1b-1** |
| 4 | `__sbi_ecall` | `arch/riscv/kernel/sbi_ecall.c` | S | load `a0`-`a7`, run `ecall` | **K1b-1** |
| 5 | `_trap_handler` | `firmware/fw_base.S` | M | switch to the firmware stack, save registers | K1a |
| 6 | `sbi_trap_handler` | `firmware/sbi_trap.c` | M | "is this an `ecall`?" then `mepc += 4` | K1a |
| 7 | `sbi_ecall_handler` | `firmware/sbi_ecall.c` | M | "which service?" (`a7`) -> putchar | K1a |
| 8 | `sbi_putc` | `firmware/sbi_console.c` | M | console rule: send `'\r'` before `'\n'` | K1a |
| 9 | `uart8250_putc` | `firmware/uart8250.c` | M | wait until the chip is ready, write the byte | K1a |

**Why so many pieces?** Each layer hides one detail from the layer above:

```text
 kernel code   "print this report"          does not know about SBI numbers
   sbi.c       "service 1 = putchar"        does not know about registers
   sbi_ecall.c "registers + ecall"          does not know what the service does
 ============  privilege boundary (ecall / mret)  ============
 firmware traps   "save, dispatch, return"  does not know about printing
 sbi_ecall.c      "service 1 -> console"    does not know the chip
 sbi_console.c    "'\n' -> '\r\n'"          does not know the chip
 uart8250.c       "chip registers"          knows only the chip
```

Swap any one layer and the others do not change. For example:
- another board needs only a new UART driver in the **firmware**, and the kernel stays the same;
- OpenSBI replaces rows 5-9 entirely, and the kernel cannot tell.

**The firmware's own banner** (`my-sbi: ...`) uses only rows 8-9. The firmware is already in M-mode, so it calls `sbi_puts()` directly, with no `ecall`.

**What comes next** (K1b-2) replaces rows 1-2:

```text
printk("%lx", v)  ->  vsnprintf (lib/vsprintf.c)  ->  log buffer (kernel/printk/printk.c)
                  ->  earlycon write (drivers/tty/serial/earlycon-riscv-sbi.c)
                  ->  row 3 or a new "write a whole string" SBI call (DBCN)
```

---

## 5. Control Flow

**(a) One line.** Printing one character `K`:

```text
start_kernel -> k0_puts -> k0_putc (init/main.c) -> sbi_console_putchar (sbi.c) -> __sbi_ecall (sbi_ecall.c)
  -> [ecall] -> _trap_handler (fw_base.S) -> sbi_trap_handler (sbi_trap.c) -> sbi_ecall_handler (sbi_ecall.c)
  -> sbi_putc (sbi_console.c) -> uart8250_putc (uart8250.c) -> UART -> [mret] -> back in __sbi_ecall
```

**(b) Step table.** Follow `K` (`0x4b`) from `k0_puts("K0 layout: PASS\n")`:

| # | Where (file : function) | What happens | Data in -> out | Mode |
| --- | --- | --- | --- | --- |
| 1 | `init/main.c : start_kernel` | report the result | `"K0 layout: PASS\n"` -> `k0_puts` | S |
| 2 | `init/main.c : k0_puts` | take the next character | `'K'` -> `k0_putc('K')` | S |
| 3 | `init/main.c : k0_putc` | hand it to SBI | `'K'` -> `sbi_console_putchar(0x4b)` | S |
| 4 | `arch/riscv/kernel/sbi.c : sbi_console_putchar` | choose the service | `0x4b` -> `a7 = 1`, `a0 = 0x4b` | S |
| 5 | `arch/riscv/kernel/sbi_ecall.c : __sbi_ecall` | trap to the firmware | `ecall` | S |
| 6 | CPU | take the trap | `mepc = __sbi_ecall+6`, `mcause = 9`, mode M, `pc = mtvec` | S -> M |
| 7 | `firmware/fw_base.S : _trap_handler` | switch stack, save registers | kernel regs -> frame on firmware stack | M |
| 8 | `firmware/sbi_trap.c : sbi_trap_handler` | check the reason | `mcause 9` -> `sbi_ecall_handler` | M |
| 9 | `firmware/sbi_ecall.c : sbi_ecall_handler` | pick the service | `a7 = 1` -> `sbi_putc(0x4b)`; saved `a0 = 0` | M |
| 10 | `firmware/sbi_console.c : sbi_putc` | console rule | not `'\n'` -> `uart8250_putc(0x4b)` | M |
| 11 | `firmware/uart8250.c : uart8250_putc` | write to the chip | `0x4b` -> UART register `0x10000000` -> `K` on screen | M |
| 12 | `firmware/sbi_trap.c : sbi_trap_handler` | skip the `ecall` | `mepc += 4` | M |
| 13 | `firmware/fw_base.S : _trap_handler` | restore registers, `mret` | `pc = __sbi_ecall+10`, mode S | M -> S |
| 14 | `arch/riscv/kernel/sbi_ecall.c : __sbi_ecall` | build the answer | `a0 = 0`, `a1 = 0` -> `{0, 0}` | S |
| 15 | `init/main.c : k0_puts` | next character | `'0'` -> step 3 again | S |

For `'\n'`, step 10 calls `uart8250_putc` twice: first `'\r'`, then `'\n'`.

**(c) Privilege timeline** (time goes down):

```text
S-mode kernel              CPU hardware                 M-mode firmware              UART
-------------------------  ---------------------------  ---------------------------  ------------
k0_puts("K0 layout...")
sbi_console_putchar('K')
__sbi_ecall: a7=1 a0='K'
ecall  ------------------> mepc = ecall address
                           mcause = 9, mode = M
                           pc = mtvec  --------------->  _trap_handler: save regs
                                                         sbi_trap_handler: cause 9
                                                         sbi_ecall_handler: a7 = 1
                                                         sbi_putc('K')
                                                         uart8250_putc('K')  ------>  THR = 'K'
                                                         mepc += 4, restore regs      'K' shown
                           mode = S, pc = mepc  <------  mret
ret = {0, 0}
next character ...
```

---

## 6. Build, Run, Prove It

From the `my-linux` root, inside the container:

```sh
make                 # kernel; no warnings
make check           # 5 PASS lines
make qemu            # OpenSBI: expected output from section 1; quit: Ctrl-A then x
make qemu-fw         # my-sbi banner, then the same output
```

**Proof 1: one trap per character** (tutor-run: 280 and 280). Run, wait for `PASS`, quit, then count:

```sh
qemu-system-riscv64 -machine virt -cpu rv64 -m 128M -smp 1 -nographic \
    -bios firmware/fw.bin -kernel Image -d int -D /tmp/int.log
grep -c supervisor_ecall /tmp/int.log
```

**Proof 2: watch the firmware handle the call** (GDB):

```sh
qemu-system-riscv64 -machine virt -cpu rv64 -m 128M -smp 1 -nographic \
    -bios firmware/fw.bin -kernel Image -s -S < /dev/null > /tmp/qemu.log 2>&1 &
gdb-multiarch -q firmware/fw.elf -ex 'target remote :1234'
(gdb) add-symbol-file -o 0x100200000 vmlinux     # answer y; kernel symbols at physical addresses
(gdb) hbreak sbi_ecall_handler
(gdb) continue
(gdb) p/x regs->a7          # 0x1  = putchar
(gdb) p/x regs->a0          # 0xa  = '\n', the first character the kernel prints
(gdb) info symbol $mepc     # __sbi_ecall + 6 in section .text of vmlinux
(gdb) delete
(gdb) hbreak uart8250_putc
(gdb) continue
(gdb) bt
```

Tutor-run backtrace. Read it from the bottom up: this is section 5, steps 7-11.

```text
#0  uart8250_putc (ch=13 '\r')               uart8250.c
#1  sbi_putc (ch=10 '\n')                    sbi_console.c   <- '\n' becomes '\r' then '\n'
#2  sbi_ecall_handler (regs=0x80001ba0)      sbi_ecall.c
#3  sbi_trap_handler (regs=0x80001ba0)       sbi_trap.c
#4  _trap_handler ()                         fw_base.S
```

Exact addresses can differ slightly in your build.

---

## 7. Deviations From Upstream

| Here | Upstream | Until |
| --- | --- | --- |
| `k0_*` helpers in `init/main.c` | `printk()` + `earlycon=sbi` | K1b-2 |
| Legacy putchar, one trap per character | Uses DBCN (whole string per trap) when SBI >= 2.0; legacy only with `CONFIG_RISCV_SBI_V01` | K1b-2 |
| `__sbi_ecall` without `trace_sbi_call`/`trace_sbi_return` | Tracepoints around the `ecall` | No tracing yet |
| `asm/sbi.h` and `types.h` hold only what is used | Full headers | Grows per lesson |

---

## 8. Checklist

- [ ] 4 new files and 2 changed files typed, saved with LF.
- [ ] `make` has no warnings; `make check` prints 5 PASS lines.
- [ ] `make qemu` and `make qemu-fw` both end with `K0 layout: PASS`.
- [ ] Proof 1: the trap count equals the number of characters printed.
- [ ] Proof 2: your GDB backtrace matches section 6.
- [ ] Commit on `v2`.

---

## 9. Arm64 Comparison

| RISC-V | Arm64 |
| --- | --- |
| `ecall` from S-mode to M-mode firmware | `smc` from EL1 to EL3 firmware (TF-A) |
| `a7` = extension, `a6` = function | `w0` = function ID (SMCCC) |
| `earlycon=sbi`: firmware prints | No firmware console: `earlycon=pl011,<addr>` writes the UART directly |

---

## 10. Interview Answers

**Q1. How does an early RISC-V Linux kernel print before any driver exists?**
With `earlycon=sbi`, the kernel asks the firmware to print. It puts a service number in `a7` and the data in `a0`, then runs `ecall`. The firmware, which already drives the UART, prints and returns. The kernel needs no board-specific UART code that early.

**Q2. What happens in hardware on `ecall` from S-mode?**
The CPU saves the `ecall` address in `mepc`, sets `mcause = 9`, switches to M-mode and jumps to `mtvec`. The firmware handles the request, adds 4 to `mepc` so it does not repeat the `ecall`, and returns with `mret`.

**Q3. Why does `__sbi_ecall` take `arg0..arg5, fid, ext` in that order?**
The calling convention passes parameters in `a0`-`a7`, and the SBI uses `a0`-`a5` for arguments, `a6` for the function ID and `a7` for the extension ID. With this order every value arrives in the right register, so the compiled function is just `ecall`. A macro keeps the friendlier call order for callers.

**Q4. Why the `"memory"` clobber on the `ecall`?**
The firmware may read memory the kernel just wrote, for example a string buffer for the DBCN call. The clobber forces the compiler to finish all memory writes before the `ecall` and to reload values afterwards.

**Q5. What is the cost of legacy putchar?**
One trap per character: about 280 privilege switches for this short report. That is why SBI 2.0 added the debug console extension, which passes a whole buffer in one call (K1b-2).

---

## 11. Corrections

None yet.
