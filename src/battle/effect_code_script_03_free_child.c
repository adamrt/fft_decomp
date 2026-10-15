#include "fft/battle_effect.h"

/* Opcode 03: free the child record in the child slot named by bits 4-5 of
 * the instruction's second byte. */
effect_script_result_e effect_code_script_03_free_child(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = instruction->header.parameters;
    s16 child = record->child_indices[parameters.free_child.slot];
    if (child != 0)
        effect_free_state_record(child);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
