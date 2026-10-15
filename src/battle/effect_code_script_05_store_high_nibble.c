#include "fft/battle_effect.h"

/* Opcode 05: store the high nibble of the instruction's second byte.
 * Retail clears this global before the script runs but never reads it. */
effect_script_result_e effect_code_script_05_store_high_nibble(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

    effect_script_parameters_t parameters = instruction->header.parameters;
    g_effect_unknown_801bf000 = parameters.opcode_05.high_nibble;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
