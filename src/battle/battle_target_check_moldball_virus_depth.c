#include "fft/battle.h"
#include "fft/data.h"
#include "fft/map.h"
#include "psx/types.h"

/* For Moldball Virus, clear the targeting panel of every tile with depth. */
void battle_target_check_moldball_virus_depth(s16 ability_id) {
    u8* tile;
    u8* panel;
    s32 i;

    if (ability_id != ABILITY_ID_MONSTER_SKILL_MOLDBALL_VIRUS)
        return;
    i = 0;
    panel = (u8*)g_battle_target_panel_data;
    tile = (u8*)g_battle_map_tile_data;
    do {
        if (tile[3] & MAP_TILE_DEPTH_MASK) {
            *panel = 0;
        }
        tile += 8;
        i += 1;
        panel += 5;
    } while (i < 0x200);
}
