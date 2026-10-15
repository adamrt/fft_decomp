#include "fft/battle_effect.h"

/* Opcode 14: jump to the offset in the third 16-bit value when the counter
 * selected by the top two bits of the second byte is at most the second
 * 16-bit value; otherwise step over the 6-byte instruction. */
effect_script_result_e effect_code_script_14_jump_if_counter_at_most(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    if (record->counters[parameters.counter.slot] <= instruction->branch.comparison) {
        record->pc = instruction->branch.target_offset;
    } else {
        record->pc += 6;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
