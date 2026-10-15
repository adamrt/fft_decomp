#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Reads one nine-bit record: bits 0-4 into dest->day, bits 5-8 into dest->month.
 * Records are packed nine bits apart in the bit array, matching
 * wldcore_write_nine_bit_record. A field that decodes to zero or beyond its
 * range falls back to 1. */
void wldcore_read_nine_bit_record(u32* bits, s32 record, calendar_date_t* dest) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    dest->day = 0;
    for (i = 0; i < 5; i++) {
        if (wldcore_test_bit(bits, base + i)) {
            dest->day |= mask;
        }
        mask <<= 1;
    }
    mask = 1;
    dest->month = 0;
    for (i = 0; i < 4; i++) {
        s32 index = i + 5;

        if (wldcore_test_bit(bits, base + index)) {
            dest->month |= mask;
        }
        mask <<= 1;
    }
    if (dest->day == 0 || dest->day >= 32) {
        dest->day = 1;
    }
    if (dest->month == 0 || dest->month >= 13) {
        dest->month = 1;
    }
}
