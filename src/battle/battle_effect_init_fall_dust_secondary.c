#include "fft/battle.h"

/* Start the current effect target's secondary effect with animation 9 at the
 * given position and return the target ID. Phase 1 is
 * BATTLE_SECONDARY_EFFECT_INITIALIZING. */
s32 battle_effect_init_fall_dust_secondary(battle_effect_secondary_init_t* source, VECTOR* position) {
    s32 target_id;

    target_id = (u8)battle_effect_allocate_secondary_slot();
    g_battle_effect_secondary_data[target_id].timer = 0;
    g_battle_effect_secondary_data[target_id].animation_id = 9;
    g_battle_effect_secondary_data[target_id].function_id
        = g_battle_effect_secondary_animations[9]
              .function_id; /* array access keeps the load after the animation store */
    g_battle_effect_secondary_data[target_id].caster_block = source->caster.block;
    g_battle_effect_secondary_data[target_id].target.position.x = position->vx >> 4;
    g_battle_effect_secondary_data[target_id].target.position.z = position->vy >> 4;
    g_battle_effect_secondary_data[target_id].target.position.y = position->vz >> 4;
    g_battle_effect_secondary_data[target_id].phase = BATTLE_SECONDARY_EFFECT_INITIALIZING;
    return target_id;
}
