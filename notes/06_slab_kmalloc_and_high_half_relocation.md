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

### 1. How Does Upstream `linux/arch/riscv/kernel/head.S` Relocate `PC`, `sp`, and `stvec`?

Let's look at the actual upstream Linux code in `C:\workspace\career_ws\linux\arch\riscv\kernel\head.S` (lines 72–118: `relocate_enable_mmu`).

When our kernel boots, `_start` is running at Low-Half physical/identity addresses (`0x000000008020xxxx`).
After `vm_init()` builds `swapper_pg_dir` (which maps **both** Low-Half `0x000000008020xxxx` and High-Half `0xFFFFFFC08020xxxx`), how does upstream Linux switch the CPU registers (`PC`, `ra`, `sp`, `s0`, and `stvec`) from `0x000000008020xxxx` up to `0xFFFFFFC08020xxxx`?

Upstream Linux writes a clean assembly function **`relocate_enable_mmu(pgd_t root_pgd)`** in **`arch/riscv/kernel/head.S`**!

#### Why Does Upstream Linux Put `relocate_enable_mmu` in `head.S` Instead of Inline Assembly in C?
Because if you change `sp` (Stack Pointer) and `s0` (Frame Pointer) in the middle of a C function, the C compiler does not know you moved the stack!
When `relocate_enable_mmu` is a dedicated assembly function in `head.S`:
1. **`a0` input**: Holds the physical address of `swapper_pg_dir`.
2. **Step 1 — Relocate Return Address (`ra`) to High Half**:
    loads `t1 = PAGE_OFFSET` (`0xFFFFFFC000000000UL`) and adds it to `ra`:
   `add ra, ra, t1`
   Now `ra` holds the High-Half return address (`0xFFFFFFC08020xxxx`)!
3. **Step 2 — Relocate Stack Pointer (`sp`) and Frame Pointer (`s0`) to High Half**:
   `add sp, sp, t1`
   `beqz s0, 1f; add s0, s0, t1; 1:`
4. **Step 3 — Point `stvec` to the High-Half Address of `handle_exception`**:
   `la t0, handle_exception`
   `add t0, t0, t1`
   `csrw stvec, t0`
5. **Step 4 — Compute and Load `satp` (`SATP_MODE_SV39 | (root_phys >> 12)`)**:
   `srli a0, a0, 12`
   `li t2, (8 << 60)`
   `or a0, a0, t2`
   `sfence.vma zero, zero`
   `csrw satp, a0`
   `sfence.vma zero, zero`
6. **Step 5 — Return (`ret`)**:
   When `ret` executes, the CPU jumps `PC` to `ra` (`0xFFFFFFC08020xxxx`)!
   At that exact instant, **`PC`, `sp`, `s0`, and `stvec` are all executing in the High Half (`0xFFFFFFC08020xxxx`)!**

```text
VISUAL TRACE OF UPSTREAM relocate_enable_mmu IN arch/riscv/kernel/head.S:

  Caller: enable_mmu() in arch/riscv/mm/init.c (at Low-Half PC = 0x80200300)
          Calls relocate_enable_mmu(root_phys)
                   |
                   v
  +-----------------------------------------------------------------------+
  | Inside relocate_enable_mmu (arch/riscv/kernel/head.S):                |
  |   t1 = 0xFFFFFFC000000000 (PAGE_OFFSET)                               |
  |   1. ra    = ra + t1    ---> 0xFFFFFFC080200304 (High-Half Return PC) |
  |   2. sp    = sp + t1    ---> 0xFFFFFFC080207FE0 (High-Half Stack!)    |
  |   3. s0    = s0 + t1    ---> 0xFFFFFFC080207FF0 (High-Half Frame Ptr) |
  |   4. stvec = handle_exception + t1 ---> High-Half Trap Vector!        |
  |   5. csrw satp, a0; sfence.vma     ---> Sv39 MMU Active!              |
  |   6. ret                           ---> Jumps PC to High-Half 'ra'!   |
  +-----------------------------------------------------------------------+
                   |
                   v
  Back in enable_mmu():
    - CPU is now executing at High-Half PC = 0xFFFFFFC080200304!
    - Stack Pointer (sp) is at High-Half 0xFFFFFFC080207FE0!
    - Trap Vector (stvec) is at High-Half 0xFFFFFFC08020xxxx!
```

