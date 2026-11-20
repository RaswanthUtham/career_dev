#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

bool c01_frame_size_bytes(size_t header_bytes, size_t sample_count,
                          size_t bytes_per_sample, size_t max_frame_bytes,
                          size_t *out_total_bytes) {
  if (out_total_bytes == NULL)
    return false;

  *out_total_bytes = 0;

  if (header_bytes > max_frame_bytes)
    return false;

  size_t max_payload_bytes = max_frame_bytes - header_bytes;
  if (bytes_per_sample != 0 &&
      sample_count > max_payload_bytes / bytes_per_sample)
    return false;

  *out_total_bytes = header_bytes + (sample_count * bytes_per_sample);
  return true;
}

int main(void) {
  size_t total = 999;

  /* Case 1: Normal Project A frame (8 + 128 * 4 = 520 bytes) */
  if (!c01_frame_size_bytes(8, 128, 4, 1024, &total) || total != 520)
    return 1;

  /* Case 2: Zero samples (8 + 0 * 4 = 8 bytes) */
  if (!c01_frame_size_bytes(8, 0, 4, 260, &total) || total != 8)
    return 2;

  /* Case 3: Exact-fit boundary (4 + 64 * 4 = 260 bytes == limit 260) */
  if (!c01_frame_size_bytes(4, 64, 4, 260, &total) || total != 260)
    return 3;

  /* Case 4: One sample over the limit (4 + 65 * 4 = 264 > 260 -> rejected) */
  if (c01_frame_size_bytes(4, 65, 4, 260, &total) || total != 0)
    return 4;

  /* Case 5: Multiplication overflow past SIZE_MAX -> rejected */
  if (c01_frame_size_bytes(8, (SIZE_MAX / 4) + 1, 4, SIZE_MAX, &total) ||
      total != 0)
    return 5;

  /* Case 6: Addition-only overflow (sample_count * 4 fits, but + header wraps
   * SIZE_MAX) */
  if (c01_frame_size_bytes(16, SIZE_MAX / 4, 4, SIZE_MAX, &total) || total != 0)
    return 6;

  /* Case 7: Header larger than max_frame_bytes (16 > 8 -> rejected) */
  if (c01_frame_size_bytes(16, 1, 4, 8, &total) || total != 0)
    return 7;

  /* Case 8: Zero bytes_per_sample (no division by zero; payload = 0, total = 8)
   */
  if (!c01_frame_size_bytes(8, 100, 0, 260, &total) || total != 8)
    return 8;

  /* Case 9: NULL output pointer -> rejected without crashing */
  if (c01_frame_size_bytes(8, 10, 4, 260, NULL))
    return 9;

  printf("C01 packet size checks: 9/9 PASS\n");
  return 0;
}
