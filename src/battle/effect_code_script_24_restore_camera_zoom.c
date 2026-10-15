#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/gte_inline.h"
#include "psx/types.h"

/* Opcode 24: restore the camera zoom opcode 0f saved. */
effect_script_result_e effect_code_script_24_restore_camera_zoom(effect_record_t* record) {
    camera_restore_zoom();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
