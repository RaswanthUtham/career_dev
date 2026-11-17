# Physical Memory Allocator (`struct page`) & Sv39 Virtual Memory (`MMU`)
## The Complete Unified Guide (Taught for a 12-Year-Old, Written in the Linux Kernel Way)

---

## PART 1: Answering Your 3 Big Architectural Questions First!

Before we look at any code, let's answer the exact three questions you just asked. Your intuition is 100% spot-on!

---

### Question 1: "Isn't `struct page *page` a confusing name? Isn't `page_index_address` or `page_descriptor` much clearer?"
**YES! You are 100% right.**
In fact, even veteran Linux kernel developers complain about this!
- When someone hears the word **"page"**, they naturally picture the **4096-byte block of RAM** (`0x80308000`).
- But in the Linux kernel (`include/linux/mm_types.h`), Linus Torvalds named the 32-byte metadata tracking card **`struct page`**, and kernel engineers write `struct page *page = alloc_page();`.
- **Why did Linux name it `struct page`?** Because 30 years ago in Linux 0.01, kernel engineers wanted a short 4-letter word to type thousands of times across the kernel.
- **How to read it in your head**: Every single time you see `struct page *page` in Linux kernel code, mentally read it as:
  **`struct page_card *card_ptr`** (the address of the **32-byte Index Card** inside the `mem_map[]` cabinet, **NOT** the 4096-byte RAM block!).

---

### Question 2: "Why doesn't `alloc_page()` just return the 4096-byte Physical Address (or Virtual Address) directly instead of the Index Card?"
This is the best design question in OS memory management!

#### A. How RTOSes (like Zephyr / FreeRTOS) and xv6 Do It:
In an **RTOS** (`k_mem_slab_alloc` / `k_heap_alloc` in Zephyr) or in a simple teaching OS like **xv6** (`kalloc()`):
- There is **NO** `struct page` index card cabinet!
- The allocator **DOES** return the raw 4096-byte memory address (`void *`, e.g., `0x80308000`) directly!
- Why? Because in an RTOS, once a task allocates a memory block, it owns it alone. There is no page sharing between processes, no Copy-On-Write (`fork`), and no swapping to disk.

#### B. Why Linux Returns the Index Card (`struct page *`) Instead of the Raw Address:
In the **Linux Kernel**, a 4096-byte physical page of RAM is not just a dumb buffer—the kernel has to track **metadata** about that page while it is being used:
1. **`refcount` (Reference Count)**: When a process calls `fork()`, Parent and Child **share the exact same physical 4096-byte page** (Copy-On-Write). Both point to the same `struct page` card, and the card says `refcount = 2`! When Parent exits, `free_page(card)` sees `refcount` drop from `2` to `1`, so it knows **NOT** to erase the page because the Child is still using it!
   *(If `alloc_page()` only returned raw address `0x80308000`, where would you store `refcount`? You can't store it inside `0x80308000`, because `0x80308000` is full of the user's data!)*
2. **`flags` (Locked, Dirty, Reserved, DMA-pinned)**: A DMA controller or disk driver needs to mark a page as "pinned for DMA" or "dirty" by setting bits in `card->flags`.
3. **Linux Actually Provides BOTH Functions!**
   Because sometimes you want the **Card** (`struct page *`) and sometimes you just want the **4096-byte RAM pointer** directly, the real Linux kernel provides **two** wrapper functions:

```text
+-----------------------------------+---------------------------------------------------------------+
| Linux Kernel Function             | What It Returns                                               |
+-----------------------------------+---------------------------------------------------------------+
| alloc_page()                      | Returns struct page * (The 32-byte Index Card: 0x8020E100)    |
| get_zeroed_page() / __get_free_page| Calls alloc_page(), converts it, and returns the 4096-byte    |
|                                   | RAM address (0x80308000) directly!                            |
+-----------------------------------+---------------------------------------------------------------+
```

Look how simple `get_zeroed_page()` is—it is literally a 3-line wrapper around `alloc_page()`:
```c
void *get_zeroed_page(void)
{
    struct page *card = alloc_page();
    if (!card) return NULL;
    return page_address(card); /* Returns 0x80308000 directly! */
}
```
So in Linux:
- Subsystems that need metadata (`refcount`, `flags`, DMA scatter-gather lists) call **`alloc_page()`** to get the **Card** (`struct page *`).
- Subsystems that just want a 4096-byte buffer call **`get_zeroed_page()`** (or `page_to_phys(alloc_page())`) to get the **4096-byte RAM address** directly!

---

## PART 2: The Physical Memory Allocator (The Index Card Cabinet)

### 1. Visualizing Physical RAM: The Cabinet vs. The Books

We have **128 MB** of physical RAM starting at `0x80000000` and ending at `0x88000000`.
Every **1 Page = 4096 bytes (4 KB)**.
How many 4096-byte pages exist in 128 MB?
`128 MB / 4 KB = 32,768 pages` (numbered `PFN 0` to `PFN 32767`).

Right after our kernel code (`_end = 0x80208000`), we build an **Index Card Cabinet** called **`mem_map[]`**:
- There are **32,768 cards** in the cabinet—one card for each 4096-byte page of RAM.
- Each card (`struct page`) is only **32 bytes** in size.
- Total size of the cabinet = `32,768 cards * 32 bytes = 1,048,576 bytes (1 MB)`.

```text
PHYSICAL DRAM LAYOUT IN my-linux (0x80000000 to 0x88000000):

0x80000000 +------------------------------------------------------------+
           | OpenSBI Firmware (2 MB: Pages 0 to 511)                    |
0x80200000 +------------------------------------------------------------+
           | my-linux Kernel Code & Data (Pages 512 to 519)             |
0x80208000 +------------------------------------------------------------+ <-- _end (mem_map starts here!)
           |                                                            |
           | THE INDEX CARD CABINET: struct page mem_map[32768]         |
           |   (Each card is only 32 bytes! Total cabinet = 1 MB)       |
           |                                                            |
           |   Card #0   (&mem_map[0])   is at 0x80208000 (32 bytes)    |
           |   Card #1   (&mem_map[1])   is at 0x80208020 (32 bytes)    |
           |   ...                                                      |
           |   Card #776 (&mem_map[776]) is at 0x8020E100 (32 bytes) <--+-- alloc_page() returns THIS!
           |   Card #777 (&mem_map[777]) is at 0x8020E120 (32 bytes)    |   (Address of the 32-byte card!)
           |   ...                                                      |
0x80308000 +------------------------------------------------------------+ <-- free_ram_start (PFN 776)
           |                                                            |
           | THE ACTUAL 4096-BYTE RAM PAGES (Usable Free Pages!):       |
           |                                                            |
           |   Page #776 (4096 bytes)    is at 0x80308000 <-------------+-- page_to_phys(card) returns THIS!
           |   Page #777 (4096 bytes)    is at 0x80309000               |   (Address of the 4096-byte page!)
           |   Page #778 (4096 bytes)    is at 0x8030A000               |
           |   ...                                                      |
0x88000000 +------------------------------------------------------------+
```

---

### 2. How the Cards Are Linked Together (`struct list_head` & `container_of`)

How do we know which 4096-byte pages are free without searching all 32,768 cards?
We put a metal ring (`struct list_head list`) inside every `struct page` card, and clip all free cards onto a circular chain called **`free_page_list`**!

**Target File**: `my-linux/include/linux/list.h`
```c
struct list_head {
    struct list_head *next, *prev;
};
```

```text
+-----------------------+       +-------------------------+       +-------------------------+
| free_page_list (Head) | <---> | Card #776 (struct page) | <---> | Card #777 (struct page) |
| struct list_head      |       |   flags, refcount       |       |   flags, refcount       |
|   next, prev          |       |   struct list_head list |       |   struct list_head list |
+-----------------------+       +-------------------------+       +-------------------------+
            ^                                                                  |
            +------------------------------------------------------------------+
```

#### How `container_of` Finds the Start of the Card from the Ring Pointer:
Look inside one 32-byte `struct page` card at address `0x8020E100`:
```text
Card #776 (&mem_map[776]):
  0x8020E100 (+0x00):  unsigned long flags;        (8 bytes)
  0x8020E108 (+0x08):  int refcount; (+4 pad)      (8 bytes)
  0x8020E110 (+0x10):  struct list_head list;      (16 bytes) <-- free_page_list.next points HERE!
```
When we follow `free_page_list.next`, we land at `0x8020E110` (the `list` ring, 16 bytes inside the card).
To get the start of the card (`0x8020E100`), `container_of` simply **subtracts 16 bytes**:
`0x8020E110 - 16 = 0x8020E100`!

**Target File**: `my-linux/include/linux/container_of.h`
```c
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({                      \
    const typeof(((type *)0)->member) *__mptr = (ptr);          \
    (type *)((char *)__mptr - offsetof(type, member));          \
})
```

---

### 3. The Math of Converting Between a Card (`struct page *`) and a 4KB RAM Address

Let's trace the exact math in **`my-linux/include/linux/mm.h`**:

#### Math 1: Card Pointer -> Card Number (`page_to_pfn`)
Suppose we have Card pointer `card = 0x8020E100`, and `mem_map` starts at `0x80208000`.
Which card number (`PFN`) is this?
```c
static inline unsigned long page_to_pfn(const struct page *page)
{
    return (unsigned long)(page - mem_map);
}
```
- Byte distance = `0x8020E100 - 0x80208000 = 0x6100` (`24,832` bytes).
- Because both pointers are `struct page *` (32 bytes each), C pointer subtraction automatically divides by 32:
  `24,832 / 32 = 776` (`0x308`)!
- So `0x8020E100` is **Card #776 (`PFN = 776`)**!

