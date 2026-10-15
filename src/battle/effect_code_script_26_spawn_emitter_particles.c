#include "fft/battle_effect.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* Opcode 26: spawn this frame's particles of the emitter named by bits 10-13
 * of the 16-bit instruction header, at the record's frame counter. */
effect_script_result_e effect_code_script_26_spawn_emitter_particles(effect_record_t* record) {
    /* A 16-bit header cache preserves the address operands; a direct field read changes them. */
    union {
        u16 packed;
        struct {
            u16 : 10;
            u16 emitter_index : 4;
            u16 : 2;
        } fields;
    } header;

    header.packed = effect_get_current_script_header(record);

    effect_spawn_emitter_particles(record->record_index, record->frame_counter, header.fields.emitter_index, 0);
    record->pc += 2;
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
