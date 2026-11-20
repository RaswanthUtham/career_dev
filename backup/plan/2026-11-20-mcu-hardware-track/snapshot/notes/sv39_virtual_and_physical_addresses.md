# Virtual and Physical Addresses: Sv39 in xv6

This lesson explains why a C pointer is usually a **virtual address (VA)**, how it becomes a **physical address (PA)** in RAM, and when addresses become meaningful during compilation, linking, loading, and execution.

Scope: 64-bit RISC-V using Sv39 paging, with xv6 as the concrete operating-system example. The hand-built page-table entries below are illustrative; they are not copied from a running xv6 system.

## 1. Purpose in one sentence

The CPU executes loads, stores, and instruction fetches using virtual addresses; the Memory Management Unit (MMU) translates each valid virtual address through page tables into the physical address that reaches RAM or a device.

## 2. Concepts needed first

### Address

Memory is byte-addressed: every byte has a numeric location. An address is that number.

### Physical address (PA)

A physical address is the address presented to the hardware memory system: RAM, a memory-mapped UART, a disk controller, and so on. It answers: "which actual hardware location should be accessed?"

### Virtual address (VA)

A virtual address is the address a program uses in an instruction. It answers: "which location does this process think it is accessing?"

Each process can use a simple private layout even when its data lives in different physical RAM pages. Thus two processes can both use VA `0x4000`, while those VAs translate to different PAs.

### Page, page table, and Page Table Entry (PTE)

A **page** is a fixed-size block of memory. xv6 uses 4 KiB pages:

$$
4\text{ KiB} = 4096 = 2^{12}\text{ bytes}
$$

A **page table** is a page-sized array of entries. A **Page Table Entry (PTE)** either points to another page table or maps a virtual page to one physical page. It also carries permissions such as readable, writable, executable, and user-accessible.

### Memory Management Unit (MMU)

The MMU is hardware inside the processor. When paging is enabled, it walks page tables, checks PTE permissions, and converts the VA in an instruction to a PA. Software creates page tables; the MMU consumes them.

### Translation Lookaside Buffer (TLB)

The TLB is a small hardware cache of recent VA-to-PA translations. It avoids a page-table walk for many accesses. xv6 uses `sfence.vma` around page-table activation to prevent old cached translations from being reused.

### `satp`

`satp` is a Supervisor-mode Control and Status Register (CSR). In Sv39 mode it identifies the root page table used by the current hart. xv6 builds its value with `MAKE_SATP()` and writes it in `kvminithart()`.

## 3. One access, from C to RAM

For this C statement:

```c
request_limit = 20;
```

the compiler emits an instruction that uses the address of `request_limit`. Once the process is running with paging enabled:

```text
CPU instruction uses VA
        |
        v
MMU checks TLB or walks page tables
        |
        v
MMU produces PA and checks PTE_W permission
        |
        v
RAM location at that PA is written
```

The pointer value is not automatically a physical address. It is a virtual address unless code is explicitly working in a context where VA and PA happen to be identical.

## 4. When does an address get assigned?

The phrase "the address is assigned" hides several different jobs. Keep these separate.

| Stage | Who acts? | What becomes known? | What is *not* known yet? |
|---|---|---|---|
| Compilation | Compiler | Section-relative symbol locations, relocations, and stack-frame offsets | Normally no final process PA; often no final linked VA either |
| Linking | Linker | Resolves cross-file symbols and lays out ELF sections at linked virtual addresses | No per-execution physical RAM page |
| Loading | xv6 `exec` | Allocates physical pages and creates mappings from ELF VAs to those pages | A future process's runtime stack contents |
| Runtime | CPU + MMU | Every instruction access receives a translated PA; locals get a live stack-frame address while a call is active | Nothing guarantees another process gets the same PA |

The compiler can decide that an automatic local is at, for example, "32 bytes below this function's stack pointer." That is an **offset**, not a final process-wide address. The actual VA exists when the function runs with a particular stack pointer.

