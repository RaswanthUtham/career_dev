### 1. Complete `my-linux/arch/riscv/kernel/head.S`

Here is the complete, clean `my-linux/arch/riscv/kernel/head.S` following the upstream `linux/arch/riscv/kernel/head.S` boot sequence:

```assembly
/* Target File: my-linux/arch/riscv/kernel/head.S */
/* SPDX-License-Identifier: GPL-2.0-only */

.section .text.init
.global _start

_start:
    /* 1. Mask all supervisor interrupts and clear pending flags */
    csrw sie, zero
    csrw sip, zero

    /* 2. Enable time CSR (rdtime) access */
    li   t0, 0x2
    csrw scounteren, t0

    /* 3. Park secondary cores (only Hart 0 runs the boot sequence) */
    bnez a0, .Lsecondary_park

    /* 4. Load initial boot stack pointer */
    la   sp, bss_stack_top

    /* 5. Clear the BSS section (__bss_start to __bss_stop) */
    la   t0, __bss_start
    la   t1, __bss_stop
.Lclear_bss_loop:
    bgeu t0, t1, .Lclear_bss_done
    sd   zero, 0(t0)
    addi t0, t0, 8
    j    .Lclear_bss_loop

.Lclear_bss_done:
    /* 6. Clear frame pointer (s0) to mark bottom of stack backtrace */
    li   s0, 0

    /* 7. Point stvec to early Low-Half trap handler while MMU is still off */
    la   t0, handle_exception
    csrw stvec, t0

    /* 8. Save boot arguments: s1 = hartid (a0), s2 = dtb physical address (a1) */
    mv   s1, a0
    mv   s2, a1

    /* 9. Build page tables in C while MMU is OFF (returns root_phys in a0) */
    call setup_vm

    /* 10. Turn ON Sv39 MMU and relocate PC (ra), sp, and stvec to High Half! */
    li   a1, 0xffffffc000000000
    call relocate_enable_mmu

    /*
     * RIGHT HERE: CPU PC, sp, and stvec are NOW in High Half (0xffffffc08020xxxx)!
     * 11. Update C global pointers (va_pa_offset, swapper_pg_dir, mem_map) to High Half
     */
    call finish_mmu_relocation

    /* 12. Restore a0 = hartid, and convert a1 = dtb + PAGE_OFFSET (High-Half VA!) */
    mv   a0, s1
    li   t0, 0xffffffc000000000
    add  a1, s2, t0
    li   s1, 0
    li   s2, 0

    /* 13. Enter start_kernel(hartid, dtb_va) in High Half! */
    tail start_kernel

.align 2
.Lsecondary_park:
    wfi
    j    .Lsecondary_park

.align 2
relocate_enable_mmu:
    /*
     * a0 = root_phys (Physical address of swapper_pg_dir)
     * a1 = PAGE_OFFSET (0xFFFFFFC000000000)
     */
    /* 1. Relocate Return Address (ra), Stack Pointer (sp), and Frame Pointer (s0) */
    add  ra, ra, a1
    add  sp, sp, a1
    beqz s0, 1f
    add  s0, s0, a1
1:
    /* 2. Relocate stvec to High-Half address of handle_exception */
    la   t0, handle_exception
    add  t0, t0, a1
    csrw stvec, t0

    /* 3. Compute satp value: SATP_MODE_SV39 (8 << 60) | (root_phys >> 12) */
    srli t0, a0, 12
    li   t1, (8 << 60)
    or   t0, t0, t1

    /* 4. Activate Sv39 MMU and flush TLB */
    sfence.vma zero, zero
    csrw satp, t0
    sfence.vma zero, zero

    /* 5. Return to High-Half 'ra' (CPU PC is now in 0xFFFFFFC08020xxxx!) */
    ret

.section .bss.stack
.align 12
bss_stack_bottom:
    .space 4096 * 4
bss_stack_top:
```

---

### 2. What Does `.align N` Do? Why Do We See `.align 2`, `.align 4`, or `.align 12`?

#### The Trap Everyone Falls Into: `.align N` on RISC-V is Powers of 2 ($2^N$), NOT $N$ Bytes!
On RISC-V (and ARM), `.align N` tells the assembler:
> *"Pad the address with zeros until the next address is a multiple of **$2^N$ bytes**!"*
*(In other words, the bottom **$N$ bits** of the address must be `0`!)*

Look at this table so you never get tricked by `.align N`:

