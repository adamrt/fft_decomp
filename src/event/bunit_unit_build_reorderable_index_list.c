#include "fft/bunit.h"

/* Build the unit-index list used by the formation reorder menu.
 *
 * Entries tagged with bit 0x40 in their formation-order key are resolved separately
 * through their encoded roster ID, so they are omitted from this direct list.
 */
s32 bunit_unit_build_reorderable_index_list(void) {
    s32 count;
    s32 i;

    count = 0;
    for (i = 0; i < g_bunit_unit_list_count; i++) {
        if ((g_bunit_unit_data[i]->formation_order_key & BUNIT_FORMATION_ORDER_KEY_ENCODED_ROSTER_ID) == 0) {
            g_bunit_unit_reorderable_indices[count] = i;
            count++;
        }
    }
    return count;
}
