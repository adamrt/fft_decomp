#include "fft/battle_gfx.h"
#include "psx/types.h"

u32 battle_gfx_get_spritesheet_flying_flag(u32 spritesheet_id) {
    return g_battle_gfx_spritesheet_data[spritesheet_id & 0xffff].flying_flag;
}
