#include "fft/event_bunit.h"
#include "psx/types.h"

/* Bubble-sort g_bunit_shown_unit_indices (the shown-unit index list) by the
 * successive keys of g_main_item_type_order_tables.order_0, then publish the
 * sorted record pointers into g_bunit_unit_data. `sort_mode` is not read. */
void bunit_unit_sort_index_list_by_order_keys(s32 sort_mode) {
    s32 count;
    s32 remaining;
    s32 j;
    s32 k;
    s32 diff;
    s16* left;
    s16* right;
    u8* records;
    s16* indices;
    s16* next_indices;
    u32 stack_padding[2]; /* unreferenced; reserves the target's 16-byte frame */

    count = g_bunit_unit_count;
    remaining = count - 1;
    if (remaining > 0) {
        records = g_bunit_unit_records;
        indices = g_bunit_shown_unit_indices;
        next_indices = indices + 1;
        do {
            j = 0;
            if (remaining > 0) {
                right = next_indices;
                left = indices;
                do {
                    s32 left_index = *left;
                    /* Pin: unpinned, loop hoists `right_index * 0x10C` out of the key loop,
                     * which the target recomputes on every pass. */
                    register s32 right_index __asm__("$6") = *right;
                    s32 record_offset = left_index * 0x10C;

                    k = 0;
                    do {
                        s32 key_offset = g_bunit_unit_sort_field_indices[g_main_item_type_order_tables.order_0[k]] * 2;
                        s32 base = record_offset + (s32)records;
                        s16* left_key = (s16*)(key_offset + base);
                        s16* right_key;
                        s32 left_value;
                        s32 right_value;

                        /* Reusing `base` for both keys keeps the target's register for the sum. */
                        base = right_index * 0x10C + (s32)records;
                        right_key = (s16*)(key_offset + base);

                        left_value = *left_key;
                        right_value = *right_key;
                        diff = right_value - left_value;
                        k++;
                    } while (diff == 0);
                    if (g_main_item_type_order_tables.order_0[k - 1] == 1
                        || g_main_item_type_order_tables.order_0[k - 1] >= 4) {
                        diff = -diff;
                    }
                    j++;
                    if (diff < 0) {
                        s32 swap = *left;
                        *left = *right;
                        *right = swap;
                    }
                    right++;
                    left++;
                } while (j < remaining);
            }
            remaining--;
        } while (remaining > 0);
    }
    count = g_bunit_unit_count;
    j = 0;
    if (j < count) {
        u8* source = g_bunit_unit_records;
        s32 total = count;
        bunit_unit_data_t** out = g_bunit_unit_data;
        s16* index_list = g_bunit_shown_unit_indices;

        do {
            *out = (bunit_unit_data_t*)(*index_list * 0x10C + (s32)source);
            index_list++;
            j++;
            out++;
        } while (j < total);
    }
}
