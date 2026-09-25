#include "fft/battle.h"

s32 battle_effect_code_script_08_set_camera_position(effect_record_t* effect) {
    VECTOR coords;

    coords.vx = ((s16*)(effect->pc + (s32)effect->script))[1] << 12;
    coords.vy = ((s16*)(effect->pc + (s32)effect->script))[2] << 12;
    coords.vz = ((s16*)(effect->pc + (s32)effect->script))[3] << 12;
    battle_camera_set_current_real_coords(&coords);
    effect->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
