#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_script.h"
#include "fft/battle_status.h"
#include "fft/main.h"
#include "fft/script.h"
#include "psx/types.h"

/*
 * Build the end-of-move event flags for a unit.
 *
 * A mounted rider targets its mount and clears the mount's status set 9;
 * the tile's Move-Find Item entry then reports a found item (Move-Find Item
 * holders only) or a trap.
 */
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

/* Post-move benefits of the unit's movement abilities: Move-HP Up, Move-MP Up,
 * Move-Get EXP and Move-Get JP, each with the common benefit bit. */
s32 move_get_support_flags(const unit_stats_t* unit) {
    u8 flags = unit->ability_flags.movement[1];
    s32 post_event_flags = (0 - (unit->ability_flags.movement[0] & BATTLE_MOVEMENT_SET_1_MOVE_HP_UP))
        & (MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_HP_UP);
    if (flags & BATTLE_MOVEMENT_SET_2_MOVE_MP_UP)
        post_event_flags |= MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_MP_UP;
    if (flags & BATTLE_MOVEMENT_SET_2_MOVE_GET_EXP)
        post_event_flags |= MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_GET_EXP;
    if (flags & BATTLE_MOVEMENT_SET_2_MOVE_GET_JP)
        post_event_flags |= MOVE_POST_EVENT_MOVEMENT_BENEFIT | MOVE_POST_EVENT_MOVE_GET_JP;
    return post_event_flags;
}

/* 1 when the unit, or its rider when it is a mount, is charging an attack of
 * the Charge skillset; with do_disable, status_disable_acting is called
 * on that unit. */
s32 status_remove_charging_ability_ct(unit_stats_t* unit, s32 do_disable) {
    u8 mount_info;

    for (;;) {
        if ((unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_CHARGING)] & STATUS_BYTE_MASK(STATUS_ID_CHARGING))
            && unit->charged_ability_ct != 0xFF && unit->action_command.skillset_id == SKILLSET_ID_CHARGE) {
            if (do_disable != 0) {
                status_disable_acting(unit);
            }
            return 1;
        }
        mount_info = unit->mount_info;
        if (!(mount_info & BATTLE_MOUNT_INFO_FLAG_MOUNT)) {
            break;
        }
        unit = &g_battle_unit_stats[mount_info & BATTLE_MOUNT_INFO_PARTNER_ID_MASK];
    }
    return 0;
}

/* Clears the unit's Charging, Jump, Defending and Performing statuses (action state 0). */
void status_disable_acting(unit_stats_t* unit) {
    main_status_set_action_state(unit, 0);
}

/* Pick the rare or common move-find item on the unit's tile and mark the tile found.
 *
 * The rare item is chosen with a (100 - Brave)% chance; the tile is the
 * unit's x and position (y in the low byte, the layer in bit 15). */
move_found_item_t* map_determine_rare_common_item(unit_stats_t* unit) {
    move_find_result_data_t* find_result;
    u8 item_id;

    find_result = map_set_item_trap_data(unit->x, unit->position.y, unit->position.higher_elevation);
    if (main_util_roll_pass_fail(0x64, 0x64 - unit->brave) == 0) {
        item_id = find_result->rare_item_id;
    } else {
        item_id = find_result->common_item_id;
    }
    g_map_rare_item_result.item_id = item_id;
    map_calculate_move_find_item_flag(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), find_result->entry_index, 1);
    g_map_rare_item_result._unknown_00 = 0;
    return &g_map_rare_item_result;
}

/*
 * Fill the move-find result record for tile (x, y, layer): flag 4 and the
 * unit id when a unit stands there, then, for a matching move-find tile, its
 * index, flag 1 and the rare/common items when the item is still unfound,
 * and flag 2 with the trap number (0 degenerator, 1 deathtrap, 2 sleeping gas,
 * 3 steel needle) unless the trap is disabled. The final flag
 * update must go through a pointer to keep the target's register address.
 */
move_find_result_data_t* map_set_item_trap_data(s32 x, u8 y, u8 layer) {
    map_move_find_item_entry_t* tile;
    s32 unit;
    s32 i;
    s32 trap;
    u8* result;

    trap = BATTLE_TRAP_ID_NONE;
    g_move_find_result_flags = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_find_at_tile((u8)x, y, layer, BATTLE_UNIT_TILE_FILTER_TRANSPARENT);
        if (unit >= 0) {
            break;
        }
    }
    if (i != BATTLE_UNIT_SLOT_COUNT) {
        g_move_find_result_flags = MOVE_FIND_RESULT_OCCUPIED;
        g_move_find_occupying_unit_id = unit;
    }
    for (i = 0; i < 4; i++) {
        tile = &g_battle_current_map_move_find_item_data.entries[i];
        if (tile->x == (u8)x && tile->y == y && tile->elevation == layer) {
            break;
        }
    }
    if (i != 4) {
        g_move_find_entry_index = i;
        if (!(tile->flags & MAP_MOVE_FIND_ENTRY_NO_ITEM)) {
            if (map_calculate_move_find_item_flag(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), i, 0) != 0) {
                g_move_find_result_flags |= MOVE_FIND_RESULT_ITEM;
                g_move_find_rare_item_id = tile->rare_item_id;
                g_move_find_common_item_id = tile->common_item_id;
            }
        }
        if (!(tile->flags & MAP_MOVE_FIND_ENTRY_NO_TRAP)) {
            if (tile->flags & MAP_MOVE_FIND_ENTRY_TRAP_DEGENERATOR) {
                trap = BATTLE_TRAP_ID_DEGENERATOR;
            } else if (tile->flags & MAP_MOVE_FIND_ENTRY_TRAP_DEATHTRAP) {
                trap = BATTLE_TRAP_ID_DEATHTRAP;
            } else if (tile->flags & MAP_MOVE_FIND_ENTRY_TRAP_SLEEPING_GAS) {
                trap = BATTLE_TRAP_ID_SLEEPING_GAS;
            } else if (tile->flags & MAP_MOVE_FIND_ENTRY_TRAP_STEEL_NEEDLE) {
                trap = BATTLE_TRAP_ID_STEEL_NEEDLE;
            }
            if (trap != BATTLE_TRAP_ID_NONE) {
                result = &g_move_find_result_flags;
                g_move_find_trap_id = trap;
                *result |= MOVE_FIND_RESULT_TRAP;
            }
        }
    }
    return &g_move_find_tile_result;
}

/* Test (save == 0) or set the found flag of one of a map's four move-find item tiles.
 *
 * Each flag byte holds two maps, four tiles each; the test returns 1 while the item is
 * still unfound. */
s32 map_calculate_move_find_item_flag(s32 map_id, s32 tile_index, s32 save) {
    s32 byte_index;
    s32 bit;

    byte_index = map_id / 2;
    bit = 0x80 >> (((map_id & 1) * 4) + tile_index);
    if (save == 0) {
        return (g_main_item_location_flags[byte_index] & bit) == 0;
    }
    g_main_item_location_flags[byte_index] = bit | g_main_item_location_flags[byte_index];
    return 1;
}
