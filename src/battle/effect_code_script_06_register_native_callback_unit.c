#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"

/* Opcode 06: put the native callback whose catalog id is the second 16-bit value in the
 * callback slot named by the top six bits of the instruction's second byte.
 * A particle keyframe whose action names the slot binds it to the record. */
effect_script_result_e effect_code_script_06_register_native_callback(effect_record_t* record) {
    const effect_script_instruction_t* command = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = command->header.parameters;
    u32 slot = parameters.callback.slot;
    g_effect_callback_slots[slot] = effect_get_native_callback(command->immediate.value);
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

/* Opcode 07: call the native callback whose catalog id is the second 16-bit value with
 * the record index and the top six bits of the instruction's second byte as
 * its callback slot, and remember it in the record. */
effect_script_result_e effect_code_script_07_call_native_callback(effect_record_t* record) {
    const effect_script_instruction_t* command = effect_get_current_script_instruction(record);
    effect_script_parameters_t parameters = command->header.parameters;
    u32 slot = parameters.callback.slot;
    effect_callback_t handler = effect_get_native_callback(command->immediate.value);
    record->last_native_callback = handler;
    handler(record->record_index, slot, 0, 0);
    record->pc += 4;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}

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

/* Opcode 09: save the camera position (opcode 0a restores it). */
effect_script_result_e effect_code_script_09_save_camera_position(effect_record_t* record) {
    camera_save_current_real_coords();
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
