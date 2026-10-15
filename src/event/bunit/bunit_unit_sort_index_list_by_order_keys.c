#include "fft/event_bunit.h"
#include "psx/types.h"

/* Bubble-sort g_bunit_shown_unit_indices (the shown-unit index list) by the
 * successive keys of g_main_item_type_order_tables.order_0, then publish the
 * sorted record pointers into g_bunit_unit_pointers. `sort_mode` is not read.
 * BUNIT twin of world_formation_sort_roster. */
void bunit_unit_sort_index_list_by_order_keys(s32 sort_mode) {
    s32 count;
    s32 j;
    s32 k;
    s32 diff;
    s32 lhs;
    s32 rhs;
    s16* rec; /* Native sort keys select signed 16-bit values within the record. */

    for (count = g_bunit_unit_count - 1; count > 0; count--) {
        for (j = 0; j < count; j++) {
            k = 0;
            do {
                rec = (s16*)&g_bunit_unit_records[g_bunit_shown_unit_indices[j]];
                lhs = rec[g_bunit_unit_sort_field_indices[g_main_item_type_order_tables.order_0[k]]];
                rec = (s16*)&g_bunit_unit_records[g_bunit_shown_unit_indices[j + 1]];
                rhs = rec[g_bunit_unit_sort_field_indices[g_main_item_type_order_tables.order_0[k]]];
                diff = rhs - lhs;
                k++;
            } while (diff == 0);
            if (g_main_item_type_order_tables.order_0[k - 1] == 1
                || g_main_item_type_order_tables.order_0[k - 1] >= 4) {
                diff = -diff;
            }
            if (diff < 0) {
                s32 swap = g_bunit_shown_unit_indices[j];
                g_bunit_shown_unit_indices[j] = g_bunit_shown_unit_indices[j + 1];
                g_bunit_shown_unit_indices[j + 1] = swap;
            }
        }
    }
    for (j = 0; j < g_bunit_unit_count; j++) {
        g_bunit_unit_pointers[j] = &g_bunit_unit_records[g_bunit_shown_unit_indices[j]];
    }
}
