#include "fft/battle.h"

s32 battle_effect_code_script_0e_set_camera_zoom(effect_record_t* effect) {
    VECTOR zoom;

    zoom.vx = *(s16*)(effect->pc + (s32)effect->script + 2);
    zoom.vy = *(s16*)(effect->pc + (s32)effect->script + 4);
    zoom.vz = *(s16*)(effect->pc + (s32)effect->script + 6);
    battle_camera_set_zoom(&zoom);
    effect->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
