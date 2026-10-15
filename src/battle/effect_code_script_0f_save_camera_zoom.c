#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"

/* Opcode 0f: save the camera zoom (opcode 24 restores it). */
effect_script_result_e effect_code_script_0f_save_camera_zoom(effect_record_t* record) {
    camera_save_zoom();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
