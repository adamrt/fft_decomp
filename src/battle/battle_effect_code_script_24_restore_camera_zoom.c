#include "fft/effect.h"

s32 battle_effect_code_script_24_restore_camera_zoom(effect_record_t* effect) {
    battle_camera_restore_zoom();
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
