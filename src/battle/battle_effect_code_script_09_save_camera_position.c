#include "fft/battle.h"

s32 battle_effect_code_script_09_save_camera_position(effect_record_t* effect) {
    battle_camera_copy_current_real_coords();
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
