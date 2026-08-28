#include "fft/text.h"
#include "psx/types.h"

/* Advance past `count` line terminators in a shift-JIS style byte stream. */
u8* world_text_find_entry_by_index(u8* text, s16 count, s32 unused) {
    u8 code;

    while (count != 0) {
        code = *text++;
        if (code >= 0xD0) {
            if (code < TEXT_FORMAT_FIRST) {
                text++;
            } else if (code >= 0xFE) {
                count--;
            }
        }
    }
    return text;
}
