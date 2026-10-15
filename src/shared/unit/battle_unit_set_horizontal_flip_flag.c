#include "fft/battle_unit.h"

void battle_unit_set_horizontal_flip_flag(u32 runtime_id) {
    battle_unit_t* unit = battle_unit_get_by_runtime_id(runtime_id & 0xffff);

    if (unit != 0) {
        unit->horizontal_flip_flag = BATTLE_SPRITE_DISPLAY_HORIZONTAL_FLIP;
    }
}
