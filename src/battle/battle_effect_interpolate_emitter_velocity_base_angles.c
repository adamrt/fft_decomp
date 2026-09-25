#include "fft/battle.h"

void battle_effect_interpolate_emitter_velocity_base_angles(const void* source, s32 factor, s32* out) {
    const effect_geometry_entry_t* entry = source;

    battle_effect_lerp_and_store(entry->motion.emitter.velocity_base_angle_start[0],
        entry->motion.emitter.velocity_base_angle_end[0], factor, out);
    battle_effect_lerp_and_store(entry->motion.emitter.velocity_base_angle_start[1],
        entry->motion.emitter.velocity_base_angle_end[1], factor, out + 1);
    battle_effect_lerp_and_store(entry->motion.emitter.velocity_base_angle_start[2],
        entry->motion.emitter.velocity_base_angle_end[2], factor, out + 2);
}
