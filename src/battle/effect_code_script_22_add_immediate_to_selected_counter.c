#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/gte_inline.h"
#include "psx/types.h"

/* Opcode 22: add the second 16-bit value to the counter selected by the top two
 * bits of the instruction's second byte. */
effect_script_result_e effect_code_script_22_add_immediate_to_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;
    const effect_script_instruction_t* instruction;
    u16 amount;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    amount = instruction->immediate.value;
    record->counters[parameters.counter.slot] += amount;
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
