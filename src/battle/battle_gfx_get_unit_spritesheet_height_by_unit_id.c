#include "fft/battle.h"
#include "psx/types.h"

struct battle_unit_misc_data;

s32 battle_gfx_get_unit_spritesheet_height_by_unit_id(u32 unit_id) {
    return battle_gfx_get_unit_spritesheet_height(battle_unit_get_misc_data_by_battle_id(unit_id & 0xffff));
}