#### Math 2: Card Pointer -> 4096-Byte Physical RAM Address (`page_to_phys`)
Where is the actual 4096-byte RAM page for Card #776?
Each page is 4096 bytes (`<< 12`), starting from `PHYS_RAM_BASE` (`0x80000000`):
```c
static inline uintptr_t page_to_phys(const struct page *page)
{
    return PHYS_RAM_BASE + (page_to_pfn(page) << PAGE_SHIFT);
}
```
- `0x80000000 + (776 << 12)` = `0x80000000 + 0x00308000` = **`0x80308000`**!

#### Math 3: 4096-Byte Physical RAM Address -> Card Pointer (`phys_to_page`)
What if we have Physical RAM address `0x80308000` and want its Card back?
```c
static inline struct page *phys_to_page(uintptr_t phys)
{
    unsigned long pfn = (phys - PHYS_RAM_BASE) >> PAGE_SHIFT;
    return &mem_map[pfn];
}
```
- `pfn = (0x80308000 - 0x80000000) >> 12 = 776`.
- Returns **`&mem_map[776]`** (`0x8020E100`)!

---

### 4. Wait — Why Is `0x80309000 >> 12` (`0x80309`) ALSO Called a "PFN" in `pfn_pte()`?!

You spotted a brilliant contradiction that confuses almost everyone reading Linux kernel code:
> *"Wait! Card #777 has index `777` (`0x309`). When we call `page_to_phys(Card #777)`, it multiplies `777 * 4096` and adds `0x80000000` to get the physical page address `0x80309000`. Then in `pfn_pte(0x80309000 >> 12, PTE_V)`, we chop off the 12 bits to get `0x80309`! Isn't `0x80309` the actual physical page address without the 12 zero bits, NOT `777`? If yes, why is the function named `pfn_pte`?!"*

**Your observation is 100% eagle-eyed!**
Here is why this happens:

1. **What "PFN" (Page Frame Number) Literally Means in Hardware**:
   In computer architecture (and in the real Linux kernel), the **Hardware PFN (or PPN — Physical Page Number)** of any physical address `PA` is defined simply as:
   $$\text{Hardware PFN} = \frac{\text{Physical Address}}{4096} = \text{PA} \gg 12$$
   Why? Because the CPU starts counting physical 4KB frames from **Address `0x00000000`**!
   - Physical Address `0x00000000` is Hardware Frame #0 (`0x00000`).
   - Physical Address `0x00001000` is Hardware Frame #1 (`0x00001`).
   - Physical Address `0x80000000` (where our RAM chip starts!) is Hardware Frame **`0x80000`** (`524,288`)!
   - Physical Address `0x80309000` is Hardware Frame **`0x80309`** (`525,065`)!

2. **Why `mem_map[]` Used `0..32767` Instead of `0x80000..0x87FFF`**:
   Because our RAM chip starts at `0x80000000` (Hardware Frame `0x80000`), there is no RAM at `0x00000000`!
   If our `mem_map[]` cabinet started at Frame `0`, we would waste 524,288 empty cards ($16\text{ MB}$ of RAM!) for addresses `0x00000000` to `0x7FFFFFFF`!
   So in `mem_map[]`, we subtracted `PHYS_RAM_BASE` (`0x80000000`) so that **Card #0** represents Hardware Frame `0x80000`!

```text
+-----------------------------------+-----------------------------------+-----------------------------------+
| What We Are Looking At            | RAM-Relative Card Index           | Hardware PFN / PPN (PA >> 12)     |
|                                   | ((PA - 0x80000000) >> 12)         | (Counted from Address 0x00000000) |
+-----------------------------------+-----------------------------------+-----------------------------------+
| Start of RAM (0x80000000)         | Card #0   (0x000)                 | Hardware PFN = 0x80000 (524,288)  |
| Our Page Table (0x80309000)       | Card #777 (0x309)                 | Hardware PFN = 0x80309 (525,065)  |
| Relationship between the two:     | Card Index = HW_PFN - 0x80000     | HW_PFN = Card Index + 0x80000     |
+-----------------------------------+-----------------------------------+-----------------------------------+
```

3. **How Upstream Linux Naming Works (`ARCH_PFN_OFFSET`)**:
   In the real Linux kernel (`arch/riscv/include/asm/page.h`), Linux calls **`0x80309`** (`PA >> 12`) the **true `pfn`**!
   And when Linux indexes into `mem_map[]`, Linux subtracts `ARCH_PFN_OFFSET` (`0x80000`):
   - In Linux, **`pfn`** ALWAYS means `Physical_Address >> 12` (so for `0x80309000`, `pfn` is **`0x80309`**!).
   - That is why `pfn_pte(pfn, prot)` is named `pfn_pte`! It literally means:
     *"Take the **Hardware PFN** (`PA >> 12 = 0x80309`, which is the physical page address with its 12 zero bits removed) and pack it into a **PTE** (`(0x80309 << 10) | prot`)!"*
   - And notice: you are 100% right that `0x80309` is **the actual physical page starting address `0x80309000` with the 12 zero offset bits chopped off**!

```text
VISUAL SUMMARY OF WHY pfn_pte IS NAMED pfn_pte:

  Physical Page Starting Address :  0x80309000  (4096-byte aligned, so bottom 12 bits are 0x000)
                                         |
                                         |  Chop off the 12 zero bits (>> 12)
                                         v
  Hardware PFN (also called PPN) :  0x00080309  <-- Linux calls THIS "pfn"!
                                         |
                                         |  pfn_pte(0x80309, PTE_V) slides it << 10 and adds flags
                                         v
  64-Bit Page Table Entry (PTE)  :  0x200C2401  (Stored in swapper_pg_dir[2]!)
```

---

## PART 3: Sv39 Virtual Memory & The 3-Level Page Table

### 1. What is Virtual Memory? (The Hotel Key Card Analogy)

```text
  WHAT PROGRAMS THINK                   THE TRANSLATOR                  ACTUAL SILICON RAM CHIPS
  (Virtual Addresses)                   (Hardware MMU)                    (Physical Addresses)
+-----------------------+           +--------------------+           +----------------------------+
| Program 1 (Browser)   |           | Program 1 Map:     |           |                            |
| "Write to 0x1000"     | --------> | 0x1000 -> 0x80500000 --------> | Physical RAM at 0x80500000 |
+-----------------------+           +--------------------+           +----------------------------+

+-----------------------+           +--------------------+           +----------------------------+
| Program 2 (Calculator)|           | Program 2 Map:     |           |                            |
| "Write to 0x1000"     | --------> | 0x1000 -> 0x80900000 --------> | Physical RAM at 0x80900000 |
+-----------------------+           +--------------------+           +----------------------------+
```

- **Virtual Address (VA)**: The address a C program *thinks* it is using. Once the MMU is turned ON, the CPU instruction pipeline **only speaks Virtual Addresses**.
- **Physical Address (PA)**: The real hardware address on the silicon RAM chip.
- **MMU (Memory Management Unit)**: The hardware box inside the CPU that translates every Virtual Address into a Physical Address using a 3-level dictionary called the **Page Table**.

---

### 2. Why 3 Levels of Tables? (The `9 + 9 + 9 + 12 = 39` Rule)

1. **1 Page = 4096 bytes**. To pick 1 byte out of 4096, you need **12 bits** ($2^{12} = 4096$). When the MMU translates an address, **the bottom 12 bits (the Byte Offset) NEVER change!**
2. **1 Page Table = 1 Page (4096 bytes)**. Each row in the table (**PTE** — Page Table Entry) is **8 bytes**.
3. How many 8-byte rows fit on one 4096-byte page?
   `4096 / 8 = 512 rows` (numbered `0` to `511`).
4. How many bits do you need to pick a row from `0` to `511`?
   Since $2^9 = 512$, you need **9 bits**!

So a 39-bit Virtual Address is simply **three 9-bit row numbers** and **one 12-bit byte offset**:

```text
39-Bit Virtual Address:
 38         30 29         21 20         12 11                  0
+-------------+-------------+-------------+---------------------+
|   VPN[2]    |   VPN[1]    |   VPN[0]    |     Page Offset     |
|   (9 bits)  |   (9 bits)  |   (9 bits)  |      (12 bits)      |
+-------------+-------------+-------------+---------------------+
  Pick row in   Pick row in   Pick row in   Pick byte (0..4095)
  Level-2 PGD   Level-1 PMD   Level-0 PTE   inside the 4KB page
   (0..511)      (0..511)      (0..511)

Addition check: 9 + 9 + 9 + 12 = 39 bits!
```

```text
ASCII DIAGRAM OF THE 3-LEVEL HARDWARE PAGE WALK (VA = 0x80201040):

[ satp Register ] ---> Points to Level-2 Root Table in Physical RAM
                             |
                             v
                    +----------------------+
                    | Level-2 Table (PGD)  |
                    |  Row VPN[2]: --------+---> Points to Level-1 Table at 0x80308000
                    +----------------------+
                                                       |
                                                       v
                                                +----------------------+
                                                | Level-1 Table (PMD)  |
                                                |  Row VPN[1]: --------+---> Points to Level-0 Table at 0x80309000
                                                +----------------------+
                                                                               |
                                                                               v
                                                                        +----------------------+
                                                                        | Level-0 Table (PTE)  |
                                                                        |  Row VPN[0]: --------+---> Physical Page 0x80201000
                                                                        +----------------------+             |
                                                                                                             | + Offset 0x040
                                                                                                             v
                                                                                                    Final RAM: 0x80201040!
```

---

### 3. Explaining the Shift Math Inside a Page Table Entry (`>> 12` and `<< 10`)

Why can't we just store raw physical address `0x80308000` inside a table row?
Because the hardware MMU expects each 8-byte table row (**PTE**) to look like this box:

