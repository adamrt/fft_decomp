#include "fft/battle_effect.h"

effect_script_result_e effect_code_script_16_jump_if_work_count_equal(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    if (record->work_count == instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
