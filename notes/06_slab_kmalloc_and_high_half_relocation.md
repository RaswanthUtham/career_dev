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
1. Instead of giving a whole 4096-byte page to one tiny struct, `kmalloc()` keeps a row of **8 buckets (Caches)** of power-of-two sizes:
   - **Bucket 0 (`32` bytes)**
   - **Bucket 1 (`64` bytes)**
   - **Bucket 2 (`128` bytes)**
   - **Bucket 3 (`256` bytes)**
   - **Bucket 4 (`512` bytes)**
   - **Bucket 5 (`1024` bytes)**
   - **Bucket 6 (`2048` bytes)**
   - *(And if someone asks for more than `2048` bytes, like `4096` bytes, it just gives them a whole page from `alloc_page()`!)*

```text
THE 6 BUCKETS (kmem_cache size classes):

  Bucket [0] (32-byte objects)   ---> [Free 32B] ---> [Free 32B] ---> NULL
  Bucket [1] (64-byte objects)   ---> [Free 64B] ---> [Free 64B] ---> NULL
  Bucket [2] (128-byte objects)  ---> NULL (Empty! Will slice a 4KB page when asked!)
  Bucket [3] (256-byte objects)  ---> NULL
  Bucket [4] (512-byte objects)  ---> NULL
  Bucket [5] (1024-byte objects) ---> NULL
  Bucket [6] (2048-byte objects) ---> NULL
```

2. **What happens when you call `kmalloc(48)`?**
   - `48` bytes rounds up to the nearest bucket size: **`64` bytes (Bucket 1)**.
   - Is Bucket 1 empty?
     - **If YES**: `kmalloc()` calls `alloc_page()` **once** to get one 4096-byte page, slices that 4096-byte page into **`4096 / 64 = 64` equal slots of 64 bytes**, and chains all 64 slots into Bucket 1's free list!
   - Then `kmalloc()` pops the first 64-byte slot off Bucket 1, zeroes it, and returns its High-Half Virtual Address (`0xFFFFFFC08xxxxxxx`)!
   - The next **63 times** you call `kmalloc(48)`, it doesn't even need to call `alloc_page()`—it instantly hands you the next 64-byte slot from Bucket 1 in 3 CPU instructions!

```text
HOW ONE 4096-BYTE PAGE IS SLICED INTO 64 SLOTS OF 64 BYTES:

  High-Half Page Virtual Address: 0xFFFFFFC08038D000 (4096 bytes total)
  +------------------+------------------+------------------+-----+------------------+
  | Slot 0 (64 B)    | Slot 1 (64 B)    | Slot 2 (64 B)    | ... | Slot 63 (64 B)   |
  | 0x...8038D000    | 0x...8038D040    | 0x...8038D080    |     | 0x...8038DFC0    |
  | [next = Slot 1]  | [next = Slot 2]  | [next = Slot 3]  |     | [next = NULL]    |
  +------------------+------------------+------------------+-----+------------------+
```

---

### 3. Wait — When Someone Calls `kfree(ptr)`, How Does `kfree()` Know Which Bucket Size `ptr` Belongs To?

Notice how in Linux, you call:
```c
void *p = kmalloc(48);
kfree(p);   /* Notice: You DO NOT pass the size '48' to kfree(p)! */
```
Wait a second! When you pass `p = 0xFFFFFFC08038D040` into `kfree(p)`, you only pass the pointer!
How on earth does `kfree(p)` know whether `p` is a `32`-byte slot, a `64`-byte slot, a `256`-byte slot, or a full `4096`-byte page?!

#### The Brilliant Linux Trick: Store the Slot Size Inside the Page's Index Card (`struct page`)!
Remember our **`mem_map[]` cabinet of 32-byte Index Cards (`struct page`)** from `mm/page_alloc.c`?
Every 4096-byte page has its own Index Card (`struct page`)!