```text
One 64-Bit Page Table Entry (PTE) Box:
 63      54 53                                          10 9   8 7 6 5 4 3 2 1 0
+----------+----------------------------------------------+-----+-+-+-+-+-+-+-+-+
| Reserved |          Physical Page Number (PPN)          | RSW |D|A|G|U|X|W|R|V|
| (10 bits)|                   (44 bits)                  | (2) | | | | | | | | |
+----------+----------------------------------------------+-----+-+-+-+-+-+-+-+-+
                                                           \___________________/
                                                             10 bits of Flags!
```
Look at Bits `0` to `9` on the right:
The bottom **10 bits** are taken by the permission switches (`V`=Valid, `R`=Read, `W`=Write, `X`=Execute, `U`=User, `G`, `A`, `D`, `RSW`).
- **Branch vs. Leaf Rule**:
  - If `V = 1` and `R = 0, W = 0, X = 0`: The MMU knows this row is a **Branch** (pointing to the next child page table).
  - If `V = 1` and any of `R, W, X` is `1`: The MMU knows this row is a **Leaf** (pointing to the final 4096-byte data page).

#### A. Packing a Physical Address INTO a Table Row (`pfn_pte`):
Suppose our child table is at Physical Address `pmd_phys = 0x80308000`:
1. **Chop off the 12-bit byte offset (`>> 12`)**:
   `pfn = 0x80308000 >> 12` = `0x80308`.
2. **Slide left by 10 bits (`<< 10`) to make room for the 10 flags**:
   `0x80308 << 10` = `0x200C2000`.
3. **Turn ON the Valid switch (`| PTE_V`)**:
   `0x200C2000 | 1` = `0x200C2001`.

In Linux (`pgtable.h`), we wrap this in `pfn_pte(pfn, prot)`:
```c
static inline pte_t pfn_pte(unsigned long pfn, uint64_t prot)
{
    return (pfn << 10) | prot;
}
```

#### B. Unpacking a Physical Address FROM a Table Row (`pte_to_phys`):
When we read `pte = 0x200C2001` from a table row and want the physical address `0x80308000` back:
1. **Throw away the 10 flag bits (`>> 10`)**:
   `0x200C2001 >> 10` = `0x80308` (the clean Page Number!).
2. **Multiply by 4096 (`<< 12`) to put the 12 zero bits back**:
   `0x80308 << 12` = `0x80308000`!

In Linux (`pgtable.h`), we wrap this in `pte_to_phys(pte)`:
```c
static inline unsigned long pte_pfn(pte_t pte)
{
    return (pte >> 10) & 0x003fffffffffffffUL;
}

static inline uintptr_t pte_to_phys(pte_t pte)
{
    return pte_pfn(pte) << PAGE_SHIFT;
}
```

---

### 4. How Physical <-> Virtual Conversion Works (`__va`, `__pa`, and `walk_va_to_pa`)

There are **two** kinds of Virtual Addresses in an operating system:

```text
+----------------------------------+--------------------------------------+--------------------------------------+
| Question                         | 1. Kernel Linear Map (RAM)           | 2. Dynamic Map (User / vmalloc / IO) |
+----------------------------------+--------------------------------------+--------------------------------------+
| Relationship between VA and PA   | VA = PA + VA_PA_OFFSET (Fixed rule!) | Mapped via map_page(pgd, va, pa, ..) |
|                                  | (In my-linux Identity Map, offset=0) | (In High-Half Linux, offset=0xFF..)  |
| How to convert PA -> VA          | __va(pa)  (Add VA_PA_OFFSET)         | Pick a free VA and call map_page()   |
| How to convert VA -> PA          | __pa(va)  (Subtract VA_PA_OFFSET)    | walk_va_to_pa(pgd, va) (Walk 3 tabs) |
| Speed                            | 1 CPU instruction                    | 3 memory reads (PGD -> PMD -> PTE)   |
+----------------------------------+--------------------------------------+--------------------------------------+
```

---

### 5. Why Does the `satp` Register Contain `(8UL << 60) | pfn` Instead of the Full Address?

You asked a super sharp question:
> *"Why does `satp` contain `(8UL << 60) | pfn`? Why not just store `pfn` directly, or why not store the full PGD physical address (`pfn << 12`)?"*

There are **two parts** to this answer:
1. Why is `(8UL << 60)` at the top of `satp`?
2. Why does `satp` store `pfn` (`root_phys >> 12`) instead of the full address `root_phys`, and who multiplies it by 4096?

#### Part A: Look Inside the 64-Bit `satp` Hardware Register Box!
The `satp` (Supervisor Address Translation and Protection) register is not just an address box—it is a **3-in-1 control panel**!
The RISC-V CPU hardware designers divided its 64 bits into **3 compartments**:

```text
The 64-Bit satp CSR Hardware Box:
 63      60 59                   44 43                                         0
+----------+-----------------------+--------------------------------------------+
|   MODE   |         ASID          |           Root Page Table PPN              |
| (4 bits) |       (16 bits)       |                 (44 bits)                  |
+----------+-----------------------+--------------------------------------------+
     ^                 ^                                 ^
     |                 |                                 |
     |                 |                                 +-- Which 4KB page holds swapper_pg_dir?
     |                 +------------------------------------ Address Space ID (0 for kernel)
     +------------------------------------------------------ What kind of MMU mode is ON?
```

Look at Compartment 1 (**`MODE`**, Bits `63..60`, the top 4 bits):
A 64-bit RISC-V CPU can run in several different paging modes! How do you tell the CPU *which* paging mode to turn on? By putting a code number into Bits `63..60`:

```text
+-------------------+-----------------------------------------------------------+
| MODE (Bits 63..60)| Meaning to the RISC-V CPU Hardware                        |
+-------------------+-----------------------------------------------------------+
|  0  (0000)        | BARE Mode: MMU is TURNED OFF! (VA == PA, no page tables!) |
|  8  (1000)        | Sv39 Mode: Turn ON 3-level page tables (39-bit VA)!       |
|  9  (1001)        | Sv48 Mode: Turn ON 4-level page tables (48-bit VA)!       |
| 10  (1010)        | Sv57 Mode: Turn ON 5-level page tables (57-bit VA)!       |
+-------------------+-----------------------------------------------------------+
```

**Aha! Look what happens if you forget `(8UL << 60)` and just write `csrw satp, pfn`:**
- If you only write `pfn` into `satp`, then Bits `63..60` (`MODE`) are **`0`**!
- And what does `MODE = 0` mean to the CPU? **"Keep the MMU TURNED OFF (Bare Mode)!"**
- Putting **`8UL << 60`** into the top 4 bits is literally the **ON SWITCH** that tells the CPU: *"Turn ON the 3-level Sv39 MMU right now!"*

---

#### Part B: Why Store `pfn` Instead of the Full Physical Address? (And Who Multiplies by 4096?)

Suppose our Root Page Table (`swapper_pg_dir`) is at physical address `0x80308000`.
Why didn't the RISC-V hardware designers store the full address `0x80308000` in `satp`?

1. **Because every Page Table MUST start on a 4096-byte boundary, the bottom 12 bits of its address are ALWAYS `0000 0000 0000` (`0x000`)!**
2. Storing twelve zeros inside a precious 64-bit CPU register would **waste 12 bits of silicon** for nothing!
3. By throwing away those 12 guaranteed zeros (`root_phys >> 12`), the hardware designers freed up space inside the 64-bit `satp` register to fit the **4-bit `MODE`** and the **16-bit `ASID`**!
4. **Who multiplies `pfn` by 4096 (`<< 12`) to get the PGD address back?**
   **THE CPU HARDWARE MMU DOES IT AUTOMATICALLY!**
   Every time your C code touches a Virtual Address, the hardware MMU reads `satp`, grabs the bottom 44 bits (`PPN = 0x80308`), **automatically shifts it left by 12 bits (`0x80308 << 12 = 0x80308000`) in hardware wires**, and goes straight to `0x80308000` in RAM!

```text
SUMMARY OF satp:
  1. Software (enable_mmu):   Chops off 12 zero bits (root_phys >> 12) -> puts PFN in bits 43..0,
                              and puts Mode 8 (8UL << 60) in bits 63..60 to switch ON Sv39!
  2. Hardware (MMU Circuit):  Sees Mode 8 is ON -> grabs PFN from bits 43..0 -> automatically
                              shifts left by 12 bits (<< 12) to find swapper_pg_dir at 0x80308000!
```

---

## PART 4: Bottom-Up & Top-Down Lifecycle of Global Variable `int a = 42;`

Let us trace a single global variable `int a = 42;` from the moment you type it in **`my-linux/init/main.c`** all the way down to physical silicon RAM chips, and then back up again!

---

### 1. Bottom-Up: How `int a = 42;` Gets Its Address (From C Code to Silicon RAM)

Suppose you write this in **`my-linux/init/main.c`**:
```c
int a = 42;
```
Where does the pointer address `&a` come from? Watch the 4 stages of its birth:

