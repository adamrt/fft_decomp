#include "fft/battle.h"
#include "fft/effect_ring_mesh.h"

/* Builds and draws a spinning, flaring ring of 2x32 textured quads.
 *
 * Three lattice rows of nine points cover one quadrant; each row grows the
 * radius and height by per-row steps. The lattice is offset by the placement
 * origin, rotated into the other three quadrants by the add helpers, projected
 * through the battle camera and linked into the ordering table by the average
 * depth of each quad. The texture scrolls in v and every lattice row takes its
 * brightness from g_effect_e461_ring_mesh_brightness_table. The quads are double-buffered in the work block.
 *
 * `i`, `j`, `k` and `n` are shared temporaries (`i`: curve index and loop
 * index; `j`: curve factor, v scroll and loop index; `k`: row radius, colour
 * curves and brightness; `n`: row height, brightness row and quad depth): the
 * target keeps each group in one register. record_index arrives as a word and
 * is narrowed once, which is what places it in $s0. The handler returns no
 * value, but the s32 return keeps $v0 live at the exit, which leaves the phase
 * dispatch's delay slots empty as in the target. */
s32 effect_e466_update_ring_mesh_state(s32 record_index, s32 byte_offset, s32 geometry_index, s32 frame) {
    SVECTOR unused_10[3]; /* frame padding the target reserves but never touches */
    VECTOR origin;
    VECTOR growth;      /* vx: radius speed step, vy: row height step, vz: row radius step */
    VECTOR growth_step; /* per-frame and per-row increments of growth */
    SVECTOR map_max;
    effect_ring_mesh_work_t* work;
    effect_ring_mesh_geometry_view_t* entry;
    s32 radius;
    s32 u;
    s32 v;
    s32 u_span;
    s32 v_span;
    s32 buffer;
    u32* ot;
    effect_state_t* state;
    effect_record_t* record;
    effect_ring_mesh_scratch_t* scratch;
    s32 abe;
    POLY_GT4* quad;
    POLY_GT4* other;
    POLY_GT4* quads;
    u16 placement;
    s32 translucent;
    u16 tpage;
    s32 target_index;
    s32 j;
    s32 i;
    s32 k;
    s32 n;
    s32 angle;
    s32 ring_radius;
    s32 sine;
    u32 blue;
    u32 green;
    u32 red;
    map_tile_t* tile;

    record_index = (s16)record_index;
    record = &g_effect_state_records[record_index];
    entry = (effect_ring_mesh_geometry_view_t*)&g_effect_geometry_table->entries[geometry_index];
    placement = entry->placement_flags;
    ot = main_gfx_get_otag();
    state = (effect_state_t*)((u8*)record + byte_offset);
    scratch = (effect_ring_mesh_scratch_t*)0x1f800000;

    switch (state->phase) {
    case EFFECT_PHASE_IDLE:
        break;

    case EFFECT_PHASE_INIT:
        if (record->work_slots[byte_offset] != 0) {
            battle_heap_free_block(record->work_slots[byte_offset]);
        }
        work = battle_heap_alloc_block(sizeof(effect_ring_mesh_work_t), record_index);
        record->work_slots[byte_offset] = work;
        abe = entry->texture_page_flags & 4;
        tpage = ((entry->texture_page_flags & 3) << 5) | 0x86;
        translucent = abe == 0;
        for (j = 0; j < 2; j++) {
            for (i = 0; i < RING_MESH_SEGMENTS * 2; i++) {
                quad = &work->quads[j][i];
                SetPolyGT4(quad);
                if (translucent) {
                    quad->code |= 2;
                } else {
                    quad->code &= ~2;
                }
                quad->tpage = tpage;
                quad->clut = 0x7b00;
            }
        }
        work->spin = 0;
        work->v_scroll = 0;
        work->buffer = 0;
        work->radius_offset = 0;
        work->radius_speed = 0;
        work->destroy_delay = 0;
        ((effect_state_t*)((u8*)record + byte_offset))->phase = EFFECT_PHASE_UPDATE;
        break;

    case EFFECT_PHASE_UPDATE:
        work = record->work_slots[byte_offset];
        target_index = g_effect_state_records[record_index].target_index;
        READ_CURVE(j, i, entry->scroll_curves & 0xf, frame);
        battle_effect_interpolate_emitter_spawn_position(entry, j, &origin.vx);
        switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
        case EFFECT_EMITTER_ORIGIN_WORLD:
            break;
        case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
            tile = battle_map_get_tile_data_pointer(g_battle_effect_target_tile.id.tile_x,
                g_battle_effect_target_tile.tile_y, g_battle_effect_target_tile.map_z);
            origin.vx += g_battle_effect_target_tile.id.tile_x * 28 + 14;
            origin.vy -= tile->height * 12;
            origin.vz += g_battle_effect_target_tile.tile_y * 28 + 14;
            break;
        case EFFECT_EMITTER_ORIGIN_CASTER:
            battle_effect_resolve_target_render_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, -1,
                &g_battle_effect_coord_data, &scratch->vertex);
            origin.vx += scratch->vertex.vx;
            origin.vy += scratch->vertex.vy;
            origin.vz += scratch->vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_TARGET:
            battle_effect_resolve_target_render_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, target_index,
                &g_battle_effect_coord_data, &scratch->vertex);
            origin.vx += scratch->vertex.vx;
            origin.vy += scratch->vertex.vy;
            origin.vz += scratch->vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
            break;
        case EFFECT_EMITTER_ORIGIN_MAP_CENTRE:
            battle_map_store_max_coordinates(&map_max);
            origin.vx += map_max.vx * 14;
            origin.vz += map_max.vz * 14;
            break;
        }

        READ_CURVE(j, i, (entry->wave_curves >> 16) & 0xf, frame);
        radius = battle_effect_lerp_linear(entry->radius_start, entry->radius_end, j);

        READ_CURVE(j, i, (entry->scroll_curves >> 8) & 0xf, frame);
        u = battle_effect_lerp_linear(entry->u_start, entry->u_end, j);
        v = battle_effect_lerp_linear(entry->v_start, entry->v_end, j);
        u_span = entry->u_span;
        v_span = entry->v_span;
        work->v_scroll += entry->v_scroll_speed;
        if (work->v_scroll > v_span << 8) {
            work->v_scroll -= v_span << 8;
        }
        if (work->v_scroll < 0) {
            work->v_scroll += v_span << 8;
        }

        READ_CURVE(j, i, entry->wave_curves & 0xf, frame);
        growth.vx = battle_effect_lerp_linear(entry->growth_start, entry->growth_end, j);
        growth.vy = battle_effect_lerp_linear(entry->height_start, entry->height_end, j) << 2;
        growth.vz = battle_effect_lerp_linear(entry->row_radius_start, entry->row_radius_end, j);

        READ_CURVE(j, i, (entry->wave_curves >> 4) & 0xf, frame);
        growth_step.vx = battle_effect_lerp_linear(entry->growth_step_start, entry->growth_step_end, j);
        growth_step.vy = battle_effect_lerp_linear(entry->height_step_start, entry->height_step_end, j) << 2;
        growth_step.vz = battle_effect_lerp_linear(entry->row_radius_step_start, entry->row_radius_step_end, j);

        READ_CURVE(j, i, entry->scroll_curves >> 28, frame);
        work->spin
            = (work->spin + battle_effect_lerp_linear(entry->spin_speed_start, entry->spin_speed_end, j)) & 0xfff;
        /* The target makes these four calls and discards their results. */
        rsin(work->spin);
        rsin(work->spin + 0x200);
        rcos(work->spin);
        rcos(work->spin + 0x200);

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
                angle += 0x80;
            }
            growth.vy += growth_step.vy;
            growth.vz += growth_step.vz;
            n += growth.vy;
            k += growth.vz;
        }
        work->radius_speed += growth_step.vx;
        work->radius_offset += growth.vx + work->radius_speed;

        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        buffer = work->buffer;
        scratch->origin.vx = origin.vx;
        scratch->origin.vy = origin.vy;
        scratch->origin.vz = origin.vz;

        /* Ring 0: quad i - 1 takes lattice rows 1 (x0/x1) and 0 (x2/x3). */
        battle_effect_add_vectors_to_svector(&scratch->origin, &scratch->points[0][0], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[2], &scratch->screen[2].pad);
        quad = &work->quads[buffer][0];
        quad->x2 = scratch->screen[2].vx;
        quad->y2 = scratch->screen[2].vy;
        scratch->depth[0][0] = scratch->screen[2].vz;
        battle_effect_add_vectors_to_svector(&scratch->origin, &scratch->points[0][1], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[3], &scratch->screen[3].pad);
        quad->x3 = scratch->screen[3].vx;
        quad->y3 = scratch->screen[3].vy;
        scratch->depth[0][1] = scratch->screen[3].vz;
        battle_effect_add_vectors_to_svector(&scratch->origin, &scratch->points[1][0], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[0], &scratch->screen[0].pad);
        quad->x0 = scratch->screen[0].vx;
        quad->y0 = scratch->screen[0].vy;
        scratch->depth[1][0] = scratch->screen[0].vz;
        battle_effect_add_vectors_to_svector(&scratch->origin, &scratch->points[1][1], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
        quad->x1 = scratch->screen[1].vx;
        quad->y1 = scratch->screen[1].vy;
        scratch->depth[1][1] = scratch->screen[1].vz;

        for (i = 2; i < 9; i++) {
            quad = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = scratch->screen[3].vx;
            quad->y2 = scratch->screen[3].vy;
            battle_effect_add_vectors_to_svector(&scratch->origin, &scratch->points[0][i], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[3], &scratch->screen[3].pad);
            quad->x3 = scratch->screen[3].vx;
            quad->y3 = scratch->screen[3].vy;
            scratch->depth[0][i] = scratch->screen[3].vz;
            battle_effect_add_vectors_to_svector(&scratch->origin, &scratch->points[1][i], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[1][i] = scratch->screen[1].vz;
        }
        for (i = 9; i < 17; i++) {
            quad = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = scratch->screen[3].vx;
            quad->y2 = scratch->screen[3].vy;
            battle_effect_add_vectors_to_svector_rot_y_270(
                &scratch->origin, &scratch->points[0][i - 8], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[3], &scratch->screen[3].pad);
            quad->x3 = scratch->screen[3].vx;
            quad->y3 = scratch->screen[3].vy;
            scratch->depth[0][i] = scratch->screen[3].vz;
            battle_effect_add_vectors_to_svector_rot_y_270(
                &scratch->origin, &scratch->points[1][i - 8], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[1][i] = scratch->screen[1].vz;
        }
        for (i = 17; i < 25; i++) {
            quad = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = scratch->screen[3].vx;
            quad->y2 = scratch->screen[3].vy;
            battle_effect_add_vectors_to_svector_rot_y_180(
                &scratch->origin, &scratch->points[0][i - 16], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[3], &scratch->screen[3].pad);
            quad->x3 = scratch->screen[3].vx;
            quad->y3 = scratch->screen[3].vy;
            scratch->depth[0][i] = scratch->screen[3].vz;
            battle_effect_add_vectors_to_svector_rot_y_180(
                &scratch->origin, &scratch->points[1][i - 16], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[1][i] = scratch->screen[1].vz;
        }
        for (i = 25; i < 33; i++) {
            quad = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = scratch->screen[3].vx;
            quad->y2 = scratch->screen[3].vy;
            battle_effect_add_vectors_to_svector_rot_y_90(
                &scratch->origin, &scratch->points[0][i - 24], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[3], &scratch->screen[3].pad);
            quad->x3 = scratch->screen[3].vx;
            quad->y3 = scratch->screen[3].vy;
            scratch->depth[0][i] = scratch->screen[3].vz;
            battle_effect_add_vectors_to_svector_rot_y_90(
                &scratch->origin, &scratch->points[1][i - 24], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[1][i] = scratch->screen[1].vz;
        }

        /* Ring 1 reuses the lower corners of ring 0 and adds lattice row 2. */
        battle_effect_add_vectors_to_svector(&scratch->origin, &scratch->points[2][0], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[0], &scratch->screen[0].pad);
        quads = work->quads[buffer];
        quads[RING_MESH_SEGMENTS].x0 = scratch->screen[0].vx;
        quads[RING_MESH_SEGMENTS].y0 = scratch->screen[0].vy;
        scratch->depth[2][0] = scratch->screen[0].vz;
        battle_effect_add_vectors_to_svector(&scratch->origin, &scratch->points[2][1], &scratch->vertex);
        RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
        quads[RING_MESH_SEGMENTS].x1 = scratch->screen[1].vx;
        quads[RING_MESH_SEGMENTS].y1 = scratch->screen[1].vy;
        scratch->depth[2][1] = scratch->screen[1].vz;
        other = quads;
        quads[RING_MESH_SEGMENTS].x2 = other->x0;
        quads[RING_MESH_SEGMENTS].y2 = other->y0;
        quads[RING_MESH_SEGMENTS].x3 = other->x1;
        quads[RING_MESH_SEGMENTS].y3 = other->y1;

        for (i = 2; i < 9; i++) {
            quad = &work->quads[buffer][i + 31];
            other = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            battle_effect_add_vectors_to_svector(&scratch->origin, &scratch->points[2][i], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[2][i] = scratch->screen[1].vz;
        }
        for (i = 9; i < 17; i++) {
            quad = &work->quads[buffer][i + 31];
            other = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            battle_effect_add_vectors_to_svector_rot_y_270(
                &scratch->origin, &scratch->points[2][i - 8], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[2][i] = scratch->screen[1].vz;
        }
        for (i = 17; i < 25; i++) {
            quad = &work->quads[buffer][i + 31];
            other = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            battle_effect_add_vectors_to_svector_rot_y_180(
                &scratch->origin, &scratch->points[2][i - 16], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[2][i] = scratch->screen[1].vz;
        }
        for (i = 25; i < 33; i++) {
            quad = &work->quads[buffer][i + 31];
            other = &work->quads[buffer][i - 1];
            quad->x0 = scratch->screen[1].vx;
            quad->y0 = scratch->screen[1].vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            battle_effect_add_vectors_to_svector_rot_y_90(
                &scratch->origin, &scratch->points[2][i - 24], &scratch->vertex);
            RotTrans(&scratch->vertex, &scratch->screen[1], &scratch->screen[1].pad);
            quad->x1 = scratch->screen[1].vx;
            quad->y1 = scratch->screen[1].vy;
            scratch->depth[2][i] = scratch->screen[1].vz;
        }

        j = work->v_scroll >> 8;
        for (i = 0; i < RING_MESH_SEGMENTS * 2; i++) {
            quad = &work->quads[buffer][i];
            quad->u0 = quad->u2 = u;
            quad->v0 = quad->v1 = v + j;
            quad->u1 = quad->u3 = u + u_span;
            quad->v2 = quad->v3 = v + v_span + j;
        }

        n = entry->brightness_row;
        blue = 0x80;
        if (entry->flags & EFFECT_GEOMETRY_FLAG_COLOUR_CURVES) {
            k = entry->colour_curves & 0xfff;
            red = g_effect_palette_table[k & 0xf].factor[frame];
            green = g_effect_palette_table[(k >> 4) & 0xf].factor[frame];
            blue = g_effect_palette_table[(u32)k >> 8].factor[frame];
        } else {
            green = 0x80;
            red = 0x80;
        }
        k = g_effect_e461_ring_mesh_brightness_table[n][0];
        for (i = 0; i < RING_MESH_SEGMENTS; i++) {
            quad = &work->quads[buffer][i];
            quad->r3 = (k * red) >> 12;
            quad->r2 = (k * red) >> 12;
            quad->g3 = (k * green) >> 12;
            quad->g2 = (k * green) >> 12;
            quad->b3 = (k * blue) >> 12;
            quad->b2 = (k * blue) >> 12;
        }
        k = g_effect_e461_ring_mesh_brightness_table[n][1];
        /* The redundant quad++ keeps quad from being strength-reduced into a
         * pointer of its own; the target recomputes it from the base. */
        for (i = 0; i < RING_MESH_SEGMENTS; i++, quad++) {
            quad = &work->quads[buffer][i];
            other = &work->quads[buffer][i + RING_MESH_SEGMENTS];
            other->r3 = (k * red) >> 12;
            other->r2 = (k * red) >> 12;
            quad->r1 = (k * red) >> 12;
            quad->r0 = (k * red) >> 12;
            other->g3 = (k * green) >> 12;
            other->g2 = (k * green) >> 12;
            quad->g1 = (k * green) >> 12;
            quad->g0 = (k * green) >> 12;
            other->b3 = (k * blue) >> 12;
            other->b2 = (k * blue) >> 12;
            quad->b1 = (k * blue) >> 12;
            quad->b0 = (k * blue) >> 12;
        }
        k = g_effect_e461_ring_mesh_brightness_table[n][2];
        for (i = 0; i < RING_MESH_SEGMENTS; i++) {
            quad = &work->quads[buffer][i + RING_MESH_SEGMENTS];
            quad->r1 = (k * red) >> 12;
            quad->r0 = (k * red) >> 12;
            quad->g1 = (k * green) >> 12;
            quad->g0 = (k * green) >> 12;
            quad->b1 = (k * blue) >> 12;
            quad->b0 = (k * blue) >> 12;
        }

        for (j = 0; j < 2; j++) {
            for (i = 0; i < RING_MESH_SEGMENTS; i++) {
                n = (scratch->depth[j][i] + scratch->depth[j][i + 1] + scratch->depth[j + 1][i]
                        + scratch->depth[j + 1][i + 1])
                    / 16;
                if ((u32)n < EFFECT_OT_DEPTH_LIMIT) {
                    setaddr(&work->quads[buffer][j * RING_MESH_SEGMENTS + i], getaddr(&ot[n]));
                    setaddr(&ot[n], &work->quads[buffer][j * RING_MESH_SEGMENTS + i]);
                }
            }
        }
        work->buffer = 1 - work->buffer;
        break;

    case EFFECT_PHASE_DESTROY:
        work = record->work_slots[byte_offset];
        if (work->destroy_delay == 0) {
            work->destroy_delay = 1;
            break;
        }
        if (work != 0) {
            battle_heap_free_block(work);
            record->work_slots[byte_offset] = 0;
        }
        state->phase = EFFECT_PHASE_IDLE;
        break;
    }
}