Let's see how `kfree(ptr)` finds the exact bucket size in **3 instant steps**:
```text
HOW kfree(0xFFFFFFC08038D040) FINDS ITS BUCKET SIZE IN O(1) TIME:

  Step 1: Convert High-Half Virtual Address 'ptr' to Physical Address:
          phys = __pa(0xFFFFFFC08038D040) = 0x000000008038D040

  Step 2: Look up the 4KB Page's Index Card in mem_map[]:
          struct page *card = phys_to_page(0x8038D040);
          (Why does this work even though 0x8038D040 has offset +0x040?
           Because phys_to_page() does (phys - 0x80000000) >> 12, which
           automatically chops off the bottom 12 bits (+0x040) and finds
           the exact Index Card for page 0x8038D000!)

  Step 3: Read 'card->slab_size' (or 'card->flags') on that Index Card!
          When kmalloc() sliced page 0x8038D000 into 64-byte slots, it wrote:
              card->slab_size = 64;
          So kfree(ptr) looks at card->slab_size, sees '64', and puts 'ptr'
          right back into the 64-byte bucket!
```

Pause and appreciate how cool that is!
Earlier, you asked: *"Why does Linux have `struct page` Index Cards in `mem_map[]` instead of just returning raw pointers?"*
**Here is your first real-world payoff!** Because every 4096-byte page has a `struct page` Index Card in `mem_map[]`, we can store `unsigned int slab_size;` on that card, and `kfree(ptr)` can find the size of ANY pointer anywhere in RAM in **one bit-shift** without wasting a single byte of header inside the object itself!

---

## PART 2: Relocating Kernel Execution (`PC`, `sp`, `stvec`) to the High Half!

### 1. Where Are Our CPU Registers Pointing Right Now?

Let's check what addresses the CPU registers hold right after `enable_mmu()` finishes in our current kernel:
- **`va_pa_offset`**: `0xFFFFFFC000000000UL` (High Half!)
- **`swapper_pg_dir`**: `0xFFFFFFC080308000` (High Half!)
- **What about `PC` (Program Counter), `sp` (Stack Pointer), and `stvec` (Trap Vector)?**
  They are still at Low-Half Identity addresses (`0x000000008020xxxx`)!

### 2. Why Should We Relocate `PC`, `sp`, and `stvec` to the High Half (`0xFFFFFFC0802xxxxx`)?

Because in Milestone 7, when we run **User Space programs** (like the shell `sh`), user programs live in the **Low Half** (`Rows 0..255` of the page table).
If the kernel's Stack Pointer (`sp`), Program Counter (`PC`), and Trap Vector (`stvec`) are already up in the **High Half** (`0xFFFFFFC0802xxxxx`, which uses `Rows 256..511`):
1. Every user process can have its own private Low Half (`Rows 0..255`) while sharing the Kernel's High Half (`Rows 256..511`).
2. Even if we unmap the temporary Low-Half identity map later, the kernel keeps running in the High Half without missing a beat!

### 3. How Do We Jump `sp`, `stvec`, and `ra` (Return Address) into the High Half?

Inside `enable_mmu()` in `my-linux/arch/riscv/mm/init.c`, right after `csrw satp` turns on the MMU (when both Low-Half and High-Half mappings are active simultaneously), we add `PAGE_OFFSET` (`0xFFFFFFC000000000UL`) to:
1. **`sp` (Stack Pointer)**: `sp = sp + PAGE_OFFSET`
2. **`s0` / `fp` (Frame Pointer)**: `s0 = s0 + PAGE_OFFSET` (if non-zero)
3. **`stvec` (Supervisor Trap Vector)**: `stvec = handle_exception + PAGE_OFFSET`
4. **`ra` (Return Address)**: `ra = ra + PAGE_OFFSET`

Look what happens when `enable_mmu()` executes `ret` (return to `start_kernel`):
Because `ra` now has `+ 0xFFFFFFC000000000UL`, the `ret` instruction jumps `PC` directly to **`0xFFFFFFC08020xxxx`** in the High Half!
Why does all of our compiled C code keep working at `0xFFFFFFC08020xxxx` even though `vmlinux.lds.S` started at `0x80200000`?
**Because in `Makefile` we compiled with `-mcmodel=medany` (PC-relative addressing)!**
Every instruction in our kernel (`auipc` + `addi`) calculates addresses *relative to the current `PC`*. So the instant `PC` jumps to `0xFFFFFFC08020xxxx`, every function call, string literal, and global variable automatically resolves to `0xFFFFFFC08020xxxx`!

