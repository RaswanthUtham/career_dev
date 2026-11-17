# W01-L04 (C01): Checked Header-Plus-Payload Size Calculation

> **Verified:** Tutor isolated scratch run in Podman (`localhost/xv6-lab:latest`),
> `gcc 13.3.0` (`-std=c11 -O2 -Wall -Wextra -Werror -pedantic`), 2026-10-23,
> `9/9` boundary checks `PASS`.
> **Learner status (updated 2026-10-25):** Learner submitted and repaired
> [C01_packet_size.c](C01_packet_size.c) (source-reviewed; runtime execution
> pending).

Previous: [W01-L03: Privilege And Address Spaces — RISC-V To Arm](W01_L03_privilege_and_address_spaces.md).

## 0. Know This First

In **Project A**, the STM32 sends SPI frames to the BeagleBone Black. Each frame
has a **fixed header** (say `8 bytes`) followed by **`sample_count` scans** (each
`4 bytes`).

Before allocating a buffer or copying bytes, your code must compute:

```text
total_bytes = header_bytes + (sample_count * bytes_per_sample)
```

Why is that one line of C a classic **Senior/Staff interview trap** and a real
embedded security bug? Because if a corrupted SPI header or buggy caller passes a
huge `sample_count`, the multiplication or addition **wraps around past the
maximum value of the integer type**, producing a tiny `total_bytes`!

| Term | Plain meaning with our Project A example |
| --- | --- |
| **`size_t` (`<stddef.h>`)** | The standard unsigned C type for byte counts and object sizes (`32-bit` on BeagleBone Black, `64-bit` on 64-bit Linux). |
| **`SIZE_MAX` (`<stdint.h>`)** | The largest number a `size_t` can hold (`4,294,967,295` on 32-bit; `18,446,744,073,709,551,615` on 64-bit). |
| **Unsigned Wraparound** | In C, unsigned math never traps on overflow—it wraps modulo `MAX + 1` like a car odometer rolling from `999` back to `000`. |
| **Units (`samples` vs. `bytes`)** | `sample_count` is a count of scans (`128` scans); `bytes_per_sample` is `4` bytes/scan; `payload_bytes` is `128 * 4 = 512` bytes. Mixing elements and bytes causes buffer overruns. |
| **Subtraction / Division Guard** | Rearranging `a + b > MAX` into `b > MAX - a` (and `a * b > LIMIT` into `a > LIMIT / b`) so the check itself **can never overflow**. |

## 1. Goal

* **Before:** Writing `size_t total = header + count * size; if (total > max_frame)` and assuming it rejects oversized frames.
* **After:** Prove with a tiny 8-bit number why `header + count * size` wraps around when stored in a fixed-width unsigned type, understand C's **integer promotion rule** (`uint8_t` vs. `size_t`), derive the overflow-safe **subtraction + division guards**, and implement `c01_frame_size_bytes()` with `9` boundary tests.

## 2. Why Naive Size Addition Fails (Tiny 8-Bit Model + C Promotion Rule)

Before touching 32-bit or 64-bit `size_t`, work the wraparound by hand using an **8-bit unsigned counter (`uint8_t`, where `MAX = 255`)**:

* Suppose:
  * `header_bytes = 8`
  * `sample_count = 63`
  * `bytes_per_sample = 4`
  * `max_frame_bytes = 200`
* What is the **true mathematical size**?
  * `payload = 63 * 4 = 252` bytes
  * `total   = 8 + 252 = 260` bytes (which is `> 200`, so it **must be rejected!**)
* What happens if code stores that sum into an 8-bit unsigned variable (`uint8_t total = header + sample_count * 4;`) before checking `if (total > 200)`?
  * Storing `260` into `uint8_t` wraps modulo `256`: `260 - 256 = 4`!
  * Now `if (total > 200)` checks `if (4 > 200)` -> **FALSE!**
  * Your code thinks the frame is only **4 bytes** long, allocates a `4-byte` buffer, and then copies `252 bytes` of payload into it—corrupting memory!

