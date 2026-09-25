#include "fft/battle.h"

s32 battle_effect_code_script_0a_restore_camera_position(effect_record_t* effect) {
    battle_camera_restore_current_real_coords();
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
