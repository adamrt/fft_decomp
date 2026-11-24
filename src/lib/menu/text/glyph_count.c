/* Counting the characters of an encoded text string. */

#include "fft/text/text.h"
#include "psx/types.h"

/* Count the characters of a 0xFE-terminated text string; bytes 0xD0..0xDF
 * carry a one-byte operand. */
s32 text_count_encoded_glyphs(const u8* data) {
    s32 count = 0;

    while (*data != TEXT_END_WAIT_FOR_CONFIRM) {
        if (*data >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST && *data <= TEXT_EXTENDED_GLYPH_PREFIX_LAST) {
            data += 2;
        } else {
            data++;
        }
        count++;
    }
    return count;
}
