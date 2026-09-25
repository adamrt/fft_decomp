#include "fft/battle.h"
#include "psx/types.h"

s32 world_unit_get_map_coords_from_misc_id(s32 misc_id, s16* out) {
    return battle_unit_get_map_coords_from_misc_id(misc_id, out);
}