> **Crucial C Interview Detail (`uint8_t` Promotions vs. `size_t`):**
> In C, integer types narrower than `int` are **promoted to `int`** before `+` or `*` runs (whenever `int` can hold all values of that smaller type, which is always true for `uint8_t`; otherwise they promote to `unsigned int`). So if you wrote `if (u8_hdr + u8_count * 4 > 200)` *without* storing into `uint8_t total`, the math would happen in `int` as `260 > 200` (`true`).
> **Why does `size_t` still wrap directly inside `if (sz_hdr + sz_count * 4 > max)` on our targets?**
> Because on our 32-bit Arm (BBB/STM32) and 64-bit Linux platforms, `size_t` is `32-bit` or `64-bit` unsigned—already at least as wide as `int`! It does **not** promote to a wider type, so every `*` and `+` on `size_t` wraps directly modulo `SIZE_MAX + 1`!

```text
Naive 8-bit counter model (MAX = 255):
  uint8_t total = header (8) + payload (252);  /* 260 wraps past 255 -> becomes 4! */
  Check: is total (4) > 200? NO! (Bug: 260-byte frame sneaks past a 200-byte limit!)

Safe Subtraction + Division Guard (NEVER wraps in 8-bit or size_t):
  Step 1 (Check header against limit):
    Is header (8) > max_frame (200)?
    No. Remaining room for payload: max_payload = 200 - 8 = 192 bytes.

  Step 2 (Check multiplication using division):
    Max allowed samples = max_payload / bytes_per_sample = 192 / 4 = 48 samples.
    Is sample_count (63) > 48?
    YES (63 > 48)! Reject immediately BEFORE multiplying or adding!
```

## 3. Starting Snapshot: All 9 Cases Your Function Must Handle

Our function contract:
* **Prototype:**
  ```c
  bool c01_frame_size_bytes(size_t header_bytes,
                            size_t sample_count,
                            size_t bytes_per_sample,
                            size_t max_frame_bytes,
                            size_t *out_total_bytes);
  ```
* **Contract:**
  * Caller precondition: `out_total_bytes` must be either `NULL` (which is safely rejected) or point to a valid, writable `size_t` object. (C cannot detect a non-NULL dangling/unmapped pointer.)
  * Returns `true` and writes `*out_total_bytes = header_bytes + (sample_count * bytes_per_sample)` if and only if `out_total_bytes != NULL` and the total fits within `max_frame_bytes` without overflowing `size_t`.
  * Returns `false` (and sets `*out_total_bytes = 0` when `out_total_bytes != NULL`) on `NULL`, overflow, or limit violation.
  * If `bytes_per_sample == 0`, the payload size is `0` bytes and the function succeeds with `*out_total_bytes = header_bytes` (assuming `header_bytes <= max_frame_bytes`).

| Case # | `header_bytes` | `sample_count` | `bytes_per_sample` | `max_frame_bytes` | Expected Return | Expected `*out_total_bytes` | Why |
| --- | --- | --- | --- | --- | --- | --- | --- |
| **1. Normal** | `8` | `128` | `4` | `1024` | `true` | `520` | `8 + (128 * 4) = 520 <= 1024` |
| **2. Zero samples** | `8` | `0` | `4` | `260` | `true` | `8` | Header-only command frame (`0` samples) |
| **3. Exact fit** | `4` | `64` | `4` | `260` | `true` | `260` | `4 + (64 * 4) = 260 == 260` (boundary!) |
| **4. 1 over limit** | `4` | `65` | `4` | `260` | `false` | `0` | `65 > (260 - 4) / 4` (`65 > 64`) |
| **5. Multiply wrap** | `8` | `(SIZE_MAX / 4) + 1` | `4` | `SIZE_MAX` | `false` | `0` | Would wrap `size_t` on `sample_count * 4` |
| **6. Add-only wrap** | `16` | `SIZE_MAX / 4` | `4` | `SIZE_MAX` | `false` | `0` | `sample_count * 4` fits, but `+ 16` would wrap `SIZE_MAX` |
| **7. Header > max** | `16` | `1` | `4` | `8` | `false` | `0` | `header_bytes (16) > max_frame_bytes (8)` |
| **8. 0 B/sample** | `8` | `100` | `0` | `260` | `true` | `8` | `0`-byte samples -> `8` bytes total (`no div-by-zero`) |
| **9. NULL output** | `8` | `10` | `4` | `260` | `false` | (N/A) | `out_total_bytes == NULL` rejected safely |

