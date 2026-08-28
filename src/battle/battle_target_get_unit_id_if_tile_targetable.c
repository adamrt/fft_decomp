#include "fft/battle.h"

s32 battle_target_get_unit_id_if_tile_targetable(s32 x, s32 y, s32 level) {
    return battle_unit_find_at_tile(x, y, level, 0);
}
