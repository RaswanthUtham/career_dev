# Timekeeping, Clocksource vs. Clockevents & Dynamic Kernel Timers (`timer_list`)

This guide covers **Milestone 5: Time and IRQ Pipeline** in the upstream Linux kernel way:
1. **The 12-Year-Old Clock Math (`timebase-frequency` vs. `HZ`)** — Why `HZ` is an OS configuration choice (`100`, `250`, or `1000`), NOT a hardware number derived from `10 MHz`!
2. **`struct clocksource` vs. `struct clock_event_device`** — How Linux separates *reading the current time* (`rdtime`) from *setting the next alarm interrupt* (`sbi_set_timer` / `Sstc`).
3. **`jiffies_64` and Dynamic Kernel Timers (`struct timer_list`)** — How drivers schedule callbacks in the future (`timer_setup`, `mod_timer`, `del_timer`) without blocking the CPU!

---

## PART 1: The Clock Math Explained to a 12-Year-Old (`timebase-frequency` vs. `HZ`)

### 1. What Are the Two Numbers in Every OS Timer?

Imagine you have a wall clock in your classroom and a teacher who claps their hands to check on the students:

```text
+-------------------------------------------------------------------------------+
| 1. THE HARDWARE STOPWATCH (timebase-frequency):                               |
|    - Inside the RISC-V chip, a hardware counter ('time' CSR) ticks upward     |
|      automatically at a fixed crystal speed.                                  |
|    - Remember when our FDT parser scanned '/cpus' in QEMU Virt?               |
|      It found:  timebase-frequency = 10,000,000 (10 MHz)!                     |
|    - That means the hardware stopwatch ticks 10,000,000 times every 1 second! |
|    - Can software change this speed? NO! It is wired in silicon hardware!     |
+-------------------------------------------------------------------------------+
| 2. THE OS HEARTBEAT RATE (CONFIG_HZ / HZ):                                    |
|    - How many times per second does the OS Kernel want a timer interrupt?     |
|    - This is called 'HZ' (defined in linux/kernel/Kconfig.hz).                |
|    - The kernel engineer CHOOSES 'HZ' when configuring the kernel:            |
|        * HZ = 100  (100 ticks/sec -> 1 tick every 10 milliseconds)            |
|        * HZ = 250  (250 ticks/sec -> 1 tick every 4 milliseconds, Linux def)  |
|        * HZ = 1000 (1000 ticks/sec -> 1 tick every 1 millisecond)             |
|    - CRITICAL RULE: 'HZ = 100' is a human choice, NOT calculated from 10 MHz! |
+-------------------------------------------------------------------------------+
```

### 2. How Do We Calculate How Many Hardware Ticks Make 1 OS Heartbeat ("Jiffy")?

In Linux, one OS heartbeat interval (`1 / HZ` of a second) is called **1 Jiffy**.
And a global counter called **`jiffies_64`** counts how many heartbeats have happened since the kernel booted!

Now let's do the 12-year-old division math:
- The hardware stopwatch ticks **`10,000,000` times per second** (`timebase-frequency = 10,000,000`).
- Suppose we choose **`HZ = 100`** (we want the alarm to ring **100 times per second**, which is once every `10 ms`).
- How many hardware stopwatch ticks happen in 1 Jiffy (`10 ms`)?

```text
THE JIFFY DELTA FORMULA:

                       timebase_frequency       10,000,000 ticks/sec
  Ticks per Jiffy  =  --------------------  =  ----------------------  =  100,000 ticks!
                              HZ                  100 jiffies/sec
```

Look how simple that is!
- Every time the hardware `time` counter goes up by **`100,000`**, **10 milliseconds (1 Jiffy)** have passed!
- So if the current hardware `time` is `500,000`, when should the next alarm ring?
  At `500,000 + 100,000 = 600,000`!

---

## PART 2: Why Does Linux Split Timers into `clocksource` and `clock_event_device`?

In a beginner project, people write one giant `timer.c` file that mixes everything together.
In the **upstream Linux kernel** (`include/linux/clocksource.h` and `include/linux/clockchips.h`), Linux separates time hardware into **two completely distinct roles**:

