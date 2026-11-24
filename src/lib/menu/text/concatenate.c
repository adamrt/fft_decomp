/* Concatenating the text of a list of entry ids. */

#include "fft/menu/menu_text.h"
#include "fft/text/text.h"
#include "psx/types.h"

/* Concatenate the text for each entry of a -1 terminated id list into `out`,
 * optionally separating entries with 0xF8, and terminate with 0xFE. */
void text_concatenate_entries(const u8* text_table, u8* out, const s16* text_ids, s32 separate) {
    const s16* ids;
    const u8* src;

    if (*text_ids != -1) {
        ids = text_ids;
        do {
            src = menu_text_find_entry_by_index(text_table, *ids & TEXT_ID_ENTRY_MASK, 1);
            while (*src != TEXT_END_WAIT_FOR_CONFIRM) {
                *out = *src;
                src++;
                out++;
            }
            ids++;
            if (separate != 0) {
                *out = TEXT_NEWLINE;
                out++;
            }
        } while (*ids != -1);
    }
    /* An empty id list with separators still backs up before the output buffer. */
    if (separate != 0) {
        out--;
    }
    *out = TEXT_END_WAIT_FOR_CONFIRM;
}
