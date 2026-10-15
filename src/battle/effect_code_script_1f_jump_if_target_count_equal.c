#include "fft/battle_effect.h"

/* Opcode 1f: jump when the target count equals the header's six-bit selector.
 * Retail scripts use zero to end an effect with no targets. */
effect_script_result_e effect_code_script_1f_jump_if_target_count_equal(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (g_effect_context.target_count == parameters.targets.count) {
        record->pc = instruction->immediate.value;
    } else {
        record->pc += 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
