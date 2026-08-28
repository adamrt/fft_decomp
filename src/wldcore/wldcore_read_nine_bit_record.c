#include "psx/types.h"

/* Reads one nine-bit record: bits 0-4 into dest[1], bits 5-8 into dest[0].
 * Records are packed nine bits apart in the bit array, matching
 * wldcore_write_nine_bit_record. A field that decodes to zero or beyond its
 * range falls back to 1. */
void wldcore_read_nine_bit_record(u32* bits, s32 record, u8* dest) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    dest[1] = 0;
    for (i = 0; i < 5; i++) {
        if (wldcore_test_bit(bits, base + i)) {
            dest[1] |= mask;
            /* Keeps i++ at the branch target; reorg otherwise moves it into the delay slot. */
            __asm__ volatile("");
        }
        mask <<= 1;
    }
    mask = 1;
    dest[0] = 0;
    for (i = 0; i < 4; i++) {
        s32 index = i + 5;

        if (wldcore_test_bit(bits, base + index)) {
            dest[0] |= mask;
            /* As above. */
            __asm__ volatile("");
        }
        mask <<= 1;
    }
    if (dest[1] == 0 || dest[1] >= 32) {
        dest[1] = 1;
    }
    if (dest[0] == 0 || dest[0] >= 13) {
        dest[0] = 1;
    }
}
