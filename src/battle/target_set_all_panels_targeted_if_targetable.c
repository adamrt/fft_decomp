#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Publish the panels in range to the map: a tile whose panel has range left gets
 * MAP_TILE_FLAG_ABILITY_RANGE unless it is blocked or a cross-section surface,
 * and every other tile loses the flag. Returns the number of flagged tiles. */
s32 target_set_all_panels_targeted_if_targetable(void) {
    map_tile_t* tile;
    target_panel_t* panel;
    s32 count;
    s32 i;

    count = 0;
    for (i = 0; i < 0x200; i++) {
        tile = &g_map_tile_data[i];
        panel = &g_target_panel_data[i];
        if (panel->remaining_range != 0 && !(tile->flags.blocked)
            && (tile->surface.type) != MAP_SURFACE_CROSS_SECTION) {
            count++;
            tile->ceiling_depth_and_marks.ability_range = 1;
        } else {
            tile->ceiling_depth_and_marks.ability_range = 0;
        }
    }
    return count;
}
