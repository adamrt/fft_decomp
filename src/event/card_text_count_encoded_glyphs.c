#include "psx/types.h"

s32 card_text_count_encoded_glyphs(const u8* data) {
    s32 count = 0;

    if (*data != 0xfe) {
        do {
            if ((u8)(*data + 0x30) < 0x10) {
                data += 2;
            } else {
                data++;
            }
            count++;
        } while (*data != 0xfe);
    }
    return count;
}
