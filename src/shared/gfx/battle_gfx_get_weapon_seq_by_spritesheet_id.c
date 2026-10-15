#include "fft/battle_gfx.h"
#include "psx/types.h"

/* The weapon SEQ table of a sheet: the second part of the record that
 * battle_gfx_get_weapon_shp_by_spritesheet_id describes. */
u8** battle_gfx_get_weapon_seq_by_spritesheet_id(u32 spritesheet_id) {
    return g_battle_gfx_weapon_graphics[g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].shp_id].seq;
}
