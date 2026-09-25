#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "fft/effect_summon_mesh.h"
#include "fft/map.h"
#include "psx/gte.h"
#include "psx/libc.h"

/* Factor of the curve selected by a 1-based palette-table nibble (0 = none) for
 * the current frame column. The index is decremented in its own statement: a
 * folded `(nibble) - 1` lets CSE compare the nibble with zero instead of -1. */
#define CURVE_FACTOR(nibble)                                                                                           \
    curve = (nibble);                                                                                                  \
    curve--;                                                                                                           \
    if (curve != -1) {                                                                                                 \
        factor = g_effect_palette_table[curve].factor[frame];                                                          \
    } else {                                                                                                           \
        factor = 0;                                                                                                    \
    }

/* Inline form of battle_effect_get_random_between. */

/* Spawns an emitter's particles for this frame and initializes their motion.
 *
 * Emitter emitter_index of the effect's particle-system section spawns every
 * spawn_interval frames. Each start/end pair is lerped by the curve that its
 * curve_indices nibble selects, and the per-particle values are drawn between
 * the resulting minimum and maximum. transform is the parent particle for the
 * 0x800 spawn anchor.
 *
 * The records returned by battle_effect_alloc_work are initialized through
 * effect_particle_physics_view_t. The 0x4c/0x50 lerps are computed and
 * discarded, as is the target's facing quadrant. */
