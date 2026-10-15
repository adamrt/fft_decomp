#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_status.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

u32 move_set_target_for_mounted_unit_and_find_item(unit_stats_t* unit) {
    unit_stats_t* target;
    u32 flags;
    s32 i;
    s32 find_item;
    move_find_result_data_t* tile;

    /* Synthesize CRYSTAL_OR_TREASURE when the filtered tile search returns a
     * non-negative unit ID. The explicit sign test keeps the target nor/srl. */
    flags = (~battle_unit_find_at_tile(unit->x, unit->position.y, unit->position.higher_elevation,
                 BATTLE_UNIT_TILE_FILTER_CRYSTAL | BATTLE_UNIT_TILE_FILTER_TREASURE)
                >> 31)
        & 1;
    if (!(unit->mount_info & BATTLE_MOUNT_INFO_FLAG_RIDER)) {
        target = unit;
    } else {
        target = &g_battle_unit_stats[unit->mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
        g_action_target = target;
        g_action_target_data = &target->action;
        action_clear_current_data(&target->action);
        for (i = 0; i < STATUS_BYTE_COUNT; i++) {
            g_action_target_data->status_changes.removal[i]
                = g_main_status_check_sets[MAIN_STATUS_CHECK_SET_MOUNT_REMOVAL][i];
        }
        if (status_modify_inflictions(0) != 0) {
            g_action_target_data->attack_type = ACTION_TYPE_STATUS_CHANGE;
            status_resolve_unit_changes(target->battle_id, 0);
            flags |= MOVE_POST_EVENT_MOUNT_STATUS_CHANGE;
        }
    }
    find_item = target->ability_flags.movement[2] & BATTLE_MOVEMENT_SET_3_MOVE_FIND_ITEM;
    tile = map_set_item_trap_data(unit->x, unit->position.y, unit->position.higher_elevation);
    if (find_item && (tile->flags & MOVE_FIND_RESULT_ITEM)) {
        flags |= MOVE_POST_EVENT_ITEM_FOUND;
    } else if (tile->flags & MOVE_FIND_RESULT_TRAP) {
        flags |= MOVE_POST_EVENT_TRAP_TRIGGERED;
    }
    if (status_remove_charging_ability_ct(unit, 0) != 0) {
        flags |= MOVE_POST_EVENT_CHARGING_CANCEL;
    }
    flags |= move_get_support_flags(unit);
    return flags;
}
