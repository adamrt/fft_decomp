/* Effect SHP/SEQ tables and the flying-idle sheet flag. */

#include "fft/battle_gfx.h"
#include "psx/types.h"

/* Every sheet shares the one effect SHP, EFF1. */
battle_gfx_weapon_shp_t* battle_gfx_get_effect_shp_data(u32 unused_spritesheet_id) {
    return &g_battle_gfx_effect_graphics[0].shp;
}

/* Every sheet shares the one effect SEQ table, EFF1. */
u8** battle_gfx_get_effect_seq_data(u32 unused_spritesheet_id) {
    return g_battle_gfx_effect_graphics[0].seq;
}

/* Nonzero for a sheet that idles with the flying animations. */
u32 battle_gfx_get_spritesheet_flying_flag(u32 spritesheet_id) {
    return g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].flying_flag;
}