void battle_effect_spawn_emitter_particles(
    s16 effect_index, s32 frame, s32 emitter_index, effect_work_record_t* transform) {
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
    SVECTOR drag_min;
    SVECTOR drag_max;
    SVECTOR target_offset;
    SVECTOR map_max;
    SVECTOR tile_anchor;
    effect_geometry_entry_t* entry;
    effect_particle_physics_view_t* particle;
    s16* unit_coords;
    s32 time;
    s32 curve;
    s32 factor;
    s32 i;
    u8 target_index;
    s16 scale;
    s16 speed_min;
    s16 speed_max;
    s16 speed;
    s16 facing;
    u32 word;
    s32 count;
    effect_particle_physics_view_t* head;
    u16 kind;
    u16 flags;
    s32 rotate;
    s16 inertia_min;
    s16 inertia_max;
    s16 weight_min;
    s16 weight_max;
    s16 lifetime_min;
    s16 lifetime_max;
    s16 homing_min;
    s16 homing_max;
    u16 blend;
    u8 colour_r;
    u8 colour_g;
    u8 colour_b;
    map_tile_t* tile;

    entry = &g_effect_geometry_table->entries[emitter_index];
    time = frame;
    frame = frame % 160;
    kind = entry->work_kind_flags;
    flags = entry->flags;
    CURVE_FACTOR(entry->curve_indices[1] >> 24 & 0xf);
    if (time % battle_effect_lerp_linear(entry->spawn_interval_start, entry->spawn_interval_end, factor) != 0) {
        return;
    }

    SetTransMatrix(&g_battle_effect_emitter_matrix);
    target_index = g_effect_state_records[effect_index].target_index;
    rotate = (flags & EFFECT_EMITTER_VELOCITY_MASK) == EFFECT_EMITTER_VELOCITY_CASTER_FACING;
    battle_effect_resolve_target_render_coords(g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
        -1, &g_battle_effect_coord_data, &caster);
    if (g_battle_effect_targets[16].target_type == 0) {
        facing = ((battle_unit_get_facing_quadrant_by_misc_id(g_battle_effect_targets[16].id.misc_id) + 1) & 3) << 10;
        rotation.vz = 0;
        rotation.vx = 0;
        rotation.vy = facing;
        RotMatrix(&rotation, &matrix);
    }
    battle_effect_resolve_target_render_coords(g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
        target_index, &g_battle_effect_coord_data, &target);
    if (g_battle_effect_targets[target_index].target_type == 0) {
        battle_unit_get_facing_quadrant_by_misc_id(g_battle_effect_targets[target_index].id.misc_id);
        unit_coords = battle_unit_get_event_offset_ptr_by_misc_id(g_battle_effect_targets[target_index].id.misc_id);
    }
    tile = battle_map_get_tile_data_pointer(
        g_battle_effect_target_tile.id.tile_x, g_battle_effect_target_tile.tile_y, g_battle_effect_target_tile.map_z);

    CURVE_FACTOR(entry->curve_indices[1] >> 20 & 0xf);
    count = battle_effect_lerp_linear(entry->particle_count_start, entry->particle_count_end, factor);
    for (i = 0; i < count; i++) {
        particle = (effect_particle_physics_view_t*)battle_effect_alloc_work(effect_index, (s16)(kind & 0xf000));
        particle->kind_flags = kind;
        particle->child_spawn_flags = flags;
        particle->animation_frame = 0;
        battle_effect_init_parameter_set(
            particle->animation_state, entry->animation_frame_group, entry->animation_index);
    }
    head = particle;

    CURVE_FACTOR(entry->curve_indices[0] & 0xf);
    battle_effect_interpolate_emitter_spawn_position(entry, factor, &spawn.vx);
    if (rotate) {
        SetRotMatrix(&matrix);
        rotation.vx = spawn.vx;
        rotation.vy = spawn.vy;
        rotation.vz = spawn.vz;
        RotTrans(&rotation, &spawn, (long*)&out.pad);
    }
    switch (kind & EFFECT_EMITTER_ORIGIN_MASK) {
    case EFFECT_EMITTER_ORIGIN_WORLD:
        break;
    case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
        spawn.vx += g_battle_effect_target_tile.id.tile_x * 28 + 14;
        spawn.vz += g_battle_effect_target_tile.tile_y * 28 + 14;
        spawn.vy -= tile->height * 12;
        break;
    case EFFECT_EMITTER_ORIGIN_CASTER:
        battle_effect_add_svector_to_vector(&caster.vx, &spawn.vx, &spawn.vx);
        break;
    case EFFECT_EMITTER_ORIGIN_TARGET:
        battle_effect_add_svector_to_vector(&target.vx, &spawn.vx, &spawn.vx);
        break;
    case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
        if (transform != 0) {
            spawn.vx += transform->position[0] >> 12;
            spawn.vy += transform->position[1] >> 12;
            spawn.vz += transform->position[2] >> 12;
        }
        break;
    case EFFECT_EMITTER_ORIGIN_MAP_CENTRE:
        battle_map_store_max_coordinates(&map_max);
        spawn.vx += map_max.vx * 14;
        spawn.vz += map_max.vz * 14;
        break;
    case EFFECT_EMITTER_ORIGIN_TRACKED_ENTITY:
        spawn.vx += target.vx + unit_coords[0];
        spawn.vy += target.vy + unit_coords[1];
        spawn.vz += target.vz + unit_coords[2];
        break;
    }

    CURVE_FACTOR(entry->curve_indices[0] >> 4 & 0xf);
    battle_effect_interpolate_emitter_spawn_spread(entry, factor, &spread.vx);
    if (rotate) {
        SetRotMatrix(&matrix);
        rotation.vx = spread.vx;
        rotation.vy = spread.vy;
        rotation.vz = spread.vz;
        RotTrans(&rotation, &spread, (long*)&out.pad);
    }
    particle = head;
    if (kind & 0x100) {
        for (i = 0; i < count; i++) {
            particle->position[0] = (spawn.vx + RANDOM_BETWEEN(spread.vx, -spread.vx)) << 12;
            particle->position[1] = (spawn.vy + RANDOM_BETWEEN(spread.vy, -spread.vy)) << 12;
            particle->position[2] = (spawn.vz + RANDOM_BETWEEN(spread.vz, -spread.vz)) << 12;
            particle = particle->next;
        }
    } else {
        scale = battle_effect_select_largest_magnitude_s16(spread.vx, spread.vy, spread.vz);
        for (i = 0; i < count; i++) {
            if (scale != 0) {
                rotation.vx = rand();
                rotation.vy = rand();
                rotation.vz = rand();
                RotMatrix(&rotation, &g_battle_effect_emitter_matrix);
                SetRotMatrix(&g_battle_effect_emitter_matrix);
                rotation.vx = scale;
                rotation.vz = 0;
                rotation.vy = 0;
                RotTrans(&rotation, &out, (long*)&out.pad);
                particle->position[0] = ((out.vx * spread.vx) / scale + spawn.vx) << 12;
                particle->position[1] = ((out.vy * spread.vy) / scale + spawn.vy) << 12;
                particle->position[2] = ((out.vz * spread.vz) / scale + spawn.vz) << 12;
            } else {
                particle->position[0] = spawn.vx << 12;
                particle->position[1] = spawn.vy << 12;
                particle->position[2] = spawn.vz << 12;
            }
            particle = particle->next;
        }
    }

    curve = entry->curve_indices[0] >> 28;
    curve--;
    if (curve != -1) {
        factor = g_effect_palette_table[curve].factor[frame];
        speed_min = battle_effect_lerp_linear(entry->scroll_speed_start, entry->scroll_speed_end, factor);
        speed_max = battle_effect_lerp_linear(entry->radial_speed_max_start, entry->radial_speed_max_end, factor);
    } else {
        speed_min = entry->scroll_speed_start;
        speed_max = entry->radial_speed_max_start;
    }
    curve = entry->curve_indices[0] >> 12 & 0xf;
    curve--;
    if (curve != -1) {
        factor = g_effect_palette_table[curve].factor[frame];
        angle_spread.vx = battle_effect_lerp_linear(
            entry->motion.emitter.velocity_spread_start[0], entry->motion.emitter.velocity_spread_end[0], factor);
        angle_spread.vy = battle_effect_lerp_linear(
            entry->motion.emitter.velocity_spread_start[1], entry->motion.emitter.velocity_spread_end[1], factor);
        angle_spread.vz = battle_effect_lerp_linear(
            entry->motion.emitter.velocity_spread_start[2], entry->motion.emitter.velocity_spread_end[2], factor);
    } else {
        angle_spread.vx = (s16)entry->motion.emitter.velocity_spread_start[0] / 2;
        angle_spread.vy = (s16)entry->motion.emitter.velocity_spread_start[1] / 2;
        angle_spread.vz = (s16)entry->motion.emitter.velocity_spread_start[2] / 2;
    }
    CURVE_FACTOR(entry->curve_indices[0] >> 8 & 0xf);
    battle_effect_interpolate_emitter_velocity_base_angles(entry, factor, &angle.vx);

    particle = head;
    switch (flags & EFFECT_EMITTER_VELOCITY_MASK) {
    case EFFECT_EMITTER_VELOCITY_OUTWARD:
        for (i = 0; i < count; i++) {
            rotation.vx = angle.vx + RANDOM_BETWEEN(angle_spread.vx, -angle_spread.vx);
            rotation.vy = angle.vy + RANDOM_BETWEEN(angle_spread.vy, -angle_spread.vy);
            rotation.vz = angle.vz + RANDOM_BETWEEN(angle_spread.vz, -angle_spread.vz);
            RotMatrix(&rotation, &g_battle_effect_emitter_matrix);
            SetRotMatrix(&g_battle_effect_emitter_matrix);
            rotation.vz = 0;
            rotation.vx = 0;
            rotation.vy = RANDOM_BETWEEN(speed_min, speed_max);
            RotTrans(&rotation, &out, (long*)&out.pad);
            particle->velocity[0] = out.vx << 3;
            particle->velocity[1] = out.vy << 3;
            particle->velocity[2] = out.vz << 3;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_VELOCITY_CASTER_FACING:
        for (i = 0; i < count; i++) {
            rotation.vx = angle.vx + RANDOM_BETWEEN(angle_spread.vx, -angle_spread.vx);
            rotation.vy = angle.vy + RANDOM_BETWEEN(angle_spread.vy, -angle_spread.vy);
            rotation.vz = angle.vz + RANDOM_BETWEEN(angle_spread.vz, -angle_spread.vz);
            RotMatrix(&rotation, &g_battle_effect_emitter_matrix);
            SetRotMatrix(&g_battle_effect_emitter_matrix);
            rotation.vz = 0;
            rotation.vx = 0;
            rotation.vy = RANDOM_BETWEEN(speed_min, speed_max);
            RotTrans(&rotation, &out, (long*)&out.pad);
            SetRotMatrix(&matrix);
            rotation.vx = out.vx;
            rotation.vy = out.vy;
            rotation.vz = out.vz;
            RotTrans(&rotation, &out, (long*)&out.pad);
            particle->velocity[0] = out.vx << 3;
            particle->velocity[1] = out.vy << 3;
            particle->velocity[2] = out.vz << 3;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_VELOCITY_TOWARD_TARGET:
        break;
    case EFFECT_EMITTER_VELOCITY_INWARD:
        for (i = 0; i < count; i++) {
            out.vx = spawn.vx - (particle->position[0] >> 12);
            out.vy = spawn.vy - (particle->position[1] >> 12);
            out.vz = spawn.vz - (particle->position[2] >> 12);
            speed = RANDOM_BETWEEN(speed_min, speed_max);
            if (out.vx != 0 || out.vy != 0 || out.vz != 0) {
                VectorNormal(&out, &out);
                particle->velocity[0] = (speed * out.vx) >> 9;
                particle->velocity[1] = (speed * out.vy) >> 9;
                particle->velocity[2] = (speed * out.vz) >> 9;
            } else {
                particle->velocity[1] = speed << 3;
                particle->velocity[2] = 0;
                particle->velocity[0] = 0;
            }
            particle = particle->next;
        }
        break;
    }

    CURVE_FACTOR(entry->curve_indices[1] & 0xf);
    accel_min.vx = battle_effect_lerp_linear(entry->wave_y_start[0], entry->wave_y_end[0], factor);
    accel_max.vx = battle_effect_lerp_linear(entry->wave_y_start[1], entry->wave_y_end[1], factor);
    accel_min.vy = battle_effect_lerp_linear(entry->wave_y_start[2], entry->wave_y_end[2], factor);
    accel_max.vy = battle_effect_lerp_linear(entry->wave_y_start[3], entry->wave_y_end[3], factor);
    accel_min.vz = battle_effect_lerp_linear(entry->wave_y_start[4], entry->wave_y_end[4], factor);
    accel_max.vz = battle_effect_lerp_linear(entry->wave_y_start[5], entry->wave_y_end[5], factor);
    if (rotate) {
        SetRotMatrix(&matrix);
        RotTrans(&accel_min, &out, (long*)&out.pad);
        accel_min.vx = out.vx;
        accel_min.vy = out.vy;
        accel_min.vz = out.vz;
        RotTrans(&accel_max, &out, (long*)&out.pad);
        accel_max.vx = out.vx;
        accel_max.vy = out.vy;
        accel_max.vz = out.vz;
    }
    CURVE_FACTOR(entry->curve_indices[1] >> 4 & 0xf);
    drag_min.vx = battle_effect_lerp_linear(entry->wave_x_start[0], entry->wave_x_end[0], factor);
    drag_max.vx = battle_effect_lerp_linear(entry->wave_x_start[1], entry->wave_x_end[1], factor);
    drag_min.vy = battle_effect_lerp_linear(entry->wave_x_start[2], entry->wave_x_end[2], factor);
    drag_max.vy = battle_effect_lerp_linear(entry->wave_x_start[3], entry->wave_x_end[3], factor);
    drag_min.vz = battle_effect_lerp_linear(entry->wave_x_start[4], entry->wave_x_end[4], factor);
    drag_max.vz = battle_effect_lerp_linear(entry->wave_x_start[5], entry->wave_x_end[5], factor);
    if (rotate) {
        RotTrans(&drag_min, &out, (long*)&out.pad);
        drag_min.vx = out.vx;
        drag_min.vy = out.vy;
        drag_min.vz = out.vz;
        RotTrans(&drag_max, &out, (long*)&out.pad);
        drag_max.vx = out.vx;
        drag_max.vy = out.vy;
        drag_max.vz = out.vz;
    }
    particle = head;
    for (i = 0; i < count; i++) {
        particle->acceleration[0] = RANDOM_BETWEEN(accel_min.vx, accel_max.vx);
        particle->acceleration[1] = RANDOM_BETWEEN(accel_min.vy, accel_max.vy);
        particle->acceleration[2] = RANDOM_BETWEEN(accel_min.vz, accel_max.vz);
        particle->drag[0] = RANDOM_BETWEEN(drag_min.vx, drag_max.vx);
        particle->drag[1] = RANDOM_BETWEEN(drag_min.vy, drag_max.vy);
        particle->drag[2] = RANDOM_BETWEEN(drag_min.vz, drag_max.vz);
        particle = particle->next;
    }

    CURVE_FACTOR(entry->curve_indices[0] >> 16 & 0xf);
    inertia_min = battle_effect_lerp_linear(
        entry->motion.emitter.inertia_start[0], entry->motion.emitter.inertia_end[0], factor);
    inertia_max = battle_effect_lerp_linear(
        entry->motion.emitter.inertia_start[1], entry->motion.emitter.inertia_end[1], factor);
    CURVE_FACTOR(entry->curve_indices[0] >> 20 & 0xf);
    battle_effect_lerp_linear((s16)entry->texture_page_flags, entry->lerp_end_4c, factor);
    battle_effect_lerp_linear(entry->brightness_row, entry->lerp_end_4e, factor);
    CURVE_FACTOR(entry->curve_indices[0] >> 24 & 0xf);
    weight_min = battle_effect_lerp_linear(entry->ot_depth, entry->weight_end[0], factor);
    weight_max = battle_effect_lerp_linear(entry->weight_max_start, entry->weight_end[1], factor);
    CURVE_FACTOR(entry->curve_indices[1] >> 8 & 0xf);
    lifetime_min = battle_effect_lerp_linear(entry->lifetime_start[0], entry->lifetime_end[0], factor);
    lifetime_max = battle_effect_lerp_linear(entry->lifetime_start[1], entry->lifetime_end[1], factor);
    particle = head;
    for (i = 0; i < count; i++) {
        particle->inertia = RANDOM_BETWEEN(inertia_min, inertia_max);
        particle->weight = RANDOM_BETWEEN(weight_min, weight_max);
        particle->lifetime = RANDOM_BETWEEN(lifetime_min, lifetime_max);
        particle = particle->next;
    }

    CURVE_FACTOR(entry->curve_indices[1] >> 12 & 0xf);
    target_offset.vx = battle_effect_lerp_linear(entry->target_offset_start[0], entry->target_offset_end[0], factor);
    target_offset.vy = battle_effect_lerp_linear(entry->target_offset_start[1], entry->target_offset_end[1], factor);
    target_offset.vz = battle_effect_lerp_linear(entry->target_offset_start[2], entry->target_offset_end[2], factor);
    particle = head;
    switch (kind & EFFECT_EMITTER_HOMING_MASK) {
    case EFFECT_EMITTER_HOMING_OFFSET:
    case EFFECT_EMITTER_HOMING_OFFSET_ALT:
        for (i = 0; i < count; i++) {
            particle->target[0] = target_offset.vx;
            particle->target[1] = target_offset.vy;
            particle->target[2] = target_offset.vz;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_MAP_CENTRE:
        battle_map_store_max_coordinates(&map_max);
        for (i = 0; i < count; i++) {
            particle->target[0] = map_max.vx * 14 + target_offset.vx;
            particle->target[2] = map_max.vz * 14 + target_offset.vz;
            particle->target[1] = target_offset.vy;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_CASTER:
        for (i = 0; i < count; i++) {
            particle->target[0] = caster.vx + target_offset.vx;
            particle->target[1] = caster.vy + target_offset.vy;
            particle->target[2] = caster.vz + target_offset.vz;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_TARGET:
        for (i = 0; i < count; i++) {
            particle->target[0] = target.vx + target_offset.vx;
            particle->target[1] = target.vy + target_offset.vy;
            particle->target[2] = target.vz + target_offset.vz;
            particle = particle->next;
        }
        break;
    case EFFECT_EMITTER_HOMING_CURSOR_TILE:
        tile_anchor.vx = g_battle_effect_target_tile.id.tile_x * 28 + 14;
        tile_anchor.vz = g_battle_effect_target_tile.tile_y * 28 + 14;
        tile_anchor.vy = -(tile->height * 12);
        for (i = 0; i < count; i++) {
            particle->target[0] = tile_anchor.vx + target_offset.vx;
            particle->target[1] = tile_anchor.vy + target_offset.vy;
            particle->target[2] = tile_anchor.vz + target_offset.vz;
            particle = particle->next;
        }
        break;
    }

    CURVE_FACTOR(entry->curve_indices[1] >> 28 & 3);
    /* The lerp results pass through curve: that reassignment keeps CSE from
     * sign-extending the call results instead of homing_min/max. */
    curve = battle_effect_lerp_linear(entry->homing_strength_start[0], entry->homing_strength_end[0], factor);
    homing_min = curve;
    curve = battle_effect_lerp_linear(entry->homing_strength_start[1], entry->homing_strength_end[1], factor);
    homing_max = curve;
    /* The dead test on word is required for the match: it keeps the word live
     * past the shift and splits the block before the loop setup. */
    word = entry->curve_indices[1];
    blend = word >> 30;
    if (word != 0) {
        curve = word >> 28 & 3;
    }
    colour_r = entry->colour_curves & 0xf;
    colour_g = entry->colour_curves >> 4 & 0xf;
    colour_b = entry->colour_curves >> 8 & 0xf;
    particle = head;
    for (i = 0; i < count; i++) {
        particle->homing_strength = RANDOM_BETWEEN(homing_min, homing_max);
        particle->homing_curve_index = blend;
        particle->colour_curve_index[0] = colour_r;
        particle->colour_curve_index[1] = colour_g;
        particle->colour_curve_index[2] = colour_b;
        if (flags & 3) {
            particle->child_emitter_on_death = entry->child_emitter_on_death;
        }
        if (flags & 0xc) {
            particle->child_emitter_mid_life = entry->child_emitter_mid_life;
        }
        particle = particle->next;
    }
}
