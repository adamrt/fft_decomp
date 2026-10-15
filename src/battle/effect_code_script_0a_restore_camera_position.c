#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"

effect_script_result_e effect_code_script_0a_restore_camera_position(effect_record_t* record) {
    camera_restore_current_real_coords();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