## 4. Target File To Implement: `Lessons/c01_packet_size.c`

Because **this is the Week 1 C01 implementation exercise**, the complete,
tutor-verified reference code is shown below with every step explained inside
the inline comments.

*(Note: Read and understand the trace first, then close this note and type your
own implementation when ready. Ask in chat if you want me to create an empty
`Lessons/c01_packet_size.c` file or run your code in Podman.)*

```c
/* Target file: Lessons/c01_packet_size.c (ISO C11) */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

bool c01_frame_size_bytes(size_t header_bytes,
                          size_t sample_count,
                          size_t bytes_per_sample,
                          size_t max_frame_bytes,
                          size_t *out_total_bytes)
{
    /* 1. REJECT NULL OUTPUT POINTER:
     *    Never dereference out_total_bytes before checking it is non-NULL. */
    if (out_total_bytes == NULL) {
        return false;
    }

    /* Default output to 0 so a caller cannot accidentally read stale garbage
     * if this function returns false. */
    *out_total_bytes = 0;

    /* 2. SUBTRACTION GUARD (CHECK HEADER AGAINST MAX FRAME LIMIT FIRST):
     *    Why check `header_bytes > max_frame_bytes` BEFORE subtracting?
     *    Because in unsigned size_t math, if header_bytes (16) > max_frame_bytes (8),
     *    then `max_frame_bytes - header_bytes` (8 - 16) would underflow/wrap
     *    to a huge number near SIZE_MAX! */
    if (header_bytes > max_frame_bytes) {
        return false;
    }

    /* Safe to subtract now: header_bytes <= max_frame_bytes is guaranteed.
     * Example (Case 3): max_payload_bytes = 260 - 4 = 256 bytes. */
    size_t max_payload_bytes = max_frame_bytes - header_bytes;

    /* 3. DIVISION GUARD (CHECK SAMPLE MULTIPLICATION BEFORE MULTIPLYING):
     *    We need: sample_count * bytes_per_sample <= max_payload_bytes.
     *    Dividing max_payload_bytes by bytes_per_sample cannot overflow!
     *    - Guard `bytes_per_sample != 0` first so we never divide by zero.
     *    - Example (Case 3): 256 / 4 = 64 max samples. sample_count = 64 -> OK.
     *    - Example (Case 4): 256 / 4 = 64 max samples. sample_count = 65 > 64 -> FALSE! */
    if (bytes_per_sample != 0 &&
        sample_count > max_payload_bytes / bytes_per_sample) {
        return false;
    }

    /* 4. COMPUTE FINAL BYTES (PROVEN SAFE FROM OVERFLOW AND <= max_frame_bytes):
     *    Because Step 2 and Step 3 passed:
     *    - payload_bytes = sample_count * bytes_per_sample <= max_payload_bytes
     *    - header_bytes + payload_bytes <= max_frame_bytes <= SIZE_MAX */
    size_t payload_bytes = sample_count * bytes_per_sample;
    *out_total_bytes = header_bytes + payload_bytes;
    return true;
}

int main(void)
{
    size_t total = 999;

    /* Case 1: Normal Project A frame (8 + 128 * 4 = 520 bytes) */
    if (!c01_frame_size_bytes(8, 128, 4, 1024, &total) || total != 520) return 1;

    /* Case 2: Zero samples (8 + 0 * 4 = 8 bytes) */
    if (!c01_frame_size_bytes(8, 0, 4, 260, &total) || total != 8) return 2;

    /* Case 3: Exact-fit boundary (4 + 64 * 4 = 260 bytes == limit 260) */
    if (!c01_frame_size_bytes(4, 64, 4, 260, &total) || total != 260) return 3;

    /* Case 4: One sample over the limit (4 + 65 * 4 = 264 > 260 -> rejected) */
    if (c01_frame_size_bytes(4, 65, 4, 260, &total) || total != 0) return 4;

    /* Case 5: Multiplication overflow past SIZE_MAX -> rejected */
    if (c01_frame_size_bytes(8, (SIZE_MAX / 4) + 1, 4, SIZE_MAX, &total) || total != 0) return 5;

    /* Case 6: Addition-only overflow (sample_count * 4 fits, but + header wraps SIZE_MAX) */
    if (c01_frame_size_bytes(16, SIZE_MAX / 4, 4, SIZE_MAX, &total) || total != 0) return 6;

    /* Case 7: Header larger than max_frame_bytes (16 > 8 -> rejected) */
    if (c01_frame_size_bytes(16, 1, 4, 8, &total) || total != 0) return 7;

    /* Case 8: Zero bytes_per_sample (no division by zero; payload = 0, total = 8) */
    if (!c01_frame_size_bytes(8, 100, 0, 260, &total) || total != 8) return 8;

    /* Case 9: NULL output pointer -> rejected without crashing */
    if (c01_frame_size_bytes(8, 10, 4, 260, NULL)) return 9;

    printf("C01 packet size checks: 9/9 PASS\n");
    return 0;
}
```

