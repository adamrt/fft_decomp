#include "fft/battle.h"

/*
 * Fill the move-find result record for tile (x, y, layer): flag 4 and the
 * unit id when a unit stands there, then, for a matching move-find tile, its
 * index, flag 1 and the rare/common items when the item is still unfound,
 * and flag 2 with the trap number (0 degenerator, 1 deathtrap, 2 sleeping gas,
 * 3 steel needle) unless the trap is disabled. The final flag
 * update must go through a pointer to keep the target's register address.
 */
battle_move_find_result_data_t* battle_map_set_item_trap_data(u8 x, u8 y, u8 layer) {
    map_move_find_item_entry_t* tile;
    s32 unit;
    s32 i;
    s32 trap;
    u8 position;
    u8* result;

    trap = BATTLE_TRAP_ID_NONE;
    g_battle_move_find_result_flags = 0;
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = battle_unit_find_at_tile(x, y, layer, BATTLE_UNIT_TILE_FILTER_TRANSPARENT);
        if (unit >= 0) {
            break;
        }
    }
    if (i != BATTLE_UNIT_SLOT_COUNT) {
        g_battle_move_find_result_flags = BATTLE_MOVE_FIND_RESULT_OCCUPIED;
        g_battle_move_find_occupying_unit_id = unit;
    }
    for (i = 0; i < 4; i++) {
        tile = &g_battle_current_map_move_find_item_data.entries[i];
        position = tile->position;
        if ((position >> 4) == x && (position & 0xF) == y && (tile->flags >> 7) == layer) {
            break;
        }
    }
    if (i != 4) {
        g_battle_move_find_entry_index = i;
        if (!(tile->flags & MAP_MOVE_FIND_ENTRY_NO_ITEM)) {
            if (battle_map_calculate_move_find_item_flag(battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP), i, 0)
                != 0) {
                g_battle_move_find_result_flags |= BATTLE_MOVE_FIND_RESULT_ITEM;
                g_battle_move_find_rare_item_id = tile->rare_item_id;
                g_battle_move_find_common_item_id = tile->common_item_id;
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
                result = &g_battle_move_find_result_flags;
                g_battle_move_find_trap_id = trap;
                *result |= BATTLE_MOVE_FIND_RESULT_TRAP;
            }
        }
    }
    return (battle_move_find_result_data_t*)&g_battle_move_find_result_flags;
}
