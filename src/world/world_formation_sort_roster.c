#include "fft/data.h"
#include "fft/world.h"

/* Halfword view of a formation record; sort keys select a halfword field. */
typedef struct world_formation_unit_halfwords {
    s16 fields[sizeof(world_formation_unit_t) / 2];
} world_formation_unit_halfwords_t;

/*
 * Sort the formation roster order by the key list selected by `index`,
 * then move guest units (gender flag bit 2) to the end in ascending order
 * and rebuild the unit pointer table.
 *
 * `rec` is reused for both records so that only the first record's 0x128
 * multiply leaves the key loop, as in the target. The second key value
 * shares `swap` with the guest loop and `lhs` doubles as the exchanged
 * order entry: both must be multi-set variables for the target's register
 * choice (a1/v0) and load order.
 */
void world_formation_sort_roster(s32 index) {
    s8 guests[24];
    s8* keys;
    s32 i;
    s32 j;
    s32 k;
    s32 count;
    s32 diff;
    s32 first;
    s32 swap;
    s32 lhs;
    world_formation_unit_halfwords_t* rec;

    keys = g_world_sort_key_lists[index];
    for (count = g_world_formation_record_count - 1; count > 0; count--) {
        for (i = 0; i < count; i++) {
            k = 0;
            do {
                rec = (world_formation_unit_halfwords_t*)&g_world_formation_unit_records
                    [g_world_formation_record_order[i]];
                lhs = rec->fields[g_world_formation_sort_field_indices[keys[k]]];
                rec = (world_formation_unit_halfwords_t*)&g_world_formation_unit_records
                    [g_world_formation_record_order[i + 1]];
                swap = rec->fields[g_world_formation_sort_field_indices[keys[k]]];
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
        if (g_world_formation_unit_records[g_world_formation_record_order[i]].gender_flags & 4) {
            guests[count++] = g_world_formation_record_order[i];
        }
    }
    for (i = 0; i < count; i++) {
        j = 0;
        swap = guests[i];
        while (1) {
            if (g_world_formation_record_order[j] == swap) {
                break;
            }
            j++;
        }
        for (; j < g_world_formation_record_count - 1; j++) {
            g_world_formation_record_order[j] = g_world_formation_record_order[j + 1];
        }
        g_world_formation_record_order[g_world_formation_record_count - 1] = guests[i];
    }
    for (i = count - 1; i > 0; i--) {
        for (j = 0; j < i; j++) {
            if (guests[j + 1] < guests[j]) {
                first = guests[j];
                guests[j] = guests[j + 1];
                guests[j + 1] = first;
            }
        }
    }
    for (i = 0; i < count; i++) {
        g_world_formation_record_order[g_world_formation_record_count - 1 - i] = guests[i];
    }
    for (i = 0; i < g_world_formation_record_count; i++) {
        g_world_formation_unit_pointers[i] = &g_world_formation_unit_records[g_world_formation_record_order[i]];
    }
    g_world_formation_unit_pointers[20] = &g_world_formation_unit_records[20];
}
