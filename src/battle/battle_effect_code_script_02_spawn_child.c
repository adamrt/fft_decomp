#include "fft/battle.h"

enum { EFFECT_SCRIPT_PHASE_SLOT_COUNT = 4 };

/* Starts a new temp effect phase from the next halfword and stores its id in
 * the phase slot selected by the instruction second byte (high nibble). */
s32 battle_effect_code_script_02_spawn_child(effect_record_t* effect) {
    u32 slot;
    s16 phase_id;
    s32 pc;
    u8* script;
    u8* instruction;

    pc = effect->pc;
    script = effect->script;
    instruction = (u8*)(pc + (u32)script);
    slot = instruction[1];
    slot >>= 4;
    phase_id
        = battle_effect_start_script_record(script, *(s16*)(instruction + 2), g_battle_effect_current_record_index);
    if ((s32)slot < EFFECT_SCRIPT_PHASE_SLOT_COUNT) {
        effect->phase_ids[slot] = phase_id;
    }
    effect->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
