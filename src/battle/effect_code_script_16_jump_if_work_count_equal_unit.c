#include "fft/battle_effect.h"

/* Opcode 16: jump to the offset in the third 16-bit value when the record has as
 * many particles as the second 16-bit value says; otherwise step over the 6-byte
 * instruction. Retail scripts end with it, waiting for 0. */
effect_script_result_e effect_code_script_16_jump_if_work_count_equal(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    if (record->work_count == instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 17: jump to the offset in the third 16-bit value when the record has
 * more particles than the second 16-bit value says; otherwise step over the
 * 6-byte instruction. */
effect_script_result_e effect_code_script_17_jump_if_work_count_greater(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    if (instruction->branch.comparison < record->work_count) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 18: jump to the offset in the third 16-bit value when the record has
 * fewer particles than the second 16-bit value says; otherwise step over the
 * 6-byte instruction. */
effect_script_result_e effect_code_script_18_jump_if_work_count_less(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    if (record->work_count < instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 19: jump to the offset in the third 16-bit value when the number of
 * occupied child slots equals the second 16-bit value; otherwise step over the
 * 6-byte instruction. */
effect_script_result_e effect_code_script_19_jump_if_child_count_equal(effect_record_t* record) {
    s16 i;
    s16 count;
    s16 target;

    target = effect_get_current_script_instruction(record)->branch.comparison;
    count = 0;
    for (i = 0; i < EFFECT_SCRIPT_CHILD_SLOT_COUNT; i++) {
        if (record->child_indices[i] != 0) {
            count++;
        }
    }
    if (count == target) {
        record->pc = effect_get_current_script_instruction(record)->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
