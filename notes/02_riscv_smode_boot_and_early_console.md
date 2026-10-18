# RISC-V S-Mode Boot, Linker Architecture, and Early Serial Console

## 1. The S-Mode Linker Script (`arch/riscv/kernel/vmlinux.lds.S`)

### Purpose
The GNU Linker script directs the linker (`ld`) on how to assemble compiled object files (`.o`) into an executable ELF kernel image (`vmlinux`), fixing absolute addresses, enforcing 4KB page alignments, and exporting memory boundary symbols.

```ld
OUTPUT_ARCH(riscv)
ENTRY(_start)

SECTIONS
{
    /* Standard OpenSBI S-mode kernel payload entry address */
    . = 0x80200000;

    _stext = .;
    .text : {
        *(.text.init)       /* Places _start at the absolute entry */
        *(.text .text.*)
    }
    . = ALIGN(4096);
    _etext = .;

    _srodata = .;
    .rodata : {
        *(.rodata .rodata.*)
    }
    . = ALIGN(4096);
    _erodata = .;

    _sdata = .;
    .data : {
        *(.data .data.*)
    }
    . = ALIGN(4096);
    _edata = .;

    __bss_start = .;
    .bss : {
        *(.bss.stack)       /* Boot stack allocated inside BSS */
        *(.bss .bss.*)
        *(COMMON)
    }
    . = ALIGN(4096);
    __bss_stop = .;
    _end = .;
}
```

### Critical Rules
1. **Load Address**: Set to `0x80200000` because OpenSBI reserves the first 2MB ($0\text{x}80000000 - 0\text{x}80200000$) for its own execution and jumps to `0x80200000` in S-mode.
2. **Page Alignment (`ALIGN(4096)`)**: Every segment boundary is forced to a 4KB multiple. This allows the Sv39 MMU to map `.text` as Read/Execute (`PTE_R | PTE_X`) and `.rodata` as Read-Only (`PTE_R`) on distinct hardware pages without permission collision.
3. **Symbol Consistency**: Symbols (`__bss_start`, `__bss_stop`, `_end`) must match the assembly and C declarations exactly.

---

## 2. Early Boot Assembly (`arch/riscv/kernel/head.S`)

### Execution Sequence
`head.S` executes before any C code can run, establishing fundamental CPU hygiene:

```assembly
.section .text.init
.global _start

_start:
    /* 1. Mask all supervisor interrupts and clear pending flags */
    csrw sie, zero
    csrw sip, zero

    /* 2. Park secondary harts (only Hart 0 executes the boot sequence) */
    bnez a0, .Lsecondary_park

    /* 3. Setup early boot stack for Hart 0 */
    la   sp, bss_stack_top

    /* 4. Zero out the BSS segment */
    la   t0, __bss_start
    la   t1, __bss_stop
clear_bss_loop:
    bgeu t0, t1, clear_bss_done
    sd   zero, 0(t0)        /* 8-byte store of zero */
    addi t0, t0, 8          /* Advance by 8 bytes */
    j    clear_bss_loop

clear_bss_done:
    /* 5. Clear frame pointer (s0) to anchor stack backtraces */
    li   s0, 0

    /* 6. Register trap vector early in assembly (Linux kernel style) */
    la   t0, handle_exception
    csrw stvec, t0

    /* 7. Handoff to C: a0 (hartid) and a1 (dtb) remain intact */
    tail start_kernel

.Lsecondary_park:
    wfi                     /* Low-power sleep */
    j    .Lsecondary_park

.section .bss.stack
.align 12                   /* 2^12 = 4096 byte alignment */
bss_stack_bottom:
    .space 4096 * 4         /* 16KB boot stack buffer */
bss_stack_top:
```

### Why Initializing `stvec` in `head.S` Matters
In early revisions, trap vectors were registered inside C via `trap_init()`. If an exception (e.g., an unaligned access or instruction fault) occurred prior to `trap_init()`, `stvec` was either uninitialized or pointed to $0\text{x}0$, triggering an unhandled double-fault loop. Configuring `stvec` in `head.S` eliminates this vulnerable unmapped trap window, matching upstream Linux (`head.S::.Lsetup_trap_vector`).

---

## 3. Early Serial Console (`drivers/tty/serial/8250_early.c`)

### The Hardware Dilemma: The Baud Rate Mismatch
A CPU core executes instructions in nanoseconds, whereas a physical UART serial line transmits characters at baud rates (e.g., $115200$ baud) where each character takes $\sim 87\text{ }\mu\text{s}$. If the CPU writes bytes in a loop without polling the hardware status, characters overwrite the single-byte Transmit Holding Register (THR) and output is corrupted.

### The Handshake: Line Status Register (LSR)
The NS16550 UART provides the Line Status Register at offset $+5$ ($0\text{x}10000005$).
- **Bit 5 (`UART_LSR_THRE = 0x20`)**: Transmitter Holding Register Empty.
  - `0`: Transmitter is busy. CPU must poll.
  - `1`: Transmitter is empty. Safe to write the next character.

```c
#define UART_BASE           0x10000000UL
#define UART_TX             0
#define UART_LSR            5
#define UART_LSR_THRE       0x20

static inline void serial_out(unsigned int offset, uint8_t val)
{
    volatile uint8_t *addr = (volatile uint8_t *)(UART_BASE + offset);
    *addr = val;
}

static inline uint8_t serial_in(unsigned int offset)
{
    volatile uint8_t *addr = (volatile uint8_t *)(UART_BASE + offset);
    return *addr;
}

void early_putc(char c)
{
    /* Prepend Carriage Return before Line Feed to prevent staircase output */
    if (c == '\n') {
        early_putc('\r');
    }

    /* Wait until the Transmitter Holding Register is empty */
    while ((serial_in(UART_LSR) & UART_LSR_THRE) == 0) {
        /* Busy poll */
    }

    /* Write character to TX buffer */
    serial_out(UART_TX, (uint8_t)c);
}
```

### The Carriage Return (`\r`) Rule
In serial terminals, Line Feed (`\n`, ASCII $10$) instructs the terminal cursor to drop down by one line, maintaining the same column. Emitting `\r` (ASCII $13$, Carriage Return) before `\n` returns the cursor to column 0, preventing terminal staircase artifacts:
```text
Without \r:
Line 1
      Line 2
            Line 3

With \r\n:
Line 1
Line 2
Line 3
```
