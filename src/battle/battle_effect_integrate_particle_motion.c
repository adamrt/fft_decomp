#include "fft/battle.h"
#include "psx/types.h"

/* Provisional particle list node (target_* is the homing target,
 * attract_weight the homing strength). */
typedef struct battle_effect_particle {
    u8 _unused_00[4];                    /* 0x00 */
    struct battle_effect_particle* next; /* 0x04 */
    battle_effect_motion_t motion;       /* 0x08 */
    s16 target_x;                        /* 0x3c */
    s16 target_y;                        /* 0x3e */
    s16 target_z;                        /* 0x40 */
    s16 lifetime;                        /* 0x42 */
    u8 _unused_44;                       /* 0x44 */
    u8 curve_track;                      /* 0x45 */
    u8 _unused_46[4];                    /* 0x46 */
    s16 attract_weight;                  /* 0x4a */
    u8 _unused_4c[4];                    /* 0x4c */
    s16 curve_offset;                    /* 0x50 */
} battle_effect_particle_t;

/* Integrate one frame of motion for every particle in a list.
 *
 * Runs the battle_effect_step_motion integrator on each node. A node with a
 * non-zero attract_weight then blends its force toward the unit vector to its
 * target (scaled by attract_weight) using a signed curve-track byte over 127;
 * otherwise force_step is added to force. The list parameter doubles as the
 * next-node cursor: that keeps both loop tests on it. */
void battle_effect_integrate_particle_motion(battle_effect_particle_t* list) {
    battle_effect_particle_t* node;
    battle_effect_motion_t* motion;
    s32 weight;
    s32 strength;
    s32 track;
    VECTOR delta;
    s32 inertia;
    s32 on_hit_x;
    s32 on_hit_y;
    s32 on_hit_z;

    inertia = g_battle_effect_inertia_threshold;
    on_hit_x = on_hit_effects_data.x;
    on_hit_y = on_hit_effects_data.y;
    on_hit_z = on_hit_effects_data.z;

    node = list;
    while (list != 0) {
        list = node->next;
        motion = &node->motion;

        motion->position[0] += motion->velocity[0];
        motion->position[1] += motion->velocity[1];
        motion->position[2] += motion->velocity[2];

        weight = motion->mass - inertia;

        motion->velocity[0] = (weight * motion->velocity[0] + (motion->force[0] << 12)) / motion->mass
            + ((on_hit_x * motion->on_hit_weight) >> 12);
        motion->velocity[1] = (weight * motion->velocity[1] + (motion->force[1] << 12)) / motion->mass
            + ((on_hit_y * motion->on_hit_weight) >> 12);
        motion->velocity[2] = (weight * motion->velocity[2] + (motion->force[2] << 12)) / motion->mass
            + ((on_hit_z * motion->on_hit_weight) >> 12);

        weight = node->attract_weight;
        if (weight != 0) {
            track = node->curve_track;
            track--;
            if (track != -1) {
                strength = (g_battle_effect_animation_curve_data + track * 0xa0 + node->curve_offset)[4] - 0x80;
            } else {
                strength = -0x80;
            }

            delta.vx = node->target_x - (motion->position[0] >> 12);
            delta.vy = node->target_y - (motion->position[1] >> 12);
            delta.vz = node->target_z - (motion->position[2] >> 12);
            if (delta.vx != 0 || delta.vy != 0 || delta.vz != 0) {
                VectorNormal(&delta, &delta);
                delta.vx = (weight * delta.vx) >> 12;
                delta.vy = (weight * delta.vy) >> 12;
                delta.vz = (weight * delta.vz) >> 12;
            }

            motion->force[0] += ((motion->force_step[0] - delta.vx) * strength) / 127 + delta.vx;
            motion->force[1] += ((motion->force_step[1] - delta.vy) * strength) / 127 + delta.vy;
            motion->force[2] += ((motion->force_step[2] - delta.vz) * strength) / 127 + delta.vz;
        } else {
            motion->force[0] += motion->force_step[0];
            motion->force[1] += motion->force_step[1];
            motion->force[2] += motion->force_step[2];
        }
        node = list;
    }
}
