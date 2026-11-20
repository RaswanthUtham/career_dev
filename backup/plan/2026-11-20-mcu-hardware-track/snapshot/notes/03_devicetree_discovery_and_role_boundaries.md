# Device Tree Discovery, FDT Binary Format, and Industry Role Boundaries

## 1. The Chicken-and-Egg Debugging Dilemma

### Why Can't the Kernel Discover the UART Dynamically on Line 1?
Parsing a Flattened Device Tree (FDT) requires parsing a 40-byte binary header, performing big-endian conversions, traversing 32-bit token streams, and matching null-terminated strings.

If a bug occurs during early DTB parsing (e.g., corrupt pointer in `a1`, endianness bit-shift bug, misaligned load, or unhandled token):
- Without an early console, the CPU triggers an access fault and halts in total silence.
- You cannot see what caused the crash.

### The Upstream Linux Solution: The Two-Phase Console
```text
+----------------------------------------------------------------------+
| PHASE 1: Early Console (earlycon)                                    |
|   - Uses bootstrap fixed address (0x10000000) or SBI ecall           |
|   - printk() / early_puts() works within the first 20 instructions!  |
+----------------------------------------------------------------------+
                                  |
                                  v
+----------------------------------------------------------------------+
| PHASE 2: DTB Parser (drivers/of/fdt.c)                               |
|   - Scans DTB in register a1 (/memory, /cpus, /chosen, /soc)         |
|   - If DTB is corrupted, errors are printed visibly on earlycon!     |
+----------------------------------------------------------------------+
                                  |
                                  v
+----------------------------------------------------------------------+
| PHASE 3: Production Driver Model                                     |
|   - DTB supplies dynamic peripheral addresses and IRQ numbers        |
|   - Full driver binds via of_match_table, replacing earlycon         |
+----------------------------------------------------------------------+
```

---

## 2. The Flattened Device Tree (FDT) Binary Format

**Target File in `my-linux`**: `my-linux/include/linux/fdt.h`

```c
struct fdt_header {
    uint32_t magic;             /* 0xd00dfeed (Big-Endian) */
    uint32_t totalsize;         /* Total size of DTB in bytes */
    uint32_t off_dt_struct;     /* Offset to structure block */
    uint32_t off_dt_strings;    /* Offset to strings block */
    uint32_t off_mem_rsvmap;    /* Offset to reserved memory block */
    uint32_t version;           /* Format version (typically 17) */
    uint32_t last_comp_version; /* Last compatible version */
    uint32_t boot_cpuid_phys;   /* Primary CPU physical ID */
    uint32_t size_dt_strings;   /* Length of strings block */
    uint32_t size_dt_struct;    /* Length of structure block */
};
```

### Big-Endian vs Little-Endian (`be32_to_cpu`)
RISC-V executes in Little-Endian mode, but the Device Tree specification mandates Big-Endian (network byte order).
Reading `header->magic` directly yields `0xEDFE0DD0`. Every integer must be byte-swapped:

```text
Big-Endian in RAM:    [ D0 ] [ 0D ] [ FE ] [ ED ]
Little-Endian Read:   0xEDFE0DD0 (Wrong!)
After be32_to_cpu():  0xD00DFEED (Matches FDT_MAGIC!)
```

### Structure Block Tokens
- `FDT_BEGIN_NODE` (`0x00000001`): Followed by null-terminated node name, padded to 4-byte alignment.
- `FDT_PROP` (`0x00000003`): Followed by `len` (4 bytes), `nameoff` (4 bytes into strings block), and value data (padded to 4 bytes).
- `FDT_END_NODE` (`0x00000002`): Closes the current node.
- `FDT_END` (`0x00000009`): Marks end of structure block.

### The 4-Byte Alignment Requirement
Misaligned 32-bit reads on RISC-V can trigger a Load Address Misaligned Exception (`scause = 4`). All token pointers must advance using 4-byte rounding:

```c
/* Target File: my-linux/arch/riscv/kernel/setup.c */
static inline uint32_t fdt_align4(uint32_t val)
{
    return (val + 3) & ~3U;
}
```

---

## 3. Industry Role Boundaries: What Do Embedded/BSP Engineers Actually Do?

In semiconductor companies (Qualcomm, NVIDIA, AMD, TI) and tier-1 automotive/embedded systems:

```text
+-----------------------------------------------------------------------+
| Hardware / Silicon Team                                               |
|   - Delivers Board Schematics (PDF) & Technical Reference Manual      |
+-----------------------------------------------------------------------+
                                   |
                                   v
+-----------------------------------------------------------------------+
| YOU: Linux BSP / Embedded Driver Engineer                             |
|                                                                       |
|   1. Author Board DTS/DTSI: Map buses, pins, IRQs, clocks             |
|   2. Author YAML Bindings: Define required properties & schemas       |
|   3. Author Driver probe(): Match compatible & read properties        |
+-----------------------------------------------------------------------+
          |                        |                        |
          v                        v                        v
+-------------------+    +-------------------+    +---------------------+
| drivers/of/fdt.c  |    | dt-schema         |    | Linux Driver Core   |
| (Already in Linux)|    | (Validates YAML)  |    | (Calls your probe!) |
+-------------------+    +-------------------+    +---------------------+
```

1. **Low-Level Binary FDT Parser (`0xd00dfeed`)**:
   - **Owned by generic kernel/architecture teams** (`drivers/of/fdt.c` in Linux; compile-time codegen in Zephyr).
   - In `my-linux`, we implement a minimal parser to understand the binary mechanics and firmware register `a1` handoff. You do not rewrite this in production.
2. **Devicetree Source (`.dts` / `.dtsi`)**:
   - **100% Owned by the Driver/BSP Engineer**. You map the board's buses, peripheral addresses, interrupt lines, clocks, and GPIO pins.
3. **Devicetree Binding Schematics (YAML)**:
   - **100% Owned by the Driver/BSP Engineer**. In upstream Linux, no driver is accepted into mainline without a corresponding YAML schema in `Documentation/devicetree/bindings/` validating required and optional properties.
4. **Driver `probe()` Parsing (`of_` APIs)**:
   - **100% Owned by the Driver/BSP Engineer**. Inside `probe()`, you match compatible strings (`of_match_table`), extract register bases (`platform_get_resource`), and map interrupts (`irq_of_parse_and_map`).
