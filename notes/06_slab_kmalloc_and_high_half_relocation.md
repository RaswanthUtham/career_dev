# Sub-Page Object Allocator (`kmalloc` / `kfree`) & High-Half Kernel Execution

This guide covers the final two building blocks of **Milestone 4** in `my-linux`, taught in the exact upstream Linux way:
1. **The Sub-Page Object Allocator (`mm/slab.c`: `kmalloc` and `kfree`)** — How the Linux kernel allocates small C structures (like `32` bytes, `64` bytes, or `128` bytes) without wasting an entire `4096`-byte page from `alloc_page()`.
2. **Relocating Kernel Execution (`PC`, `sp`, and `stvec`) to the High Half (`0xFFFFFFC0802xxxxx`)** — How to jump the CPU instruction pointer, stack pointer, and trap vector into the High Half so the Low Half (`0x0000000000000000` to `0x0000003FFFFFFFFF`) is 100% free for User Space!

---

## PART 1: Why Do We Need `kmalloc()` When We Already Have `alloc_page()`?

### 1. What Is `kmalloc()`? (The Egg-Carton Analogy)

Think about how our physical page allocator (`alloc_page()`) works:
- Every time you call `alloc_page()`, it hands you an entire **4096-byte (4 KB) page**.
- That is like going to a wholesale warehouse where you can **only** buy eggs in giant crates of **4,096 eggs**!

Now imagine you are writing the next parts of our kernel:
- You want to create a Timer (`struct timer_list`), which is only **48 bytes**!
- Or a Workqueue item (`struct work_struct`), which is only **32 bytes**!
- Or an Open File structure (`struct file`), which is only **64 bytes**!

#### What Happens If You Use `alloc_page()` for a 32-Byte Struct?
```text
ONE 4096-BYTE PAGE FROM alloc_page():
+-------+-------------------------------------------------------------------+
| 32 B  |               4,064 BYTES OF WASTED EMPTY RAM!                    |
| struct|                    (99.2% OF RAM WASTED!)                         |
+-------+-------------------------------------------------------------------+
```
If you allocate 1,000 small 32-byte structs using `alloc_page()`, you should only need `32 KB` of RAM—but `alloc_page()` would consume `1,000 * 4 KB = 4 Megabytes` of RAM! You would waste **99.2%** of your memory!

### 2. The Linux Solution: The Slab / Size-Class Cache (`kmalloc` and `kfree`)

How does the real Linux kernel (`linux/mm/slub.c` and `include/linux/slab.h`) solve this?
Linux builds a **Slab Allocator** on top of `alloc_page()`!

Here is the simple idea (explained for a 12-year-old):
1. Instead of giving a whole 4096-byte page to one tiny struct, `kmalloc()` keeps a row of **9 buckets (Caches)** of power-of-two sizes starting from **8 bytes** (matching upstream `linux/include/linux/slab.h`):
   - **Bucket 0 (`8` bytes)** — `kmalloc-8`
   - **Bucket 1 (`16` bytes)** — `kmalloc-16`
   - **Bucket 2 (`32` bytes)** — `kmalloc-32`
   - **Bucket 3 (`64` bytes)** — `kmalloc-64`
   - **Bucket 4 (`128` bytes)** — `kmalloc-128`
   - **Bucket 5 (`256` bytes)** — `kmalloc-256`
   - **Bucket 6 (`512` bytes)** — `kmalloc-512`
   - **Bucket 7 (`1024` bytes)** — `kmalloc-1k`
   - **Bucket 8 (`2048` bytes)** — `kmalloc-2k`
   - *(And if someone asks for more than `2048` bytes, like `4096` bytes, it gives them a whole page from `alloc_page()`!)*

```text
THE 9 UPSTREAM LINUX BUCKETS (kmalloc_caches[0..8]):

  Bucket [0] (8-byte objects)    ---> [Slab Page A] <---> [Slab Page B]
  Bucket [1] (16-byte objects)   ---> [Slab Page C]
  Bucket [2] (32-byte objects)   ---> [Slab Page D]
  Bucket [3] (64-byte objects)   ---> [Slab Page E]
  Bucket [4] (128-byte objects)  ---> Empty (Will slice a 4KB page when asked!)
  Bucket [5] (256-byte objects)  ---> Empty
  Bucket [6] (512-byte objects)  ---> Empty
  Bucket [7] (1024-byte objects) ---> Empty
  Bucket [8] (2048-byte objects) ---> Empty
```

2. **What happens when you call `kmalloc(48)`?**
   - `48` bytes rounds up to the nearest bucket size: **`64` bytes (Bucket 3)**.
   - Does Bucket 3 have a partially-free 4KB slab page on its `partial` list?
     - **If NO**: `kmalloc()` calls `alloc_page()` **once** to get one 4096-byte page, slices that 4096-byte page into **`4096 / 64 = 64` equal slots of 64 bytes**, chains all 64 slots into that page's `freelist`, and adds that page to Bucket 3's `partial` list using `list_add_tail(&page->list, &cache->partial)`!
   - Then `kmalloc()` pops the first 64-byte slot off `page->freelist` and returns its High-Half Virtual Address (`0xFFFFFFC08xxxxxxx`)!