```text
+-------------+-------------------+------------------------------------------------------------+
| Directive   | Byte Alignment    | Why and Where Is It Used?                                  |
|             | (2^N bytes)       |                                                            |
+-------------+-------------------+------------------------------------------------------------+
| .align 2    | 2^2  = 4 bytes    | Standard 32-bit RISC-V instructions & trap vectors!        |
|             | (Bottom 2 bits=0) | Why? Because stvec uses Bits 1..0 for the MODE (0 = Direct,|
|             |                   | 1 = Vectored), so any trap handler in stvec MUST have its  |
|             |                   | bottom 2 bits equal to 00 (4-byte aligned)!                |
+-------------+-------------------+------------------------------------------------------------+
| .align 3    | 2^3  = 8 bytes    | 64-bit variables/pointers (.dword / uint64_t).             |
+-------------+-------------------+------------------------------------------------------------+
| .align 4    | 2^4  = 16 bytes   | RISC-V Stack Pointer (sp) ABI requirement, or Vectored     |
|             | (Bottom 4 bits=0) | trap tables.                                               |
+-------------+-------------------+------------------------------------------------------------+
| .align 12   | 2^12 = 4096 bytes | 1 Full MMU Page (4 KB)!                                    |
|             | (Bottom 12 bits=0)| Used for page tables and stacks (.bss.stack) so they start |
|             |                   | on an exact 4096-byte page boundary!                       |
+-------------+-------------------+------------------------------------------------------------+
```
*(Tip: If you ever see `.balign 16` with a **`b`** in front, `.balign` means **B**yte alignment directly—16 bytes—whereas `.align N` on RISC-V/ARM is always $2^N$ bytes!)*

---

### 3. Difference Between Labels Starting With `.L`, Normal Names, and Numbers (`1:`)

In assembly, you will see three kinds of labels. Here is the exact difference:

```text
+-----------------------+---------------------------+---------------------------------------------------+
| Label Style           | Example                   | What Does the Assembler Do With It?               |
+-----------------------+---------------------------+---------------------------------------------------+
| 1. Normal Name        | _start:                   | Saved in the .o file's Symbol Table!              |
|    (No dot in front)  | relocate_enable_mmu:      | Shows up in GDB backtraces, 'nm', and 'objdump'   |
|                       | clear_bss_done:           | so you can see the function name when debugging!  |
+-----------------------+---------------------------+---------------------------------------------------+
| 2. Local '.L' Label   | .Lclear_bss_loop:         | PRIVATE temporary label!                          |
|    (Starts with .L)   | .Lsecondary_park:         | The assembler erases '.L' labels after assembling |
|                       |                           | so they don't clutter the Symbol Table or confuse |
|                       |                           | GDB into thinking a tiny 'while' loop is a new    |
|                       |                           | function!                                         |
+-----------------------+---------------------------+---------------------------------------------------+
| 3. Numeric Label      | 1:                        | REUSABLE ultra-short local label!                 |
|    (1:, 2:, 3:)       |   (jumped to via 1f / 1b) | Used for 2-line hops so you don't even have to    |
|                       |                           | think of a name! '1f' jumps to the next '1:'      |
|                       |                           | forward; '1b' jumps to the previous '1:' backward.|
+-----------------------+---------------------------+---------------------------------------------------+
```

---

### 4. Difference Between All the Jump / Call / Return / Trap Instructions

This is one of the best tables to keep in your head for RISC-V interviews. Every one of these changes the Program Counter (`PC`), but they differ in **whether they save `ra`** and **whether they change Privilege Mode (U $\leftrightarrow$ S $\leftrightarrow$ M)**:

```text
+----------------------+-----------------------------------+-------------------+------------------------+
| Instruction          | What Does It Actually Do?         | Saves Return Addr | Changes CPU Privilege  |
|                      |                                   | in Register 'ra'? | Mode (U / S / M)?      |
+----------------------+-----------------------------------+-------------------+------------------------+
| j label              | PC = label                        | NO!               | NO (Stays in same mode)|
| (Jump)               | Simple goto inside a function.    |                   |                        |
+----------------------+-----------------------------------+-------------------+------------------------+
| jr rs1               | PC = value inside register rs1    | NO!               | NO                     |
| (Jump Register)      | Jump to an address in a register. |                   |                        |
+----------------------+-----------------------------------+-------------------+------------------------+
| call func            | 1. ra = PC + 4 (Saves return!)    | YES! (Saves PC+4  | NO                     |
| (Function Call)      | 2. PC = func                      | into 'ra')        |                        |
|                      | Used when 'func' will return here.|                   |                        |
+----------------------+-----------------------------------+-------------------+------------------------+
| ret                  | PC = ra                           | NO (Reads 'ra'!)  | NO                     |
| (Return from call)   | Jumps back to whoever ran 'call'. |                   |                        |
+----------------------+-----------------------------------+-------------------+------------------------+
| tail func            | PC = func (Does NOT touch 'ra'!)  | NO!               | NO                     |
| (Tail Call)          | Used as the LAST line of a func   | (Keeps caller's   |                        |
|                      | (or when 'func' never returns,    |  original 'ra'!)  |                        |
|                      | like 'tail start_kernel').        |                   |                        |
+----------------------+-----------------------------------+-------------------+------------------------+
| ecall                | 1. Saves PC into sepc (or mepc)   | NO (Hardware      | YES! Jumps UP to       |
| (Environment Call)   | 2. Saves cause (8/9) into scause  | saves PC in       | HIGHER privilege!      |
|                      | 3. Jumps PC to stvec (or mtvec)!  | sepc / mepc CSR!) | (U -> S, or S -> M)    |
+----------------------+-----------------------------------+-------------------+------------------------+
| sret                 | 1. Restores SIE from SPIE         | NO (Hardware      | YES! Drops from S-mode |
| (Supervisor Return)  | 2. Sets mode to sstatus.SPP (U/S) | reads sepc CSR!)  | back to whatever mode  |
|                      | 3. PC = sepc!                     |                   | trapped (S or U)!      |
+----------------------+-----------------------------------+-------------------+------------------------+
| mret                 | 1. Restores MIE from MPIE         | NO (Hardware      | YES! Drops from M-mode |
| (Machine Return)     | 2. Sets mode to mstatus.MPP (M/S/U| reads mepc CSR!)  | to S-mode or U-mode!   |
|                      | 3. PC = mepc!                     |                   | (Used by OpenSBI!)     |
+----------------------+-----------------------------------+-------------------+------------------------+
```

