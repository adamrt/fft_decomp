#include "fft/battle_text.h"
#include "psx/types.h"

const u8* card_text_skip_encoded_segments(const u8* data, s16 count) {
    if (count != 0) {
        do {
            u32 raw = *data++;
            u32 value = raw & 0xff;

            if (value >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                if (value <= TEXT_EXTENDED_GLYPH_PREFIX_LAST) {
                    data++;
                } else if (value >= TEXT_END_WAIT_FOR_CONFIRM) {
                    count--;
                }
            }
        } while (count != 0);
    }
    return data;
}