```text
HOW ONE 4096-BYTE SLAB PAGE IS SLICED INTO 64 SLOTS OF 64 BYTES:

  struct page (Index Card in mem_map[])
  +-----------------------------------------+
  | slab_cache ---> points to Bucket [3]    |
  | list       ---> linked in cache->partial|
  | inuse      = 0 (counts allocated slots) |
  | freelist   ---+                         |
  +---------------|-------------------------+
                  |
                  v
  High-Half Page Virtual Address: 0xFFFFFFC08038D000 (4096 bytes total)
  +------------------+------------------+------------------+-----+------------------+
  | Slot 0 (64 B)    | Slot 1 (64 B)    | Slot 2 (64 B)    | ... | Slot 63 (64 B)   |
  | 0x...8038D000    | 0x...8038D040    | 0x...8038D080    |     | 0x...8038DFC0    |
  | [first 8B=Slot1] | [first 8B=Slot2] | [first 8B=Slot3] |     | [first 8B=NULL]  |
  +------------------+------------------+------------------+-----+------------------+
```

---

### 3. How Upstream Linux (`linux/mm/slab.h` & `linux/mm/slub.c`) Organizes `struct page` and Links 8-Byte Slots!

Let's inspect the actual upstream Linux code in `C:\workspace\career_ws\linux\mm\slab.h` (line 116) and `C:\workspace\career_ws\linux\mm\slub.c` (line 557).
Upstream Linux uses a two-level design where **`struct list_head`** AND **`void *freelist`** each do the job they are best at:

#### Level 1: Inside `struct page` (`include/linux/mm.h` / `mm/slab.h`) — Linking 4KB Slab Pages with `struct list_head`!
When a 4096-byte page is used as a Slab Page, it is not on `free_page_list`. So its `struct list_head list` inside `struct page` is free to be used!
Upstream Linux adds:
1. **`struct kmem_cache *slab_cache;`**: Points directly to the bucket (`kmalloc-8` .. `kmalloc-2k`) that owns this page!
2. **`void *freelist;`**: Points to the first free object slot inside this 4KB page!
3. **`unsigned int inuse;`**: Counts how many slots in this page are currently allocated.
4. **`struct list_head list;`**: Links this 4KB page into `cache->partial` (the doubly-linked list of slab pages that have free slots available)!

#### Level 2: Inside Each Free Slot (`8B .. 2048B`) — `set_freepointer()` and `get_freepointer()`!
Why can't we put a 16-byte `struct list_head` (`next` + `prev`) inside a free **8-byte** slot (`kmalloc-8`)?
Because `struct list_head` is **16 bytes** (`2 * 8` bytes), which is twice as big as an **8-byte** slot!
And how does upstream `linux/mm/slub.c` (lines 557–579) link free slots without inventing any extra `struct slab_obj`?
It uses two tiny 1-line inline functions that read/write a single 8-byte `void *` pointer directly into the first 8 bytes of the free slot:

```c
/* From upstream linux/mm/slub.c: No extra struct needed! */
static inline void *get_freepointer(void *object)
{
    return *(void **)object;
}

static inline void set_freepointer(void *object, void *fp)
{
    *(void **)object = fp;
}
```

Let's explain `*(void **)object = fp;` to a 12-year-old!
- `object` is the memory address of a free slot (for example, `0xFFFFFFC08038D000`).
- Even our smallest slot (`kmalloc-8`) is **8 bytes** wide!
- And on 64-bit RISC-V, a memory address (`void *`) is **8 bytes** wide!
- Casting `object` to `(void **)` simply tells C: *"Treat the first 8 bytes of this free slot as a box that holds a `void *` address!"*
- `*(void **)object = fp;` writes the address of the next free slot (`fp`) into those 8 bytes!
- `return *(void **)object;` reads the address of the next free slot back out of those 8 bytes!

```text
WHY UPSTREAM LINUX'S TWO-LEVEL DESIGN IS SO CLEAN:

  1. How are 4KB Slab Pages linked inside a bucket (struct kmem_cache)?
     ---> Using our existing <linux/list.h> (struct list_head partial)!
          Each struct page links its 'page->list' into 'cache->partial'.

  2. How are the 8B..2048B free slots linked inside one 4KB Slab Page?
     ---> Using page->freelist and set_freepointer() / get_freepointer()!
          Fits inside 8-byte slots (kmalloc-8) with ZERO extra structs!

  3. How does kfree(ptr) work in O(1) time without knowing the size?
     ---> page = phys_to_page(__pa(ptr));
     ---> Pushes 'ptr' back onto 'page->freelist' via set_freepointer()!
```

---

## PART 2: Relocating Kernel Execution (`PC`, `sp`, `stvec`) to the High Half — The Exact Upstream `head.S` Way!

### 1. Why Did `PC` Revert to `0x000000008020xxxx` When `vm_init()` Was Called Inside `start_kernel()`?

Earlier, you saw this super interesting output when `vm_init()` was called inside `start_kernel()`:
```text
[MMU TEST] Current High-Half PC: 0x000000008020078a | High-Half SP: 0xffffffc080207fc0
```
Why was `SP` in the High Half (`0xffffffc080207fc0`), while `PC` reverted back to the Low Half (`0x000000008020078a`)?

