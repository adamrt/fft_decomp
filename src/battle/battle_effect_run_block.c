#include "fft/battle.h"

s32 battle_effect_run_block(s16 index) {
    effect_record_t* rec;
    s16 result;

    rec = &g_effect_state_records[index];
    for (;;) {
        result = g_battle_effect_instructions[*(u16*)(rec->script + rec->pc) & EFFECT_SCRIPT_OPCODE_MASK](rec);
        if (result == EFFECT_SCRIPT_RESULT_ABORT) {
            return 0;
        }
        if (result == EFFECT_SCRIPT_RESULT_YIELD) {
            return 1;
        }
    }
}
