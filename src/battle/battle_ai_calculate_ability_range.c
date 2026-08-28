#include "fft/battle.h"
#include "fft/battle_ai.h"
#include "fft/map.h"

/* Intersect attack-origin tiles with the current ability's range.
 *
 * The acting unit's coordinates are restored before every return. */
s32 battle_ai_calculate_ability_range(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    map_tile_t* tile;
    s32 count;
    s32 level;
    s32 y;
    s32 x;
    u16* target_rows;
    u16* origin_rows;
    u16 row;
    s32 tile_mask;
    u32 level_address;
    char unused[24];

    battle_ai_move_temp_unit_to_coords(&ai->current_action.coords);
    count = 0;
    if (ai->considered_ability.parameters.bytes.range != 0) {
        if (battle_target_set_panels_for_action(&ai->current_action.unit_id) == 3) {
            battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
            return 0;
        }
    } else {
        x = 0;
        tile = g_battle_map_tile_data;
        do {
            x++;
            tile->ceiling_depth_and_marks &= ~MAP_TILE_FLAG_ABILITY_RANGE;
            tile++;
        } while (x < 512);
        g_battle_map_tile_data[ai->current_action.coords.bytes.y * ai->map_max_x + ai->current_action.coords.bytes.x
            + (ai->current_action.coords.bytes.elevation << 8)]
            .ceiling_depth_and_marks |= MAP_TILE_FLAG_ABILITY_RANGE;
    }

    level = 0;
    for (level_address = (u32)ai; level < 2; level++, level_address += 18 * sizeof(u16)) {
        for (y = 0; y < ai->map_max_y; y++) {
            target_rows = (u16*)(level_address + ((u32)&ai->targetable_tiles - (u32)ai));
            origin_rows = (u16*)(level_address + ((u32)&ai->attack_origin_tiles - (u32)ai));
            target_rows[y] = 0;
            row = origin_rows[y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    tile_mask = row << x;
                    if (tile_mask & 0x8000) {
                        if ((g_battle_map_tile_data[y * ai->map_max_x + x + (level << 8)].ceiling_depth_and_marks
                                >> MAP_TILE_FLAG_ABILITY_RANGE_SHIFT)
                            & 1) {
                            count++;
                            tile_mask = 0x8000;
                            target_rows[y] |= tile_mask >> x;
                        }
                    }
                }
            }
        }
    }
    battle_ai_move_temp_unit_to_coords(&ai->acting_unit_coords);
    return count;
}