Let's trace the 3 nested function calls and their saved Return Addresses (`ra`) on the stack!

```text
WHY CALLING relocate_enable_mmu 3 FUNCTIONS DEEP REVERTED PC:

  1. start_kernel() (at Low-Half PC = 0x80200780) calls vm_init():
     - vm_init()'s function prologue saves start_kernel()'s return address
       (0x0000000080200784) ON THE STACK (`sd ra, 24(sp)`)!

  2. vm_init() (at Low-Half PC = 0x80200500) calls enable_mmu():
     - enable_mmu()'s function prologue saves vm_init()'s return address
       (0x0000000080200504) ON THE STACK (`sd ra, 8(sp)`)!

  3. enable_mmu() calls relocate_enable_mmu():
     - relocate_enable_mmu() adds PAGE_OFFSET to register 'ra' and returns!
     - Inside enable_mmu(), PC IS now in the High Half!
     - BUT when enable_mmu() finishes (`ld ra, 8(sp); ret`) and when vm_init()
       finishes (`ld ra, 24(sp); ret`), they pop the OLD Low-Half return
       addresses (`0x0000000080200784`) off the stack and jump PC right back
       to the Low Half!
```

### 2. How Does Upstream `linux/arch/riscv/kernel/head.S` Solve This Permanently?

Let's look at `C:\workspace\career_ws\linux\arch\riscv\kernel\head.S` (lines 285–315)!
In upstream Linux (and ARM64 Linux `arch/arm64/kernel/head.S`), the kernel **NEVER** turns on the MMU inside `start_kernel()`!
Instead, **`head.S` calls `setup_vm` and `relocate_enable_mmu` BEFORE calling `start_kernel()`**—when there are **ZERO C stack frames on the stack**!

```text
UPSTREAM LINUX BOOT FLOW IN arch/riscv/kernel/head.S:

  _start (in head.S, running at Physical 0x80200000, MMU OFF):
    |
    +-- 1. Mask interrupts (sie = 0, sip = 0) & Park secondary harts
    +-- 2. Set sp = bss_stack_top & Clear BSS (__bss_start .. __bss_stop)
    +-- 3. Save a0 (hartid) in s1, and a1 (dtb physical address) in s2
    |
    +-- 4. call setup_vm
    |      (Runs in C while MMU is OFF: calls mm_init() and builds swapper_pg_dir
    |       with Identity + High-Half mappings, returns root_phys in a0!)
    |
    +-- 5. call relocate_enable_mmu
    |      (Turns ON satp! Adds PAGE_OFFSET 0xFFFFFFC000000000 to ra, sp, and stvec!
    |       When 'ret' executes, PC jumps to High-Half 0xFFFFFFC0802000xx in head.S!)
    |
    +-- 6. call finish_mmu_relocation
    |      (Updates va_pa_offset = PAGE_OFFSET, swapper_pg_dir, and mem_map to High Half!)
    |
    +-- 7. Restore a0 = s1 (hartid) and a1 = s2 + PAGE_OFFSET (High-Half DTB VA!)
    |
    +-- 8. tail start_kernel
           (Enters start_kernel() at High-Half PC = 0xFFFFFFC08020xxxx from Line 1!)
```

### 3. Advantages vs. Disadvantages & How Other Embedded OSes/RTOSes Do It

```text
+-----------------------------------+---------------------------------------+---------------------------------------+
| Feature                           | Approach A: Enable MMU inside         | Approach B: Enable MMU in head.S      |
|                                   | start_kernel()                        | before start_kernel() (Upstream Linux)|
+-----------------------------------+---------------------------------------+---------------------------------------+
| 1. Call Stack & Return Addresses  | AWKWARD: Caller stack frames hold old | 100% CLEAN: Zero C frames on stack    |
|    (ra / PC / sp)                 | Low-Half 'ra' values on the stack!    | when MMU turns on! start_kernel() and |
|                                   |                                       | all functions run at High-Half PC!    |
+-----------------------------------+---------------------------------------+---------------------------------------+
| 2. Pointer Consistency (dtb,      | MIXED: Code before vm_init() sees     | 100% UNIFORM: Every subsystem in      |
|    mem_map, global pointers)      | Low-Half pointers; code after sees    | start_kernel() sees ONLY High-Half    |
|                                   | High-Half pointers.                   | pointers (0xFFFFFFC08xxxxxxx) always! |
+-----------------------------------+---------------------------------------+---------------------------------------+
| 3. How Other Embedded OSes Do It  | - MCU RTOS (FreeRTOS/Zephyr Cortex-M):| - Linux (RISC-V & ARM64): Always in   |
|    (Interview Reference!)         |   No MMU (uses MPU), VA == PA forever.|   head.S before start_kernel()!       |
|                                   | - App-Core RTOS (Zephyr ARM64/RV64):  | - QNX / BSD / Windows: Always in      |
|                                   |   Enables MMU in reset.S before C!    |   early boot stub before main kernel! |
+-----------------------------------+---------------------------------------+---------------------------------------+
```

---

### 4. Bonus: Explaining the Inline Assembly Syntax (`mv` vs `li`/`la` and `auipc`)

