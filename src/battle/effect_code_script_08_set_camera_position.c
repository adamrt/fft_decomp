#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"

/* Opcode 08: move the camera to the position in the three 16-bit values that
 * follow (x, y, z in world units). */
effect_script_result_e effect_code_script_08_set_camera_position(effect_record_t* record) {
    VECTOR coords;

    setVector(&coords, effect_get_current_script_instruction(record)->vector.x * ONE,
        effect_get_current_script_instruction(record)->vector.y * ONE,
        effect_get_current_script_instruction(record)->vector.z * ONE);
    camera_set_current_real_coords(&coords);
    record->pc += 8;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
