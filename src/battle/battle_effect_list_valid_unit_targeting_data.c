#include "fft/battle.h"
#include "psx/types.h"

/*
 * List every present unit other than `first` and `second` that is neither
 * flagged by current-status bit 0x04 nor mount bit 0x40, with its tile
 * position and sprite height, into a scratchpad record returned via `out`.
 */
void battle_effect_list_valid_unit_targeting_data(s32 first, s32 second, battle_effect_obstacle_unit_list_t** out) {
    SVECTOR position;
    battle_stats_t* unit;
    s32 i;
    s32 slot;

    *out = (battle_effect_obstacle_unit_list_t*)0x1F800000;
    ((battle_effect_obstacle_unit_list_t*)0x1F800000)->count = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_get_stats_from_battle_id(i);
        if (unit->entd_slot != first && unit->entd_slot != second && unit->entd_slot != BATTLE_ENTD_SLOT_NONE
            && !(unit->status_sets.current[0] & BATTLE_STATUS_BYTE_MASK(BATTLE_STATUS_ID_JUMP))
            && !(unit->mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
            slot = (*out)->count++;
            (*out)->battle_ids[slot] = unit->entd_slot;
            battle_unit_get_tile_center_and_height_by_battle_id(unit->entd_slot, (battle_screen_coords_t*)&position);
            (*out)->positions[slot] = position;
            (*out)->sprite_heights[slot] = battle_gfx_get_unit_spritesheet_height_by_unit_id(unit->entd_slot);
        }
    }
}
