#include "fft/event_bunit.h"

/* Find the BUNIT record with a battle id, or return the active record count.
 *
 * The not-found result doubles as the next/end index for both known callers. */
s32 bunit_unit_find_index_by_battle_id(s16 battle_id) {
    s32 stack_scratch[2]; /* Unused; the target keeps an 8-byte leaf frame. */
    s32 count;
    s32 i;
    s32 search_value;
    bunit_unit_t** units;

    count = g_bunit_unit_count;
    i = 0;
    if (count > 0) {
        search_value = battle_id;
        units = g_bunit_unit_pointers;
        do {
            if ((*units)->battle_id == search_value) {
                return i;
            }
            i++;
            units++;
        } while (i < count);
    }
    return i;
}
