#include "fft/wldcore.h"
#include "psx/libgpu.h"
#include "psx/types.h"

party_unit_t* wldcore_get_party_unit(s32 party_index) {
    return main_party_get_unit(party_index);
}

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

/* Clear both frame buffers (256 x 480 at the VRAM origin) to black and, when
 * asked, zero the 0x7800-entry scratch buffer. */
void wldcore_gfx_clear_vram_and_scratch(s32 clear_scratch) {
    RECT image_rect;
    s32 i;

    DrawSync(0);
    VSync(0);
    image_rect.x = 0;
    image_rect.y = 0;
    image_rect.w = 0x100;
    image_rect.h = 0x1e0;
    ClearImage(&image_rect, 0, 0, 0);
    if (clear_scratch != 0) {
        for (i = 0x77ff; i >= 0; i--) {
            g_wldcore_scratch_buffer[i] = 0;
        }
    }
    DrawSync(0);
}
