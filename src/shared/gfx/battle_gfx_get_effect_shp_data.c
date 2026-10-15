#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Every sheet shares the one effect SHP, EFF1. */
battle_gfx_weapon_shp_t* battle_gfx_get_effect_shp_data(u32 unused_spritesheet_id) {
    return &g_battle_gfx_effect_graphics[0].shp;
}
