#include "fft/battle_ai.h"

/*
 * Take the lowest-key remaining movement origin for target evaluation.
 *
 * Rank by signed tile priority, then foe proximity; later tiles win ties.
 * Requires a consistent nonempty movable mask/count; there is no empty guard.
 */
void battle_ai_take_next_movement_origin(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 best_key = 0x7fffffff;
    s32 x, y, level;
    s32 key;
    u16 row;
    u16* movable_rows;
    u32 mask_level_address;

    for (level = 0; level < 2; level++) {
        for (y = 0; y < ai->map_max_y; y++) {
            /* Typed indexing changes the mask-base instruction order. */
            mask_level_address = level * (18 * sizeof(u16)) + (u32)ai;
            movable_rows = (u16*)(mask_level_address + ((u32)&ai->movable_tiles - (u32)ai));
            row = movable_rows[y];
            if (row != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((row << x) & 0x8000) {
                        s32 priority_scale = 65536;
                        key = (s16)ai->tile_priority[level][y][x];
                        key *= priority_scale;
                        key += ai->tile_foe_proximity[level][y][x];
                        if (best_key >= key) {
                            best_key = key;
                            ai->current_action.coords.bytes.x = x;
                            ai->current_action.coords.bytes.y = y;
                            ai->current_action.coords.bytes.elevation = level;
                        }
                    }
                }
            }
        }
    }
    {
        /* Typed indexing folds the field offset before the selected level. */
        u32 selected_level_address = ai->current_action.coords.bytes.elevation * (18 * sizeof(u16)) + (u32)ai;
        movable_rows = (u16*)(selected_level_address + ((u32)&ai->movable_tiles - (u32)ai));
        movable_rows[ai->current_action.coords.bytes.y] ^= 0x8000 >> ai->current_action.coords.bytes.x;
    }
    ai->movable_tile_count--;
}