In `main.c`, we inspect our current `sp` and `PC` registers using these two lines:
```c
__asm__ volatile("mv %0, sp"    : "=r"(current_sp));
__asm__ volatile("auipc %0, 0"  : "=r"(current_pc));
```

1. **What does `: "=r"(current_sp)` and `%0` mean?**
   - `"=r"(current_sp)` tells GCC: *"Pick any general-purpose CPU register (like `a5`) as Write-Only Output `#0` (`%0`), and store its final value into the C variable `current_sp`."*
   - `%0` inside the string is replaced by that register (`mv a5, sp`).
2. **Why `mv %0, sp` instead of `li` or `la`?**
   - **`mv` (Move Register)**: Copies the value inside **CPU register `sp`** into **CPU register `%0`**.
   - **`li` (Load Immediate)**: Only works for fixed compile-time numbers (like `li a5, 42`), NOT registers!
   - **`la` (Load Address)**: Only works for global C variable/function symbol names (like `la a5, _start`), NOT registers!
3. **What does `auipc %0, 0` do?**
   - RISC-V does not allow `mv %0, pc` because `pc` is not one of the 32 general-purpose registers (`x0..x31`).
   - **`auipc rd, imm`** (**A**dd **U**pper **I**mmediate to **PC**) computes `rd = Current_PC + (imm << 12)`.
   - When `imm` is `0`, `auipc %0, 0` computes `%0 = Current_PC + 0`, copying the exact current **Program Counter (`PC`)** into `current_pc`!

---

## PART 3: What You Need to Update Next in `my-linux/` (Industry-Standard `head.S` Flow!)

Since you already implemented `slab.h` and `slab.c` and verified `kmalloc-8`, here are the **3 clean updates** in `my-linux/` to move `setup_vm` + `relocate_enable_mmu` into `head.S` (matching upstream `linux/arch/riscv/kernel/head.S`) so that `Current High-Half PC` prints `0xffffffc08020xxxx`!

---

### Update 1: Update `_start` in `my-linux/arch/riscv/kernel/head.S`

**What this change does:**
In `head.S`, after clearing BSS and setting `li s0, 0`:
1. Saves `a0` (`hartid`) into `s1`, and `a1` (`dtb` physical address) into `s2`.
2. Calls `setup_vm` (which initializes `mm_init` and builds `swapper_pg_dir` while `satp = 0`, returning `root_phys` in `a0`).
3. Loads `a1 = 0xFFFFFFC000000000` (`PAGE_OFFSET`) and calls `relocate_enable_mmu`.
   - When `relocate_enable_mmu` executes `ret`, **`PC`, `sp`, and `stvec` are NOW in the High Half (`0xffffffc08020xxxx`)**!
4. Calls `finish_mmu_relocation` (which updates `va_pa_offset`, `swapper_pg_dir`, and `mem_map` to the High Half).
5. Restores `a0 = s1` (`hartid`) and `a1 = s2 + PAGE_OFFSET` (High-Half `dtb` pointer!), clears `s1` and `s2`, and executes `tail start_kernel`!

Update `clear_bss_done:` in **`my-linux/arch/riscv/kernel/head.S`**:

```assembly
/* Target File: my-linux/arch/riscv/kernel/head.S */

clear_bss_done:
    /* Clear frame pointer */
    li   s0, 0

    /* Set early Low-Half trap vector while MMU is still off */
    la   t0, handle_exception
    csrw stvec, t0

    /* Save boot arguments: s1 = hartid (a0), s2 = dtb physical address (a1) */
    mv   s1, a0
    mv   s2, a1

    /* 1. Build page tables in C while MMU is OFF (returns root_phys in a0) */
    call setup_vm

    /* 2. Turn ON Sv39 MMU and relocate PC (ra), sp, and stvec to High Half! */
    li   a1, 0xffffffc000000000
    call relocate_enable_mmu

    /*
     * RIGHT HERE: CPU PC, sp, and stvec are NOW in High Half (0xffffffc08020xxxx)!
     * 3. Update C global pointers (va_pa_offset, swapper_pg_dir, mem_map) to High Half
     */
    call finish_mmu_relocation

    /* 4. Restore a0 = hartid, and convert a1 = dtb + PAGE_OFFSET (High-Half VA!) */
    mv   a0, s1
    li   t0, 0xffffffc000000000
    add  a1, s2, t0
    li   s1, 0
    li   s2, 0

    /* 5. Enter start_kernel(hartid, dtb_va) in High Half! */
    tail start_kernel
```

---

### Update 2: Split `setup_vm()` and `finish_mmu_relocation()` in `my-linux/arch/riscv/mm/init.c`

**What this change does:**
In `my-linux/arch/riscv/mm/init.c`, instead of calling `vm_init()` from `main.c`, `head.S` calls:
1. **`uintptr_t setup_vm(void)`**: Runs while `satp = 0`. Calls `mm_init((uintptr_t)_end, 0x88000000UL)`, allocates `swapper_pg_dir`, maps the Identity and High-Half Linear ranges, and returns `root_phys` (`__pa(swapper_pg_dir)`) in register `a0` for `relocate_enable_mmu`!
2. **`void finish_mmu_relocation(void)`**: Runs immediately after `relocate_enable_mmu` when `PC` and `sp` are already in the High Half! Sets `va_pa_offset = PAGE_OFFSET`, updates `swapper_pg_dir` to its High-Half VA, and calls `mm_relocate_high_half(PAGE_OFFSET)`.

