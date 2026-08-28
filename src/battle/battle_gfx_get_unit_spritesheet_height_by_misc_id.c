#include "fft/battle.h"
#include "psx/types.h"

u32 battle_gfx_get_unit_spritesheet_height_by_misc_id(u32 misc_id) {
    return battle_gfx_get_unit_spritesheet_height(battle_unit_get_misc_data_by_misc_id(misc_id & 0xffff));
}
