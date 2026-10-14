#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"

/* Spawns an emitter's particles for this frame and initializes their motion.
 *
 * Emitter emitter_index of the effect's particle-system section spawns every
 * spawn_interval frames of `frame`, which also picks the column of the
 * animation curves, modulo EFFECT_ANIMATION_CURVE_FRAMES. Each start/end pair
 * is lerped by its motion-curve binding, and the
 * per-particle values are drawn between the resulting minimum and maximum.
 * The particles belong to state record record_index and are placed for its
 * target; parent is the particle that the PARENT_PARTICLE origin spawns from.
 *
 * Positions have 12 fractional bits; initial speed values scale into velocity by eight.
 * The lerps of the pair at 0x4c and the target's facing quadrant are discarded. */
s32 effect_spawn_emitter_particles(s16 record_index, s32 frame, s32 emitter_index, effect_work_record_t* parent) {
    SVECTOR rotation;
    VECTOR out;
    SVECTOR caster;
    MATRIX matrix;
    SVECTOR target;
    VECTOR spawn;
    VECTOR spread;
    VECTOR angle;
    VECTOR angle_spread;
    SVECTOR accel_min;
    SVECTOR accel_max;
    SVECTOR accel_step_min;
    SVECTOR accel_step_max;
    SVECTOR target_offset;
    SVECTOR map_max;
    SVECTOR tile_anchor;
    const effect_geometry_record_t* entry;
    effect_work_record_t* particle;
    SVECTOR* unit_coords;
    s32 time;
    s32 selector;
    u16 inertia_binding;
    u8 weight_binding;
    s32 discarded_selector;
    s32 curve;
    s32 factor;
    s32 i;
    u8 target_index;
    s16 scale;
    s16 speed_min;
    s16 speed_max;
    s16 speed;
    s16 facing;
    u32 motion_curve_selectors;
    s32 count;
    effect_work_record_t* head;
    u16 placement_flags;
    u16 behavior_flags;
    s32 rotate;
    s16 inertia_min;
    s16 inertia_max;
    s16 weight_min;
    s16 weight_max;
    s16 lifetime_min;
    s16 lifetime_max;
    s16 homing_min;
    s16 homing_max;
    u16 homing_curve_selector;
    u8 color_r;
    u8 color_g;
    u8 color_b;
    map_tile_t* tile;

    entry = &g_effect_geometry_table->entries[emitter_index];
    time = frame;
    frame = frame % EFFECT_ANIMATION_CURVE_FRAMES;
    placement_flags = entry->emitter.placement_flags;
    behavior_flags = entry->emitter.flags;
    /* Keep the binding word packed: the first lookup loads its last byte,
     * later lookups and the homing setup load the complete word. */
    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_SPAWN_INTERVAL_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    if (time % battle_lerp_fixed8(entry->emitter.spawn_interval_start, entry->emitter.spawn_interval_end, factor)
        != 0) {
        return;
    }

    SetTransMatrix(&g_effect_emitter_matrix);
    target_index = g_effect_state_records[record_index].timeline.target_index;
    rotate = (behavior_flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING;
    effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
        EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &caster);
    if (g_effect_context.targets[EFFECT_CASTER_ENTRY].target_type == EFFECT_TARGET_TYPE_UNIT) {
        facing = ((battle_unit_get_facing_quadrant_by_runtime_id(
                       g_effect_context.targets[EFFECT_CASTER_ENTRY].data.unit.runtime_id)
                      + 1)
                     & 3)
            << 10;
        rotation.vz = 0;
        rotation.vx = 0;
        rotation.vy = facing;
        RotMatrix(&rotation, &matrix);
    }
    effect_resolve_target_world_coords(
        g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, target_index, &g_effect_context, &target);
    if (g_effect_context.targets[target_index].target_type == EFFECT_TARGET_TYPE_UNIT) {
        battle_unit_get_facing_quadrant_by_runtime_id(g_effect_context.targets[target_index].data.unit.runtime_id);
        unit_coords
            = battle_unit_get_event_offset_by_runtime_id(g_effect_context.targets[target_index].data.unit.runtime_id);
    }
    tile = map_get_tile_data_pointer(
        g_effect_target_tile.data.tile.vx, g_effect_target_tile.data.tile.vz, g_effect_target_tile.data.tile.vy);

    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_PARTICLE_COUNT_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    count = battle_lerp_fixed8(entry->emitter.particle_count_start, entry->emitter.particle_count_end, factor);
    for (i = 0; i < count; i++) {
        particle = effect_alloc_work_record(record_index, (placement_flags & EFFECT_WORK_KIND_MASK));
        particle->kind_flags = placement_flags;
        particle->flags = behavior_flags;
        particle->curve_frame = 0;
        effect_init_animation_sequence(
            particle->data, entry->emitter.animation_frame_group, entry->emitter.animation_index);
    }
    head = particle;

    curve = entry->emitter.spawn_bindings.position;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    effect_interpolate_emitter_spawn_position(entry, factor, &spawn);
    if (rotate) {
        SetRotMatrix(&matrix);
        copyVector(&rotation, &spawn);
        RotTrans(&rotation, &spawn, &out.pad);
    }
    switch (placement_flags & EFFECT_EMITTER_ORIGIN_MASK) {
    case EFFECT_EMITTER_ORIGIN_WORLD:
        break;
    case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
        spawn.vx += g_effect_target_tile.data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        spawn.vz += g_effect_target_tile.data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        spawn.vy -= tile->height * 12;
        break;
    case EFFECT_EMITTER_ORIGIN_CASTER:
        effect_add_svector_to_vector(&caster, &spawn, &spawn);
        break;
    case EFFECT_EMITTER_ORIGIN_TARGET:
        effect_add_svector_to_vector(&target, &spawn, &spawn);
        break;
    case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
        if (parent != 0) {
            spawn.vx += parent->motion.position.x >> 12;
            spawn.vy += parent->motion.position.y >> 12;
            spawn.vz += parent->motion.position.z >> 12;
        }
        break;
    case EFFECT_EMITTER_ORIGIN_MAP_CENTER:
        map_store_max_coordinates(&map_max);
        spawn.vx += map_max.vx * (MAP_TILE_SIZE / 2);
        spawn.vz += map_max.vz * (MAP_TILE_SIZE / 2);
        break;
    case EFFECT_EMITTER_ORIGIN_TRACKED_ENTITY:
        spawn.vx += target.vx + unit_coords->vx;
        spawn.vy += target.vy + unit_coords->vy;
        spawn.vz += target.vz + unit_coords->vz;
        break;
    }

    selector = entry->emitter.spawn_bindings.spread;
    curve = selector;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    effect_interpolate_emitter_spawn_spread(entry, factor, &spread);
    if (rotate) {
        SetRotMatrix(&matrix);
        copyVector(&rotation, &spread);
        RotTrans(&rotation, &spread, &out.pad);
    }
    particle = head;
    if (placement_flags & EFFECT_EMITTER_BOX_SPREAD) {
        for (i = 0; i < count; i++) {
            particle->motion.position.x = (spawn.vx + RANDOM_BETWEEN(spread.vx, -spread.vx)) * ONE;
            particle->motion.position.y = (spawn.vy + RANDOM_BETWEEN(spread.vy, -spread.vy)) * ONE;
            particle->motion.position.z = (spawn.vz + RANDOM_BETWEEN(spread.vz, -spread.vz)) * ONE;
            particle = particle->next;
        }
    } else {
        scale = effect_select_largest_magnitude_s16(spread.vx, spread.vy, spread.vz);
        for (i = 0; i < count; i++) {
            if (scale != 0) {
                setVector(&rotation, rand(), rand(), rand());
                RotMatrix(&rotation, &g_effect_emitter_matrix);
                SetRotMatrix(&g_effect_emitter_matrix);
                rotation.vx = scale;
                rotation.vz = 0;
                rotation.vy = 0;
                RotTrans(&rotation, &out, &out.pad);
                particle->motion.position.x = ((out.vx * spread.vx) / scale + spawn.vx) * ONE;
                particle->motion.position.y = ((out.vy * spread.vy) / scale + spawn.vy) * ONE;
                particle->motion.position.z = ((out.vz * spread.vz) / scale + spawn.vz) * ONE;
            } else {
                particle->motion.position.x = spawn.vx * ONE;
                particle->motion.position.y = spawn.vy * ONE;
                particle->motion.position.z = spawn.vz * ONE;
            }
            particle = particle->next;
        }
    }

    curve = entry->emitter.spawn_bindings.radial_speed;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
        speed_min
            = battle_lerp_fixed8(entry->emitter.radial_speed_start.min, entry->emitter.radial_speed_end.min, factor);
        speed_max
            = battle_lerp_fixed8(entry->emitter.radial_speed_start.max, entry->emitter.radial_speed_end.max, factor);
    } else {
        speed_min = entry->emitter.radial_speed_start.min;
        speed_max = entry->emitter.radial_speed_start.max;
    }
    selector = entry->emitter.spawn_bindings.velocity_spread;
    curve = selector;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
        angle_spread.vx = battle_lerp_fixed8(
            entry->emitter.spawn.velocity_spread_start.x, entry->emitter.spawn.velocity_spread_end.x, factor);
        angle_spread.vy = battle_lerp_fixed8(
            entry->emitter.spawn.velocity_spread_start.y, entry->emitter.spawn.velocity_spread_end.y, factor);
        angle_spread.vz = battle_lerp_fixed8(
            entry->emitter.spawn.velocity_spread_start.z, entry->emitter.spawn.velocity_spread_end.z, factor);
    } else {
        setVector(&angle_spread, entry->emitter.spawn.velocity_spread_start.x / 2,
            entry->emitter.spawn.velocity_spread_start.y / 2, entry->emitter.spawn.velocity_spread_start.z / 2);
    }
    selector = entry->emitter.spawn_bindings.velocity_angle;
    curve = selector;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    effect_interpolate_emitter_velocity_base_angles(entry, factor, &angle);

    particle = head;
    switch (behavior_flags & EFFECT_EMITTER_VELOCITY_MASK) {
    case EFFECT_EMITTER_VELOCITY_OUTWARD:
        for (i = 0; i < count; i++) {
            setVector(&rotation, angle.vx + RANDOM_BETWEEN(angle_spread.vx, -angle_spread.vx),
                angle.vy + RANDOM_BETWEEN(angle_spread.vy, -angle_spread.vy),
                angle.vz + RANDOM_BETWEEN(angle_spread.vz, -angle_spread.vz));
            RotMatrix(&rotation, &g_effect_emitter_matrix);
            SetRotMatrix(&g_effect_emitter_matrix);
            rotation.vz = 0;
            rotation.vx = 0;
            rotation.vy = RANDOM_BETWEEN(speed_min, speed_max);
            RotTrans(&rotation, &out, &out.pad);
            particle->motion.velocity.x = out.vx * 8;
            particle->motion.velocity.y = out.vy * 8;
            particle->motion.velocity.z = out.vz * 8;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_VELOCITY_CASTER_FACING:
        for (i = 0; i < count; i++) {
            setVector(&rotation, angle.vx + RANDOM_BETWEEN(angle_spread.vx, -angle_spread.vx),
                angle.vy + RANDOM_BETWEEN(angle_spread.vy, -angle_spread.vy),
                angle.vz + RANDOM_BETWEEN(angle_spread.vz, -angle_spread.vz));
            RotMatrix(&rotation, &g_effect_emitter_matrix);
            SetRotMatrix(&g_effect_emitter_matrix);
            rotation.vz = 0;
            rotation.vx = 0;
            rotation.vy = RANDOM_BETWEEN(speed_min, speed_max);
            RotTrans(&rotation, &out, &out.pad);
            SetRotMatrix(&matrix);
            copyVector(&rotation, &out);
            RotTrans(&rotation, &out, &out.pad);
            particle->motion.velocity.x = out.vx * 8;
            particle->motion.velocity.y = out.vy * 8;
            particle->motion.velocity.z = out.vz * 8;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_VELOCITY_TOWARD_TARGET:
        break;
    case EFFECT_EMITTER_VELOCITY_INWARD:
        for (i = 0; i < count; i++) {
            setVector(&out, spawn.vx - (particle->motion.position.x >> 12),
                spawn.vy - (particle->motion.position.y >> 12), spawn.vz - (particle->motion.position.z >> 12));
            speed = RANDOM_BETWEEN(speed_min, speed_max);
            if (out.vx != 0 || out.vy != 0 || out.vz != 0) {
                VectorNormal(&out, &out);
                particle->motion.velocity.x = (speed * out.vx) >> 9;
                particle->motion.velocity.y = (speed * out.vy) >> 9;
                particle->motion.velocity.z = (speed * out.vz) >> 9;
            } else {
                particle->motion.velocity.y = speed * 8;
                particle->motion.velocity.z = 0;
                particle->motion.velocity.x = 0;
            }
            particle = particle->next;
        }
        break;
    }

    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_ACCELERATION_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    accel_min.vx
        = battle_lerp_fixed8(entry->emitter.acceleration_start.x.min, entry->emitter.acceleration_end.x.min, factor);
    accel_max.vx
        = battle_lerp_fixed8(entry->emitter.acceleration_start.x.max, entry->emitter.acceleration_end.x.max, factor);
    accel_min.vy
        = battle_lerp_fixed8(entry->emitter.acceleration_start.y.min, entry->emitter.acceleration_end.y.min, factor);
    accel_max.vy
        = battle_lerp_fixed8(entry->emitter.acceleration_start.y.max, entry->emitter.acceleration_end.y.max, factor);
    accel_min.vz
        = battle_lerp_fixed8(entry->emitter.acceleration_start.z.min, entry->emitter.acceleration_end.z.min, factor);
    accel_max.vz
        = battle_lerp_fixed8(entry->emitter.acceleration_start.z.max, entry->emitter.acceleration_end.z.max, factor);
    if (rotate) {
        SetRotMatrix(&matrix);
        RotTrans(&accel_min, &out, &out.pad);
        copyVector(&accel_min, &out);
        RotTrans(&accel_max, &out, &out.pad);
        copyVector(&accel_max, &out);
    }
    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_ACCELERATION_STEP_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    accel_step_min.vx = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.x.min, entry->emitter.acceleration_step_end.x.min, factor);
    accel_step_max.vx = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.x.max, entry->emitter.acceleration_step_end.x.max, factor);
    accel_step_min.vy = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.y.min, entry->emitter.acceleration_step_end.y.min, factor);
    accel_step_max.vy = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.y.max, entry->emitter.acceleration_step_end.y.max, factor);
    accel_step_min.vz = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.z.min, entry->emitter.acceleration_step_end.z.min, factor);
    accel_step_max.vz = battle_lerp_fixed8(
        entry->emitter.acceleration_step_start.z.max, entry->emitter.acceleration_step_end.z.max, factor);
    if (rotate) {
        RotTrans(&accel_step_min, &out, &out.pad);
        copyVector(&accel_step_min, &out);
        RotTrans(&accel_step_max, &out, &out.pad);
        copyVector(&accel_step_max, &out);
    }
    particle = head;
    for (i = 0; i < count; i++) {
        particle->motion.acceleration.x = RANDOM_BETWEEN(accel_min.vx, accel_max.vx);
        particle->motion.acceleration.y = RANDOM_BETWEEN(accel_min.vy, accel_max.vy);
        particle->motion.acceleration.z = RANDOM_BETWEEN(accel_min.vz, accel_max.vz);
        particle->motion.acceleration_step.x = RANDOM_BETWEEN(accel_step_min.vx, accel_step_max.vx);
        particle->motion.acceleration_step.y = RANDOM_BETWEEN(accel_step_min.vy, accel_step_max.vy);
        particle->motion.acceleration_step.z = RANDOM_BETWEEN(accel_step_min.vz, accel_step_max.vz);
        particle = particle->next;
    }

    inertia_binding = entry->emitter.spawn_bindings.inertia;
    curve = inertia_binding;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    inertia_min
        = battle_lerp_fixed8(entry->emitter.spawn.inertia_start.min, entry->emitter.spawn.inertia_end.min, factor);
    inertia_max
        = battle_lerp_fixed8(entry->emitter.spawn.inertia_start.max, entry->emitter.spawn.inertia_end.max, factor);
    discarded_selector = entry->emitter.spawn_bindings._unknown_02;
    curve = discarded_selector;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    battle_lerp_fixed8(entry->emitter._unknown_4c[0], entry->emitter._unknown_50[0], factor);
    battle_lerp_fixed8(entry->emitter._unknown_4c[1], entry->emitter._unknown_50[1], factor);
    weight_binding = entry->emitter.spawn_bindings.weight;
    curve = weight_binding;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    weight_min = battle_lerp_fixed8(entry->emitter.weight_start.min, entry->emitter.weight_end.min, factor);
    weight_max = battle_lerp_fixed8(entry->emitter.weight_start.max, entry->emitter.weight_end.max, factor);
    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_LIFETIME_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    lifetime_min = battle_lerp_fixed8(entry->emitter.lifetime_start.min, entry->emitter.lifetime_end.min, factor);
    lifetime_max = battle_lerp_fixed8(entry->emitter.lifetime_start.max, entry->emitter.lifetime_end.max, factor);
    particle = head;
    for (i = 0; i < count; i++) {
        particle->motion.inertia = RANDOM_BETWEEN(inertia_min, inertia_max);
        particle->motion.weight = RANDOM_BETWEEN(weight_min, weight_max);
        particle->lifetime = RANDOM_BETWEEN(lifetime_min, lifetime_max);
        particle = particle->next;
    }

    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_TARGET_OFFSET_SHIFT & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    target_offset.vx
        = battle_lerp_fixed8(entry->emitter.target_offset_start.x, entry->emitter.target_offset_end.x, factor);
    target_offset.vy
        = battle_lerp_fixed8(entry->emitter.target_offset_start.y, entry->emitter.target_offset_end.y, factor);
    target_offset.vz
        = battle_lerp_fixed8(entry->emitter.target_offset_start.z, entry->emitter.target_offset_end.z, factor);
    particle = head;
    switch (placement_flags & EFFECT_EMITTER_HOMING_MASK) {
    case EFFECT_EMITTER_HOMING_OFFSET:
    case EFFECT_EMITTER_HOMING_OFFSET_ALT:
        for (i = 0; i < count; i++) {
            particle->target.x = target_offset.vx;
            particle->target.y = target_offset.vy;
            particle->target.z = target_offset.vz;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_MAP_CENTER:
        map_store_max_coordinates(&map_max);
        for (i = 0; i < count; i++) {
            particle->target.x = map_max.vx * (MAP_TILE_SIZE / 2) + target_offset.vx;
            particle->target.z = map_max.vz * (MAP_TILE_SIZE / 2) + target_offset.vz;
            particle->target.y = target_offset.vy;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_CASTER:
        for (i = 0; i < count; i++) {
            particle->target.x = caster.vx + target_offset.vx;
            particle->target.y = caster.vy + target_offset.vy;
            particle->target.z = caster.vz + target_offset.vz;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_TARGET:
        for (i = 0; i < count; i++) {
            particle->target.x = target.vx + target_offset.vx;
            particle->target.y = target.vy + target_offset.vy;
            particle->target.z = target.vz + target_offset.vz;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_CURSOR_TILE:
        tile_anchor.vx = g_effect_target_tile.data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        tile_anchor.vz = g_effect_target_tile.data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        tile_anchor.vy = -(tile->height * 12);
        for (i = 0; i < count; i++) {
            particle->target.x = tile_anchor.vx + target_offset.vx;
            particle->target.y = tile_anchor.vy + target_offset.vy;
            particle->target.z = tile_anchor.vz + target_offset.vz;
            particle = particle->next;
        }
        break;
    }

    curve = entry->emitter.motion_curve_selectors >> EFFECT_EMITTER_CURVE_HOMING_STRENGTH_SHIFT & 3;
    curve--;
    if (curve != -1) {
        factor = g_effect_animation_curve_data->curves[curve][frame];
    } else {
        factor = 0;
    }
    /* The lerp results pass through curve: that reassignment keeps CSE from
     * sign-extending the call results instead of homing_min/max. */
    curve
        = battle_lerp_fixed8(entry->emitter.homing_strength_start.min, entry->emitter.homing_strength_end.min, factor);
    homing_min = curve;
    curve
        = battle_lerp_fixed8(entry->emitter.homing_strength_start.max, entry->emitter.homing_strength_end.max, factor);
    homing_max = curve;
    /* This otherwise-unused test keeps the packed selectors live through the loop setup. */
    motion_curve_selectors = entry->emitter.motion_curve_selectors;
    homing_curve_selector = motion_curve_selectors >> EFFECT_EMITTER_CURVE_HOMING_MOTION_SHIFT;
    if (motion_curve_selectors != 0) {
        curve = motion_curve_selectors >> EFFECT_EMITTER_CURVE_HOMING_STRENGTH_SHIFT & 3;
    }
    color_r = entry->emitter.color_curves & 0xf;
    color_g = entry->emitter.color_curves >> 4 & 0xf;
    color_b = entry->emitter.color_curves >> 8 & 0xf;
    particle = head;
    for (i = 0; i < count; i++) {
        particle->homing_strength = RANDOM_BETWEEN(homing_min, homing_max);
        particle->homing_curve_selector = homing_curve_selector;
        particle->color_curves.r = color_r;
        particle->color_curves.g = color_g;
        particle->color_curves.b = color_b;
        if (behavior_flags & EFFECT_EMITTER_FLAG_CHILD_ON_DEATH_MASK) {
            particle->child_emitter_on_death = entry->emitter.child_emitter_on_death;
        }
        if (behavior_flags & EFFECT_EMITTER_FLAG_CHILD_MID_LIFE_MASK) {
            particle->child_emitter_mid_life = entry->emitter.child_emitter_mid_life;
        }
        particle = particle->next;
    }
}
