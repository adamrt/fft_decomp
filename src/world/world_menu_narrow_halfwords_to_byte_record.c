#include "fft/world.h"
#include "psx/types.h"

/* Table of 0x8018d7ac signed-byte records; entry -1 ends a record. */

/* Narrow the halfword list `source` into the signed-byte record selected by
 * `index`, storing the -1 terminator as well. Inverse of
 * world_menu_widen_byte_record_to_halfwords. */
void world_menu_narrow_halfwords_to_byte_record(s32 index, s16* source) {
    s8* out;
    s32 terminator;
    s8* entry;

    out = g_world_sort_key_lists[index];
    if ((*out = *source) != -1) {
        terminator = -1;
        entry = out;
        do {
            source++;
            entry++;
        } while ((*entry = *source) != terminator);
    }
}
