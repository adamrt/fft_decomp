#include "fft/battle.h"

/* Start the current effect target's secondary effect and return the target ID.
 * Phase 1 is BATTLE_SECONDARY_EFFECT_INITIALIZING. */
s32 battle_effect_init_secondary(s32 animation_id, s32 sprite_palette, battle_effect_secondary_init_t* source) {
    s32 target_id;

    target_id = (u8)battle_effect_allocate_secondary_slot();
    g_battle_effect_secondary_data[target_id].animation_id = animation_id;
    g_battle_effect_secondary_data[target_id].timer = 0;
    g_battle_effect_secondary_data[target_id].parameter = sprite_palette;
    g_battle_effect_secondary_data[target_id].function_id
        = g_battle_effect_secondary_animations[g_battle_effect_secondary_data[target_id].animation_id].function_id;
    g_battle_effect_secondary_data[target_id].caster_block = source->caster.block;
    g_battle_effect_secondary_data[target_id].target.block = source->target.block;
    g_battle_effect_secondary_data[target_id].used_weapon_id = source->used_weapon_id;
    g_battle_effect_secondary_data[target_id].phase = BATTLE_SECONDARY_EFFECT_INITIALIZING;
    return target_id;
}