Update `enable_mmu` / `vm_init` in **`my-linux/arch/riscv/mm/init.c`** (and update prototypes in **`my-linux/arch/riscv/include/asm/pgtable.h`** if desired):

```c
/* Target File: my-linux/arch/riscv/mm/init.c */

extern char _end[];
static uintptr_t saved_root_phys = 0;

/*
 * setup_vm - Called from head.S while MMU is OFF (satp = 0).
 * Initializes page_alloc, builds Identity + High-Half page tables,
 * and returns the physical address of swapper_pg_dir in a0.
 */
uintptr_t setup_vm(void)
{
    /* 1. Initialize physical page allocator while MMU is off */
    mm_init((uintptr_t)_end, 0x88000000UL);

    /* 2. Allocate root page table (swapper_pg_dir) */
    struct page *root_page = alloc_page();
    if (!root_page) {
        early_puts("[MMU PANIC] Failed to allocate swapper_pg_dir!\n");
        return 0;
    }
    swapper_pg_dir = (pgd_t)page_address(root_page);
    saved_root_phys = __pa(swapper_pg_dir);

    /* 3. Low-Half Identity Map (Rows 0..255) for boot transition and UART MMIO */
    map_page(swapper_pg_dir, 0x10000000UL, 0x10000000UL, PAGE_KERNEL_RW);
    map_range(swapper_pg_dir, (uintptr_t)_stext, (uintptr_t)_etext, PAGE_KERNEL_EXEC);
    map_range(swapper_pg_dir, (uintptr_t)_srodata, (uintptr_t)_erodata, PAGE_KERNEL_RO);
    map_range(swapper_pg_dir, (uintptr_t)_sdata, 0x88000000UL, PAGE_KERNEL_RW);

    /* 4. High-Half Kernel Linear Map (Rows 256..511): VA 0xFFFFFFC080000000 -> PA 0x80000000 */
    create_linear_mapping_range(swapper_pg_dir, (uintptr_t)_stext, (uintptr_t)_etext, PAGE_KERNEL_EXEC);
    create_linear_mapping_range(swapper_pg_dir, (uintptr_t)_srodata, (uintptr_t)_erodata, PAGE_KERNEL_RO);
    create_linear_mapping_range(swapper_pg_dir, (uintptr_t)_sdata, 0x88000000UL, PAGE_KERNEL_RW);

    return saved_root_phys;
}

/*
 * finish_mmu_relocation - Called from head.S immediately after relocate_enable_mmu.
 * At this point, PC, sp, and stvec are ALREADY in the High Half (0xFFFFFFC08020xxxx)!
 */
void finish_mmu_relocation(void)
{
    va_pa_offset = PAGE_OFFSET;
    swapper_pg_dir = (pgd_t)__va(saved_root_phys);
    mm_relocate_high_half(PAGE_OFFSET);
}
```

---

### Update 3: Update `start_kernel()` in `my-linux/init/main.c`

**What this change does:**
Because `head.S` already ran `setup_vm()`, `relocate_enable_mmu()`, and `finish_mmu_relocation()` before calling `start_kernel()`, remove `mm_init(...)` and `vm_init()` from `start_kernel()`!
And look what happens to `&a` inside `start_kernel()` now:
- Because `PC` is already at `0xffffffc08020xxxx`, PC-relative addressing (`-mcmodel=medany`) makes `&a` **already a High-Half Virtual Address (`0xffffffc08020xxxx`)**!
- And `Current High-Half PC` and `High-Half SP` both print `0xffffffc08020xxxx`!

In **`my-linux/init/main.c`**:
1. Remove the `mm_init(mem_start, mem_end);` and `vm_init();` calls inside `start_kernel()`.
2. Run `make clean && make qemu` and look at the output:
   - `Current High-Half PC` will now print **`0xffffffc080200xxx`**!
   - `High-Half SP` will print **`0xffffffc080207xxx`**!
   - `Global 'a'` address `&a` will naturally be **`0xffffffc08020xxxx`**!
   - Even the `dtb` pointer passed into `start_kernel(hartid, dtb)` is now in the High Half (`0xffffffc087e00000`)!

---

### Step 1: Update `struct page` in `my-linux/include/linux/mm.h` & `my-linux/mm/page_alloc.c`

**What this change does:**
1. Adds the upstream Linux slab fields (`struct kmem_cache *slab_cache;`, `void *freelist;`, and `unsigned int inuse;`) to `struct page`.
2. Updates `page_address(page)` to use `va_pa_offset` so `page_address()` returns High-Half virtual addresses (`0xFFFFFFC08xxxxxxx`) once the MMU is enabled.
3. Adds `mm_relocate_high_half(unsigned long offset)` in `mm/page_alloc.c` so `mem_map` and `free_page_list` pointers also move to High-Half virtual addresses (`0xFFFFFFC0802xxxxx`) when `enable_mmu()` turns on!

