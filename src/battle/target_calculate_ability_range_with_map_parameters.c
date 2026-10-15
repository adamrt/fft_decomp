#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Spread an ability range from the unit's tile and mark the reachable panels.
 *
 * Range bits 0-6 seed the unit's panel on both map levels and bit 7 excludes
 * the unit's own tile afterwards. Panels flagged in flags bits 0-1 are never
 * marked. Returns the number of marked panels. */
s32 target_calculate_ability_range_with_map_parameters(const unit_stats_t* unit, u8 range) {
    /* A byte collision test retains the original single-mask branch. */
    target_panel_t* panel;
    target_panel_t* upper;
    target_panel_t* panels;
    map_tile_t* tile;
    s32 index;
    s32 i;
    s32 count;
    u8 remaining;

    index = unit->position.y * g_map_max_x + unit->x;
    target_clear_panel_data();
    remaining = (range & 0x7f) + 1;
    panel = &g_target_panels[index];
    upper = &g_target_panels[index + 0x100];
    panel->remaining_range = remaining;
    upper->remaining_range = remaining;
    panel->mark = 1;
    target_spread_panels(range, 0);
    if (range & 0x80) {
        panel->remaining_range = 0;
        upper->remaining_range = 0;
    }
    count = 0;
    tile = g_map_tile_data;
    for (i = 0, panels = g_target_panels; i < 0x200; i++) {
        if (panels->remaining_range != 0 && !(((*(u8*)&tile[i].flags) & MAP_TILE_COLLISION_MASK))) {
            tile[i].ceiling_depth_and_marks.ability_range = 1;
            count++;
        } else {
            tile[i].ceiling_depth_and_marks.ability_range = 0;
        }
        panels++;
    }
    return count;
}
