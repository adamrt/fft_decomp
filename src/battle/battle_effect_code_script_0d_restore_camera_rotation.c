#include "fft/effect.h"

s32 battle_effect_code_script_0d_restore_camera_rotation(effect_record_t* effect) {
    battle_camera_restore_rotation();
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
