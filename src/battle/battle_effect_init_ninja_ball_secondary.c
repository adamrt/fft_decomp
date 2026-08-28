#include "fft/battle.h"
#include "psx/types.h"

/* Start the current effect target's thrown-ball secondary effect.
 *
 * The target's result animation selects animation 7 (hit, critical, guarded),
 * 0x12 (6-7) or 0x14 (5); result 2 keeps the slot's animation. The parameter
 * is the ball effect index for the given element mask. Returns the target ID,
 * or 0 when the init record has no targets. */
s32 battle_effect_init_ninja_ball_secondary(s32 elements, battle_effect_secondary_init_t* source) {
    u8 target_id;

    if (source->target_count != 0) {
        elements = battle_effect_get_ninja_ball(elements);
        target_id = battle_effect_allocate_secondary_slot();
        switch (source->target.fields.result_animation) {
        case 0:
        case 1:
        case 3:
        case 4:
            g_battle_effect_secondary_data[target_id].animation_id = 7;
            break;
        case 6:
        case 7:
            g_battle_effect_secondary_data[target_id].animation_id = 0x12;
            break;
        case 5:
            g_battle_effect_secondary_data[target_id].animation_id = 0x14;
            break;
        }
        g_battle_effect_secondary_data[target_id].parameter = elements;
        g_battle_effect_secondary_data[target_id].function_id
            = g_battle_effect_secondary_animations[g_battle_effect_secondary_data[target_id].animation_id].function_id;
        g_battle_effect_secondary_data[target_id].caster_block = source->caster.block;
        g_battle_effect_secondary_data[target_id].target.block = source->target.block;
        g_battle_effect_secondary_data[target_id].timer = 0;
        g_battle_effect_secondary_data[target_id].phase = BATTLE_SECONDARY_EFFECT_INITIALIZING;
    } else {
        target_id = 0;
    }
    return target_id;
}
