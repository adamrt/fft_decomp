/* Opcodes 02-05: child record spawn and free, script end and the unread nibble store. */

#include "fft/battle_effect.h"

/* Opcode 02: start a child record on this script at the offset in the second
 * 16-bit value and keep its index in the child slot named by the high nibble of
 * the instruction's second byte (slots 4 and up keep nothing). */
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

/* Opcode 04: end the script; effect_run_block's caller then frees the
 * record. */
effect_script_result_e effect_code_script_04_end(effect_record_t* unused_record) {
    return EFFECT_SCRIPT_RESULT_ABORT;
}

/* Opcode 05: store the high nibble of the instruction's second byte.
 * Retail clears this global before the script runs but never reads it. */
effect_script_result_e effect_code_script_05_store_high_nibble(effect_record_t* record) {
    const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

    effect_script_parameters_t parameters = instruction->header.parameters;
    g_effect_unknown_801bf000 = parameters.opcode_05.high_nibble;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