```text
STAGE 1: The Compiler (gcc -c init/main.c -o init/main.o)
+-------------------------------------------------------------------------+
| GCC compiles main.c by itself. GCC has NO IDEA where the final kernel   |
| will live in RAM!                                                       |
| So GCC places 'int a = 42' in the .data section of main.o at a          |
| temporary RELATIVE OFFSET (say, Offset +0x028 from the start of .data): |
|                                                                         |
|   Inside main.o (.data section):                                        |
|     Offset +0x028:  int a = 42;                                         |
|                                                                         |
| Any machine instruction in main.o that touches 'a' is left with an      |
| empty blank slot (called a RELOCATION RECORD) that says:                |
|   "Dear Linker: Please fill in the final address of 'a' later!"         |
+-------------------------------------------------------------------------+
                                    |
                                    v
STAGE 2: The Linker (ld -T arch/riscv/kernel/vmlinux.lds -o vmlinux)
+-------------------------------------------------------------------------+
| The Linker reads vmlinux.lds.S:                                         |
|   - Kernel starts at:           . = 0x80200000                          |
|   - Suppose .data starts at:    _sdata = 0x80204000                     |
|                                                                         |
| The Linker adds the base of .data (0x80204000) + Offset of 'a' (+0x028):|
|   Final Address of 'a' (&a) = 0x80204028!                               |
|                                                                         |
| The Linker patches every blank Relocation Record in the code with       |
| 0x80204028 and writes the final ELF executable 'vmlinux'!               |
+-------------------------------------------------------------------------+
                                    |
                                    v
STAGE 3: Bootloader / OpenSBI (Loads vmlinux into Physical DRAM)
+-------------------------------------------------------------------------+
| OpenSBI loads the vmlinux binary into physical RAM starting at          |
| physical address 0x80200000.                                            |
| Now, physical RAM chip cell 0x80204028 physically holds the number 42!  |
+-------------------------------------------------------------------------+
                                    |
                                    v
STAGE 4: Kernel vm_init() (Registers &a in the 3-Level MMU Page Table)
+-------------------------------------------------------------------------+
| Before turning on the MMU, vm_init() maps the .data section into        |
| swapper_pg_dir so Virtual Page 0x80204000 -> Physical Page 0x80204000,  |
| and turns ON the MMU via 'csrw satp'!                                   |
+-------------------------------------------------------------------------+
```

#### Crucial Question: Is the Address Stamped by the Linker (`&a = 0x80204028`) a Virtual Address or a Physical Address?
**It is ALWAYS a Virtual Address!**
Once the MMU is turned ON, the CPU core's instruction pipeline (`ld`, `sd`, `auipc`, `jalr`) **only speaks Virtual Addresses**. Every pointer in your C program (`&a`), every function pointer (`&start_kernel`), and the stack pointer (`sp`) is a Virtual Address.

---

### 2. Complete `PA -> VA` and `VA -> PA` Conversion (Identity Map vs. High-Half Kernel)

How does a Virtual Address (`VA`) convert to a Physical Address (`PA`), and how does a Physical Address (`PA`) convert back to a Virtual Address (`VA`)?

In Linux, all memory mappings fall into **two categories**:

---

#### Category A: The Kernel Linear / Direct Map (Fixed Constant Offset: `__va` and `__pa`)

For all of the kernel's own code, global variables (like `int a = 42`), stacks, and physical pages in `mem_map[]`, the Virtual Address and Physical Address are separated by a **single fixed constant** called **`VA_PA_OFFSET`**.

There are two ways to set `VA_PA_OFFSET`:

```text
SETUP 1: Identity-Mapped Kernel (What my-linux uses!)
  - Linker starts kernel at VA :  0x80200000
  - Physical RAM starts at PA  :  0x80200000
  - VA_PA_OFFSET               :  0x80200000 - 0x80200000 = 0!
  - Address of 'a' (&a)        :  VA = 0x80204028  |  PA = 0x80204028

SETUP 2: High-Half Kernel (What production Linux uses!)
  - Linker starts kernel at VA :  0xFFFFFFC080200000  (High Virtual Memory!)
  - Physical RAM starts at PA  :  0x0000000080200000  (Real DRAM Chip!)
  - VA_PA_OFFSET               :  0xFFFFFFC000000000  (Fixed Constant!)
  - Address of 'a' (&a)        :  VA = 0xFFFFFFC080204028  |  PA = 0x0000000080204028
```

##### Why Does Production Linux Use Setup 2 (High-Half Kernel)?
In Sv39, the 39-bit Virtual Address space is divided into two 256 GB halves:
- **Low Half (`0x0000000000000000` to `0x0000003FFFFFFFFF`)**: Reserved for **User Space** programs (`sh`, `ls`, browsers) starting near `0x10000`.
- **High Half (`0xFFFFFFC000000000` to `0xFFFFFFFFFFFFFFFF`)**: Reserved for the **OS Kernel**.
If the kernel stayed at `0x80200000` (at the 2 GB mark in the Low Half), a user program allocating more than 2 GB of RAM would collide straight into the kernel! Moving the kernel to `0xFFFFFFC080200000` leaves the entire Low Half free for user programs.

##### How Upstream Linux Bridges Setup 1 and Setup 2: The Dynamic `va_pa_offset` Variable!
Look at `linux/arch/riscv/mm/init.c` in upstream Linux: instead of hardcoding `va_pa_offset` as a fixed `#define`, Linux declares `va_pa_offset` as a **runtime variable** (`unsigned long va_pa_offset = 0UL;`):

```text
+-------------------------------------------------------------------------------+
| STAGE 1: Early Boot (While MMU is OFF, satp = 0)                              |
|   - va_pa_offset = 0UL                                                        |
|   - __va(pa) returns (pa + 0) = pa (Physical Address directly!)               |
|   - Why? Because while satp = 0, the CPU can ONLY read/write Physical RAM     |
|     addresses (0x80000000..0x88000000). If __va() returned 0xFFFFFFC08xxxxxxx |
|     while satp = 0, map_page() would crash immediately!                       |
|   - During vm_init(), we map RAM TWICE in swapper_pg_dir:                     |
|       1. Low-Half Identity Map (Rows 0..255):                                 |
|          VA 0x0000000080200000 -> PA 0x80200000 (So current PC doesn't fault) |
|       2. High-Half Linear Map (Rows 256..511):                                |
|          VA 0xFFFFFFC080000000 -> PA 0x80000000 (All 128 MB of RAM!)          |
+-------------------------------------------------------------------------------+
                                       |
                                       v
+-------------------------------------------------------------------------------+
| STAGE 2: Inside enable_mmu() (The Instant satp Turns ON!)                     |
|   - CPU writes satp and executes sfence.vma.                                  |
|   - Next line:  va_pa_offset = PAGE_OFFSET; (0xFFFFFFC000000000UL!)           |
|   - From this moment onward:                                                  |
|       * __va(0x80309000) returns High-Half VA 0xFFFFFFC080309000!             |
|       * __pa(0xFFFFFFC080309000) returns Physical PA 0x80309000!              |
|       * map_page() and walk_va_to_pa() automatically use High-Half VAs!       |
+-------------------------------------------------------------------------------+
```

```c
/* Target File: my-linux/arch/riscv/include/asm/pgtable.h */

#define PAGE_OFFSET         0xFFFFFFC000000000UL
extern unsigned long va_pa_offset;

/* Convert Kernel Virtual Address -> Physical Address (Subtract va_pa_offset) */
#define __pa(va)            ((uintptr_t)(va) - va_pa_offset)

/* Convert Physical Address -> Kernel Virtual Address (Add va_pa_offset) */
#define __va(pa)            ((void *)((uintptr_t)(pa) + va_pa_offset))
```

---

#### Category B: Dynamic / Arbitrary Mappings (NO Fixed Offset — Must Walk Page Table!)

What if a Virtual Address was dynamically mapped to a random physical page from `alloc_page()`, or to a hardware device register?
Examples:
- Our test mapping: `VA 0x40000000 -> PA 0x8034B000`.
- Linux `vmalloc()`, `ioremap()`, or User Space process memory (`VA 0x10000 -> PA 0x80512000`).

Here, **`VA - PA` is NOT a fixed constant!** Calling `__pa(0x40000000)` would do blind subtraction and give a completely wrong answer!
To convert a dynamically mapped `VA` to `PA`, you **must** walk the 3-level page table using **`walk_va_to_pa(swapper_pg_dir, va)`**!

```text
+-----------------------------------+---------------------------------------+---------------------------------------+
| Operation                         | Category A: Kernel Linear Map         | Category B: Dynamic / User Map        |
+-----------------------------------+---------------------------------------+---------------------------------------+
| Example                           | Global 'int a = 42;' at 0x80204028    | Custom VA 0x40000028 -> PA 0x8034B028 |
| Convert PA -> VA                  | __va(pa)  (Add VA_PA_OFFSET)          | Pick a free VA & call map_page()      |
| Convert VA -> PA (in Hardware)    | MMU walks 3-level table automatically | MMU walks 3-level table automatically |
| Convert VA -> PA (in Software C)  | __pa(va) OR walk_va_to_pa(pgd, va)    | MUST use walk_va_to_pa(pgd, va)!      |
+-----------------------------------+---------------------------------------+---------------------------------------+
```

---

### 3. Complete Round-Trip Trace of `int a = 42;` (Filling the 3-Level Table & Translating Back!)

Let us trace **one complete round trip** for `int a = 42;` at **`&a = 0x80204028`** using exact binary and hex numbers!

#### Step 1: Slice `&a` (`0x80204028`) into Its 4 Sv39 Pieces
Let's write `0x80204028` in binary (39 bits) and slice it into `9 + 9 + 9 + 12`:

```text
Virtual Address &a = 0x80204028:

  Bits 38..30 (9 bits)    Bits 29..21 (9 bits)    Bits 20..12 (9 bits)    Bits 11..0 (12 bits)
+-----------------------+-----------------------+-----------------------+-----------------------+
|      000000010        |      000000001        |      000000100        |     000000101000      |
+-----------------------+-----------------------+-----------------------+-----------------------+
  pgd_index = Row 2       pmd_index = Row 1       pte_index = Row 4       Byte Offset = 0x028
  (Each row = 1 GB)       (Each row = 2 MB)       (Each row = 4 KB)       (Byte 40 inside page)

Check the math:
  (2 * 1 GB)   = 2 GB   = 0x80000000
+ (1 * 2 MB)   = 2 MB   = 0x00200000
+ (4 * 4 KB)   = 16 KB  = 0x00004000
+ Byte Offset  = 40 B   = 0x00000028
-------------------------------------
  Total VA              = 0x80204028!
```