## 5. Scope, linkage, and storage: four variables

Use these files as a compact mental model:

```c
// ids.c
typedef unsigned int uint;

uint request_limit = 16; // definition; external linkage; static storage duration
static uint audit_count; // internal linkage; static storage duration
```

```c
// worker.c
typedef unsigned int uint;

extern uint request_limit; // declaration only: allocates no object

void work(void)
{
  static uint calls; // block scope, no linkage, static storage duration
  char scratch[32];  // block scope, no linkage, automatic storage duration

  calls++;
  scratch[0] = (char)calls;
  request_limit++;
}
```

| Name | Scope / linkage | Lifetime | Typical storage | Address story |
|---|---|---|---|---|
| `request_limit` | File scope; external linkage | Entire process lifetime | `.data` because it has nonzero initialization | The linker resolves references from `worker.c` and selects its linked VA. `exec` maps the page containing that VA to a physical page. |
| `audit_count` | File scope; internal linkage, so only `ids.c` can name it | Entire process lifetime | `.bss` because it is zero-initialized | The linker allocates it in `ids.c`'s data image. xv6 allocates a zeroed page at load time. |
| `calls` | Block scope; no linkage | Entire process lifetime despite being reachable only inside `work` | Usually `.bss` | It has one persistent object, not one object per call. Its VA is linked like other static data. |
| `scratch` | Block scope; no linkage | From entry to `work` until `work` returns | Current process's user stack | The compiler chooses a stack-frame offset. On each call, the current stack pointer gives that offset a live VA. |

Two corrections worth memorizing:

1. `static` says something about storage duration; at file scope it also gives internal linkage. A function-local `static` has block scope but persists for the process lifetime.
2. `extern` is usually only a declaration that tells the compiler an object is defined elsewhere. It does not reserve memory. The definition of `request_limit` reserves the object.

## 6. How xv6 performs the load stage

The xv6 user linker script starts the user ELF layout at VA `0x0` and groups code, read-only data, `.data`, and `.bss`. The linker therefore decides virtual locations inside the program image, not physical RAM locations.

When xv6 runs `exec`, `kexec()` creates a fresh user page table. For every loadable ELF program header, it calls `uvmalloc()` to allocate and map enough pages, then `loadseg()` copies the file-backed bytes into those pages. `uvmalloc()` zeroes a newly allocated physical page before mapping it, which provides zero initialization for BSS portions not filled from the ELF file.

The key split is:

```text
ELF says: "place this data at user VA X"
xv6 chooses: "back VA X with physical page Y"
MMU does: "when the program uses X, access Y"
```

## 7. Sv39: the three-layer virtual address

Sv39 uses 4 KiB base pages. A 64-bit virtual address used by xv6 is split as follows:

```text
63                         39 38        30 29        21 20        12 11       0
+----------------------------+------------+------------+------------+----------+
| must be zero in xv6's range |   VPN[2]   |   VPN[1]   |   VPN[0]   |  offset  |
+----------------------------+------------+------------+------------+----------+
                                 9 bits       9 bits       9 bits      12 bits
```

- **VPN** means Virtual Page Number. Each 9-bit field selects one of 512 PTEs because $2^9 = 512$.
- The **offset** selects a byte inside the final 4 KiB physical page. Translation does not alter this low 12-bit value.
- xv6 rejects VAs at or above `MAXVA`; it intentionally uses only the lower half of the Sv39 range to avoid sign-extension complexity.

Each normal xv6 4 KiB leaf PTE contains a Physical Page Number (PPN) plus flags. Important flags are:

| Flag | Meaning |
|---|---|
| `PTE_V` | The entry is valid. |
| `PTE_R` | Reads are allowed. |
| `PTE_W` | Writes are allowed. |
| `PTE_X` | Instruction fetches are allowed. |
| `PTE_U` | User mode may access this mapping. |