In **`my-linux/include/linux/mm.h`**:
```c
/* Target File: my-linux/include/linux/mm.h */

struct kmem_cache; /* Forward declaration for slab allocator */
extern unsigned long va_pa_offset;

struct page {
    unsigned long flags;
    int refcount;
    unsigned int inuse;             /* Number of active slab objects allocated in page */
    struct kmem_cache *slab_cache;  /* Owning kmem_cache bucket (NULL if full page) */
    void *freelist;                 /* First free object slot inside this slab page */
    struct list_head list;          /* Links into free_page_list OR cache->partial */
};

static inline void *page_address(const struct page *page)
{
    return (void *)(page_to_phys(page) + va_pa_offset);
}

void mm_relocate_high_half(unsigned long offset);
```

In **`my-linux/mm/page_alloc.c`**:
1. Inside `mm_init()`, initialize the new fields in the loop:
   ```c
   /* Target File: my-linux/mm/page_alloc.c (inside mm_init loop) */
   mem_map[i].flags = 0;
   mem_map[i].refcount = 0;
   mem_map[i].inuse = 0;
   mem_map[i].slab_cache = NULL;
   mem_map[i].freelist = NULL;
   INIT_LIST_HEAD(&mem_map[i].list);
   ```
2. Add `mm_relocate_high_half(unsigned long offset)` at the bottom of `my-linux/mm/page_alloc.c`:
   ```c
   /* Target File: my-linux/mm/page_alloc.c */

   /*
    * mm_relocate_high_half - Relocate mem_map and free_page_list pointers to High Half
    */
   void mm_relocate_high_half(unsigned long offset)
   {
       /* 1. Move the mem_map cabinet base pointer to High-Half (0xFFFFFFC080208000) */
       mem_map = (struct page *)((uintptr_t)mem_map + offset);

       /* 2. Re-thread all FREE pages (refcount == 0) onto the High-Half free_page_list */
       INIT_LIST_HEAD(&free_page_list);
       for (unsigned long pfn = min_low_pfn; pfn < max_pfn; pfn++) {
           struct page *page = pfn_to_page(pfn);
           if (page->refcount == 0) {
               list_add_tail(&page->list, &free_page_list);
           }
       }
   }
   ```

---

### Step 2: Add `relocate_enable_mmu` in `my-linux/arch/riscv/kernel/head.S` & Call It from `enable_mmu()`

**What this change does:**
Implements upstream Linux's `relocate_enable_mmu` in `head.S` so that when `enable_mmu()` turns on `satp`, it simultaneously jumps `PC` (`ra`), `sp`, `s0`, and `stvec` into the High Half (`0xFFFFFFC08020xxxx`)!

Add `relocate_enable_mmu` to **`my-linux/arch/riscv/kernel/head.S`**:
```assembly
/* Target File: my-linux/arch/riscv/kernel/head.S */

.global relocate_enable_mmu
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
```

And update `enable_mmu()` in **`my-linux/arch/riscv/mm/init.c`**:
```c
/* Target File: my-linux/arch/riscv/mm/init.c */

extern void relocate_enable_mmu(uintptr_t root_phys, unsigned long page_offset);

void enable_mmu(pgd_t root_pgd)
{
    uintptr_t root_phys = __pa(root_pgd);

    /*
     * Jump PC, sp, s0, and stvec to High Half while turning on satp!
     * When relocate_enable_mmu returns, the CPU is executing at 0xFFFFFFC08020xxxx!
     */
    relocate_enable_mmu(root_phys, PAGE_OFFSET);

    /* Update va_pa_offset, swapper_pg_dir, and mem_map to High Half */
    va_pa_offset = PAGE_OFFSET;
    swapper_pg_dir = (pgd_t)__va(root_phys);
    mm_relocate_high_half(PAGE_OFFSET);

    early_puts("[MMU] Sv39 Enabled & Kernel Relocated to High Half (0xFFFFFFC08xxxxxxx)!\n");
}
```

---

### Step 3: Create `my-linux/include/linux/slab.h` (9 Buckets: `8B .. 2048B`)

**What this file does:**
Defines the 9 power-of-two `kmalloc_caches` starting at **8 bytes** (`8, 16, 32, 64, 128, 256, 512, 1024, 2048`), matching upstream `include/linux/slab.h`.
Each `struct kmem_cache` uses `struct list_head partial;` (from `<linux/list.h>`) to link its active 4KB slab pages!

```c
/* Target File: my-linux/include/linux/slab.h */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SLAB_H
#define _LINUX_SLAB_H

#include <linux/types.h>
#include <linux/list.h>

#define KMALLOC_MIN_SIZE    8UL
#define KMALLOC_MAX_CACHE   2048UL
#define KMALLOC_NUM_CACHES  9       /* 8, 16, 32, 64, 128, 256, 512, 1024, 2048 */

/*
 * struct kmem_cache - Upstream Linux SLUB style cache descriptor
 * @obj_size: Size of each object slot in bytes (8..2048)
 * @partial:  Doubly-linked list of struct page slabs that have free slots
 */
struct kmem_cache {
    unsigned int obj_size;
    struct list_head partial;
};

void kmem_cache_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void kfree(const void *ptr);

#endif /* _LINUX_SLAB_H */
```

---