### 2. Wait — What About `mem_map` and `free_page_list` After We Jump to High Half?
Think carefully!
Before `enable_mmu()` ran, `mm_init()` initialized:
- `mem_map` = `0x0000000080208000` (Low-Half address).
- Because our Low-Half identity map (`0x80200000..0x88000000`) is **still mapped** in `swapper_pg_dir` alongside the High-Half map, those pointers still work, OR we can relocate `mem_map` to the High Half inside `mm_init()` by calling `vm_init()` right after `mm_init()`, or adding `PAGE_OFFSET`!
Even simpler: if we relocate `mem_map` and `free_page_list` (or keep Low-Half identity mapped until user processes start), let's see what happens when `phys_to_page(pa)` and `page_to_pfn(page)` run:
- Look at `page_to_pfn(page)` in `include/linux/mm.h`:
  ```c
  return (unsigned long)(page - mem_map);
  ```
  Notice that `page_to_pfn` subtracts `page - mem_map`!
  As long as `page` comes from `pfn_to_page(pfn)` (`&mem_map[pfn]`), `page` and `mem_map` are in the same address space!
  And if we update `mem_map` and `free_page_list` to High-Half in `enable_mmu()` (or call a 5-line helper `mm_relocate_high_half(PAGE_OFFSET)` in `mm/page_alloc.c`), then **even `struct page *` pointers live in `0xFFFFFFC08020xxxx`**!

---

## PART 3: Step-by-Step Implementation in `my-linux/`

Here are the exact files for you to update or create in `my-linux/`.

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
       /* 1. Relocate all next/prev pointers in the circular free_page_list */
       struct list_head *curr = &free_page_list;
       do {
           struct list_head *next_low = curr->next;
           if (curr->next != &free_page_list) {
               curr->next = (struct list_head *)((uintptr_t)curr->next + offset);
           }
           if (curr->prev != &free_page_list) {
               curr->prev = (struct list_head *)((uintptr_t)curr->prev + offset);
           }
           curr = next_low;
       } while (curr != &free_page_list);

       /* 2. Relocate the mem_map base pointer itself to High Half */
       mem_map = (struct page *)((uintptr_t)mem_map + offset);
   }
   ```
   *(Notice how clever that is: because `free_page_list` itself is a global variable in `.data`, once `PC` jumps to the High Half in `-mcmodel=medany`, `&free_page_list` automatically resolves to its High-Half address! Better yet, if we re-thread `free_page_list` cleanly after moving `mem_map`, let's see how easy a 5-line re-init of the `next`/`prev` pointers is:)*
   ```c
   /* Even simpler and 100% bug-free way to relocate free_page_list in mm/page_alloc.c: */
   void mm_relocate_high_half(unsigned long offset)
   {
       mem_map = (struct page *)((uintptr_t)mem_map + offset);
       INIT_LIST_HEAD(&free_page_list);
       for (unsigned long pfn = min_low_pfn; pfn < max_pfn; pfn++) {
           struct page *page = pfn_to_page(pfn);
           if (page->refcount == 0) {
               list_add_tail(&page->list, &free_page_list);
           }
       }
   }
   ```
   Look at those 8 lines!
   - `mem_map` moves to High-Half `0xFFFFFFC080208000`.
   - `INIT_LIST_HEAD(&free_page_list)` uses the current High-Half `&free_page_list` address.
   - The `for` loop re-links every unallocated page (`page->refcount == 0`) using its new High-Half `&mem_map[pfn].list` address! Zero pointer-aliasing bugs!

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

