#include "fft/battle.h"
#include "psx/gte.h"
#include "psx/libc.h"

/* Spawns one particle's motion block from the interpolated emitter values and
 * returns its random lifetime.
 *
 * Position is the emitter offset plus a random point in the spread box, both
 * optionally rotated to face the caster or the target line (motion_flags 0x410
 * / 0x400) and anchored at the caster or target (anchor_flags 0x400 / 0x600).
 * Velocity is a random speed along the angle cone, or aimed back at the
 * position when motion_flags is 0x10. The caller stores the result as the
 * particle lifetime.
 *
 * The caster and target blocks go through a secondary-init record, as in the
 * secondary initializers; resolve_target_render_coords reads the same layout. */
s32 battle_effect_spawn_particle_motion(battle_effect_emitter_values_t* values, battle_effect_motion_t* motion) {
    MATRIX anchor_matrix;
    MATRIX rotation;
    SVECTOR anchor_angle;
    SVECTOR vector;
    SVECTOR origin;
    SVECTOR target;
    VECTOR direction;
    VECTOR position;
    VECTOR spread;
    VECTOR angle;
    VECTOR half_spread;
    u8 _unused_c0[0x28]; /* reserved by the target frame, never referenced */
    battle_effect_secondary_init_t init;
    s32 oriented;
    s16 scale;
    u16 speed_min;
    u16 speed_max;
    s16 speed;

    init.caster.block = g_battle_effect_current_secondary->caster_block;
    battle_effect_resolve_target_render_coords(0, -1, (battle_effect_coord_data_t*)&init, &origin);
    init.target.block = g_battle_effect_current_secondary->target.block;
    battle_effect_resolve_target_render_coords(0, 0, (battle_effect_coord_data_t*)&init, &target);
    oriented = 0;
    SetTransMatrix(&g_battle_effect_projectile_matrix);
    if ((values->motion_flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING) {
        oriented = 1;
        anchor_angle.vy = ((battle_unit_get_facing_quadrant_by_misc_id(
                                (u8)g_battle_effect_current_secondary->caster_block.values[1])
                               + 1)
                              & 3)
            << 10;
        anchor_angle.vz = 0;
        anchor_angle.vx = 0;
        RotMatrix(&anchor_angle, &anchor_matrix);
    } else if ((values->motion_flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_TOWARD_TARGET) {
        direction.vz = target.vz - origin.vz;
        direction.vx = target.vx - origin.vx;
        direction.vy = target.vy - origin.vy;
        oriented = 1;
        anchor_angle.vy = ratan2(-direction.vz, direction.vx);
        anchor_angle.vz
            = ratan2(direction.vy, SquareRoot0(direction.vx * direction.vx + direction.vz * direction.vz)) + 0x400;
        anchor_angle.vx = 0;
        RotMatrix(&anchor_angle, &anchor_matrix);
    }

    position.vx = values->position[0];
    position.vy = values->position[1];
    position.vz = values->position[2];
    if (oriented) {
        SetRotMatrix(&anchor_matrix);
        vector.vx = position.vx;
        vector.vy = position.vy;
        vector.vz = position.vz;
        RotTrans(&vector, &position, (long*)&direction.pad);
    }
    switch (values->anchor_flags & EFFECT_EMITTER_ORIGIN_MASK) {
    case EFFECT_EMITTER_ORIGIN_CASTER:
        position.vx += origin.vx;
        position.vy += origin.vy;
        position.vz += origin.vz;
        break;
    case EFFECT_EMITTER_ORIGIN_TARGET:
        position.vx += target.vx;
        position.vy += target.vy;
        position.vz += target.vz;
        break;
    }

    spread.vx = values->spread[0];
    spread.vy = values->spread[1];
    spread.vz = values->spread[2];
    if (oriented) {
        vector.vx = spread.vx;
        vector.vy = spread.vy;
        vector.vz = spread.vz;
        RotTrans(&vector, &spread, (long*)&direction.pad);
    }
    scale = battle_effect_select_largest_magnitude_s16(spread.vx, spread.vy, spread.vz);
    vector.vx = rand();
    vector.vy = rand();
    vector.vz = rand();
    RotMatrix(&vector, &rotation);
    SetRotMatrix(&rotation);
    vector.vx = scale;
    vector.vz = 0;
    vector.vy = 0;
    RotTrans(&vector, &direction, (long*)&direction.pad);
    motion->position[0] = ((direction.vx * spread.vx) / scale + position.vx) << 12;
    motion->position[1] = ((direction.vy * spread.vy) / scale + position.vy) << 12;
    motion->position[2] = ((direction.vz * spread.vz) / scale + position.vz) << 12;

    speed_min = values->speed_min;
    speed_max = values->speed_max;
    angle.vx = values->angle[0];
    angle.vy = values->angle[1];
    angle.vz = values->angle[2];
    half_spread.vx = (s16)values->angle_spread[0] / 2;
    half_spread.vy = (s16)values->angle_spread[1] / 2;
    half_spread.vz = (s16)values->angle_spread[2] / 2;
    switch (values->motion_flags & EFFECT_EMITTER_VELOCITY_MASK) {
    case EFFECT_EMITTER_VELOCITY_OUTWARD:
    case EFFECT_EMITTER_VELOCITY_TOWARD_TARGET:
    case EFFECT_EMITTER_VELOCITY_CASTER_FACING:
        vector.vx = angle.vx + battle_effect_get_random_between(half_spread.vx, -half_spread.vx);
        vector.vy = angle.vy + battle_effect_get_random_between(half_spread.vy, -half_spread.vy);
        vector.vz = angle.vz + battle_effect_get_random_between(half_spread.vz, -half_spread.vz);
        RotMatrix(&vector, &rotation);
        SetRotMatrix(&rotation);
        vector.vz = 0;
        vector.vx = 0;
        vector.vy = battle_effect_get_random_between((s16)speed_min, (s16)speed_max);
        RotTrans(&vector, &direction, (long*)&direction.pad);
        if (oriented) {
            vector.vx = direction.vx;
            vector.vy = direction.vy;
            vector.vz = direction.vz;
            SetRotMatrix(&anchor_matrix);
            RotTrans(&vector, &direction, (long*)&direction.pad);
        }
        motion->velocity[0] = direction.vx << 3;
        motion->velocity[1] = direction.vy << 3;
        motion->velocity[2] = direction.vz << 3;
        break;
    case EFFECT_EMITTER_VELOCITY_INWARD:
        direction.vx = position.vx - (motion->position[0] >> 12);
        direction.vy = position.vy - (motion->position[1] >> 12);
        direction.vz = position.vz - (motion->position[2] >> 12);
        speed = battle_effect_get_random_between((s16)speed_min, (s16)speed_max);
        if (direction.vx != 0 || direction.vy != 0 || direction.vz != 0) {
            VectorNormal(&direction, &direction);
            motion->velocity[0] = (speed * direction.vx) >> 9;
            motion->velocity[1] = (speed * direction.vy) >> 9;
            motion->velocity[2] = (speed * direction.vz) >> 9;
        } else {
            motion->velocity[1] = speed << 3;
            motion->velocity[2] = 0;
            motion->velocity[0] = 0;
        }
        break;
    }
    motion->force[0] = motion->force[1] = motion->force[2] = motion->force_step[0] = motion->force_step[1]
        = motion->force_step[2] = 0;
    motion->mass = ONE;
    motion->on_hit_weight = battle_effect_get_random_between(values->weight_min, values->weight_max);
    return battle_effect_get_random_between(values->lifetime_min, values->lifetime_max);
}
