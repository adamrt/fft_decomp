#include "fft/world.h"
#include "psx/types.h"

/* Table of 0x8018d7ac signed-byte records; entry -1 ends a record. */

/* Widen the signed-byte record selected by `index` into halfwords, storing the
 * -1 terminator as well.
 *
 * The explicit `terminator` local is required: the target materialises the
 * sentinel before copying the record pointer into the loop induction
 * variable, and a literal -1 in the loop condition reverses those two
 * instructions. */
void world_menu_widen_byte_record_to_halfwords(s32 index, s16* out) {
    s8* source;
    s32 terminator;
    s8* entry;

    source = g_world_sort_key_lists[index];
    if ((*out = source[0]) != -1) {
        terminator = -1;
        entry = source;
        do {
            entry++;
            out++;
        } while ((*out = *entry) != terminator);
    }
}
