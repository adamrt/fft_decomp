#include "fft/battle.h"
#include "psx/types.h"

void battle_unit_start_post_attack_animation_display(s32 flag, u8* ctx) {
    if (flag != 0 && ctx != 0) {
        /* The target calls the two-parameter callee without loading arguments. */
        ((void (*)(void))battle_unit_set_target_animation_from_attack_type)();
        battle_gfx_prepare_post_action_display_by_misc_id(ctx[4]);
    }
}
