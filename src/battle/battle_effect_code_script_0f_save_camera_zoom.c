#include "fft/battle.h"

s32 battle_effect_code_script_0f_save_camera_zoom(effect_record_t* effect) {
    battle_camera_copy_zoom();
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
