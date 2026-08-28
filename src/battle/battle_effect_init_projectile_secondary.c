#include "fft/battle.h"

/* Start the current effect target's secondary effect: the low half of
 * animation selects the animation id (and its function id), the high half is
 * stored alongside it. Returns the target ID; phase 1 is
 * BATTLE_SECONDARY_EFFECT_INITIALIZING. */
s32 battle_effect_init_projectile_secondary(s32 animation, battle_effect_secondary_init_t* source) {
    u8 target_id;

    target_id = battle_effect_allocate_secondary_slot();
    g_battle_effect_secondary_data[target_id].animation_id = animation;
    g_battle_effect_secondary_data[target_id].timer = 0;
    g_battle_effect_secondary_data[target_id].parameter = animation >> 16;
    g_battle_effect_secondary_data[target_id].function_id
        = g_battle_effect_secondary_animations[g_battle_effect_secondary_data[target_id].animation_id].function_id;
    g_battle_effect_secondary_data[target_id].caster_block = source->caster.block;
    g_battle_effect_secondary_data[target_id].target.block = source->target.block;
    g_battle_effect_secondary_data[target_id].used_weapon_id = source->used_weapon_id;
    g_battle_effect_secondary_data[target_id].phase = BATTLE_SECONDARY_EFFECT_INITIALIZING;
    g_battle_effect_secondary_data[target_id].target_count = source->target_count;
    return target_id;
}