An intermediate PTE points to the next page-table page and has `PTE_V`; it is not the final data-page mapping. A normal leaf PTE points to the physical data page and has access permission bits such as `PTE_R`, `PTE_W`, or `PTE_X`.

## 8. Full hand walk: VA to a different PA

Suppose a user instruction accesses this virtual address:

```text
VA = 0x0000000041234567
```

Extract each field with the same idea as xv6's `PX(level, va)` macro:

```text
VPN[2] = (VA >> 30) & 0x1ff = 0x001
VPN[1] = (VA >> 21) & 0x1ff = 0x009
VPN[0] = (VA >> 12) & 0x1ff = 0x034
offset =  VA        & 0xfff = 0x567
```

Assume the current hart's `satp` selects a root table at PA `0x80200000`. The following table entries are invented solely for this worked example:

| Walk step | Table PA being read | Index | PTE says | Result |
|---|---:|---:|---|---|
| Level 2 | `0x80200000` | `0x001` | Valid, next table PPN is `0x80400` | Continue at L1 table PA `0x80400000` |
| Level 1 | `0x80400000` | `0x009` | Valid, next table PPN is `0x80600` | Continue at L0 table PA `0x80600000` |
| Level 0 | `0x80600000` | `0x034` | Valid leaf, PPN is `0x87a5b`, flags are `V|R|W|U` | Final physical page base is `0x87a5b000` |

The MMU keeps the original offset and attaches it to the leaf's physical page base:

$$
\text{PA} = 0x0000000087a5b000 + 0x567 = \boxed{0x0000000087a5b567}
$$

Notice what did and did not change:

```text
VA page number:       0x0000000041234
PA page number:       0x0000000087a5b   <- changed by the leaf PTE
page offset:                       0x567 <- preserved exactly
```

This illustrates virtual memory's central freedom: the VA need not resemble the PA. The mapping is the authority.

For readers who want to see the PTE encoding, xv6 stores a PA in bits starting at PTE bit 10. For this example, the leaf is:

```text
PA2PTE(0x87a5b000) = 0x21e96c00
flags V|R|W|U      = 0x00000017
leaf PTE           = 0x21e96c17
```

Applying `PTE2PA(0x21e96c17)` returns `0x87a5b000`; the translation code then adds the original `0x567` offset when needed.

## 9. The same walk in xv6 source

xv6 defines the page size, PTE flags, PTE conversion macros, index shift, and `MAXVA` in `kernel/riscv.h`:

```c
#define PGSIZE  4096
#define PA2PTE(pa) ((((uint64)pa) >> 12) << 10)
#define PTE2PA(pte) (((pte) >> 10) << 12)
#define PXSHIFT(level) (PGSHIFT + (9 * (level)))
#define PX(level, va)  ((((uint64)(va)) >> PXSHIFT(level)) & PXMASK)
```

`walk()` in `kernel/vm.c` starts at level 2, uses `PX(level, va)` to select a PTE, converts an intermediate PTE to the next table's PA with `PTE2PA()`, and finally returns the level-0 PTE. `walkaddr()` then checks `PTE_V` and `PTE_U` before returning the mapped physical page base for a user VA.

`mappages()` creates leaf mappings: for every 4 KiB VA step, it writes `PA2PTE(pa) | perm | PTE_V` and advances both addresses by one page.

## 10. Why xv6 kernel addresses sometimes look physical

In `kvmmake()`, xv6 deliberately makes direct mappings for kernel RAM and several devices. For example, kernel text begins at `KERNBASE`, and xv6 maps it with the same PA:

```c
kvmmap(kpgtbl, KERNBASE, KERNBASE, ..., PTE_R | PTE_X);
```

That means a particular kernel VA can equal the corresponding PA. This does **not** mean virtual and physical addresses are inherently the same. It means the kernel page table was intentionally configured that way.

The contrast is useful:

| Situation | VA equals PA? | Why? |
|---|---|---|
| Early xv6 boot before paging is enabled | The hart is using physical addresses | No translation is active yet. |
| xv6 kernel's direct-mapped RAM region | Often yes | The kernel page table deliberately maps equal values. |
| xv6 user memory | Do not assume so | `exec`, `fork`, and memory allocation select backing physical pages separately. |

After the kernel page table is ready, `kvminithart()` writes `satp` and executes `sfence.vma`; subsequent accesses use the mappings selected by that page table.

## 11. Follow one variable through its lifetime

Imagine the linker places `request_limit` at user VA `0x4000`.

1. **Compile `worker.c`:** the compiler sees only `extern uint request_limit;`. It emits a relocation because another object will supply the definition.
2. **Compile `ids.c`:** the compiler emits initialized bytes for `request_limit = 16` into a data section.
3. **Link:** the linker matches the external reference to the definition and places the combined `.data` location at VA `0x4000` in the ELF image.
4. **Run `exec`:** xv6 allocates a physical page, perhaps PA `0x87a5b000`, maps VA page `0x4000` to it, and loads the initialized byte value.
5. **Run `work`:** the instruction that increments `request_limit` uses VA `0x4000`. The MMU translates it to PA `0x87a5b000` plus the variable's offset in that page.

On a later run, xv6 may choose PA `0x80612000` instead. The program can still use VA `0x4000` unchanged because the new page table supplies the new mapping.

`scratch` is different: it has no fixed ELF data address. The compiler might use an instruction equivalent to "address equals current `sp` plus or minus a fixed offset." Its VA changes with each active call frame; after `work` returns, that storage duration has ended.

## 12. Common misconceptions

- **"A pointer is a physical address."** Usually false for user code with paging enabled. It is normally a VA.
- **"The compiler assigns final addresses."** It assigns code/data layout information and stack offsets. The linker and loader complete different parts of the story.
- **"`extern` creates a global variable."** No. It declares that a definition exists elsewhere.
- **"A static local is on the stack because it is written inside a function."** No. It has static storage duration, so it persists after returns.
- **"If a kernel VA equals a PA, all VAs equal PAs."** No. Identity mapping is a selected mapping policy.
- **"The MMU changes the whole address."** For 4 KiB pages, the leaf PTE replaces the page number while preserving the 12-bit page offset.

## 13. Recap and drills

Remember these four sentences:

1. A VA is what the program uses; a PA is what RAM or a device receives.
2. The linker assigns ELF virtual layout; xv6 chooses physical backing pages when it loads or grows an address space.
3. Sv39 walks three 9-bit indexes, then preserves the 12-bit offset.
4. A static lifetime object keeps its storage; an automatic local exists only while its function call is active.

Try these without looking above:

1. For VA `0x0000000041234567`, write `VPN[2]`, `VPN[1]`, `VPN[0]`, and the page offset.
2. Which declaration allocates storage: `extern uint request_limit;` or `uint request_limit = 16;`?
3. Which stage first chooses the physical page that backs a user's `.data` VA: compiler, linker, xv6 `exec`, or MMU?
4. Why can two separate processes both validly use VA `0x4000`?
5. If the leaf PTE maps physical page base `0x81234000`, what PA results from a VA whose page offset is `0xabc`?

## xv6 source anchors

- `xv6-riscv/kernel/riscv.h`: `SATP_SV39`, `MAKE_SATP`, page/PTE macros, `PX()`, and `MAXVA`.
- `xv6-riscv/kernel/vm.c`: `kvmmake()`, `kvminithart()`, `walk()`, `walkaddr()`, `mappages()`, and `uvmalloc()`.
- `xv6-riscv/kernel/exec.c`: `kexec()` creates a new user page table and loads ELF segments.
- `xv6-riscv/user/user.ld`: user ELF layout begins at VA `0x0` and describes section placement.
- `xv6-riscv/kernel/kernel.ld`: kernel image layout begins at `0x80000000` and provides the `etext` linker symbol.