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
