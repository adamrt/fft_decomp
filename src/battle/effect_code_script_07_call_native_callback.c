#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "psx/libgpu.h"

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
