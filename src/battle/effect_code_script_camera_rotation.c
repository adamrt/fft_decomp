/* Opcodes 0a-0d: camera position restore and the rotation set, save and restore. */

#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"

/* Opcode 0a: restore the camera position opcode 09 saved. */
effect_script_result_e effect_code_script_0a_restore_camera_position(effect_record_t* record) {
    camera_restore_current_real_coords();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

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

/* Opcode 0c: save the camera rotation (opcode 0d restores it). */
effect_script_result_e effect_code_script_0c_save_camera_rotation(effect_record_t* record) {
    camera_save_rotation();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 0d: restore the camera rotation opcode 0c saved. */
effect_script_result_e effect_code_script_0d_restore_camera_rotation(effect_record_t* record) {
    camera_restore_rotation();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
