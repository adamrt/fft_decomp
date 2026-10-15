#include "fft/battle_effect.h"

/* Opcode 1e: jump to the offset in the second 16-bit value once the record's
 * timeline frame reaches the end of the parent timeline (phase 1, a spawn
 * delay for each target after the first, the last child's duration and phase
 * 2); otherwise step over the 4-byte instruction. */
effect_script_result_e effect_code_script_1e_jump_after_total_duration(effect_record_t* record) {
    effect_timeline_t* timing;

    timing = g_effect_timeline;
    if (record->timeline.frame >= timing->phase1_duration + timing->phase2_duration
            + (g_effect_context.target_count - 1) * timing->spawn_delay + timing->child.duration) {
        const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

        record->pc = instruction->immediate.value;
    } else {
        record->pc += 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

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

/* Opcode 20: add 1 to the counter selected by the top two bits of the
 * instruction's second byte. */
effect_script_result_e effect_code_script_20_increment_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;

    parameters = effect_get_current_script_instruction(record)->header.parameters;
    record->counters[parameters.counter.slot] += 1;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 21: subtract 1 from the counter selected by the top two bits of the
 * instruction's second byte. */
effect_script_result_e effect_code_script_21_decrement_selected_counter(effect_record_t* record) {
    effect_script_parameters_t parameters;

    parameters = effect_get_current_script_instruction(record)->header.parameters;
    record->counters[parameters.counter.slot] -= 1;
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
