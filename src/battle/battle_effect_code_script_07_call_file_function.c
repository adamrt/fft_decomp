#include "fft/battle.h"

typedef void (*battle_effect_function_t)(s16, u32, s32, s32);

/* Load the file function named by the second halfword into the last work
 * slot and call it with the record index and the second byte's top six bits. */
s32 battle_effect_code_script_07_call_file_function(effect_record_t* effect) {
    u8* command = (u8*)(effect->pc + (s32)effect->script);
    u32 slot = command[1] >> 2;
    battle_effect_function_t handler = (battle_effect_function_t)battle_effect_get_file_pointer(*(s16*)(command + 2));
    effect->work_slots[4] = handler;
    handler(effect->record_index, slot, 0, 0);
    effect->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