```text
VISUAL TRACE OF THE HIGH-HALF JUMP IN enable_mmu():

  Before enable_mmu():
    PC    = 0x0000000080200150  (Low-Half Identity Address)
    sp    = 0x0000000080207FF0  (Low-Half Identity Stack)
    stvec = 0x0000000080200040  (Low-Half Trap Vector)

  Inside enable_mmu() after csrw satp:
    1. sp    += 0xFFFFFFC000000000  --->  sp    = 0xFFFFFFC080207FF0 (High-Half Stack!)
    2. stvec += 0xFFFFFFC000000000  --->  stvec = 0xFFFFFFC080200040 (High-Half Trap Vector!)
    3. ra    += 0xFFFFFFC000000000  --->  ra    = 0xFFFFFFC080200310 (High-Half Return Addr!)
    4. ret (jumps PC to ra!)        --->  PC    = 0xFFFFFFC080200310 (High-Half Code!)
```

---

## PART 3: Step-by-Step Implementation in `my-linux/`

Here are the exact files to update or create in your `my-linux/` workspace.

---

### Step 1: Add `slab_size` to `struct page` in `my-linux/include/linux/mm.h`

**What this change does:**
We add `unsigned int slab_size;` inside `struct page` so that when a 4KB page is sliced by `kmalloc()` into `32`, `64`, `128`, ..., `2048` byte slots, its Index Card remembers the slot size! (And if a whole 4KB page is allocated for a large `kmalloc(4096)`, `slab_size` is set to `4096`).

Update `struct page` in **`my-linux/include/linux/mm.h`**:

```c
/* Target File: my-linux/include/linux/mm.h */

struct page {
    unsigned long flags;
    int refcount;
    unsigned int slab_size;     /* Object size in bytes if managed by kmalloc/slab */
    struct list_head list;
};
```

Wait—let's check the size of `struct page` in 64-bit C!
- `unsigned long flags`: `8` bytes (offset `0..7`)
- `int refcount`: `4` bytes (offset `8..11`)
- `unsigned int slab_size`: `4` bytes (offset `12..15` — fits right into the 4-byte padding hole that was already next to `int refcount`!)
- `struct list_head list`: `16` bytes (offset `16..31`)
**Total size of `struct page` is STILL 32 bytes!** Putting `slab_size` next to `int refcount` uses the 4 bytes of compiler padding that were sitting unused, so `sizeof(struct page)` does not grow by a single byte!

Also in **`my-linux/mm/page_alloc.c`**, initialize `mem_map[i].slab_size = 0;` inside `mm_init()`, and update `page_address(page)` in **`my-linux/include/linux/mm.h`** so it uses `__va(page_to_phys(page))` (or keep `page_address()` returning the virtual address):

In **`my-linux/include/linux/mm.h`**, make sure `page_address` uses `va_pa_offset` so it works even after relocating to the High Half:
```c
/* Target File: my-linux/include/linux/mm.h */
extern unsigned long va_pa_offset;

static inline void *page_address(const struct page *page)
{
    return (void *)(page_to_phys(page) + va_pa_offset);
}
```
And in **`my-linux/mm/page_alloc.c`**, inside `enable_mmu()` when we switch to the High Half, we will also add `PAGE_OFFSET` to `mem_map` and `free_page_list` pointers—or even simpler: let's see how cleanly that works!

---

### Step 2: Create `my-linux/include/linux/slab.h`

**What this file does:**
Defines the standard Linux kernel `kmalloc()`, `kzalloc()`, and `kfree()` interface (matching upstream `linux/include/linux/slab.h`).

```c
/* Target File: my-linux/include/linux/slab.h */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_SLAB_H
#define _LINUX_SLAB_H

#include <linux/types.h>

#define KMALLOC_MIN_SIZE    32UL
#define KMALLOC_MAX_CACHE   2048UL
#define KMALLOC_NUM_CACHES  7       /* 32, 64, 128, 256, 512, 1024, 2048 */

/*
 * struct kmem_cache - Manages a pool of fixed-size objects (e.g. 64-byte slots)
 */
struct kmem_cache {
    unsigned int obj_size;          /* Size of each slot in bytes (32..2048) */
    unsigned long nr_free;          /* Number of free slots currently in list */
    void *free_list;                /* Singly-linked intrusive free list of slots */
};

void kmem_cache_init(void);
void *kmalloc(size_t size);
void *kzalloc(size_t size);
void kfree(const void *ptr);

#endif /* _LINUX_SLAB_H */
```

