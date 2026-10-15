#include "fft/battle_effect.h"

effect_script_result_e effect_code_script_2a_reset_child_timeline(effect_record_t* record) {
    effect_reset_child_timeline(record, 0);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 2b: clear the parent-timeline state (frame, spawn counters and
 * keyframe tracks) that opcode 29 steps. */
effect_script_result_e effect_code_script_2b_reset_parent_timeline(effect_record_t* record) {
    effect_reset_parent_timeline(record);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 2c: nothing. */
effect_script_result_e effect_code_script_2c_nop(effect_record_t* record) {
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 2d: nothing. */
effect_script_result_e effect_code_script_2d_nop(effect_record_t* record) {
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Run until yield or abort; returns 1 on yield and 0 on abort. */
s32 effect_run_block(s16 index) {
    effect_record_t* rec;
    /* A narrow result preserves the retail sign extension before the tests. */
    s16 result;

    rec = &g_effect_state_records[index];
    for (;;) {
        result = g_effect_instructions[effect_get_current_script_header(rec) & EFFECT_SCRIPT_OPCODE_MASK](rec);
        if (result == EFFECT_SCRIPT_RESULT_ABORT) {
            return 0;
        }
        if (result == EFFECT_SCRIPT_RESULT_YIELD) {
            return 1;
        }
    }
}
