#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_add_signed_byte_to_height(battle_unit_misc_data_t* unit, s32 delta) {
    unit->effect_vector.vy += (s8)delta;
}
