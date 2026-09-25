#include "fft/battle.h"
#include "psx/types.h"

u8* battle_gfx_get_spritesheet_seq_data(u32 spritesheet_id) {
    return g_battle_gfx_spritesheet_seq_data + g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].shp_id * 3136;
}
