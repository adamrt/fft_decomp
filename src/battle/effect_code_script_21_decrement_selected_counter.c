#include "fft/battle_effect.h"

/* Opcode 21: subtract 1 from the counter selected by the top two bits of the
 * instruction's second byte. */
effect_script_result_e effect_code_script_21_decrement_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;

    parameters = effect_get_current_script_instruction(record)->header.parameters;
    record->counters[parameters.counter.slot] -= 1;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