---

#### Step 2: How `map_page()` Fills the 3-Level Table Using `VA = 0x80204000`

During `vm_init()`, `map_range()` maps the 4KB page containing `a` (`va = 0x80204000`, `pa = 0x80204000`, `prot = PAGE_KERNEL_RW = 0xCF`):

```text
1. ROOT TABLE (Level 2: swapper_pg_dir at Physical 0x80308000):
   - Index is vpn2 = 2. We check swapper_pg_dir[2].
   - If empty:
     * Call alloc_page() -> returns Card #777 (0x8020E120).
     * Call page_to_phys(Card #777) -> returns Physical Page 0x80309000 (for Level-1 PMD).
     * Pack into Row 2: pfn_pte(0x80309000 >> 12, PTE_V) = (0x80309 << 10) | 1 = 0x200C2401.
     * Write swapper_pg_dir[2] = 0x200C2401.

2. MIDDLE TABLE (Level 1: pmd at Physical 0x80309000):
   - Unpack swapper_pg_dir[2]: pte_to_phys(0x200C2401) -> (0x200C2401 >> 10) << 12 = 0x80309000.
   - Convert to C pointer: pmd = (pte_t *)__va(0x80309000).
   - Index is vpn1 = 1. We check pmd[1].
   - If empty:
     * Call alloc_page() -> returns Card #778 (0x8020E140).
     * Call page_to_phys(Card #778) -> returns Physical Page 0x8030A000 (for Level-0 PTE table).
     * Pack into Row 1: pfn_pte(0x8030A000 >> 12, PTE_V) = (0x8030A << 10) | 1 = 0x200C2801.
     * Write pmd[1] = 0x200C2801.

3. LEAF TABLE (Level 0: pte_table at Physical 0x8030A000):
   - Unpack pmd[1]: pte_to_phys(0x200C2801) -> (0x200C2801 >> 10) << 12 = 0x8030A000.
   - Convert to C pointer: pte_table = (pte_t *)__va(0x8030A000).
   - Index is vpn0 = 4. We write the Leaf PTE at pte_table[4]:
     * Target Physical Page is pa = 0x80204000 -> PFN = 0x80204000 >> 12 = 0x80204.
     * Pack with PAGE_KERNEL_RW (0xCF): pfn_pte(0x80204, 0xCF) = (0x80204 << 10) | 0xCF = 0x200810CF.
     * Write pte_table[4] = 0x200810CF!
```

---

#### Step 3: How `walk_va_to_pa(swapper_pg_dir, (uintptr_t)&a)` Recovers the Physical Address Using Only `&a`!

Now suppose your C code only has the virtual pointer `&a = 0x80204028`, and calls `walk_va_to_pa(swapper_pg_dir, 0x80204028)`:

```text
Input: va = 0x80204028
  -> vpn2   = pgd_index(0x80204028) = 2
  -> vpn1   = pmd_index(0x80204028) = 1
  -> vpn0   = pte_index(0x80204028) = 4
  -> offset = 0x80204028 & 0xFFF    = 0x028

1. Walk Level-2 Root Table:
   - Read swapper_pg_dir[2] -> gets 0x200C2401.
   - Unpack: pte_to_phys(0x200C2401) -> (0x200C2401 >> 10) << 12 = 0x80309000 (Level-1 PMD).

2. Walk Level-1 Middle Table:
   - Read pmd[1] at 0x80309000 -> gets 0x200C2801.
   - Unpack: pte_to_phys(0x200C2801) -> (0x200C2801 >> 10) << 12 = 0x8030A000 (Level-0 PTE Table).

3. Walk Level-0 Leaf Table:
   - Read pte_table[4] at 0x8030A000 -> gets 0x200810CF.
   - Unpack Physical Page Base: pte_to_phys(0x200810CF) -> (0x200810CF >> 10) << 12 = 0x80204000!

4. Add the 12-Bit Byte Offset:
   - return 0x80204000 + 0x028 = 0x80204028!
```
And when your C code reads `int x = a;`, the **Hardware MMU** performs Steps 1, 2, 3, and 4 in silicon wires automatically and fetches the integer `42` from physical RAM `0x80204028`!

---

## PART 5: Five Essential MMU Concepts Every Kernel Engineer Must Know (Zero Gaps!)

To make sure your foundation has zero blind spots for interviews or real driver development, here are the 5 remaining hardware rules explained simply:

---

### 1. Why Does the TLB Cache Exist, and Why Must We Call `sfence.vma`?

Look at the 3-level walk above: to read `int x = a;` just once, the CPU had to read RAM **4 times** (1. Read PGD + 2. Read PMD + 3. Read PTE + 4. Read `a`).
If the CPU did 4 RAM reads for every single instruction, your computer would run **4 times slower**!

**The Hardware Solution: The TLB (Translation Lookaside Buffer)**
- Inside the MMU is a tiny, ultra-fast hardware dictionary cache called the **TLB** (it holds the last ~64 or ~512 translations).
- The first time you read `a` at `0x80204028`, the MMU does the 3-level walk in RAM and saves the shortcut **`VA Page 0x80204 -> PA Page 0x80204`** inside the TLB.
- The next 1,000,000 times you touch `a`, the MMU finds it in the TLB in **0 extra RAM reads** (1 clock cycle)!

**The Catch (`sfence.vma`)**:
The TLB does **not** automatically watch RAM! If your C code changes a page table entry in RAM (using `map_page`), the TLB might still hold the old translation!
Executing **`sfence.vma`** tells the CPU: *"Clear the TLB cache so the MMU reads our new page table entries from RAM!"*
- `sfence.vma zero, zero`: Flushes the **entire** TLB (used when turning on `satp`).
- `sfence.vma va, zero`: Flushes **only the single virtual address `va`** (much faster when mapping just one page!).

---

### 2. Huge Pages (2 MB Megapages & 1 GB Gigapages)

Remember our **Branch vs. Leaf Rule**:
- If `R = 0, W = 0, X = 0`, a table entry is a **Branch** (points to the next lower table).
- If any of `R, W, X` is `1`, a table entry is a **Leaf** (points to real RAM data!).

Wait—what happens if you set **`R = 1, W = 1` directly inside the Level-1 Middle Table (`pmd`)** instead of `0`?
- The hardware MMU sees `R = 1` at Level 1 and says: *"Oh! This is a Leaf! I don't need a Level-0 table at all!"*
- Because each row of Level 1 covers $512 \times 4\text{ KB} = \mathbf{2\text{ Megabytes}}$, that single entry maps a contiguous **2 MB Megapage**!
- And if you set `R = 1, W = 1` at **Level 2 (`pgd`)**, that single entry maps a **1 GB Gigapage**!
Production Linux uses 2 MB Megapages for the kernel linear map because mapping 128 MB of RAM takes only **64 rows in Level 1** (0 Level-0 tables needed!).

---

### 3. Why Did We Include `PTE_A` (Accessed) and `PTE_D` (Dirty) in `PAGE_KERNEL_RW`?

Look at our macro in `pgtable.h`:
```c
#define PAGE_KERNEL_RW      (PTE_V | PTE_R | PTE_W | PTE_A | PTE_D)
```
Why did we turn ON **`PTE_A` (Bit 6)** and **`PTE_D` (Bit 7)**?
- **`PTE_A` (Accessed)** means: *"This page has been read or executed."*
- **`PTE_D` (Dirty)** means: *"This page has been written to."*
- **The RISC-V Hardware Rule**: On many RISC-V processors (including QEMU when `svadu` is not enabled), if the CPU tries to read a page where `PTE_A == 0`, or write to a page where `PTE_D == 0`, **the MMU refuses and triggers a Page Fault Exception (`scause = 13` or `15`)** so the OS knows the page was touched!
- By pre-setting `PTE_A | PTE_D` on our kernel pages, the MMU never faults on normal kernel reads and writes!

---

### 4. Can the Kernel Freely Touch User Pages (`PTE_U`)? (The `sstatus.SUM` Security Lock)

People often assume: *"Supervisor Mode (S-mode) is the boss, so S-mode can always read/write User Mode (`PTE_U = 1`) pages, right?"*
**NO!**
- By default on RISC-V, if S-mode code tries to read or write a page that has **`PTE_U = 1`**, the MMU **triggers a Page Fault**!
- **Why?** Security! If a hacker tricks a buggy kernel pointer into pointing at user memory, the CPU blocks the kernel from accidentally reading or executing fake user data!
- When the kernel *intentionally* wants to copy data from a user buffer (inside `copy_from_user()` or `sys_write()`), the kernel temporarily sets **Bit 18 (`SUM` = Supervisor User Memory access)** inside the `sstatus` CSR, copies the bytes, and immediately clears `SUM` back to `0`!
- Furthermore, S-mode is **NEVER** allowed to execute instructions (`X = 1`) from a page with `PTE_U = 1`, ever!

---

### 5. Device Memory (`UART` / `PLIC`) vs. Normal RAM, and ARM64 Comparison

