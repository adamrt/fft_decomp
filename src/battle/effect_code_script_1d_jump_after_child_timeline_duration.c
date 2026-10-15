#include "fft/battle_effect.h"

/* Opcode 1d: jump to the offset in the second 16-bit value once the record's
 * timeline frame reaches the child timeline's duration; otherwise step over
 * the 4-byte instruction. */
effect_script_result_e effect_code_script_1d_jump_after_child_timeline_duration(effect_record_t* record) {
    if (record->timeline.frame >= g_effect_child_timeline->duration) {
        const effect_script_instruction_t* instruction = effect_get_current_script_instruction(record);

        record->pc = instruction->immediate.value;
    } else {
        record->pc = record->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
