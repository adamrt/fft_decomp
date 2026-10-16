/* Counting the characters of an encoded text string. */

#include "psx/types.h"

/* Count the characters of a 0xFE-terminated text string; bytes 0xD0..0xDF
 * carry a one-byte operand. */
s32 text_count_encoded_glyphs(const u8* data) {
    s32 count = 0;

    if (*data != 0xfe) {
        do {
            if (*data >= 0xd0 && *data <= 0xdf) {
                data += 2;
            } else {
                data++;
            }
            count++;
        } while (*data != 0xfe);
    }
    return count;
}
