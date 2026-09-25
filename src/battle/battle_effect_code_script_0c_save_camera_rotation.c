#include "fft/battle.h"

s32 battle_effect_code_script_0c_save_camera_rotation(effect_record_t* effect) {
    battle_camera_copy_rotation();
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
