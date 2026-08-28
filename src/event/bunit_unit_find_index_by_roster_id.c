#include "fft/bunit.h"

/* Find the BUNIT record with a roster ID, or return the active record count.
 *
 * The not-found result doubles as the next/end index for both known callers. */
s32 bunit_unit_find_index_by_roster_id(s16 roster_id) {
    s32 stack_scratch[2]; /* Unused; the target keeps an 8-byte leaf frame. */
    s32 count;
    s32 i;
    s32 search_value;
    bunit_unit_data_t** unit_data;

    count = g_bunit_unit_count;
    i = 0;
    if (count > 0) {
        search_value = roster_id;
        unit_data = g_bunit_unit_data;
        do {
            if ((*unit_data)->roster_id == search_value) {
                return i;
            }
            i++;
            unit_data++;
        } while (i < count);
    }
    return i;
}
