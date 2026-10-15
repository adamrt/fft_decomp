#include "fft/battle_gfx.h"
#include "psx/types.h"

/* The weapon SHP of a sheet. The weapon tables are 0xc40-byte records, a
 * battle_gfx_weapon_shp_t with 0x200 frames and then its 0x100 sequence
 * pointers: WEP1 for SHP type TYPE1 and WEP2 for TYPE2. The other SHP types
 * index past the two records; none of their sequences starts a weapon block. */
battle_gfx_weapon_shp_t* battle_gfx_get_weapon_shp_by_spritesheet_id(u32 spritesheet_id) {
    return &g_battle_gfx_weapon_graphics[g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].shp_id].shp;
}