```text
+--------------------------------------+--------------------------------------+
| 1. struct clocksource                | 2. struct clock_event_device         |
|    ("The Wristwatch You Read")       |    ("The Kitchen Alarm That Rings")  |
+--------------------------------------+--------------------------------------+
| - Duty: Passively read a monotonically| - Duty: Program the hardware to fire |
|   increasing counter whenever someone|   an interrupt at a future deadline! |
|   asks "What time is it right now?"  |                                      |
| - Does it ever fire an interrupt?    | - Does it fire an interrupt?         |
|   NO!                                |   YES! (Supervisor Timer IRQ = 5)    |
| - RISC-V Hardware Used:              | - RISC-V Hardware Used:              |
|   'rdtime' instruction (reads 'time' |   OpenSBI sbi_set_timer(next_tick)   |
|   CSR in 1 clock cycle!)             |   (or Sstc 'stimecmp' CSR)           |
| - Upstream Linux File:               | - Upstream Linux File:               |
|   drivers/clocksource/timer-riscv.c  |   drivers/clocksource/timer-riscv.c  |
+--------------------------------------+--------------------------------------+
```

### How Does the Alarm Actually Ring on RISC-V? (OpenSBI `sbi_set_timer`)

Remember from Milestone 1: in RISC-V, the raw Machine Timer comparator (`mtimecmp`) is an M-mode resource owned by OpenSBI firmware, while our kernel runs in S-mode.
To set the next alarm deadline, our S-mode kernel makes an **SBI `ecall`** to OpenSBI (`EID = 0x54494D45` "TIME", `FID = 0`), passing the 64-bit `next_tick` deadline in register `a0`!
*(Note: Why didn't our earlier `ecall` in `main.c` trap into `stvec`? Because S-mode `ecall` goes to OpenSBI! Now we use that exact S-mode `ecall` to tell OpenSBI when to fire our next timer interrupt!)*

```text
LIFE OF ONE TIMER INTERRUPT IN my-linux:

  1. Kernel calls riscv_clock_next_event(rdtime() + 100000):
     Executes SBI ecall (a7 = 0x54494D45, a0 = deadline).
     OpenSBI programs hardware comparator and clears pending STIP bit.

  2. Kernel enables Supervisor Timer Interrupts (sie.STIE = 1)
     and Global Supervisor Interrupts (sstatus.SIE = 1).

  3. 10 milliseconds pass (hardware 'time' reaches deadline)!

  4. Hardware sets sip.STIP = 1 and traps CPU to 'stvec' (handle_exception)
     with:
       scause = (1UL << 63) | 5   (Bit 63 = Interrupt, Code 5 = Supervisor Timer!)

  5. do_trap() sees Interrupt Code 5 -> calls timer_interrupt_handler():
       a) Increments jiffies_64++
       b) Rearms next alarm: riscv_clock_next_event(rdtime() + 100000)
       c) Checks if any dynamic software timers (struct timer_list) expired!
```

---

## PART 3: Dynamic Kernel Timers (`struct timer_list`) — The Linux Way!

### 1. What Is `struct timer_list` and Why Do Drivers Need It?

Suppose you are writing an LED driver, a network timeout, or an SPI sensor polling driver.
You want to say: *"Run my function `my_timeout_callback()` 500 milliseconds (50 jiffies) from now, without freezing the CPU in a `while` loop!"*

In Linux (`include/linux/timer.h`), you use **`struct timer_list`**!
Look how our **intrusive linked list (`struct list_head`)** from Milestone 4 comes back to shine:

```c
/* Target File Concept: include/linux/timer.h */
struct timer_list {
    struct list_head entry;             /* Links into the kernel's active timer list! */
    unsigned long expires;              /* The jiffies_64 value when this timer should fire */
    void (*function)(struct timer_list *t); /* Callback function to run when expired */
};
```

```text
ACTIVE KERNEL TIMERS LINKED ON timer_list_head:

  [timer_list_head] <---> [Timer A: expires = 50] <---> [Timer B: expires = 200] <---> [timer_list_head]
```

Every time `jiffies_64` ticks upward in the timer interrupt (or in `TIMER_SOFTIRQ`), the kernel walks `timer_list_head`:
- If `jiffies_64 >= timer->expires`:
  1. Unlink `timer->entry` from the list (`list_del`).
  2. Call `timer->function(timer)`!
- And if the driver wants a repeating timer, inside `timer->function(timer)` it simply calls `mod_timer(timer, jiffies_64 + 50)` to schedule itself again!

---

## PART 4: How Upstream `linux/arch/riscv/kernel/time.c` & `drivers/clocksource/timer-riscv.c` Work

Let's inspect the actual upstream Linux files in `C:\workspace\career_ws\linux`:
1. **`linux/arch/riscv/kernel/head.S` (line 245)**:
   Enables the `time` counter CSR via `scounteren`:
   ```assembly
   li t0, 0x2
   csrw scounteren, t0
   ```
2. **`linux/arch/riscv/kernel/time.c` (lines 18–45)**:
   Defines `unsigned long riscv_timebase;` and reads the `"timebase-frequency"` property dynamically from the `/cpus` node in the Device Tree! (No hardcoded `#define RISCV_TIMEBASE_FREQ 10000000`!).
   Then calculates ticks per jiffy (`lpj_fine = riscv_timebase / HZ;`).
3. **`linux/drivers/clocksource/timer-riscv.c` (lines 28–152)**:
   - **`riscv_clock_event_stop()`**: Calls `sbi_set_timer(U64_MAX)` (`~0ULL`) to clear/stop any pending timer interrupt before programming or handling an event.
   - **`riscv_clock_next_event(unsigned long delta, struct clock_event_device *ce)`**: Reads `rdtime() + delta` and calls `sbi_set_timer(next_tval)`.
   - **`riscv_clocksource_rdtime(struct clocksource *cs)`**: Returns `rdtime()`.
   - **`riscv_timer_interrupt()`**: Stops the current event (`riscv_clock_event_stop()`), then invokes `evdev->event_handler(evdev)` (which increments `jiffies_64`, rearms the next tick `riscv_timebase / HZ`, and runs expired `struct timer_list` callbacks!).

---

## PART 5: Step-by-Step Implementation in `my-linux/`

Here are the exact files for you to update or create in `my-linux/`.

---

### Step 1: Enable `scounteren` in `my-linux/arch/riscv/kernel/head.S`

**What this change does:**
Matching `linux/arch/riscv/kernel/head.S` (line 245), setting Bit 1 (`0x2`) in `scounteren` permits reading the hardware `time` CSR (`rdtime`).

Add these two lines right after `csrw sip, zero` in **`my-linux/arch/riscv/kernel/head.S`**:
```assembly
/* Target File: my-linux/arch/riscv/kernel/head.S */
    /* Enable time CSR access (matching upstream linux/arch/riscv/kernel/head.S) */
    li   t0, 0x2
    csrw scounteren, t0
```

---

### Step 2: Dynamically Read `"timebase-frequency"` in `my-linux/arch/riscv/kernel/setup.c`

**What this change does:**
In `my-linux/arch/riscv/kernel/setup.c`, your `fdt_scan_nodes()` loop already visits every `FDT_PROP` token and gets `const char *prop_name` and the value pointer `p`!
When `prop_name` matches `"timebase-frequency"`, we read the 32-bit Big-Endian integer `be32_to_cpu(*p)` and store it in **`unsigned long riscv_timebase`** (matching upstream `linux/arch/riscv/kernel/time.c`)!

Add a simple string comparison helper `fdt_streq` and capture `riscv_timebase` in **`my-linux/arch/riscv/kernel/setup.c`**:
```c
/* Target File: my-linux/arch/riscv/kernel/setup.c */

void early_print_dec(unsigned long val);

/* Global hardware timebase frequency (ticks/sec) discovered from DTB /cpus */
unsigned long riscv_timebase = 0;

static int fdt_streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/* Inside fdt_scan_nodes(), inside the 'else if (token == FDT_PROP)' block: */
        } else if (token == FDT_PROP) {
            uint32_t prop_len = be32_to_cpu(*p++);
            uint32_t name_offset = be32_to_cpu(*p++);
            const char *prop_name = strings + name_offset;

            /* Dynamically extract timebase-frequency from /cpus node! */
            if (fdt_streq(prop_name, "timebase-frequency") && prop_len == 4) {
                riscv_timebase = (unsigned long)be32_to_cpu(*p);
                early_puts("  [FDT] Discovered riscv_timebase = ");
                early_print_dec(riscv_timebase);
                early_puts(" Hz\n");
            }

            /* Advance pointer past property value and 4-byte padding */
            p += fdt_align4(prop_len) / sizeof(uint32_t);
```

---

### Step 3: Create `my-linux/arch/riscv/include/asm/csr.h` (Interrupt & Counter Helpers)

```c
/* Target File: my-linux/arch/riscv/include/asm/csr.h */
/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _ASM_RISCV_CSR_H
#define _ASM_RISCV_CSR_H

/* sstatus CSR bits */
#define SR_SIE              (1UL << 1)  /* Supervisor Interrupt Enable */
#define SR_SPIE             (1UL << 5)  /* Previous Supervisor Interrupt Enable */
#define SR_SPP              (1UL << 8)  /* Previously Supervisor (1=S, 0=U) */
#define SR_SUM              (1UL << 18) /* Supervisor User Memory access */

/* sie / sip CSR bits */
#define SIE_SSIE            (1UL << 1)  /* Software Interrupt */
#define SIE_STIE            (1UL << 5)  /* Timer Interrupt */
#define SIE_SEIE            (1UL << 9)  /* External Interrupt (PLIC) */

/* Interrupt cause codes (without the top bit 63) */
#define RV_IRQ_SOFT         1
#define RV_IRQ_TIMER        5
#define RV_IRQ_EXT          9

static inline unsigned long get_cycles64(void)
{
    unsigned long val;
    __asm__ volatile("rdtime %0" : "=r"(val));
    return val;
}

static inline void local_irq_enable(void)
{
    __asm__ volatile("csrs sstatus, %0" : : "r"(SR_SIE) : "memory");
}

static inline void local_irq_disable(void)
{
    __asm__ volatile("csrc sstatus, %0" : : "r"(SR_SIE) : "memory");
}

#endif /* _ASM_RISCV_CSR_H */
```

---

### Step 4: Create `my-linux/include/linux/timer.h` (Upstream Linux Timer & Clock Structures)

```c
/* Target File: my-linux/include/linux/timer.h */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_TIMER_H
#define _LINUX_TIMER_H

#include <linux/types.h>
#include <linux/list.h>

/* Configured OS heartbeat rate: 100 ticks per second (10 ms per jiffy) */
#define HZ                  100UL
#define TICK_NSEC           (1000000000UL / HZ)
#define U64_MAX             (~0ULL)

extern unsigned long riscv_timebase;
extern volatile uint64_t jiffies_64;

/*
 * struct clocksource - Upstream Linux monotonic counter abstraction
 */
struct clocksource {
    const char *name;
    int rating;
    uint64_t (*read)(struct clocksource *cs);
};

/*
 * struct clock_event_device - Upstream Linux timer event generator abstraction
 */
struct clock_event_device {
    const char *name;
    int rating;
    int (*set_next_event)(unsigned long delta, struct clock_event_device *ce);
    int (*set_state_shutdown)(struct clock_event_device *ce);
    void (*event_handler)(struct clock_event_device *ce);
};

/*
 * struct timer_list - Dynamic kernel software timer
 */
struct timer_list {
    struct list_head entry;
    uint64_t expires;
    void (*function)(struct timer_list *t);
};

void timer_setup(struct timer_list *timer, void (*callback)(struct timer_list *), unsigned int flags);
int mod_timer(struct timer_list *timer, uint64_t expires);
int del_timer(struct timer_list *timer);
void run_timer_softirq(void);

void time_init(void);
void riscv_timer_interrupt(void);

#endif /* _LINUX_TIMER_H */
```

---

### Step 5: Create `my-linux/drivers/clocksource/timer-riscv.c` (Matching Upstream `timer-riscv.c`)

**Before writing the code, trace how upstream `drivers/clocksource/timer-riscv.c` works:**
1. **`riscv_clock_event_stop()`**: Calls `sbi_set_timer(U64_MAX)` (`0xFFFFFFFFFFFFFFFF`) so the hardware comparator is set to infinity and `sip.STIP` is cleared until the next deadline is programmed.
2. **`riscv_clock_next_event(delta, ce)`**: Reads `next_tval = get_cycles64() + delta` and calls `sbi_set_timer(next_tval)`.
3. **`riscv_timer_interrupt()`**: Called from `do_trap()` when `scause` is `RV_IRQ_TIMER` (`5`). Just like upstream `timer-riscv.c` (line 145), it calls `riscv_clock_event_stop()` and then invokes `riscv_clock_event.event_handler(&riscv_clock_event)`!
4. **`tick_handle_periodic(ce)` (our `event_handler`)**: Increments `jiffies_64++`, rearms the next tick (`riscv_timebase / HZ`), and runs any expired `struct timer_list` callbacks!

```c
/* Target File: my-linux/drivers/clocksource/timer-riscv.c */
/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <asm/csr.h>

void early_puts(const char *s);
void early_print_dec(unsigned long val);

/* Global jiffies counter (incremented at HZ = 100 times per second) */
volatile uint64_t jiffies_64 = 0;

static LIST_HEAD(active_timers);

/*
 * sbi_set_timer - Ask OpenSBI firmware to program the next timer interrupt deadline
 */
static void sbi_set_timer(uint64_t stime_value)
{
    register unsigned long a0 __asm__("a0") = (unsigned long)stime_value;
    register unsigned long a6 __asm__("a6") = 0;            /* FID = 0 */
    register unsigned long a7 __asm__("a7") = 0x54494D45UL; /* EID = "TIME" */

    __asm__ volatile("ecall"
                     : "+r"(a0)
                     : "r"(a6), "r"(a7)
                     : "memory");
}

/*
 * riscv_clock_event_stop - Clear pending timer interrupt by setting comparator to U64_MAX
 * (Matches upstream linux/drivers/clocksource/timer-riscv.c::riscv_clock_event_stop)
 */
static void riscv_clock_event_stop(void)
{
    sbi_set_timer(U64_MAX);
}

/*
 * riscv_clock_next_event - Program next timer interrupt 'delta' counter ticks from now
 * (Matches upstream linux/drivers/clocksource/timer-riscv.c::riscv_clock_next_event)
 */
static int riscv_clock_next_event(unsigned long delta, struct clock_event_device *ce)
{
    (void)ce;
    uint64_t next_tval = get_cycles64() + delta;
    sbi_set_timer(next_tval);
    return 0;
}

static int riscv_clock_shutdown(struct clock_event_device *evt)
{
    (void)evt;
    riscv_clock_event_stop();
    return 0;
}

/*
 * riscv_clocksource_rdtime - Read monotonic hardware counter ('time' CSR)
 * (Matches upstream linux/drivers/clocksource/timer-riscv.c::riscv_clocksource_rdtime)
 */
static uint64_t riscv_clocksource_rdtime(struct clocksource *cs)
{
    (void)cs;
    return get_cycles64();
}

struct clocksource riscv_clocksource = {
    .name   = "riscv_clocksource",
    .rating = 400,
    .read   = riscv_clocksource_rdtime,
};

/*
 * tick_handle_periodic - Generic tick handler registered as ce->event_handler
 */
static void tick_handle_periodic(struct clock_event_device *ce)
{
    /* 1. Advance the OS heartbeat counter */
    jiffies_64++;

    /* 2. Rearm hardware comparator for the next jiffy (riscv_timebase / HZ)! */
    unsigned long ticks_per_jiffy = riscv_timebase / HZ;
    ce->set_next_event(ticks_per_jiffy, ce);

    /* 3. Run expired dynamic software timers (struct timer_list) */
    run_timer_softirq();
}

struct clock_event_device riscv_clock_event = {
    .name               = "riscv_timer_clockevent",
    .rating             = 100,
    .set_next_event     = riscv_clock_next_event,
    .set_state_shutdown = riscv_clock_shutdown,
    .event_handler      = tick_handle_periodic,
};

/*
 * riscv_timer_interrupt - Low-level timer interrupt handler called from do_trap()
 * (Matches upstream linux/drivers/clocksource/timer-riscv.c::riscv_timer_interrupt)
 */
void riscv_timer_interrupt(void)
{
    struct clock_event_device *evdev = &riscv_clock_event;

    riscv_clock_event_stop();
    if (evdev->event_handler) {
        evdev->event_handler(evdev);
    }
}

/*
 * timer_setup - Initialize a dynamic kernel timer_list struct
 */
void timer_setup(struct timer_list *timer, void (*callback)(struct timer_list *), unsigned int flags)
{
    (void)flags;
    INIT_LIST_HEAD(&timer->entry);
    timer->expires = 0;
    timer->function = callback;
}

/*
 * mod_timer - Activate or update a timer's expiration jiffy
 */
int mod_timer(struct timer_list *timer, uint64_t expires)
{
    timer->expires = expires;
    if (list_empty(&timer->entry)) {
        list_add_tail(&timer->entry, &active_timers);
    }
    return 0;
}

/*
 * del_timer - Remove a timer before it expires
 */
int del_timer(struct timer_list *timer)
{
    if (!list_empty(&timer->entry)) {
        list_del(&timer->entry);
        INIT_LIST_HEAD(&timer->entry);
        return 1;
    }
    return 0;
}

/*
 * run_timer_softirq - Check active_timers and fire any expired callbacks
 */
void run_timer_softirq(void)
{
    struct list_head *curr = active_timers.next;
    while (curr != &active_timers) {
        struct timer_list *t = list_entry(curr, struct timer_list, entry);
        struct list_head *next = curr->next;

        if (jiffies_64 >= t->expires) {
            list_del(&t->entry);
            INIT_LIST_HEAD(&t->entry);
            if (t->function) {
                t->function(t);
            }
        }
        curr = next;
    }
}

/*
 * time_init - Verify DTB timebase, arm first tick, and enable Supervisor Timer Interrupts
 * (Matches upstream linux/arch/riscv/kernel/time.c::time_init & riscv_timer_starting_cpu)
 */
void time_init(void)
{
    if (riscv_timebase == 0) {
        early_puts("[TIME PANIC] No timebase-frequency discovered in DTB!\n");
        return;
    }

    unsigned long ticks_per_jiffy = riscv_timebase / HZ;
    early_puts("[TIME] Initializing RISC-V clocksource & clockevent (ticks_per_jiffy = ");
    early_print_dec(ticks_per_jiffy);
    early_puts(")...\n");

    /* Clear any pending timer interrupt and arm the first tick */
    riscv_clock_event_stop();
    riscv_clock_event.set_next_event(ticks_per_jiffy, &riscv_clock_event);

    /* Enable Supervisor Timer Interrupt bit (SIE_STIE) in sie CSR */
    unsigned long sie_val = SIE_STIE;
    __asm__ volatile("csrs sie, %0" : : "r"(sie_val) : "memory");

    /* Enable Master Supervisor Interrupts (SR_SIE) in sstatus CSR */
    local_irq_enable();

    early_puts("[TIME] Supervisor Timer Interrupts Enabled!\n");
}
```

---

### Step 6: Update `my-linux/arch/riscv/kernel/traps.c` to Call `riscv_timer_interrupt()`

Inside `do_trap()` in **`my-linux/arch/riscv/kernel/traps.c`**, when `is_interrupt` is true and `code == RV_IRQ_TIMER` (`5`), call `riscv_timer_interrupt()`:

```c
/* Target File: my-linux/arch/riscv/kernel/traps.c */
#include <linux/timer.h>
#include <asm/csr.h>

/* Inside do_trap(struct pt_regs *regs): */
    if (is_interrupt) {
        if (code == RV_IRQ_TIMER) {
            /* Supervisor Timer Interrupt (scause = 5) */
            riscv_timer_interrupt();
        } else {
            early_puts("[TRAP] Supervisor Interrupt received!\n");
        }
    }
```

---

## PART 6: Plain-Language Feynman Recap

1. **`riscv_timebase` (10 MHz)** is discovered dynamically from the Device Tree `/cpus` `"timebase-frequency"` property (matching `linux/arch/riscv/kernel/time.c`), never hardcoded!
2. **`HZ` (100)** is our chosen OS heartbeat rate (`100` interrupts/sec = `10 ms` per jiffy).
3. **`ticks_per_jiffy = riscv_timebase / HZ` (`100,000`)** is how far ahead `riscv_clock_next_event()` sets the alarm on each tick.
4. **`riscv_clocksource`** reads `get_cycles64()` (`rdtime`); **`riscv_clock_event`** stops/arms the hardware comparator via `sbi_set_timer()` and invokes `event_handler` (`tick_handle_periodic`); **`struct timer_list`** lets any driver schedule callbacks in the future using `jiffies_64`!
