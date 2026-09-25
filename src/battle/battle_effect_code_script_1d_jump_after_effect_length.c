#include "fft/battle.h"
#include "psx/types.h"

/* Jump to the offset in the second halfword once the timeline frame reaches
 * the effect length in the timing data; otherwise step over the 4-byte
 * instruction. */
s32 battle_effect_code_script_1d_jump_after_effect_length(effect_record_t* effect) {
    if (effect->timeline_frame >= g_battle_effect_timing_channels->duration) {
        effect->pc = *(u16*)((s16)effect->pc + (s32)effect->script + 2);
    } else {
        effect->pc = (u16)effect->pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
