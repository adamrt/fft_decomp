#include "fft/battle.h"

s32 battle_effect_code_script_0b_set_camera_rotation(effect_record_t* effect) {
    battle_camera_rotation_t rotation;

    rotation.x = *(u16*)(effect->pc + (s32)effect->script + 2);
    rotation.y = *(u16*)(effect->pc + (s32)effect->script + 4);
    rotation.z = *(u16*)(effect->pc + (s32)effect->script + 6);
    battle_camera_set_rotation(&rotation);
    effect->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