### Bonus: How Upstream Linux Writes This Inside Drivers (`include/linux/overflow.h`)
In portable ISO C (user space or MCU code), you write the subtraction and division guards above. Inside the **Linux kernel**, `<linux/overflow.h>` provides compiler-builtin helpers ([include/linux/overflow.h:61](../../../linux/include/linux/overflow.h#L61)):
```c
/* Inside Linux kernel code only (uses GCC/Clang __builtin_mul_overflow / __builtin_add_overflow): */
if (check_mul_overflow(sample_count, bytes_per_sample, &payload_bytes) ||
    check_add_overflow(header_bytes, payload_bytes, &total_bytes) ||
    total_bytes > max_frame_bytes)
        return -EINVAL;
```
In interviews, write the **portable ISO C subtraction/division guards first**, then mention `check_mul_overflow()` / `check_add_overflow()` as the Linux kernel equivalent!

## 5. Control Flow

### (a) One-Line Call Chain

```text
main() test runner -> c01_frame_size_bytes(hdr, count, sample_sz, max_frame, &total)
  -> NULL check -> header subtraction guard -> sample division guard -> multiply & add -> return true/false
```

### (b) Step Table (Tracing Case 3 Exact Fit `64` vs. Case 4 Overflow `65`, with `hdr = 4, sz = 4, max = 260`)

| # | Step in `c01_frame_size_bytes` | Goal / guard | Case 3 (`count = 64`) | Case 4 (`count = 65`) |
| --- | --- | --- | --- | --- |
| 1 | `if (out_total_bytes == NULL)` | Reject invalid output pointer | Non-NULL -> set `*out = 0` | Non-NULL -> set `*out = 0` |
| 2 | `if (header_bytes > max_frame_bytes)` | Prevent subtraction underflow | `4 > 260` is `false` (OK) | `4 > 260` is `false` (OK) |
| 3 | `max_payload_bytes = max - hdr` | Compute remaining byte budget | `260 - 4 = 256` bytes | `260 - 4 = 256` bytes |
| 4 | `count > max_payload / sz` | Prevent multiplication overflow / overrun | `64 > 256 / 4` (`64 > 64`) is `false` | `65 > 256 / 4` (`65 > 64`) is **`true` -> return `false`!** |
| 5 | `*out = hdr + (count * sz)` | Safe multiply & add | `4 + (64 * 4) = 260` -> return `true` | *(Not reached; `*out` stays `0`)* |

## 6. Build And Run Command (Podman Environment)

When you have typed `Lessons/c01_packet_size.c` and want to compile and run it using our approved Podman workflow from PowerShell:

```powershell
podman run --rm -v "${PWD}:/work:Z" -w /work localhost/xv6-lab:latest sh -c "gcc -std=c11 -O2 -Wall -Wextra -Werror -pedantic Lessons/c01_packet_size.c -o /tmp/c01_packet_size && /tmp/c01_packet_size"
```

* **Expected output:** `C01 packet size checks: 9/9 PASS` and exit code `0`.
* **Complexity:** `O(1)` time, `O(1)` auxiliary space.

## 7. Feynman Recap And Interview Answers

**Feynman Recap (for a 12-year-old):**
If your bag can hold at most `260` marbles (`max_frame_bytes`) and there are already `4` marbles inside (`header_bytes`), never multiply `65 boxes * 4 marbles` and add `4` first—because on a computer counter with a fixed number of digits, a huge result can roll past `999` back to `003` and trick you into thinking it fits! Instead, subtract first (`260 - 4 = 256` marbles of free space left), then divide (`256 / 4 = 64` boxes max). Since `65 > 64`, you know immediately that `65` boxes will not fit, without ever risking a counter rollover.

---

### Interview Q1: Why is `if (header_bytes + sample_count * sample_size > max_bytes)` a dangerous bug in C?
* **Simple answer:** Because in C, `size_t` is unsigned, and **unsigned integer overflow wraps around modulo `SIZE_MAX + 1`** instead of trapping or saturating.
* **What goes wrong:** A large `sample_count` makes `sample_count * sample_size` (or the `+ header_bytes` sum) wrap around to a **small number** that passes the `<= max_bytes` check. The caller then allocates a tiny buffer and copies a huge number of bytes into it, causing heap/stack corruption.

### Interview Q2: Why must you check `header_bytes > max_frame_bytes` BEFORE computing `max_frame_bytes - header_bytes`?
* **Simple answer:** To prevent **unsigned subtraction underflow**.
* **How it fails without the check:** Because `size_t` is unsigned, if `header_bytes` (`16`) is larger than `max_frame_bytes` (`8`), then `8 - 16` wraps around to `SIZE_MAX - 7` (a gigantic positive number), which completely defeats the payload limit check that follows.

### Interview Q3: What is the difference between unsigned overflow and signed overflow in C?
* **Simple answer:**
  * **Unsigned overflow (`size_t`, `uint32_t`):** Well-defined by the ISO C standard—it wraps modulo `2^N`. (Defined by the language, but still a logic/security bug if you didn't intend it!)
  * **Signed overflow (`int`, `int32_t`):** **Undefined Behavior (UB)** in C. The compiler is allowed to assume signed overflow never happens and can optimize away your post-addition check (`if (a + b < a)`) completely!
* **Rule:** Always use `size_t` for byte/object counts and check bounds **before** adding or multiplying using subtraction/division guards (or `__builtin_add_overflow` / `check_add_overflow()` in Linux kernel code).

## 8. Corrections

- 2026-10-23: First publication of W01-L04 (`C01`), verified in isolated Podman scratch container (`7/7` checks `PASS`).
- 2026-10-23 (revision 2): Applied technical review fixes: distinguished narrowing `uint8_t total = ...` wraparound from C's `int` integer-promotion rule (and contrasted it with unpromoted `size_t` arithmetic), refined the `out_total_bytes` precondition (`NULL` vs. valid writable `size_t *`), and added Case 6 (addition-only overflow) and Case 8 (`bytes_per_sample == 0`)—re-verified `9/9 PASS` in Podman.
- 2026-10-25 (revision 3): Updated learner status header (`LR6`) to match the index and log record for [C01_packet_size.c](C01_packet_size.c).
