#include "fft/battle.h"
#include "fft/effect.h"

s32 battle_effect_code_script_06_load_callback_pointer(effect_record_t* effect) {
    u8* command = (u8*)(effect->pc + (s32)effect->script);
    u32 idx = (u32)command[1] >> 2;
    g_battle_effect_callback_slots[idx] = (s32)battle_effect_get_file_pointer(*(s16*)(command + 2));
    effect->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
