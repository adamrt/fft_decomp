#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"

/* Opcode 0d: restore the camera rotation opcode 0c saved. */
effect_script_result_e effect_code_script_0d_restore_camera_rotation(effect_record_t* record) {
    camera_restore_rotation();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