---

### Step 3: Create `my-linux/mm/slab.c`

**Before writing the code, let's trace how the intrusive slot list inside `mm/slab.c` works:**
1. Each free slot (at least `32` bytes wide) is currently unused memory.
2. We use the first `8` bytes of each free slot to store a `struct slab_obj { struct slab_obj *next; };` pointer to the next free slot in that bucket!
3. When `kmalloc(size)` needs to replenish an empty bucket (say, the `64`-byte bucket):
   - It calls `alloc_page()` to get a `struct page *card`.
   - Sets `card->slab_size = 64`.
   - Gets the page's virtual address: `char *page_va = (char *)page_address(card);`.
   - Loops through the 4096 bytes in steps of `64` bytes (`offset = 0, 64, 128, ..., 4032`) and pushes each slot onto `cache->free_list`!
4. When `kfree(ptr)` is called:
   - Converts `ptr` to its physical address: `uintptr_t pa = __pa(ptr);`.
   - Finds its Index Card: `struct page *card = phys_to_page(pa);`.
   - Checks `card->slab_size`:
     - If `card->slab_size == PAGE_SIZE` (`4096`): calls `free_page(card)`.
     - If `card->slab_size` is `32..2048`: finds the matching bucket and pushes `ptr` right back onto `cache->free_list`!

```c
/* Target File: my-linux/mm/slab.c */
/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/pgtable.h>

void early_puts(const char *s);

/* Intrusive pointer stored in the first 8 bytes of every free object slot */
struct slab_obj {
    struct slab_obj *next;
};

/*
 * The 7 power-of-two size-class caches:
 * Index 0: 32 B | 1: 64 B | 2: 128 B | 3: 256 B | 4: 512 B | 5: 1024 B | 6: 2048 B
 */
static struct kmem_cache kmalloc_caches[KMALLOC_NUM_CACHES];

/*
 * size_to_cache_index - Find which bucket (0..6) fits 'size' bytes
 */
static int size_to_cache_index(size_t size)
{
    unsigned int bucket_size = KMALLOC_MIN_SIZE; /* 32 */
    for (int i = 0; i < KMALLOC_NUM_CACHES; i++) {
        if (size <= bucket_size) {
            return i;
        }
        bucket_size <<= 1; /* Multiply by 2: 32 -> 64 -> 128 -> ... */
    }
    return -1; /* Larger than 2048 bytes */
}

/*
 * kmem_cache_init - Initialize the 7 power-of-two kmalloc buckets
 */
void kmem_cache_init(void)
{
    unsigned int size = KMALLOC_MIN_SIZE;
    for (int i = 0; i < KMALLOC_NUM_CACHES; i++) {
        kmalloc_caches[i].obj_size = size;
        kmalloc_caches[i].nr_free = 0;
        kmalloc_caches[i].free_list = NULL;
        size <<= 1;
    }
    early_puts("[SLAB] kmalloc size-class caches (32B .. 2048B) initialized.\n");
}

/*
 * cache_grow - Allocate one 4KB page and slice it into fixed-size slots
 */
static int cache_grow(struct kmem_cache *cache)
{
    struct page *card = alloc_page();
    if (!card) {
        return -1;
    }

    /* Record the slot size on the page's Index Card so kfree() knows its size! */
    card->slab_size = cache->obj_size;

    char *page_va = (char *)page_address(card);
    unsigned int obj_size = cache->obj_size;
    unsigned int count = PAGE_SIZE / obj_size;

    /* Slice the 4096-byte page from back to front so Slot 0 is popped first! */
    for (int i = (int)count - 1; i >= 0; i--) {
        struct slab_obj *obj = (struct slab_obj *)(page_va + (i * obj_size));
        obj->next = (struct slab_obj *)cache->free_list;
        cache->free_list = obj;
        cache->nr_free++;
    }

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

    /* If request is larger than 2048 bytes (up to 4096 bytes), allocate a full page */
    int idx = size_to_cache_index(size);
    if (idx < 0) {
        if (size > PAGE_SIZE) {
            early_puts("[SLAB ERROR] kmalloc size exceeds PAGE_SIZE!\n");
            return NULL;
        }
        struct page *card = alloc_page();
        if (!card) {
            return NULL;
        }
        card->slab_size = PAGE_SIZE;
        return page_address(card);
    }

    struct kmem_cache *cache = &kmalloc_caches[idx];

    /* If bucket is empty, slice a new 4KB page into slots */
    if (!cache->free_list) {
        if (cache_grow(cache) < 0) {
            return NULL;
        }
    }

    /* Pop the first free slot from the bucket */
    struct slab_obj *obj = (struct slab_obj *)cache->free_list;
    cache->free_list = obj->next;
    cache->nr_free--;

    return (void *)obj;
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

    /* Zero out the requested bytes */
    uint8_t *p = (uint8_t *)ptr;
    for (size_t i = 0; i < size; i++) {
        p[i] = 0;
    }
    return ptr;
}

/*
 * kfree - Return a kmalloc-allocated object back to its size-class bucket
 */
void kfree(const void *ptr)
{
    if (!ptr) {
        return;
    }

    /* 1. Convert Kernel VA -> Physical Address -> struct page Index Card! */
    uintptr_t pa = __pa(ptr);
    struct page *card = phys_to_page(pa);
    unsigned int obj_size = card->slab_size;

    /* 2. If it was a full 4KB page allocation, return it to page_alloc */
    if (obj_size == PAGE_SIZE) {
        card->slab_size = 0;
        free_page(card);
        return;
    }

    /* 3. Otherwise, find the matching bucket and push the slot back onto free_list */
    int idx = size_to_cache_index(obj_size);
    if (idx < 0) {
        early_puts("[SLAB ERROR] kfree() called on invalid pointer!\n");
        return;
    }

    struct kmem_cache *cache = &kmalloc_caches[idx];
    struct slab_obj *obj = (struct slab_obj *)ptr;
    obj->next = (struct slab_obj *)cache->free_list;
    cache->free_list = obj;
    cache->nr_free++;
}
```

