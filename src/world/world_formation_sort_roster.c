#include "fft/world.h"

/*
 * Sort the formation roster order by the key list selected by `index`,
 * then move eggs to the end in descending record-index order
 * and rebuild the unit pointer table.
 *
 * `rec` is reused for both records so that only the first record's 0x128
 * multiply leaves the key loop, as in the target. The second key value
 * shares `swap` with the egg loop and `lhs` doubles as the exchanged
 * order entry: both must be multi-set variables for the target's register
 * choice (a1/v0) and load order.
 */
void world_formation_sort_roster(s32 index) {
    s8 egg_indices[PARTY_ROSTER_SLOT_COUNT];
    s8* keys;
    s32 i;
    s32 j;
    s32 k;
    s32 count;
    s32 diff;
    s32 first;
    s32 swap;
    s32 lhs;
    s16* rec; /* Sort keys select signed 16-bit values within the unit record. */

    keys = g_item_type_order_lists[index];
    for (count = g_world_formation_record_count - 1; count > 0; count--) {
        for (i = 0; i < count; i++) {
            k = 0;
            do {
                rec = (s16*)&g_world_unit_records[g_world_formation_record_order[i]];
                lhs = rec[g_world_formation_sort_field_indices[keys[k]]];
                rec = (s16*)&g_world_unit_records[g_world_formation_record_order[i + 1]];
                swap = rec[g_world_formation_sort_field_indices[keys[k]]];
                diff = swap - lhs;
                k++;
            } while (diff == 0);
            if (keys[k - 1] != 0) {
                diff = -diff;
            }
            if (diff < 0) {
                diff = g_world_formation_record_order[i];
                lhs = (u16)g_world_formation_record_order[i + 1];
                g_world_formation_record_order[i] = lhs;
                g_world_formation_record_order[i + 1] = diff;
            }
        }
    }
    count = 0;
    for (i = 0; i < g_world_formation_record_count; i++) {
        if (g_world_unit_records[g_world_formation_record_order[i]].gender_flags & UNIT_FLAG_EGG) {
            egg_indices[count++] = g_world_formation_record_order[i];
        }
    }
    for (i = 0; i < count; i++) {
        j = 0;
        swap = egg_indices[i];
        while (1) {
            if (g_world_formation_record_order[j] == swap) {
                break;
            }
            j++;
        }
        for (; j < g_world_formation_record_count - 1; j++) {
            g_world_formation_record_order[j] = g_world_formation_record_order[j + 1];
        }
        g_world_formation_record_order[g_world_formation_record_count - 1] = egg_indices[i];
    }
    for (i = count - 1; i > 0; i--) {
        for (j = 0; j < i; j++) {
            if (egg_indices[j + 1] < egg_indices[j]) {
                first = egg_indices[j];
                egg_indices[j] = egg_indices[j + 1];
                egg_indices[j + 1] = first;
            }
        }
    }
    for (i = 0; i < count; i++) {
        g_world_formation_record_order[g_world_formation_record_count - 1 - i] = egg_indices[i];
    }
    for (i = 0; i < g_world_formation_record_count; i++) {
        g_world_unit_pointers[i] = &g_world_unit_records[g_world_formation_record_order[i]];
    }
    g_world_unit_pointers[WORLD_FORMATION_PREVIEW_UNIT_INDEX]
        = &g_world_unit_records[WORLD_FORMATION_PREVIEW_UNIT_INDEX];
}
