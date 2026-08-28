#include "fft/battle.h"

void battle_unit_shift_right_left(battle_unit_misc_data_t* unit, s8 delta) {
    s16* vector;
    switch (unit->facing & 0xC00) {
    case 0x800:
        vector = &unit->effect_vector.vx;
        *vector = *vector + delta;
        return;
    case 0x0:
        vector = &unit->effect_vector.vx;
        *vector = *vector - delta;
        return;
    case 0x400:
        vector = &unit->effect_vector.vz;
        *vector = *vector + delta;
        return;
    case 0xC00:
        vector = &unit->effect_vector.vz;
        *vector = *vector - delta;
        return;
    }
}
