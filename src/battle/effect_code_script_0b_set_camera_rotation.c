#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"

/* Opcode 0b: set the camera rotation from the three 16-bit values that follow. */
effect_script_result_e effect_code_script_0b_set_camera_rotation(effect_record_t* record) {
    SVECTOR rotation;

    setVector(&rotation, effect_get_current_script_instruction(record)->vector.x,
        effect_get_current_script_instruction(record)->vector.y,
        effect_get_current_script_instruction(record)->vector.z);
    camera_set_rotation(&rotation);
    record->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
