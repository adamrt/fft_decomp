#include "fft/menu.h"
#include "psx/types.h"

/* Concatenate the text for each entry of a -1 terminated id list into `out`,
 * optionally separating entries with 0xF8, and terminate with 0xFE. */
void text_concatenate_entries(const u8* text_table, u8* out, s16* list, s32 separate) {
    s16* ids;
    const u8* src;

    if (*list != -1) {
        ids = list;
        do {
            src = menu_text_find_entry_by_index(text_table, *ids & 0x7FF, 1);
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
