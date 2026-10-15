#include "fft/battle_effect.h"

/* Opcode 04: end the script; effect_run_block's caller then frees the
 * record. */
effect_script_result_e effect_code_script_04_end(effect_record_t* unused_record) {
    return EFFECT_SCRIPT_RESULT_ABORT;
}
