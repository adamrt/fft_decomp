#include "fft/battle.h"
#include "psx/types.h"

/* Jump to the offset in the second halfword once the timeline frame reaches
 * the phase-1 duration, the phase-2 duration, the spawn delay for each further hit
 * and the phase-2 delay combined; otherwise step over the 4-byte
 * instruction. */
s32 battle_effect_code_script_1e_jump_after_total_duration(effect_record_t* effect) {
    effect_misc_data_t* timing;

    timing = g_battle_effect_misc_data;
    if (effect->timeline_frame >= (s32)(timing->phase1_duration + timing->phase2_duration
            + (g_battle_effect_coord_data.hit_counter - 1) * timing->spawn_delay + timing->phase2_delay)) {
        effect->pc = *(u16*)(effect->pc + (s32)effect->script + 2);
    } else {
        effect->pc += 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
