#include "fft/battle_ai.h"
#include "fft/map.h"
#include "fft/unit_slots.h"

/* The target loads 0xff as 255, not -1, for the first 2 directions. */
/*
 * Build the targetable-tile mask around same-team Monster Skill holders.
 *
 * Only the low map level is visited; the high-level mask remains cleared.
 * Tile base heights must differ by at most 2; slope and depth are ignored.
 */
void battle_ai_build_monster_skill_tile_mask(void) {
    battle_ai_data_t* ai = &g_battle_ai_data_base;
    battle_stats_t* unit;
    s32 i;
    s32 direction;
    s32 x;
    s32 y;
    s32 level;
    s32 height;
    u16* row;
    u32 level_base;

    for (level = 0; level < 18; level++) {
        ai->targetable_tiles[level] = 0;
        ai->targetable_tiles[level + 18] = 0;
    }
    for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
        unit = &g_battle_unit_stats[i];
        if (unit->entd_slot == BATTLE_ENTD_SLOT_NONE)
            continue;
        if (!(unit->support_abilities[3] & BATTLE_SUPPORT_SET_4_MONSTER_SKILL))
            continue;
        if ((unit->initial_team_flags & BATTLE_TEAM_MASK) != ai->acting_unit_team)
            continue;
        if (main_unit_has_status_in_set(unit, MAIN_STATUS_CHECK_SET_PREVENT_REACTION))
            continue;
        height
            = g_battle_map_tile_data[(unit->position.raw >> 15) * 256 + unit->position.bits.y * ai->map_max_x + unit->x]
                  .height;
        for (direction = 0; direction < 4; direction++) {
            x = unit->x + g_battle_ai_facing_tile_offsets.by_direction[direction][0];
            if (x < 0 || x >= ai->map_max_x)
                continue;
            y = unit->position.bits.y + g_battle_ai_facing_tile_offsets.by_direction[direction][1];
            if (y < 0 || y >= ai->map_max_y)
                continue;
            for (level = 0; level <= 0; level++) {
                if ((u32)(g_battle_map_tile_data[(level << 8) + y * ai->map_max_x + x].height - height + 2) < 5) {
                    /* A typed level-row pointer folds the field offset into
                     * the induction value. Preserve the target's AI-base
                     * cursor with a member-derived displacement. */
                    level_base = level * sizeof(u16[18]) + (u32)ai;
                    row = (u16*)(level_base + ((u32)&ai->targetable_tiles - (u32)ai));
                    row[y] |= 0x8000 >> x;
                }
            }
        }
    }
}
