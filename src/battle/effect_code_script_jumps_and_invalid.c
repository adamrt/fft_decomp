#include "fft/battle_effect.h"
#include "fft/main.h"

/* Opcodes 2e-77, the rest of the 120-entry handler table: the error report. */
effect_script_result_e effect_code_script_2e_invalid(effect_record_t* unused_record) {
    main_system_report_stub(1, 0x22);
    /* Retail leaves the dispatch result undefined after an invalid opcode. */
}

/* Opcode 00: jump to the script offset in the second 16-bit value and end the
 * record's run for this frame. */
effect_script_result_e effect_code_script_00_jump_and_yield(effect_record_t* record) {
    record->pc = effect_get_current_script_instruction(record)->immediate.value;
    return EFFECT_SCRIPT_RESULT_YIELD;
}

/* Opcode 01: jump to the script offset in the second 16-bit value. */
effect_script_result_e effect_code_script_01_jump(effect_record_t* record) {
    record->pc = effect_get_current_script_instruction(record)->immediate.value;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
