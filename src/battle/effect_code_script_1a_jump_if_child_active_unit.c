#include "fft/battle_effect.h"

/* Opcode 1a: jump to the offset in the second 16-bit value when the child slot
 * named by the second byte's high nibble holds a record; otherwise step over
 * the 4-byte instruction. */
effect_script_result_e effect_code_script_1a_jump_if_child_active(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    if (record->child_indices[parameters.child.slot] != 0) {
        record->pc = instruction->immediate.value;
    } else {
        record->pc = record->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1b: jump to the offset in the second 16-bit value when the child slot
 * named by the second byte's high nibble is empty; otherwise step over the
 * 4-byte instruction. */
effect_script_result_e effect_code_script_1b_jump_if_child_inactive(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    if (record->child_indices[parameters.child.slot] == 0) {
        record->pc = instruction->immediate.value;
    } else {
        record->pc = record->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1c: jump to the offset in the third 16-bit value when the counter
 * selected by the second byte's top two bits differs from the second
 * 16-bit value; otherwise step over the 6-byte instruction. */
effect_script_result_e effect_code_script_1c_jump_if_counter_not_equal(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] != instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 1d: jump to the offset in the second 16-bit value once the record's
 * timeline frame reaches the child timeline's duration; otherwise step over
 * the 4-byte instruction. */
effect_script_result_e effect_code_script_1d_jump_after_child_timeline_duration(effect_record_t* record) {
    if (record->timeline.frame >= g_effect_child_timeline->duration) {
        const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

        record->pc = instruction->immediate.value;
    } else {
        record->pc = record->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
