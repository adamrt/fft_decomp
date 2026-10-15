#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"

/* Opcode 09: save the camera position (opcode 0a restores it). */
effect_script_result_e effect_code_script_09_save_camera_position(effect_record_t* record) {
    camera_save_current_real_coords();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
