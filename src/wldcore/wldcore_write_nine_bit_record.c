#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Writes one nine-bit record: bits 0-4 from source->day, bits 5-8 from
 * source->month. Records are packed nine bits apart in the bit array. */
void wldcore_write_nine_bit_record(u32* bits, s32 record, calendar_date_t* source) {
    s32 base = record * 9;
    s32 mask;
    s32 i;

    mask = 1;
    for (i = 0; i < 5; i++) {
        wldcore_set_bit_value(bits, base + i, source->day & mask);
        mask <<= 1;
    }
    mask = 1;
    for (i = 0; i < 4; i++) {
        /* The separate temporary keeps the target's add order: folding
         * base + (i + 5) in one expression hoists base + 5 out of the loop. */
        s32 index = i + 5;

        wldcore_set_bit_value(bits, base + index, source->month & mask);
        mask <<= 1;
    }
}