---

### Step 4: Test `kmalloc()` and `kfree()` in `my-linux/init/main.c`

1. In **`my-linux/Makefile`**, add `mm/slab.c` to `SRCS_C`:
   ```makefile
   SRCS_C  := drivers/tty/serial/8250_early.c init/main.c arch/riscv/kernel/setup.c arch/riscv/kernel/traps.c mm/page_alloc.c arch/riscv/mm/init.c mm/slab.c
   ```
2. In **`my-linux/init/main.c`**, include `<linux/slab.h>` and add the Slab Allocator verification after `vm_init()`:

```c
/* Target File: my-linux/init/main.c */
#include <linux/slab.h>

/* Inside start_kernel(), after the MMU tests: */
    /* 9. Initialize and Test Sub-Page Slab Allocator (kmalloc / kfree) */
    kmem_cache_init();

    void *obj1 = kzalloc(48); /* Rounds up to 64-byte bucket */
    void *obj2 = kzalloc(48); /* Should be 64 bytes (+0x40) right after obj1! */
    early_puts("[SLAB TEST] Allocated two 48B objects (64B bucket):\n  obj1 VA: ");
    early_print_hex((uintptr_t)obj1);
    early_puts("\n  obj2 VA: ");
    early_print_hex((uintptr_t)obj2);
    early_puts("\n");

    /* Free obj1, then allocate obj3 (should reuse obj1's exact slot!) */
    kfree(obj1);
    void *obj3 = kzalloc(48);
    if (obj3 == obj1) {
        early_puts("  [SUCCESS] kfree(obj1) + kzalloc(48) reused exact 64B slot!\n\n");
    }
    kfree(obj2);
    kfree(obj3);
```

---

## PART 4: Plain-Language Feynman Recap

1. **`alloc_page()` is the Wholesale Warehouse**: It hands out whole 4096-byte pages (`struct page *` Index Cards).
2. **`kmalloc()` is the Retail Store Built on Top**: It takes a single 4096-byte page from `alloc_page()`, slices it into `32`, `64`, `128`, `256`, `512`, `1024`, or `2048` byte slots, and stamps `card->slab_size` onto the page's Index Card.
3. **`kfree(ptr)` is Automatic Sorting**: It converts `ptr` to its physical address (`__pa(ptr)`), looks up its Index Card (`phys_to_page(pa)`), reads `card->slab_size`, and drops the slot right back into the matching bucket!