### Step 4: Create `my-linux/mm/slab.c` (Upstream `slub.c` Design!)

**Before writing the code, trace how upstream `slub.c` works here:**
1. **`set_freepointer(obj, next)` and `get_freepointer(obj)`**:
   Stores/reads the `void *` pointer to the next free slot in the first 8 bytes of `obj`. Because `sizeof(void *) == 8`, this works for **8-byte slots** (`kmalloc-8`) all the way up to `2048-byte slots`!
2. **`cache_grow(cache)`**:
   - Calls `alloc_page()` to get a `struct page *page`.
   - Sets `page->slab_cache = cache;` and `page->inuse = 0;`.
   - Slices the 4096-byte page into `4096 / obj_size` slots, linking them into `page->freelist` using `set_freepointer()`.
   - Links `&page->list` into `&cache->partial` using `list_add_tail(&page->list, &cache->partial)`!
3. **`kmalloc(size)`**:
   - Finds the bucket `cache = &kmalloc_caches[idx]`.
   - Takes the first slab page from `cache->partial` (`struct page *page = list_first_entry(&cache->partial, struct page, list)`).
   - Pops the first object from `page->freelist` using `get_freepointer(obj)` and increments `page->inuse++`.
   - If `page->freelist == NULL` (all slots in this 4KB page are now in use!), removes `page->list` from `cache->partial` (`list_del(&page->list)`).
4. **`kfree(ptr)`**:
   - Finds the `struct page *page = phys_to_page(__pa(ptr))`.
   - Reads `struct kmem_cache *cache = page->slab_cache;`.
   - If `cache == NULL`, it was a full 4KB page $\rightarrow$ calls `free_page(page)`.
   - Otherwise, pushes `ptr` back onto `page->freelist` via `set_freepointer(ptr, page->freelist)` and decrements `page->inuse--`.
   - If the page was previously 100% full (`was_full`), adds it back to `cache->partial` (`list_add(&page->list, &cache->partial)`).

```c
/* Target File: my-linux/mm/slab.c */
/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/pgtable.h>

void early_puts(const char *s);

/*
 * The 9 power-of-two size-class caches (matching upstream linux/mm/slub.c):
 * 0: 8B | 1: 16B | 2: 32B | 3: 64B | 4: 128B | 5: 256B | 6: 512B | 7: 1024B | 8: 2048B
 */
static struct kmem_cache kmalloc_caches[KMALLOC_NUM_CACHES];

/*
 * Upstream linux/mm/slub.c helpers: read/write the next-free-slot pointer
 * directly in the first 8 bytes of a free object slot.
 */
static inline void *get_freepointer(void *object)
{
    return *(void **)object;
}

static inline void set_freepointer(void *object, void *fp)
{
    *(void **)object = fp;
}

/*
 * size_to_cache_index - Find which bucket (0..8) fits 'size' bytes
 */
static int size_to_cache_index(size_t size)
{
    unsigned int bucket_size = KMALLOC_MIN_SIZE; /* Starts at 8 bytes! */
    for (int i = 0; i < KMALLOC_NUM_CACHES; i++) {
        if (size <= bucket_size) {
            return i;
        }
        bucket_size <<= 1; /* 8 -> 16 -> 32 -> 64 -> 128 -> 256 -> 512 -> 1024 -> 2048 */
    }
    return -1; /* Larger than 2048 bytes */
}

/*
 * kmem_cache_init - Initialize the 9 power-of-two kmalloc buckets (8B .. 2048B)
 */
void kmem_cache_init(void)
{
    unsigned int size = KMALLOC_MIN_SIZE;
    for (int i = 0; i < KMALLOC_NUM_CACHES; i++) {
        kmalloc_caches[i].obj_size = size;
        INIT_LIST_HEAD(&kmalloc_caches[i].partial);
        size <<= 1;
    }
    early_puts("[SLAB] Upstream SLUB kmalloc caches (8B .. 2048B) initialized.\n");
}

/*
 * allocate_slab - Allocate one 4KB page, slice into objects, and add to cache->partial
 */
static int allocate_slab(struct kmem_cache *cache)
{
    struct page *page = alloc_page();
    if (!page) {
        return -1;
    }

    page->slab_cache = cache;
    page->inuse = 0;
    page->freelist = NULL;

    char *page_va = (char *)page_address(page);
    unsigned int obj_size = cache->obj_size;
    unsigned int count = PAGE_SIZE / obj_size;

    /* Link all slots from back to front so Slot 0 is at the head of page->freelist */
    for (int i = (int)count - 1; i >= 0; i--) {
        void *obj = (void *)(page_va + (i * obj_size));
        set_freepointer(obj, page->freelist);
        page->freelist = obj;
    }

    /* Link this slab page into the cache's partial list using <linux/list.h> */
    list_add_tail(&page->list, &cache->partial);
    return 0;
}

/*
 * kmalloc - Allocate a kernel memory object of at least 'size' bytes
 */
void *kmalloc(size_t size)
{
    if (size == 0) {
        return NULL;
    }

    int idx = size_to_cache_index(size);
    if (idx < 0) {
        if (size > PAGE_SIZE) {
            early_puts("[SLAB ERROR] kmalloc size exceeds PAGE_SIZE!\n");
            return NULL;
        }
        struct page *page = alloc_page();
        if (!page) {
            return NULL;
        }
        page->slab_cache = NULL; /* Full-page allocation */
        return page_address(page);
    }

    struct kmem_cache *cache = &kmalloc_caches[idx];

    /* If no partial slab page is available, allocate a new 4KB slab page */
    if (list_empty(&cache->partial)) {
        if (allocate_slab(cache) < 0) {
            return NULL;
        }
    }

    /* Get the first partial slab page */
    struct page *page = list_first_entry(&cache->partial, struct page, list);

    /* Pop the first free object from page->freelist */
    void *object = page->freelist;
    page->freelist = get_freepointer(object);
    page->inuse++;

    /* If this slab page is now completely full, remove it from cache->partial */
    if (page->freelist == NULL) {
        list_del(&page->list);
        INIT_LIST_HEAD(&page->list);
    }

    return object;
}

/*
 * kzalloc - Allocate a zero-initialized kernel memory object
 */
void *kzalloc(size_t size)
{
    void *ptr = kmalloc(size);
    if (!ptr) {
        return NULL;
    }

    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = 0;
    }
    return ptr;
}

/*
 * kfree - Return a kmalloc-allocated object back to its slab page
 */
void kfree(const void *ptr)
{
    if (!ptr) {
        return;
    }

    uintptr_t pa = __pa(ptr);
    struct page *page = phys_to_page(pa);
    struct kmem_cache *cache = page->slab_cache;

    /* Full-page allocation -> return directly to page allocator */
    if (!cache) {
        free_page(page);
        return;
    }

    int was_full = (page->freelist == NULL);

    /* Push object back onto the slab page's freelist */
    set_freepointer((void *)ptr, page->freelist);
    page->freelist = (void *)ptr;
    page->inuse--;

    /* If the slab page was previously full, put it back onto cache->partial */
    if (was_full) {
        list_add(&page->list, &cache->partial);
    }
}
```

