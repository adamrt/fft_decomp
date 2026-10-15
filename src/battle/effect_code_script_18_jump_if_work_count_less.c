#include "fft/battle_effect.h"

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
