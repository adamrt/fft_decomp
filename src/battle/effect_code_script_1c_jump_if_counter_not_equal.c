#include "fft/battle_effect.h"

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
