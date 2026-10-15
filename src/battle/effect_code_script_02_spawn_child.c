#include "fft/battle_effect.h"

effect_script_result_e effect_code_script_02_spawn_child(effect_record_t* record) {
    effect_script_parameters_t parameters;
    s16 slot;
    s16 child;
    const effect_script_instruction_t* instruction;

    instruction = effect_get_current_script_instruction(record);
    parameters = instruction->header.parameters;
    slot = parameters.child.slot;
    child = effect_start_script_record(record->script, instruction->immediate.value, g_effect_current_record_index);
    if (slot < EFFECT_SCRIPT_CHILD_SLOT_COUNT) {
        record->child_indices[slot] = child;
    }
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