Look how clear the pair relationships are:
- **`call` $\longleftrightarrow$ `ret`**: Normal function call and return using register **`ra`** (stays in the same privilege mode).
- **`ecall` / Interrupt $\longleftrightarrow$ `sret`**: Supervisor Trap entry and return using CSR **`sepc`** (switches privilege mode between U-mode and S-mode).
- **OpenSBI M-mode Trap $\longleftrightarrow$ `mret`**: Machine Trap entry and return using CSR **`mepc`** (switches privilege mode between M-mode and S-mode).

---

### 5. How to Export and Import Functions Between Assembly (`.S`) and C (`.c`)? Is `.global` Exporting or Importing?

**`.global symbol_name` is EXPORTING!**

Let's compare how **Exporting** and **Importing** work in both Assembly (`.S`) and C (`.c`):

```text
+--------------------+---------------------------------------+---------------------------------------+
| Action             | In Assembly (.S file)                 | In C (.c / .h file)                   |
+--------------------+---------------------------------------+---------------------------------------+
| EXPORT a function  | Put '.global func_name' above it!     | Just write the function normally      |
| (Make it visible   | Example in entry.S:                   | WITHOUT the word 'static'!            |
|  to other files)   |   .global handle_exception            | Example in init.c:                    |
|                    |   handle_exception:                   |   uintptr_t setup_vm(void) { ... }    |
|                    | (Without '.global', a label in .S is  | (In C, functions are exported/.global |
|                    |  private/'static' to that .S file!)   |  by default unless marked 'static'!)  |
+--------------------+---------------------------------------+---------------------------------------+
| IMPORT a function  | NOTHING needed!                       | Write 'extern' prototype!             |
| (Call a function   | Just write:                           | Example in traps.c:                   |
|  defined in        |   call setup_vm                       |   extern void handle_exception(void); |
|  another file)     |   tail start_kernel                   |                                       |
|                    | If the assembler doesn't see          | Tells the C compiler the parameter    |
|                    | 'setup_vm' in head.S, it automatically| and return types so it doesn't throw  |
|                    | treats it as an imported symbol and   | an "implicit declaration" error!      |
|                    | lets the linker (ld) connect it!      |                                       |
+--------------------+---------------------------------------+---------------------------------------+
```

Look at why we removed `.global relocate_enable_mmu` in the new `head.S` above:
- When `enable_mmu()` in `init.c` (a **different file**) was calling `relocate_enable_mmu`, we needed `.global relocate_enable_mmu` in `head.S` to **export** it to `init.c`.
- Now that `_start` in `head.S` calls `relocate_enable_mmu` inside the **same `head.S` file**, no other file needs to call `relocate_enable_mmu`—only `_start` needs `.global _start` (so the linker script `ENTRY(_start)` can see it)!
- And for `setup_vm`, `finish_mmu_relocation`, `start_kernel`, and `handle_exception`:
  - `setup_vm`, `finish_mmu_relocation`, and `start_kernel` are non-`static` C functions, so C **exports** them automatically.
  - `handle_exception` in `entry.S` has `.global handle_exception`, so `entry.S` **exports** it.
  - `head.S` simply calls them (`call setup_vm`, `call finish_mmu_relocation`, `tail start_kernel`, `la t0, handle_exception`) and the assembler **imports** them automatically!