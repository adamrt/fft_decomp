#include "fft/battle_ai.h"
#include "fft/map.h"
#include "psx/types.h"

/* Spread an ability range from the unit's tile and mark the reachable panels.
 *
 * Range bits 0-6 seed the unit's panel on both map levels and bit 7 excludes
 * the unit's own tile afterwards. Panels flagged in flags_06 bits 0-1 are never
 * marked. Returns the number of marked panels. */
s32 battle_target_calculate_ability_range_with_map_parameters(battle_stats_t* unit, u8 range) {
    battle_target_panel_t* panel;
    battle_target_panel_t* upper;
    battle_target_panel_t* panels;
    map_tile_t* tile;
    s32 index;
    s32 i;
    s32 count;
    u8 remaining;

    index = unit->position.bits.y * g_map_max_x + unit->x;
    battle_target_clear_panel_data();
    remaining = (range & 0x7f) + 1;
    panel = &g_battle_target_panels[index];
    upper = &g_battle_target_panels[index + 0x100];
    panel->remaining_range = remaining;
    upper->remaining_range = remaining;
    panel->mark = 1;
    battle_target_spread_panels(range, 0);
    if (range & 0x80) {
        panel->remaining_range = 0;
        upper->remaining_range = 0;
    }
    count = 0;
    tile = g_battle_map_tile_data;
    for (i = 0, panels = g_battle_target_panels; i < 0x200; i++) {
        if (panels->remaining_range != 0 && !(tile[i].flags_06.value & MAP_TILE_COLLISION_MASK)) {
            tile[i].ceiling_depth_and_marks |= MAP_TILE_FLAG_ABILITY_RANGE;
            count++;
        } else {
            tile[i].ceiling_depth_and_marks &= ~MAP_TILE_FLAG_ABILITY_RANGE;
        }
        panels++;
    }
    return count;
}