Why is mapping the UART at `0x10000000` different from mapping normal DRAM at `0x80200000`?
- **Normal DRAM (`0x80000000..0x88000000`)**: Uses the CPU's L1/L2 **Data Cache**. If you read `a` ten times, the CPU reads RAM once and serves the next 9 reads from the fast L1 cache.
- **Device MMIO (`0x10000000` UART)**: Must **NEVER be cached**! If the CPU cached the UART Line Status Register (`UART_LSR`), then your `while ((serial_in(UART_LSR) & 0x20) == 0)` loop would read the same stale cached byte forever and hang!
- On **RISC-V**, hardware Physical Memory Attributes (**PMA**) built into the chip (and the optional `Svpbmt` PTE bits `62..61`) automatically mark `0x10000000` as **Non-Cacheable, Strongly-Ordered I/O Memory**.
- On **ARM64**, there is no automatic PMA—you **must** set the Memory Attribute Indirection Register (**`MAIR_EL1`**) index in the PTE to **`MT_DEVICE_nGnRnE`** for UART/SPI/I2C registers, and **`MT_NORMAL`** for DRAM!

```text
+---------------------------+---------------------------+-------------------------------+
| Concept                   | RISC-V 64 (Sv39)          | ARM64 (AArch64 - 4KB Pages)   |
+---------------------------+---------------------------+-------------------------------+
| Page Size                 | 4096 bytes (12-bit offset)| 4096 bytes (12-bit offset)    |
| Entries per Table         | 512 entries (9 bits/level)| 512 entries (9 bits/level)    |
| Address Split             | 9 + 9 + 9 + 12 = 39 bits  | 9 + 9 + 9 + 12 = 39 bits      |
| Root Table Register (User)| satp                      | TTBR0_EL1 (Low Half User)     |
| Root Table Register (Kern)| satp                      | TTBR1_EL1 (High Half Kernel)  |
| Turn On MMU Register      | satp (MODE = 8)           | SCTLR_EL1 (Bit 0: M = 1)      |
| Flush TLB Instruction     | sfence.vma                | tlbi vmalle1is; dsb ish; isb  |
| Faulting Address Register | stval                     | FAR_EL1 (Fault Address Reg)   |
| Fault Reason Register     | scause                    | ESR_EL1 (Exception Syndrome)  |
| Physical Address in PTE   | Bits 53..10 (PFN << 10)   | Bits 47..12 (PA stays at 12!) |
| User Memory Lock in Kernel| sstatus.SUM               | PSTATE.PAN (Privileged Access)|
| Device vs RAM Caching     | Hardware PMA / Svpbmt     | MAIR_EL1 (Device vs Normal)   |
+---------------------------+---------------------------+-------------------------------+
```

---

## PART 6: Complete Dictionary & Full Round-Trip Trace of ALL Memory Functions

Here is every single memory management function in `my-linux`, explained in plain English, followed by one complete end-to-end round trip!

### 1. Plain-English Dictionary of All 14 Functions

