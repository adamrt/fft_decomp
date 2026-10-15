#include "fft/battle_effect.h"

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
