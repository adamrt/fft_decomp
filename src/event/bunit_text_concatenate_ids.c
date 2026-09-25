#include "fft/event_bunit.h"
#include "psx/types.h"

/* Concatenate the text for each entry of a -1 terminated id list into `out`,
 * optionally separating entries with 0xF8, and terminate with 0xFE. */
void bunit_text_concatenate_ids(s32 text_table, u8* out, s16* list, s32 separate) {
    s16* ids;
    u8* src;

    if (*list != -1) {
        ids = list;
        do {
            src = ((u8 * (*)(s32, s32, s32)) bunit_text_skip_encoded_segments)(text_table, *ids & 0x7FF, 1);
            while (*src != 0xFE) {
                *out = *src;
                src++;
                out++;
            }
            ids++;
            if (separate != 0) {
                *out = 0xF8;
                out++;
            }
        } while (*ids != -1);
    }
    if (separate != 0) {
        out--;
    }
    *out = 0xFE;
}
