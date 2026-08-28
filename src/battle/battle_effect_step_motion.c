#include "fft/battle_effect.h"
#include "psx/types.h"

/* Advance an effect slot's motion state by one frame. */
void battle_effect_step_motion(battle_effect_motion_t* motion) {
    s32 unused[4]; /* The target reserves an unused 16-byte frame. */

    motion->position[0] += motion->velocity[0];
    motion->position[1] += motion->velocity[1];
    motion->position[2] += motion->velocity[2];
    motion->velocity[0]
        = ((motion->mass - g_battle_effect_inertia_threshold) * motion->velocity[0] + (motion->force[0] << 12))
            / motion->mass
        + ((motion->on_hit_weight * on_hit_effects_data.x) >> 12);
    motion->velocity[1]
        = ((motion->mass - g_battle_effect_inertia_threshold) * motion->velocity[1] + (motion->force[1] << 12))
            / motion->mass
        + ((motion->on_hit_weight * on_hit_effects_data.y) >> 12);
    motion->velocity[2]
        = ((motion->mass - g_battle_effect_inertia_threshold) * motion->velocity[2] + (motion->force[2] << 12))
            / motion->mass
        + ((motion->on_hit_weight * on_hit_effects_data.z) >> 12);
    motion->force[0] += motion->force_step[0];
    motion->force[1] += motion->force_step[1];
    motion->force[2] += motion->force_step[2];
}
