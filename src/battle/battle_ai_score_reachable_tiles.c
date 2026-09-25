#include "fft/battle.h"

/*
 * Score each reachable tile, resuming suspended evaluations.
 *
 * Return -1 to suspend and 0 after restoring the simulation coordinates.
 * Integer address stages preserve the target's addition order; direct typed
 * indexing changes the machine code. Each level has 18 rows of 16 tiles.
 */
s32 battle_ai_score_reachable_tiles(void) {
    s32 x, y, level;
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    u32 row;

    if (g_battle_ai_data_base.decision_state != 0) {
        x = g_battle_ai_data_base.current_action.coords.bytes.x;
        y = g_battle_ai_data_base.current_action.coords.bytes.y;
        level = g_battle_ai_data_base.current_action.coords.bytes.elevation;
        {
            u32 scenario_address = g_battle_ai_data_base.movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
            u32 level_address;
            u32 row_address;
            scenario_address += (u32)&ai->reachable_tiles - (u32)ai;
            level_address = level * 36 + scenario_address;
            row_address = y * 2 + level_address;
            row = *(u16*)row_address;
        }
        /* This tile was already set up before the interrupted evaluation. */
        goto evaluate_tile;
    }
    level = 0;
    g_battle_ai_data_base.current_action.target_flags_set = 0;
    g_battle_ai_data_base.current_action.skillset = 0;
    for (; level < 2; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            {
                u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
                u32 level_address;
                u32 row_address;
                scenario_address += (u32)&ai->reachable_tiles - (u32)ai;
                level_address = level * 36 + scenario_address;
                row_address = y * 2 + level_address;
                row = *(u16*)row_address;
            }
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((row << x) & 0x8000) {
                        ai->current_action.coords.bytes.x = x;
                        ai->current_action.coords.bytes.y = y;
                        ai->current_action.coords.bytes.elevation = level;
                        battle_ai_check_unit_for_crystal_or_treasure_status();
                    evaluate_tile:
                        if (battle_ai_simulate_action_and_score() == -1) {
                            /* Keep the current coordinates and uncommitted score. */
                            return -1;
                        }
                        {
                            u32 level_address = level * 576 + (u32)ai;
                            u32 row_address;
                            u32 tile_address;
                            level_address += (u32)&ai->tile_priority - (u32)ai;
                            row_address = y * 32 + level_address;
                            tile_address = x * 2 + row_address;
                            *(u16*)tile_address = -ai->current_action.priority;
                        }
                    }
                }
            }
        }
    }
    /* Restore the simulation unit's coordinates only after the whole scan completes. */
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    return 0;
}
