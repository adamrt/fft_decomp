#include "fft/battle.h"

/*
 * Render the emitter selected by effect-script opcode 0x26.
 *
 * Bits 10–13 select the emitter. The command then advances by one halfword and
 * returns 1 so effect-script execution can continue.
 */
s32 battle_effect_code_script_26_render_emitter(effect_record_t* effect) {
    s32 result;
    /* Pin: unpinned, the pc takes $v0, so the result constant cannot fill its load delay slot (adds a nop). */
    register u16 next_instruction_offset __asm__("$3");

    battle_effect_spawn_emitter_particles(
        effect->record_index, effect->emitter_index, (*(u16*)(effect->script + effect->pc) >> 10) & 0xf, 0);
    next_instruction_offset = effect->pc;
    result = EFFECT_SCRIPT_RESULT_CONTINUE;
    next_instruction_offset += 2;
    effect->pc = next_instruction_offset;
    return result;
}
