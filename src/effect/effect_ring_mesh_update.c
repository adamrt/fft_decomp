/* A flaring ring of textured quads, two rows high and 32 around, with scrolling texture. */

#include "fft/battle_map.h"
#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/effect.h"
#include "fft/effect_file.h"
#include "fft/effect_geometry.h"
#include "fft/effect_heap.h"
#include "fft/effect_particle.h"
#include "fft/effect_script.h"
#include "fft/gfx.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Callback handler for the ring mesh: a band two quads high and 32 around
 * the emitter origin, spinning about the vertical axis. Its three lattice rows
 * are circles whose height and radius step apart by the growth curves, and the
 * whole radius grows from frame to frame. Each update builds one quadrant of
 * the lattice in the scratchpad (the other three quadrants are the same points
 * turned by quarter turns), projects it through the battle camera into the
 * current quad buffer, shades the rows, depth-sorts the quads and flips
 * buffers. The narrow record index, shared temporaries and non-void signature
 * preserve the renderer's register allocation and delay slots. */
s32 effect_ring_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
    SVECTOR unused_10[3]; /* frame padding the target reserves but never touches */
    VECTOR origin;
    VECTOR growth;      /* vx: radius speed step, vy: row height step, vz: row radius step */
    VECTOR growth_step; /* per-frame and per-row increments of growth */
    SVECTOR map_max;
    effect_ring_mesh_work_t* work;
    const effect_geometry_record_t* entry;
    s32 radius;
    s32 u;
    s32 v;
    s32 u_span;
    s32 v_span;
    s32 buffer;
    u32* ot;
    effect_record_t* record;
    effect_ring_mesh_scratch_t* scratch;
    POLY_GT4* quad;
    POLY_GT4* other;
    POLY_GT4* quads;
    u16 placement;
    s32 translucent;
    u16 tpage;
    effect_texture_page_config_t configuration;
    u8 target_index;
    s32 j;
    s32 texture_selector;
    s32 growth_step_selector;
    u16 radius_binding;
    s32 i;
    s32 k;
    s32 n;
    s32 angle;
    s32 ring_radius;
    s32 sine;
    u32 blue;
    u32 green;
    u32 red;
    battle_map_tile_t* tile;

    record_index = (s16)record_index;
    record = &g_effect_state_records[record_index];
    entry = &g_effect_geometry_table->entries[emitter_index];
    placement = entry->radial_mesh.placement_flags;
    ot = gfx_get_battle_otag();
    scratch = (effect_ring_mesh_scratch_t*)getScratchAddr(0);

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            effect_heap_free_block(record->work_slots[callback_slot]);
        }
        work = effect_heap_alloc_block(sizeof(effect_ring_mesh_work_t), record_index);
        record->work_slots[callback_slot] = work;

        configuration = entry->radial_mesh.texture_page_config;
        tpage = (configuration.blend_mode << EFFECT_RENDER_TEXTURE_PAGE_BLEND_SHIFT) | EFFECT_RENDER_TEXTURE_PAGE_BASE;
        translucent = 1;
        if (configuration.clear_abe) {
            translucent = 0;
        }
        for (j = 0; j < 2; j++) {
            for (i = 0; i < RING_MESH_SEGMENTS * 2; i++) {
                quad = &work->quads[j][i];
                SetPolyGT4(quad);
                setSemiTrans(quad, translucent);
                quad->tpage = tpage;
                quad->clut = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_EFFECT, 0);
            }
        }

        work->spin = 0;
        work->v_scroll = 0;
        work->buffer = 0;
        work->radius_offset = 0;
        work->radius_speed = 0;
        work->destroy_delay = 0;
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;

    case EFFECT_CALLBACK_PHASE_UPDATE:
        work = record->work_slots[callback_slot];
        /* Indexed again rather than read through record, as the target does. */
        target_index = g_effect_state_records[record_index].timeline.target_index;

        EFFECT_READ_CURVE(i, j, entry->radial_mesh.motion_curves.origin, frame);
        effect_interpolate_emitter_spawn_position(entry, j, &origin);
        switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
        case EFFECT_EMITTER_ORIGIN_WORLD:
            break;
        case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
            tile = battle_map_tile_get_pointer(g_effect_target_tile.data.tile.vx, g_effect_target_tile.data.tile.vz,
                g_effect_target_tile.data.tile.vy);
            origin.vx += g_effect_target_tile.data.tile.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
            origin.vy -= tile->height * BATTLE_MAP_HEIGHT_STEP;
            origin.vz += g_effect_target_tile.data.tile.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
            break;
        case EFFECT_EMITTER_ORIGIN_CASTER:
            effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
                EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &scratch->vertex);
            origin.vx += scratch->vertex.vx;
            origin.vy += scratch->vertex.vy;
            origin.vz += scratch->vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_TARGET:
            effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
                target_index, &g_effect_context, &scratch->vertex);
            origin.vx += scratch->vertex.vx;
            origin.vy += scratch->vertex.vy;
            origin.vz += scratch->vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
            break;
        case EFFECT_EMITTER_ORIGIN_MAP_CENTER:
            battle_map_store_max_coordinates(&map_max);
            origin.vx += map_max.vx * (BATTLE_MAP_TILE_SIZE / 2);
            origin.vz += map_max.vz * (BATTLE_MAP_TILE_SIZE / 2);
            break;
        }

        radius_binding = entry->radial_mesh.shape_curves.radius;
        EFFECT_READ_CURVE(i, j, radius_binding, frame);
        radius = effect_lerp_q8(entry->radial_mesh.radius_start, entry->radial_mesh.radius_end, j);

        texture_selector = entry->radial_mesh.motion_curves.texture;
        EFFECT_READ_CURVE(i, j, texture_selector, frame);
        u = effect_lerp_q8(entry->radial_mesh.u_start, entry->radial_mesh.u_end, j);
        v = effect_lerp_q8(entry->radial_mesh.v_start, entry->radial_mesh.v_end, j);
        u_span = entry->radial_mesh.u_span;
        v_span = entry->radial_mesh.v_span;
        work->v_scroll += entry->radial_mesh.v_scroll_speed;
        if (work->v_scroll > v_span << 8) {
            work->v_scroll -= v_span << 8;
        }
        if (work->v_scroll < 0) {
            work->v_scroll += v_span << 8;
        }

        EFFECT_READ_CURVE(i, j, entry->radial_mesh.shape_curves.growth, frame);
        growth.vx = effect_lerp_q8(entry->radial_mesh.growth_start, entry->radial_mesh.growth_end, j);
        growth.vy = effect_lerp_q8(entry->radial_mesh.height_start, entry->radial_mesh.height_end, j) << 2;
        growth.vz = effect_lerp_q8(entry->radial_mesh.row_radius_start, entry->radial_mesh.row_radius_end, j);

        growth_step_selector = entry->radial_mesh.shape_curves.growth_step;
        EFFECT_READ_CURVE(i, j, growth_step_selector, frame);
        growth_step.vx = effect_lerp_q8(entry->radial_mesh.growth_step_start, entry->radial_mesh.growth_step_end, j);
        growth_step.vy = effect_lerp_q8(entry->radial_mesh.height_step_start, entry->radial_mesh.height_step_end, j)
            << 2;
        growth_step.vz
            = effect_lerp_q8(entry->radial_mesh.row_radius_step_start, entry->radial_mesh.row_radius_step_end, j);

        EFFECT_READ_CURVE(i, j, entry->radial_mesh.motion_curves.spin, frame);
        work->spin
            = (work->spin + effect_lerp_q8(entry->radial_mesh.spin_speed_start, entry->radial_mesh.spin_speed_end, j))
            & (ONE - 1);
        /* The target makes these four calls and discards their results. */
        rsin(work->spin);
        rsin(work->spin + (ONE / 8));
        rcos(work->spin);
        rcos(work->spin + (ONE / 8));

        /* Each lattice row rises by growth.vy and widens by growth.vz; both steps grow by growth_step per row. */
        n = 0;
        k = 0;
        for (i = 0; i < 3; i++) {
            angle = work->spin;
            for (j = 0; j < RING_MESH_ARC_POINTS; j++) {
                ring_radius = radius + ((k + work->radius_offset) >> 8);
                scratch->points[i][j].vx = (rcos(angle) * ring_radius) >> 12;
                sine = rsin(angle);
                scratch->points[i][j].vy = n >> 8;
                scratch->points[i][j].vz = (sine * ring_radius) >> 12;
                angle += ONE / RING_MESH_SEGMENTS;
            }
            growth.vy += growth_step.vy;
            growth.vz += growth_step.vz;
            n += growth.vy;
            k += growth.vz;
        }

        /* Grow the radius for the next frame; growth_step.vx accelerates the growth. */
        work->radius_speed += growth_step.vx;
        work->radius_offset += growth.vx + work->radius_speed;

        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        buffer = work->buffer;
        scratch->origin.vx = origin.vx;
        scratch->origin.vy = origin.vy;
        scratch->origin.vz = origin.vz;

        /* Ring 0: quad i - 1 takes lattice rows 1 (x0/x1) and 0 (x2/x3). */
        effect_vector_add_to_svector(&scratch->origin, &scratch->points[0][0], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[2], &scratch->screen[2].pad);
        quad = &work->quads[buffer][0];
        quad->x2 = scratch->screen[2].vx;
        quad->y2 = scratch->screen[2].vy;
        scratch->depth[0][0] = scratch->screen[2].vz;
        effect_vector_add_to_svector(&scratch->origin, &scratch->points[0][1], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[3], &scratch->screen[3].pad);
        quad->x3 = scratch->screen[3].vx;
        quad->y3 = scratch->screen[3].vy;
        scratch->depth[0][1] = scratch->screen[3].vz;
        effect_vector_add_to_svector(&scratch->origin, &scratch->points[1][0], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[0], &scratch->screen[0].pad);
        quad->x0 = scratch->screen[0].vx;
        quad->y0 = scratch->screen[0].vy;
        scratch->depth[1][0] = scratch->screen[0].vz;
        effect_vector_add_to_svector(&scratch->origin, &scratch->points[1][1], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
        quad->x1 = scratch->screen[1].vx;
        quad->y1 = scratch->screen[1].vy;
        scratch->depth[1][1] = scratch->screen[1].vz;

        for (i = 2; i < RING_MESH_ARC_POINTS; i++) {
            quad = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = scratch->screen[3].vx;
            quad->y2 = scratch->screen[3].vy;
            effect_vector_add_to_svector(&scratch->origin, &scratch->points[0][i], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[3], &scratch->screen[3].pad);
            quad->x3 = scratch->screen[3].vx;
            quad->y3 = scratch->screen[3].vy;
            scratch->depth[0][i] = scratch->screen[3].vz;
            effect_vector_add_to_svector(&scratch->origin, &scratch->points[1][i], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[1][i] = scratch->screen[1].vz;
        }

        /* Quadrants 1-3 reuse the quadrant-0 lattice points, turned a quarter turn at a time. */
        for (i = RING_MESH_ARC_POINTS; i < 2 * RING_MESH_SEGMENTS_PER_QUADRANT + 1; i++) {
            quad = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = scratch->screen[3].vx;
            quad->y2 = scratch->screen[3].vy;
            effect_vector_add_to_svector_rot_y_270(
                &scratch->origin, &scratch->points[0][i - RING_MESH_SEGMENTS_PER_QUADRANT], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[3], &scratch->screen[3].pad);
            quad->x3 = scratch->screen[3].vx;
            quad->y3 = scratch->screen[3].vy;
            scratch->depth[0][i] = scratch->screen[3].vz;
            effect_vector_add_to_svector_rot_y_270(
                &scratch->origin, &scratch->points[1][i - RING_MESH_SEGMENTS_PER_QUADRANT], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[1][i] = scratch->screen[1].vz;
        }
        for (i = 2 * RING_MESH_SEGMENTS_PER_QUADRANT + 1; i < 3 * RING_MESH_SEGMENTS_PER_QUADRANT + 1; i++) {
            quad = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = scratch->screen[3].vx;
            quad->y2 = scratch->screen[3].vy;
            effect_vector_add_to_svector_rot_y_180(
                &scratch->origin, &scratch->points[0][i - 2 * RING_MESH_SEGMENTS_PER_QUADRANT], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[3], &scratch->screen[3].pad);
            quad->x3 = scratch->screen[3].vx;
            quad->y3 = scratch->screen[3].vy;
            scratch->depth[0][i] = scratch->screen[3].vz;
            effect_vector_add_to_svector_rot_y_180(
                &scratch->origin, &scratch->points[1][i - 2 * RING_MESH_SEGMENTS_PER_QUADRANT], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[1][i] = scratch->screen[1].vz;
        }
        for (i = 3 * RING_MESH_SEGMENTS_PER_QUADRANT + 1; i < RING_MESH_SEGMENTS + 1; i++) {
            quad = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = scratch->screen[3].vx;
            quad->y2 = scratch->screen[3].vy;
            effect_vector_add_to_svector_rot_y_90(
                &scratch->origin, &scratch->points[0][i - 3 * RING_MESH_SEGMENTS_PER_QUADRANT], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[3], &scratch->screen[3].pad);
            quad->x3 = scratch->screen[3].vx;
            quad->y3 = scratch->screen[3].vy;
            scratch->depth[0][i] = scratch->screen[3].vz;
            effect_vector_add_to_svector_rot_y_90(
                &scratch->origin, &scratch->points[1][i - 3 * RING_MESH_SEGMENTS_PER_QUADRANT], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[1][i] = scratch->screen[1].vz;
        }

        /* Ring 1: corners 2/3 repeat ring 0's corners 0/1, and lattice row 2 gives corners 0/1. */
        effect_vector_add_to_svector(&scratch->origin, &scratch->points[2][0], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[0], &scratch->screen[0].pad);
        quads = work->quads[buffer];
        quads[RING_MESH_SEGMENTS].x0 = scratch->screen[0].vx;
        quads[RING_MESH_SEGMENTS].y0 = scratch->screen[0].vy;
        scratch->depth[2][0] = scratch->screen[0].vz;
        effect_vector_add_to_svector(&scratch->origin, &scratch->points[2][1], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
        quads[RING_MESH_SEGMENTS].x1 = scratch->screen[1].vx;
        quads[RING_MESH_SEGMENTS].y1 = scratch->screen[1].vy;
        scratch->depth[2][1] = scratch->screen[1].vz;
        other = quads;
        quads[RING_MESH_SEGMENTS].x2 = other->x0;
        quads[RING_MESH_SEGMENTS].y2 = other->y0;
        quads[RING_MESH_SEGMENTS].x3 = other->x1;
        quads[RING_MESH_SEGMENTS].y3 = other->y1;

        for (i = 2; i < RING_MESH_ARC_POINTS; i++) {
            quad = &work->quads[buffer][i + RING_MESH_SEGMENTS - 1];
            other = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            effect_vector_add_to_svector(&scratch->origin, &scratch->points[2][i], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[2][i] = scratch->screen[1].vz;
        }
        for (i = RING_MESH_ARC_POINTS; i < 2 * RING_MESH_SEGMENTS_PER_QUADRANT + 1; i++) {
            quad = &work->quads[buffer][i + RING_MESH_SEGMENTS - 1];
            other = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            effect_vector_add_to_svector_rot_y_270(
                &scratch->origin, &scratch->points[2][i - RING_MESH_SEGMENTS_PER_QUADRANT], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[2][i] = scratch->screen[1].vz;
        }
        for (i = 2 * RING_MESH_SEGMENTS_PER_QUADRANT + 1; i < 3 * RING_MESH_SEGMENTS_PER_QUADRANT + 1; i++) {
            quad = &work->quads[buffer][i + RING_MESH_SEGMENTS - 1];
            other = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            effect_vector_add_to_svector_rot_y_180(
                &scratch->origin, &scratch->points[2][i - 2 * RING_MESH_SEGMENTS_PER_QUADRANT], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[2][i] = scratch->screen[1].vz;
        }
        for (i = 3 * RING_MESH_SEGMENTS_PER_QUADRANT + 1; i < RING_MESH_SEGMENTS + 1; i++) {
            quad = &work->quads[buffer][i + RING_MESH_SEGMENTS - 1];
            other = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            effect_vector_add_to_svector_rot_y_90(
                &scratch->origin, &scratch->points[2][i - 3 * RING_MESH_SEGMENTS_PER_QUADRANT], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[2][i] = scratch->screen[1].vz;
        }

        /* Every quad maps the same texture window, scrolled by v_scroll (8.8 fixed). */
        j = work->v_scroll >> 8;
        for (i = 0; i < RING_MESH_SEGMENTS * 2; i++) {
            quad = &work->quads[buffer][i];
            quad->u0 = quad->u2 = u;
            quad->v0 = quad->v1 = v + j;
            quad->u1 = quad->u3 = u + u_span;
            quad->v2 = quad->v3 = v + v_span + j;
        }

        /* Vertex colors: the color curves (neutral without them) scaled by each lattice row's brightness. */
        n = entry->radial_mesh.brightness_row;
        blue = EFFECT_COLOR_NEUTRAL;
        if (entry->radial_mesh.flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
            k = (entry->radial_mesh.color_curves & 0xfff);
            red = g_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_RED_SHIFT) & 0xf][frame];
            green = g_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_GREEN_SHIFT) & 0xf][frame];
            blue = g_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_BLUE_SHIFT) & 0xf][frame];
        } else {
            green = EFFECT_COLOR_NEUTRAL;
            red = EFFECT_COLOR_NEUTRAL;
        }

        /* Lattice row r shades corners 2/3 of ring r and corners 0/1 of ring r - 1. */
        k = g_effect_ring_mesh_brightness_table[n][0];
        for (i = 0; i < RING_MESH_SEGMENTS; i++) {
            quad = &work->quads[buffer][i];
            quad->r3 = (k * red) / ONE;
            quad->r2 = (k * red) / ONE;
            quad->g3 = (k * green) / ONE;
            quad->g2 = (k * green) / ONE;
            quad->b3 = (k * blue) / ONE;
            quad->b2 = (k * blue) / ONE;
        }

        k = g_effect_ring_mesh_brightness_table[n][1];
        /* The redundant quad++ keeps quad from being strength-reduced into a
         * pointer of its own; the target recomputes it from the base. */
        for (i = 0; i < RING_MESH_SEGMENTS; i++, quad++) {
            quad = &work->quads[buffer][i];
            other = &work->quads[buffer][i + RING_MESH_SEGMENTS];
            other->r3 = (k * red) / ONE;
            other->r2 = (k * red) / ONE;
            quad->r1 = (k * red) / ONE;
            quad->r0 = (k * red) / ONE;
            other->g3 = (k * green) / ONE;
            other->g2 = (k * green) / ONE;
            quad->g1 = (k * green) / ONE;
            quad->g0 = (k * green) / ONE;
            other->b3 = (k * blue) / ONE;
            other->b2 = (k * blue) / ONE;
            quad->b1 = (k * blue) / ONE;
            quad->b0 = (k * blue) / ONE;
        }

        k = g_effect_ring_mesh_brightness_table[n][2];
        for (i = 0; i < RING_MESH_SEGMENTS; i++) {
            quad = &work->quads[buffer][i + RING_MESH_SEGMENTS];
            quad->r1 = (k * red) / ONE;
            quad->r0 = (k * red) / ONE;
            quad->g1 = (k * green) / ONE;
            quad->g0 = (k * green) / ONE;
            quad->b1 = (k * blue) / ONE;
            quad->b0 = (k * blue) / ONE;
        }

        /* Sort each quad by a quarter of its mean corner depth; quads outside the ordering table are skipped. */
        for (j = 0; j < 2; j++) {
            for (i = 0; i < RING_MESH_SEGMENTS; i++) {
                n = (scratch->depth[j][i] + scratch->depth[j][i + 1] + scratch->depth[j + 1][i]
                        + scratch->depth[j + 1][i + 1])
                    / 16;
                if (n >= 0 && n < EFFECT_OT_DEPTH_LIMIT) {
                    addPrim(&ot[n], &work->quads[buffer][j * RING_MESH_SEGMENTS + i]);
                }
            }
        }

        /* The next update fills the other buffer while this one is drawn. */
        work->buffer = 1 - work->buffer;
        break;

    case EFFECT_CALLBACK_PHASE_DESTROY:
        work = record->work_slots[callback_slot];
        /* Free one call late, as the last update's quads may still be drawing. The null
         * check below comes after this read through work (retail order). */
        if (work->destroy_delay == 0) {
            work->destroy_delay = 1;
            break;
        }
        if (work != 0) {
            effect_heap_free_block(work);
            record->work_slots[callback_slot] = 0;
        }
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
