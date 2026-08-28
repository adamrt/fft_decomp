#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "psx/types.h"

/* Jump to the offset in the second halfword when the effect hit counter
 * equals the second byte shifted right by 2; otherwise step over the 4-byte
 * instruction. */
s32 battle_effect_code_script_1f_jump_if_hit_counter_equal(effect_record_t* effect) {
    s16 pc = effect->pc;
    s32 script = (s32)effect->script;
    u8* instruction = (u8*)(pc + script);
    if (g_battle_effect_coord_data.hit_counter == ((u32)instruction[1] >> 2)) {
        effect->pc = *(u16*)(instruction + 2);
    } else {
        effect->pc = pc + 4;
    }
    return EFFECT_SCRIPT_RESULT_CONTINUE;
}
