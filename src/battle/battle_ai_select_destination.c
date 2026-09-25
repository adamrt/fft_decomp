#include "fft/battle.h"

/*
 * Select a reachable destination using priority-first or distance-first ordering.
 *
 * Return 1 after selecting a tile, or 0 without changing the destination.
 * Both orders favor the last scanned tile when their two ranking values tie.
 * Integer address stages preserve the target's shared grid-index calculation;
 * direct multidimensional indexing changes the loop code. Strides and field
 * offsets come from the workspace layout, with 16 tiles in each row.
 */
s32 battle_ai_select_destination(s32 max_distance, battle_ai_destination_order_e order) {
    s32 x, y, level;
    s32 best_score = 0x7fffffff;
    s32 best_distance = 0x7fffffff;
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_ai_coords_t* destination = &ai->candidate_coords[g_battle_ai_data_base.movement_scenario];
    u16 row;
    s32 distance, score;

    for (level = 0; level < 2; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            {
                u32 scenario_address = ai->movement_scenario * sizeof(ai->reachable_tiles[0]) + (u32)ai;
                u32 level_address;
                scenario_address += (u32)&ai->reachable_tiles - (u32)ai;
                level_address = level * sizeof(ai->reachable_tiles[0][0]) + scenario_address;
                row = *(u16*)(y * sizeof(ai->reachable_tiles[0][0][0]) + level_address);
            }
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((row << x) & 0x8000) {
                        u32 level_tile_index = y << 4;
                        u32 tile_index;
                        u32 byte_address;
                        level_tile_index += x;
                        tile_index = level * sizeof(ai->tile_target_distance[0]) + level_tile_index;
                        byte_address = tile_index + (u32)ai;
                        distance = *(u8*)(byte_address + ((u32)&ai->tile_target_distance - (u32)ai));
                        if (max_distance < distance) {
                            continue;
                        }
                        {
                            s32 proximity = *(u8*)(byte_address + ((u32)&ai->tile_foe_proximity - (u32)ai));
                            score = *(u16*)(tile_index * sizeof(ai->tile_priority[0][0][0]) + (u32)ai
                                + ((u32)&ai->tile_priority - (u32)ai));
                            /* Compare the packed priority and proximity as a signed score. */
                            score = (u32)score << 16;
                            score += proximity;
                        }
                        if (order != BATTLE_AI_DESTINATION_PRIORITY_FIRST) {
                            if (max_distance == distance && best_score < score) {
                                continue;
                            }
                            max_distance = distance;
                        } else {
                            if (best_score < score) {
                                continue;
                            }
                            if (score == best_score && best_distance < distance) {
                                continue;
                            }
                        }
                        best_score = score;
                        best_distance = distance;
                        destination->bytes.x = x;
                        destination->bytes.y = y;
                        destination->bytes.elevation = level;
                        destination->bytes.zero = 0;
                    }
                }
            }
        }
    }
    return best_score != 0x7fffffff;
}
