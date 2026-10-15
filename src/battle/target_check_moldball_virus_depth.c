#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

void target_check_moldball_virus_depth(s16 ability_id) {
    map_tile_t* tile;
    target_panel_t* panel;
    s32 i;

    if (ability_id != ABILITY_ID_MONSTER_GREAT_MORBOL_MOLDBALL_VIRUS)
        return;
    i = 0;
    panel = g_target_panel_data;
    tile = g_map_tile_data;
    do {
        if (tile->depth_half_height.depth) {
            panel->remaining_range = 0;
        }
        tile++;
        i += 1;
        panel++;
    } while (i < 0x200);
}
