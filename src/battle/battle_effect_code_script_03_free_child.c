#include "fft/battle.h"
#include "fft/effect.h"

s32 battle_effect_code_script_03_free_child(effect_record_t* effect) {
    s32 slot_offset = (*(u8*)(effect->pc + (s32)effect->script + 1) >> 3) & 6;
    s16 phase_id = effect->phase_ids[slot_offset >> 1];
    if (phase_id != 0)
        battle_effect_free_state_record(phase_id);
    effect->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
