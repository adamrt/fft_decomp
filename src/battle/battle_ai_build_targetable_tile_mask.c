#include "fft/battle_ai.h"

/*
 * Build the considered origin's targetable-tile mask on both map levels.
 *
 * Modes 0, 1, and 2 select range, area, or their sum; other modes use radius.
 * Radius 16 or greater fills both masks without applying the linear filter.
 * Radii 8 through 15 read the following status-priority weights instead of
 * row templates. This helper does not guard those inputs; caller restrictions
 * are not established.
 */
void battle_ai_build_targetable_tile_mask(battle_ai_tile_mask_mode_e mode, s32 radius) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    s32 i;
    s32 x;
    s32 y;

    /* Preserve the target's signed mode dispatch. */
    switch ((s32)mode) {
    case BATTLE_AI_TILE_MASK_RANGE:
        radius = g_battle_ai_data_base.considered_ability.parameters.bytes.range;
        break;
    case BATTLE_AI_TILE_MASK_AOE:
        radius = g_battle_ai_data_base.considered_ability.parameters.bytes.aoe;
        break;
    case BATTLE_AI_TILE_MASK_RANGE_PLUS_AOE:
        radius = g_battle_ai_data_base.considered_ability.parameters.bytes.range
            + g_battle_ai_data_base.considered_ability.parameters.bytes.aoe;
        break;
    }
    if (radius >= 16) {
        for (i = 0; i < 18; i++) {
            ai->targetable_tiles[i] = 0xffff;
            ai->targetable_tiles[i + 18] = 0xffff;
        }
        return;
    }
    for (i = 17; i >= 0; i--)
        ai->targetable_tiles[i] = 0;
    y = ai->considered_unit_coords.bytes.y;
    x = ai->considered_unit_coords.bytes.x;
    for (i = 0; i <= radius; i++) {
        if (y - i < 0)
            break;
        if (x < 8)
            ai->targetable_tiles[y - i] = g_battle_ai_range_row_masks[radius - i] << (7 - x);
        else
            ai->targetable_tiles[y - i] = g_battle_ai_range_row_masks[radius - i] >> (x - 7);
    }
    for (i = 1; i <= radius; i++) {
        if (y + i >= ai->map_max_y)
            break;
        if (x < 8)
            ai->targetable_tiles[y + i] = g_battle_ai_range_row_masks[radius - i] << (7 - x);
        else
            ai->targetable_tiles[y + i] = g_battle_ai_range_row_masks[radius - i] >> (x - 7);
    }
    if (ai->considered_ability.ai_flags.word & BATTLE_AI_ABILITY_LINEAR_TRAJECTORY) {
        for (y = 0; y < ai->map_max_y; y++) {
            i = ai->targetable_tiles[y];
            if (i != 0) {
                for (x = 0; x < ai->map_max_x; x++) {
                    if ((i << x) & 0x8000) {
                        if (x != ai->considered_unit_coords.bytes.x && y != ai->considered_unit_coords.bytes.y)
                            ai->targetable_tiles[y] ^= 0x8000 >> x;
                    }
                }
            }
        }
    }
    battle_ai_transfer_halfword_values(&ai->targetable_tiles[18], ai->targetable_tiles, 36);
}
