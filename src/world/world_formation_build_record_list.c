#include "fft/main_unit.h"
#include "fft/world.h"
#include "psx/types.h"

/*
 * Rebuilds the compacted WORLD formation records from the 20 roster slots.
 *
 * Mode 1 drops units with a proposition in progress and the egg-unit flag, a
 * monster skillset, or a special-monster job; mode 2 drops gender flag 0x4
 * and any unit with the top two gender bits set. A nonzero item id keeps only
 * units with that item equipped. The list argument is unused here; the sorter
 * at 0x80121c60 fills g_world_formation_unit_pointers.
 */
s16 world_formation_build_record_list(s32 item_id, world_formation_unit_t** list, s32 mode) {
    battle_stats_t unit;
    s32 slot;
    s32 count;
    s32 id;
    s32 found;
    s32 i;

    slot = 0;
    count = 0;
    id = item_id & WORLD_ITEM_ID_MASK;
    for (; slot < PARTY_ROSTER_SLOT_COUNT; slot++) {
        if (main_unit_init_job_data(&unit, slot, 1) != 0) {
            continue;
        }
        world_formation_build_unit_record(
            &unit, &g_world_formation_unit_records[count], main_party_get_data_pointer(slot));
        if (mode == 1) {
            if (g_world_formation_unit_records[count].proposition_status != 0) {
                continue;
            }
            if (g_world_formation_unit_records[count].gender_flags & UNIT_FLAG_EGG) {
                continue;
            }
            if (g_world_formation_unit_records[count].uses_monster_skillset != 0) {
                continue;
            }
            if (world_job_is_special_monster(g_world_formation_unit_records[count].job_id) != 0) {
                continue;
            }
        } else if (mode == 2) {
            if (g_world_formation_unit_records[count].gender_flags & UNIT_FLAG_EGG) {
                continue;
            }
            if ((g_world_formation_unit_records[count].gender_flags >> 6) != 0) {
                continue;
            }
        }
        if (id > 0) {
            found = 0;
            for (i = 0; i < 5; i++) {
                if (g_world_formation_unit_records[count].equipment[i] == id) {
                    found = 1;
                }
            }
            if (found == 0) {
                continue;
            }
        }
        g_world_formation_record_order[count] = count;
        count++;
    }
    g_world_formation_record_count = count;
    world_formation_sort_roster(0);
    return g_world_formation_record_count;
}
