#include "psx/types.h"

/* Count the characters of a 0xFE-terminated text string; bytes 0xD0..0xDF
 * carry a one-byte operand. */
s32 world_text_count_characters(u8* text) {
    s32 count = 0;
    while (*text != 0xFE) {
        if ((u8)(*text + 0x30) < 0x10) {
            text += 2;
        } else {
            text += 1;
        }
        count += 1;
    }
    return count;
}