---

### Step 5: Test High-Half Execution & `kmalloc(8)` in `my-linux/init/main.c`

1. In **`my-linux/Makefile`**, add `mm/slab.c` to `SRCS_C`:
   ```makefile
   SRCS_C  := drivers/tty/serial/8250_early.c init/main.c arch/riscv/kernel/setup.c arch/riscv/kernel/traps.c mm/page_alloc.c arch/riscv/mm/init.c mm/slab.c
   ```
2. In **`my-linux/init/main.c`**, include `<linux/slab.h>` and add the verification after `vm_init()`:

```c
/* Target File: my-linux/init/main.c */
#include <linux/slab.h>

/* Inside start_kernel(), after the MMU tests: */
    /* 9. Verify High-Half Stack Pointer (sp) and Program Counter (PC) */
    unsigned long current_sp, current_pc;
    __asm__ volatile("mv %0, sp" : "=r"(current_sp));
    __asm__ volatile("auipc %0, 0" : "=r"(current_pc));
    early_puts("[MMU TEST] Current High-Half PC: ");
    early_print_hex(current_pc);
    early_puts(" | High-Half SP: ");
    early_print_hex(current_sp);
    early_puts("\n");

    /* 10. Initialize and Test Upstream SLUB Allocator (8B .. 2048B) */
    kmem_cache_init();

    uint64_t *tiny1 = (uint64_t *)kzalloc(8); /* Uses 8-byte bucket (kmalloc-8)! */
    uint64_t *tiny2 = (uint64_t *)kzalloc(8); /* Should be exactly +8 bytes after tiny1! */
    early_puts("[SLAB TEST] Allocated two 8-byte objects (kmalloc-8):\n  tiny1 VA: ");
    early_print_hex((uintptr_t)tiny1);
    early_puts("\n  tiny2 VA: ");
    early_print_hex((uintptr_t)tiny2);
    early_puts("\n");

    /* Free tiny1, then allocate tiny3 (should reuse tiny1's exact 8B slot!) */
    kfree(tiny1);
    uint64_t *tiny3 = (uint64_t *)kzalloc(8);
    if (tiny3 == tiny1) {
        early_puts("  [SUCCESS] kfree(tiny1) + kzalloc(8) reused exact 8-byte slot!\n\n");
    }
    kfree(tiny2);
    kfree(tiny3);
```

---

## PART 4: Plain-Language Feynman Recap

1. **Upstream `relocate_enable_mmu` in `head.S`**: Relocates `ra` (`PC`), `sp`, `s0`, and `stvec` by adding `PAGE_OFFSET` (`0xFFFFFFC000000000UL`) right as `satp` turns on, so the kernel genuinely executes in the High Half!
2. **Upstream Two-Level SLUB (`mm/slab.c`)**:
   - **Level 1 (`struct list_head`)**: Links 4KB `struct page` slabs onto `cache->partial`.
   - **Level 2 (`set_freepointer` / `get_freepointer`)**: Links `8B..2048B` free object slots inside each `page->freelist` with zero extra structs!
   - **`kfree(ptr)`**: Finds `page = phys_to_page(__pa(ptr))` in $O(1)$ time, reads `page->slab_cache`, and pushes `ptr` right back onto `page->freelist`!