| # | Function / Macro | File Location | Plain-English Explanation |
| :--- | :--- | :--- | :--- |
| **1** | `mm_init(start, end)` | `mm/page_alloc.c` | Builds the `mem_map[]` cabinet of 32-byte index cards at `_end` and clips all free cards (from Card #776 to #32767) onto `free_page_list`. |
| **2** | `alloc_page()` | `mm/page_alloc.c` | Unclips the first free 32-byte **Index Card** (`struct page *`) from `free_page_list`, zeroes its corresponding 4096-byte RAM page, sets `refcount = 1`, and returns the **Card pointer** (e.g., `0x8020E100`). |
| **3** | `free_page(card)` | `mm/page_alloc.c` | Sets `card->refcount = 0` and clips the 32-byte **Index Card** right back onto the front of `free_page_list` so it can be reused. |
| **4** | `page_to_pfn(card)` | `include/linux/mm.h` | Takes a **Card pointer** (`&mem_map[776]`) and subtracts `mem_map` to return the **Card Number / Page Frame Number** (`776`). |
| **5** | `pfn_to_page(pfn)` | `include/linux/mm.h` | Takes a **Page Frame Number** (`776`) and returns the **Card pointer** (`&mem_map[776]`). |
| **6** | `page_to_phys(card)` | `include/linux/mm.h` | Takes a **Card pointer** (`&mem_map[776]`) and calculates the **4096-byte Physical RAM Address** (`0x80000000 + (776 << 12) = 0x80308000`). |
| **7** | `phys_to_page(pa)` | `include/linux/mm.h` | Takes a **4096-byte Physical RAM Address** (`0x80308000`) and returns its **Card pointer** (`&mem_map[776]`). |
| **8** | `page_address(card)` | `include/linux/mm.h` | Takes a **Card pointer** and returns the **Kernel Virtual Pointer (`void *`)** to the 4096-byte RAM page (`__va(page_to_phys(card))`). |
| **9** | `__va(pa)` / `__pa(va)` | `asm/pgtable.h` | Converts between Physical Address and Kernel Linear Virtual Address by adding/subtracting `VA_PA_OFFSET` (`0` in our identity map). |
| **10** | `pfn_pte(pfn, prot)` | `asm/pgtable.h` | Packs a Page Number (`pfn`) and permission flags (`prot`) into an 8-byte Page Table Entry (`(pfn << 10) \| prot`). |
| **11** | `pte_to_phys(pte)` | `asm/pgtable.h` | Unpacks an 8-byte Page Table Entry back into a 4096-byte Physical RAM Address (`((pte >> 10) << 12)`). |
| **12** | `map_page(pgd, va, pa, prot)` | `arch/riscv/mm/init.c` | Connects Virtual Address `va` to Physical Address `pa` inside the 3-level page table, calling `alloc_page()` to create Level-1 or Level-0 tables if needed. |
| **13** | `walk_va_to_pa(pgd, va)` | `arch/riscv/mm/init.c` | Walks the 3-level page table in C for any Virtual Address `va` and returns the exact Physical RAM Address `pa` it is mapped to. |
| **14** | `vm_init()` & `enable_mmu()` | `arch/riscv/mm/init.c` | Allocates the master root table (`swapper_pg_dir`), identity-maps the UART and kernel RAM, writes `satp` (Mode 8 = Sv39), and flushes the TLB (`sfence.vma`). |

---

### 2. The Complete Round-Trip Walkthrough (From Boot to Page Walk to Free!)

Watch how all 14 functions work together in one continuous story with exact numbers:

```text
================================================================================
ACT 1: INITIALIZING THE MEMORY POOL (mm_init)
================================================================================
1. Kernel ends at _end = 0x80208000. RAM ends at 0x88000000 (128 MB = 32,768 pages).
2. mm_init() places mem_map[32768] at 0x80208000 (occupying 1 MB up to 0x80308000).
3. First usable 4KB page is at 0x80308000 -> PFN = (0x80308000 - 0x80000000) >> 12 = 776.
4. Cards #776 to #32767 are linked onto free_page_list.
   Front of free_page_list = Card #776 (&mem_map[776] at 0x8020E100).

================================================================================
ACT 2: TURNING ON VIRTUAL MEMORY (vm_init -> alloc_page -> map_page -> enable_mmu)
================================================================================
5. vm_init() calls alloc_page() to get a 4KB page for the Root Table (swapper_pg_dir):
   - alloc_page() pops Card #776 (0x8020E100).
   - page_address(Card #776) -> page_to_phys(Card #776) -> returns 0x80308000!
   - swapper_pg_dir now points to the 4KB physical page at 0x80308000.
6. vm_init() identity-maps UART (0x10000000) and Kernel RAM (0x80200000..0x88000000)
   using map_page() (which consumes Cards #777..#842 for intermediate PMD/PTE tables).
7. enable_mmu(swapper_pg_dir) computes:
   - root_pfn = __pa(0x80308000) >> 12 = 0x80308.
   - satp_val = (8UL << 60) | 0x80308.
   - Executes 'csrw satp, satp_val' and 'sfence.vma'.
   --> THE HARDWARE MMU IS NOW LIVE!

================================================================================
ACT 3: DYNAMIC VIRTUAL MAPPING & SOFTWARE PAGE WALK (alloc_page -> map_page -> walk_va_to_pa)
================================================================================
8. In main.c, we want to map a brand-new Virtual Address (test_va = 0x40000028)
   to a fresh physical page:
   - struct page *test_card = alloc_page();
     --> Pops Card #843 (at 0x8020E960).
   - uintptr_t test_pa = page_to_phys(test_card);
     --> Card #843 corresponds to 4KB Physical Page 0x8034B000!
9. We call map_page(swapper_pg_dir, 0x40000000, 0x8034B000, PAGE_KERNEL_RW):
   - Slices VA 0x40000000: VPN[2] = 1, VPN[1] = 0, VPN[0] = 0.
   - swapper_pg_dir[1] is empty! Calls alloc_page() -> gets Card #844 (PA 0x8034C000) for PMD.
     Packs into swapper_pg_dir[1] = pfn_pte(0x8034C, PTE_V) = 0x200D3001.
   - pmd[0] is empty! Calls alloc_page() -> gets Card #845 (PA 0x8034D000) for PTE table.
     Packs into pmd[0] = pfn_pte(0x8034D, PTE_V) = 0x200D3401.
   - Installs Leaf PTE at pte_table[0]:
     pte_table[0] = pfn_pte(0x8034B, PAGE_KERNEL_RW) = 0x200D2CCF.
10. We write 0x123456789ABCDEF0 to Virtual Address *(uint64_t *)0x40000028:
    - Hardware MMU walks swapper_pg_dir[1] -> pmd[0] -> pte_table[0] -> PA 0x8034B000,
      adds offset 0x028, and writes the 8 bytes into Physical RAM 0x8034B028!
11. We call walk_va_to_pa(swapper_pg_dir, 0x40000028) to verify in software:
    - Reads swapper_pg_dir[1] -> unpacks PMD at 0x8034C000.
    - Reads pmd[0]            -> unpacks PTE table at 0x8034D000.
    - Reads pte_table[0]      -> unpacks Physical Page 0x8034B000.
    - Adds offset 0x028       -> returns 0x8034B028! (Exact match!)

================================================================================
ACT 4: RETURNING THE PAGE TO THE FREE POOL (phys_to_page -> free_page)
================================================================================
12. When done with physical page 0x8034B000:
    - struct page *card = phys_to_page(0x8034B000);
      --> Calculates PFN = (0x8034B000 - 0x80000000) >> 12 = 843 -> returns &mem_map[843].
    - free_page(card);
      --> Resets refcount = 0 and clips Card #843 back onto the front of free_page_list!
```

---

## PART 7: Implementation Files for `my-linux`

### File 1: `my-linux/arch/riscv/include/asm/pgtable.h`

```c
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_RISCV_PGTABLE_H
#define _ASM_RISCV_PGTABLE_H

#include <linux/types.h>
#include <linux/mm.h>

typedef uint64_t pte_t;
typedef uint64_t *pgd_t;

/*
 * Sv39 Virtual Address Level Shifts:
 * Bits 11..0  (12 bits) = Page Offset (PAGE_SHIFT = 12)
 * Bits 20..12 (9 bits)  = Level-0 PTE Index
 * Bits 29..21 (9 bits)  = Level-1 PMD Index (PMD_SHIFT = 21)
 * Bits 38..30 (9 bits)  = Level-2 PGD Index (PGDIR_SHIFT = 30)
 */
#define PMD_SHIFT           21
#define PGDIR_SHIFT         30

/* 4096 bytes / 8 bytes per entry = 512 entries per table */
#define PTRS_PER_PTE        512
#define PTRS_PER_PMD        512
#define PTRS_PER_PGD        512

/*
 * Hardware PTE Permission & Status Flags (Bits 0..7)
 */
#define PTE_V               (1UL << 0)  /* Valid bit: 1 = entry is active */
#define PTE_R               (1UL << 1)  /* Readable */
#define PTE_W               (1UL << 2)  /* Writable */
#define PTE_X               (1UL << 3)  /* Executable (instructions) */
#define PTE_U               (1UL << 4)  /* User mode allowed */
#define PTE_G               (1UL << 5)  /* Global mapping */
#define PTE_A               (1UL << 6)  /* Accessed (set by 1) */
#define PTE_D               (1UL << 7)  /* Dirty / written (set to 1) */

/* Standard Linux Kernel Page Protection Combinations */
#define PAGE_KERNEL_RO      (PTE_V | PTE_R | PTE_A | PTE_D)
#define PAGE_KERNEL_RW      (PTE_V | PTE_R | PTE_W | PTE_A | PTE_D)
#define PAGE_KERNEL_EXEC    (PTE_V | PTE_R | PTE_X | PTE_A | PTE_D)
#define PAGE_KERNEL_RWX     (PTE_V | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D)

/*
 * Extract the 9-bit table row index (0..511) for each level from a Virtual Address
 */
#define pgd_index(va)       (((va) >> PGDIR_SHIFT) & (PTRS_PER_PGD - 1))
#define pmd_index(va)       (((va) >> PMD_SHIFT)   & (PTRS_PER_PMD - 1))
#define pte_index(va)       (((va) >> PAGE_SHIFT)  & (PTRS_PER_PTE - 1))

/*
 * Pack a Page Frame Number (PFN) and permission flags into a 64-bit PTE:
 * Slides PFN left by 10 bits to leave room for the 10 flag bits (0..9).
 */
static inline pte_t pfn_pte(unsigned long pfn, uint64_t prot)
{
    return (pfn << 10) | prot;
}

/*
 * Unpack the Page Frame Number (PFN) from a 64-bit PTE:
 * Shifts right by 10 bits to discard the 10 flag bits.
 */
static inline unsigned long pte_pfn(pte_t pte)
{
    return (pte >> 10) & 0x003fffffffffffffUL;
}

/*
 * Convert a PTE back into a 4KB-aligned Physical Address:
 * Extracts the PFN, then shifts left by 12 bits (multiplies by 4096).
 */
static inline uintptr_t pte_to_phys(pte_t pte)
{
    return pte_pfn(pte) << PAGE_SHIFT;
}

/*
 * Convert between Physical Address and Kernel Virtual Address (Upstream Linux style).
 * Before MMU is enabled: va_pa_offset == 0UL (Identity Map).
 * After MMU is enabled:  va_pa_offset == PAGE_OFFSET (0xFFFFFFC000000000UL High-Half Linear Map).
 */
#define PAGE_OFFSET         0xFFFFFFC000000000UL
extern unsigned long va_pa_offset;

#define __va(pa)            ((void *)((uintptr_t)(pa) + va_pa_offset))
#define __pa(va)            ((uintptr_t)(va) - va_pa_offset)

/* satp CSR Mode 8 selects Sv39 3-level translation */
#define SATP_MODE_SV39      (8UL << 60)

extern pgd_t swapper_pg_dir;

int map_page(pgd_t pgdir, uintptr_t va, uintptr_t pa, uint64_t prot);
uintptr_t walk_va_to_pa(pgd_t pgdir, uintptr_t va);
void vm_init(void);
void enable_mmu(pgd_t root_pgd);

#endif /* _ASM_RISCV_PGTABLE_H */
```

---

### File 2: `my-linux/arch/riscv/mm/init.c`

```c
/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/mm.h>
#include <asm/pgtable.h>

void early_puts(const char *s);

/* Linker script section boundary symbols from vmlinux.lds.S */
extern char _stext[], _etext[];
extern char _srodata[], _erodata[];
extern char _sdata[];

/* Master kernel root page table (Level 2 PGD) */
pgd_t swapper_pg_dir = NULL;

/*
 * Runtime VA-to-PA offset (matching upstream linux/arch/riscv/mm/init.c):
 * Starts at 0UL while satp = 0; switches to PAGE_OFFSET once satp is active!
 */
unsigned long va_pa_offset = 0UL;

/*
 * map_page - Map one 4KB Virtual Page (va) to a Physical Page (pa)
 */
int map_page(pgd_t pgdir, uintptr_t va, uintptr_t pa, uint64_t prot)
{
    /* Step 1: Extract the three 9-bit row numbers (0..511) */
    unsigned long vpn2 = pgd_index(va);
    unsigned long vpn1 = pmd_index(va);
    unsigned long vpn0 = pte_index(va);

    /* Step 2: Check Level-2 Root Table. If empty, allocate Level-1 PMD table */
    if (!(pgdir[vpn2] & PTE_V)) {
        struct page *pmd_page = alloc_page();
        if (!pmd_page) {
            return -1;
        }
        uintptr_t pmd_phys = page_to_phys(pmd_page);
        /* Branch entry: R=W=X=0, only PTE_V is set! */
        pgdir[vpn2] = pfn_pte(pmd_phys >> PAGE_SHIFT, PTE_V);
    }

    /* Unpack Level-1 table physical address and convert to C pointer */
    pte_t *pmd = (pte_t *)__va(pte_to_phys(pgdir[vpn2]));

    /* Step 3: Check Level-1 Middle Table. If empty, allocate Level-0 PTE table */
    if (!(pmd[vpn1] & PTE_V)) {
        struct page *pte_page = alloc_page();
        if (!pte_page) {
            return -1;
        }
        uintptr_t pte_phys = page_to_phys(pte_page);
        /* Branch entry: R=W=X=0, only PTE_V is set! */
        pmd[vpn1] = pfn_pte(pte_phys >> PAGE_SHIFT, PTE_V);
    }

    /* Unpack Level-0 table physical address and convert to C pointer */
    pte_t *pte_table = (pte_t *)__va(pte_to_phys(pmd[vpn1]));

    /* Step 4: Write the final Leaf entry at Level-0 with R/W/X permissions */
    pte_table[vpn0] = pfn_pte(pa >> PAGE_SHIFT, prot | PTE_V);

    return 0;
}

/*
 * walk_va_to_pa - Walk the 3-level Sv39 page table to translate any VA to PA
 */
uintptr_t walk_va_to_pa(pgd_t pgdir, uintptr_t va)
{
    unsigned long vpn2   = pgd_index(va);
    unsigned long vpn1   = pmd_index(va);
    unsigned long vpn0   = pte_index(va);
    unsigned long offset = va & ~PAGE_MASK;

    if (!(pgdir[vpn2] & PTE_V)) {
        return 0;
    }
    pte_t *pmd = (pte_t *)__va(pte_to_phys(pgdir[vpn2]));

    if (!(pmd[vpn1] & PTE_V)) {
        return 0;
    }
    pte_t *pte_table = (pte_t *)__va(pte_to_phys(pmd[vpn1]));

    if (!(pte_table[vpn0] & PTE_V)) {
        return 0;
    }

    uintptr_t page_phys_base = pte_to_phys(pte_table[vpn0]);
    return page_phys_base + offset;
}

/*
 * map_range - Helper to identity-map a contiguous range of physical memory (VA == PA)
 */
static void map_range(pgd_t pgdir, uintptr_t start, uintptr_t end, uint64_t prot)
{
    uintptr_t addr = start & PAGE_MASK;
    end = PAGE_ALIGN(end);

    while (addr < end) {
        map_page(pgdir, addr, addr, prot);
        addr += PAGE_SIZE;
    }
}

/*
 * create_linear_mapping_range - Map physical memory into the High-Half Linear Window
 * Maps VA = (pa + PAGE_OFFSET) -> PA = pa
 */
static void create_linear_mapping_range(pgd_t pgdir, uintptr_t start_pa, uintptr_t end_pa, uint64_t prot)
{
    uintptr_t pa = start_pa & PAGE_MASK;
    end_pa = PAGE_ALIGN(end_pa);

    while (pa < end_pa) {
        uintptr_t high_va = pa + PAGE_OFFSET;
        map_page(pgdir, high_va, pa, prot);
        pa += PAGE_SIZE;
    }
}

/*
 * enable_mmu - Load root page table into satp CSR, turn on Sv39, and switch to High-Half Linear Map
 */
void enable_mmu(pgd_t root_pgd)
{
    /* Step 1: While va_pa_offset is still 0, get physical address of root_pgd */
    uintptr_t root_phys = __pa(root_pgd);
    unsigned long root_pfn = root_phys >> PAGE_SHIFT;
    unsigned long satp_val = SATP_MODE_SV39 | root_pfn;

    /* Step 2: Turn on Sv39 hardware translation */
    __asm__ volatile("sfence.vma zero, zero" ::: "memory");
    __asm__ volatile("csrw satp, %0" : : "r"(satp_val) : "memory");
    __asm__ volatile("sfence.vma zero, zero" ::: "memory");

    /*
     * Step 3: Now that satp is active and High-Half Linear Mapping is live in Hardware,
     * switch va_pa_offset to PAGE_OFFSET (0xFFFFFFC000000000UL) and update swapper_pg_dir
     * to its High-Half virtual address!
     */
    va_pa_offset = PAGE_OFFSET;
    swapper_pg_dir = (pgd_t)__va(root_phys);

    early_puts("[MMU] Sv39 Virtual Memory & High-Half Linear Mapping Enabled!\n");
}

/*
 * vm_init - Build both Identity and High-Half Linear page tables and activate the MMU
 */
void vm_init(void)
{
    struct page *root_page = alloc_page();
    if (!root_page) {
        early_puts("[MMU PANIC] Failed to allocate swapper_pg_dir!\n");
        return;
    }
    swapper_pg_dir = (pgd_t)page_address(root_page);

    early_puts("[MMU] Building Sv39 Identity + High-Half Linear page tables...\n");

    /* 1. Low-Half Identity Map (Rows 0..255) so current PC/SP at 0x802xxxxx and UART work */
    map_page(swapper_pg_dir, 0x10000000UL, 0x10000000UL, PAGE_KERNEL_RW);
    map_range(swapper_pg_dir, (uintptr_t)_stext, (uintptr_t)_etext, PAGE_KERNEL_EXEC);
    map_range(swapper_pg_dir, (uintptr_t)_srodata, (uintptr_t)_erodata, PAGE_KERNEL_RO);
    map_range(swapper_pg_dir, (uintptr_t)_sdata, 0x88000000UL, PAGE_KERNEL_RW);

    /* 2. High-Half Kernel Linear Map (Rows 256..511): VA 0xFFFFFFC080000000 -> PA 0x80000000 */
    create_linear_mapping_range(swapper_pg_dir, (uintptr_t)_stext, (uintptr_t)_etext, PAGE_KERNEL_EXEC);
    create_linear_mapping_range(swapper_pg_dir, (uintptr_t)_srodata, (uintptr_t)_erodata, PAGE_KERNEL_RO);
    create_linear_mapping_range(swapper_pg_dir, (uintptr_t)_sdata, 0x88000000UL, PAGE_KERNEL_RW);

    /* 3. Activate satp and switch va_pa_offset to PAGE_OFFSET (0xFFFFFFC000000000UL)! */
    enable_mmu(swapper_pg_dir);
}
```

---

### File 3: `my-linux/arch/riscv/kernel/traps.c` (Handling Page Faults)

**What this change does:**
When the MMU is active, if any code tries to read, write, or execute an unmapped address (or violates permissions, like writing to Read-Only memory), the CPU traps to `do_trap()` with:
- `scause = 12`: Instruction Page Fault
- `scause = 13`: Load (Read) Page Fault
- `scause = 15`: Store (Write) Page Fault
And the CPU puts the exact bad Virtual Address into `regs->badvaddr` (`stval` CSR)!

```c
/* Target File: my-linux/arch/riscv/kernel/traps.c */
void early_print_hex(unsigned long val);
void early_print_dec(unsigned long val);

void do_trap(struct pt_regs *regs)
{
    unsigned long cause = regs->cause;
    unsigned long is_interrupt = cause & (1UL << 63);
    unsigned long code = cause & ~(1UL << 63);

    if (is_interrupt) {
        early_puts("[TRAP] Supervisor Interrupt received!\n");
    } else {
        if (code == 3) {
            early_puts("[TRAP] Supervisor Breakpoint (ebreak) trapped to stvec successfully!\n");
            (((*(volatile uint16_t *)regs->epc) & 3) == 3) ? (regs->epc += 4) : (regs->epc += 2);
        } else if (code == 9) {
            early_puts("[TRAP] Synchronous ecall handled successfully!\n");
            (((*(volatile uint16_t *)regs->epc) & 3) == 3) ? (regs->epc += 4) : (regs->epc += 2);
        } else if (code == 12 || code == 13 || code == 15) {
            early_puts("[TRAP] Page Fault Caught! scause = ");
            early_print_dec(code);
            early_puts(" | Faulting Virtual Address (stval) = ");
            early_print_hex(regs->badvaddr);
            early_puts("\n");
            /* Advance past the faulting instruction so a controlled test can continue */
            (((*(volatile uint16_t *)regs->epc) & 3) == 3) ? (regs->epc += 4) : (regs->epc += 2);
        } else {
            early_puts("[TRAP FATAL] Unhandled Exception! System halted.\n");
            for (;;) {
                __asm__ volatile("wfi");
            }
        }
    }
}
```

---

### File 4: `my-linux/Makefile` & `my-linux/init/main.c` (Complete End-to-End Verification)

1. In **`my-linux/Makefile`**, add `arch/riscv/mm/init.c` to `SRCS_C`:
   ```makefile
   SRCS_C  += arch/riscv/mm/init.c
   ```
2. In **`my-linux/init/main.c`**, define a global variable `int a = 42;`, include `<asm/pgtable.h>`, and after `vm_init()`, test:
   - Translating global variable `&a` via `walk_va_to_pa(swapper_pg_dir, (uintptr_t)&a)`.
   - Mapping a dynamic virtual address `0x40000028` to a newly allocated page and verifying both hardware MMU translation and software `walk_va_to_pa()`.
   - Triggering and recovering from a controlled Load Page Fault (`scause = 13`) on unmapped address `0x50000000`.

```c
/* Target File: my-linux/init/main.c */
#include <linux/types.h>
#include <linux/fdt.h>
#include <linux/mm.h>
#include <asm/pgtable.h>

/* Global variable in .data section to test VA -> PA translation */
int a = 42;

/* Inside start_kernel(): */
    /* 5. Initialize Sv39 Page Tables and Enable MMU */
    vm_init();

    /* 6. Test Global Variable 'int a = 42' (Identity VA + High-Half Linear VA!) */
    uintptr_t a_va = (uintptr_t)&a;
    uintptr_t a_pa = walk_va_to_pa(swapper_pg_dir, a_va);
    volatile int *high_a_ptr = (volatile int *)__va(a_pa); /* 0xFFFFFFC08020xxxx! */
    uintptr_t high_a_walked_pa = walk_va_to_pa(swapper_pg_dir, (uintptr_t)high_a_ptr);

    early_puts("[MMU TEST] Global 'a' Low-Half VA: ");
    early_print_hex(a_va);
    early_puts(" | High-Half __va(a_pa): ");
    early_print_hex((uintptr_t)high_a_ptr);
    early_puts("\n           Walked PA: ");
    early_print_hex(high_a_walked_pa);
    early_puts(" | __pa(high_a_ptr): ");
    early_print_hex(__pa(high_a_ptr));
    early_puts(" | Value via High-Half VA: ");
    early_print_dec((unsigned long)(*high_a_ptr));
    early_puts("\n");

    /* 7. Test Custom Dynamic Virtual-to-Physical Mapping (VA != PA)! */
    struct page *test_card = alloc_page();
    uintptr_t test_pa = page_to_phys(test_card);
    uintptr_t test_va = 0x40000000UL; /* Brand-new Virtual Page at 1 GB mark */

    early_puts("[MMU TEST] Mapping Dynamic VA 0x40000028 -> Physical ");
    early_print_hex(test_pa + 0x28);
    early_puts("\n");

    map_page(swapper_pg_dir, test_va, test_pa, PAGE_KERNEL_RW);
    __asm__ volatile("sfence.vma zero, zero" ::: "memory");

    /* Write through the VIRTUAL address + offset 0x28 */
    volatile uint64_t *virt_ptr = (volatile uint64_t *)(test_va + 0x28);
    *virt_ptr = 0x123456789abcdef0UL;

    /* Walk page table in software for VA 0x40000028 and verify via High-Half __va(walked_pa) */
    uintptr_t walked_pa = walk_va_to_pa(swapper_pg_dir, test_va + 0x28);
    volatile uint64_t *linear_ptr = (volatile uint64_t *)__va(walked_pa);
    if (walked_pa == (test_pa + 0x28) && *linear_ptr == 0x123456789abcdef0UL) {
        early_puts("  [SUCCESS] Wrote to VA 0x40000028, walked to PA ");
        early_print_hex(walked_pa);
        early_puts(", read back via High-Half __va(): ");
        early_print_hex((uintptr_t)linear_ptr);
        early_puts("!\n");
    }

    /* 8. Test a Controlled Page Fault on an unmapped address (0x50000000) */
    early_puts("[MMU TEST] Touching unmapped Virtual Address 0x50000000...\n");
    volatile uint64_t *bad_ptr = (volatile uint64_t *)0x50000000UL;
    uint64_t dummy = *bad_ptr; /* Triggers Load Page Fault (scause = 13)! */
    (void)dummy;
    early_puts("  [SUCCESS] Page Fault caught and recovered cleanly!\n\n");
```


