#include "fft/battle_gfx.h"
#include "psx/types.h"

s32 battle_gfx_get_unit_spritesheet_height_by_runtime_id(u32 runtime_id) {
    return battle_gfx_get_unit_spritesheet_height(battle_unit_get_by_runtime_id(runtime_id));
}
