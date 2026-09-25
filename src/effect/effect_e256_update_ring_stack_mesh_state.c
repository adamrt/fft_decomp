#include "fft/effect.h"
#include "psx/gte_inline.h"

/* Builds and draws a spinning stack of 8 rings of 8 textured quads.
 *
 * The 5,368-byte sibling of the ring mesh renderer (effect_ring_mesh.h), using
 * the same geometry view and work block with the 64 quads of each buffer
 * indexed ring * 8 + segment. Nine lattice rows of eight points (45 degrees
 * apart, mirrored from two rsin/rcos pairs) grow in radius and height by
 * per-row steps. Each point is offset by the placement origin, transformed by
 * the battle camera through the inline GTE, and linked into the ordering
 * table by the average depth of each quad. The texture scrolls in v and each
 * lattice row takes its brightness from g_effect_e256_ring_stack_mesh_brightness_table.
 *
 * `i`, `j`, `k` and `n` are shared temporaries as in the ring mesh renderer.
 * The projected corners are four separate VECTORs: as one array, the unused
 * base address of &screen[2] is CSE'd into the later &screen[0] operand. The
 * colour stores are chained so that loop.c keeps the blue product in the
 * inner loop, as in the target. */
s32 effect_e256_update_ring_stack_mesh_state(s32 record_index, s32 byte_offset, s32 geometry_index, s32 frame) {
    SVECTOR vertex;
    SVECTOR unused_18[3]; /* frame padding the target reserves but never touches */
    VECTOR growth;
    VECTOR growth_step;
    VECTOR points[9][8];
    VECTOR screen0;
    VECTOR screen1;
    VECTOR screen2;
    VECTOR screen3;
    s16 depth[10][8]; /* row 9 is never written */
    VECTOR origin;
    VECTOR unused_5c0[3]; /* frame padding */
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
    effect_record_t* record;
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
    s32 sin0; /* rsin(spin) */
    s32 sin1; /* rsin(spin + 45 degrees) */
    s32 cos0;
    s32 cos1;
    s32 ox;
    s32 oy;
    s32 oz;
    u32 blue;
    u32 green;
    u32 red;
    map_tile_t* tile;

    record_index = (s16)record_index;
    record = &g_effect_state_records[record_index];
    entry = (effect_ring_mesh_geometry_view_t*)&g_effect_geometry_table->entries[geometry_index];
    placement = entry->placement_flags;
    ot = main_gfx_get_otag();

    switch (record->phase[byte_offset]) {
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
            for (i = 0; i < 64; i++) {
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
        record->phase[byte_offset] = EFFECT_PHASE_UPDATE;
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
                &g_battle_effect_coord_data, &vertex);
            origin.vx += vertex.vx;
            origin.vy += vertex.vy;
            origin.vz += vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_TARGET:
            battle_effect_resolve_target_render_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, target_index,
                &g_battle_effect_coord_data, &vertex);
            origin.vx += vertex.vx;
            origin.vy += vertex.vy;
            origin.vz += vertex.vz;
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
        growth.vy = battle_effect_lerp_linear(entry->height_start, entry->height_end, j);
        growth.vz = battle_effect_lerp_linear(entry->row_radius_start, entry->row_radius_end, j);

        READ_CURVE(j, i, (entry->wave_curves >> 4) & 0xf, frame);
        growth_step.vx = battle_effect_lerp_linear(entry->growth_step_start, entry->growth_step_end, j);
        growth_step.vy = battle_effect_lerp_linear(entry->height_step_start, entry->height_step_end, j);
        growth_step.vz = battle_effect_lerp_linear(entry->row_radius_step_start, entry->row_radius_step_end, j);

        READ_CURVE(j, i, entry->scroll_curves >> 28, frame);
        work->spin
            = (work->spin + battle_effect_lerp_linear(entry->spin_speed_start, entry->spin_speed_end, j)) & 0xfff;
        sin0 = rsin(work->spin);
        sin1 = rsin(work->spin + 0x200);
        cos0 = rcos(work->spin);
        cos1 = rcos(work->spin + 0x200);

        n = 0;
        k = 0;
        for (i = 0; i < 9; i++) {
            points[i][0].vx = points[i][2].vz = (cos0 * (radius + ((k + work->radius_offset) >> 8))) >> 12;
            points[i][4].vx = points[i][6].vz = -points[i][0].vx;
            points[i][0].vz = points[i][6].vx = (sin0 * (radius + ((k + work->radius_offset) >> 8))) >> 12;
            points[i][2].vx = points[i][4].vz = -points[i][0].vz;
            points[i][1].vx = points[i][3].vz = (cos1 * (radius + ((k + work->radius_offset) >> 8))) >> 12;
            points[i][5].vx = points[i][7].vz = -points[i][1].vx;
            points[i][1].vz = points[i][7].vx = (sin1 * (radius + ((k + work->radius_offset) >> 8))) >> 12;
            points[i][3].vx = points[i][5].vz = -points[i][1].vz;
            points[i][0].vy = points[i][1].vy = points[i][2].vy = points[i][3].vy = points[i][4].vy = points[i][5].vy
                = points[i][6].vy = points[i][7].vy = n >> 8;
            growth.vy += growth_step.vy;
            growth.vz += growth_step.vz;
            n += growth.vy;
            k += growth.vz;
        }
        work->radius_speed += growth_step.vx;
        work->radius_offset += growth.vx + work->radius_speed;

        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);
        ox = origin.vx;
        buffer = work->buffer;
        oy = origin.vy;
        oz = origin.vz;

        /* Ring 0: quad i - 1 takes lattice rows 1 (x0/x1) and 0 (x2/x3); quad 7
         * closes the ring on quad 0. */
        vertex.vx = points[0][0].vx + ox;
        vertex.vy = points[0][0].vy + oy;
        vertex.vz = points[0][0].vz + oz;
        gte_RotTrans_split(&vertex, &screen2, &screen2.pad);
        quad = &work->quads[buffer][0];
        quad->x2 = screen2.vx;
        quad->y2 = screen2.vy;
        depth[0][0] = screen2.vz;
        vertex.vx = points[0][1].vx + ox;
        vertex.vy = points[0][1].vy + oy;
        vertex.vz = points[0][1].vz + oz;
        gte_RotTrans_split(&vertex, &screen3, &screen3.pad);
        quad->x3 = screen3.vx;
        quad->y3 = screen3.vy;
        depth[0][1] = screen3.vz;
        vertex.vx = points[1][0].vx + ox;
        vertex.vy = points[1][0].vy + oy;
        vertex.vz = points[1][0].vz + oz;
        gte_RotTrans_split(&vertex, &screen0, &screen0.pad);
        quad->x0 = screen0.vx;
        quad->y0 = screen0.vy;
        depth[1][0] = screen0.vz;
        vertex.vx = points[1][1].vx + ox;
        vertex.vy = points[1][1].vy + oy;
        vertex.vz = points[1][1].vz + oz;
        gte_RotTrans_split(&vertex, &screen1, &screen1.pad);
        quad->x1 = screen1.vx;
        quad->y1 = screen1.vy;
        depth[1][1] = screen1.vz;

        for (i = 2; i < 8; i++) {
            quad = &work->quads[buffer][i - 1];
            quad->x0 = screen1.vx;
            quad->y0 = screen1.vy;
            quad->x2 = screen3.vx;
            quad->y2 = screen3.vy;
            vertex.vx = points[0][i].vx + ox;
            vertex.vy = points[0][i].vy + oy;
            vertex.vz = points[0][i].vz + oz;
            gte_RotTrans_split(&vertex, &screen3, &screen3.pad);
            quad->x3 = screen3.vx;
            quad->y3 = screen3.vy;
            depth[0][i] = screen3.vz;
            vertex.vx = points[1][i].vx + ox;
            vertex.vy = points[1][i].vy + oy;
            vertex.vz = points[1][i].vz + oz;
            gte_RotTrans_split(&vertex, &screen1, &screen1.pad);
            quad->x1 = screen1.vx;
            quad->y1 = screen1.vy;
            depth[1][i] = screen1.vz;
        }

        quads = work->quads[buffer];
        quads[7].x0 = screen1.vx;
        quads[7].y0 = screen1.vy;
        quads[7].x2 = screen3.vx;
        quads[7].y2 = screen3.vy;
        other = quads;
        quads[7].x1 = other->x0;
        quads[7].y1 = other->y0;
        quads[7].x3 = other->x2;
        quads[7].y3 = other->y2;

        /* Ring j reuses the lower corners of ring j - 1 and adds lattice row
         * j + 1. */
        for (j = 1; j < 8; j++) {
            n = j + 1;

            vertex.vx = points[n][0].vx + ox;
            vertex.vy = points[n][0].vy + oy;
            vertex.vz = points[n][0].vz + oz;
            gte_RotTrans_split(&vertex, &screen0, &screen0.pad);
            quad = &work->quads[buffer][j * 8];
            quad->x0 = screen0.vx;
            quad->y0 = screen0.vy;
            depth[n][0] = screen0.vz;
            vertex.vx = points[n][1].vx + ox;
            vertex.vy = points[n][1].vy + oy;
            vertex.vz = points[n][1].vz + oz;
            gte_RotTrans_split(&vertex, &screen1, &screen1.pad);
            quad->x1 = screen1.vx;
            quad->y1 = screen1.vy;
            depth[n][1] = screen1.vz;
            other = &work->quads[buffer][(j - 1) * 8];
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            for (i = 2; i < 8; i++) {
                quad = &work->quads[buffer][j * 8 + i - 1];
                quad->x0 = screen1.vx;
                quad->y0 = screen1.vy;
                other = &work->quads[buffer][(j - 1) * 8 + i - 1];
                quad->x2 = other->x0;
                quad->y2 = other->y0;
                quad->x3 = other->x1;
                quad->y3 = other->y1;
                vertex.vx = points[n][i].vx + ox;
                vertex.vy = points[n][i].vy + oy;
                vertex.vz = points[n][i].vz + oz;
                gte_RotTrans_split(&vertex, &screen1, &screen1.pad);
                quad->x1 = screen1.vx;
                quad->y1 = screen1.vy;
                depth[n][i] = screen1.vz;
            }
            quad = &work->quads[buffer][j * 8 + 7];
            other = &work->quads[buffer][(j - 1) * 8 + 7];
            quad->x0 = screen1.vx;
            quad->y0 = screen1.vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            other = &work->quads[buffer][j * 8];
            quad->x1 = other->x0;
            quad->y1 = other->y0;
        }

        j = work->v_scroll >> 8;
        for (i = 0; i < 64; i++) {
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
        k = g_effect_e256_ring_stack_mesh_brightness_table[n][0];
        for (i = 0; i < 8; i++) {
            quad = &work->quads[buffer][i];
            quad->r3 = (k * red) >> 12;
            quad->r2 = (k * red) >> 12;
            quad->g3 = (k * green) >> 12;
            quad->g2 = (k * green) >> 12;
            quad->b3 = (k * blue) >> 12;
            quad->b2 = (k * blue) >> 12;
        }
        for (j = 0; j < 7; j++) {
            k = g_effect_e256_ring_stack_mesh_brightness_table[n][j + 1];
            for (i = 0; i < 8; i++) {
                quad = &work->quads[buffer][j * 8 + i];
                other = &work->quads[buffer][(j + 1) * 8 + i];
                quad->r0 = quad->r1 = other->r2 = other->r3 = (k * red) >> 12;
                quad->g0 = quad->g1 = other->g2 = other->g3 = (k * green) >> 12;
                quad->b0 = quad->b1 = other->b2 = other->b3 = (k * blue) >> 12;
            }
        }
        k = g_effect_e256_ring_stack_mesh_brightness_table[n][8];
        for (i = 0; i < 8; i++) {
            quad = &work->quads[buffer][i + 56];
            quad->r1 = (k * red) >> 12;
            quad->r0 = (k * red) >> 12;
            quad->g1 = (k * green) >> 12;
            quad->g0 = (k * green) >> 12;
            quad->b1 = (k * blue) >> 12;
            quad->b0 = (k * blue) >> 12;
        }

        for (j = 0; j < 8; j++) {
            for (i = 0; i < 7; i++) {
                n = (depth[j][i] + depth[j][i + 1] + depth[j + 1][i] + depth[j + 1][i + 1]) / 16;
                if ((u32)n < EFFECT_OT_DEPTH_LIMIT) {
                    setaddr(&work->quads[buffer][j * 8 + i], getaddr(&ot[n]));
                    setaddr(&ot[n], &work->quads[buffer][j * 8 + i]);
                }
            }
            n = (depth[j][0] + depth[j][7] + depth[j + 1][0] + depth[j + 1][7]) / 16;
            if ((u32)n < EFFECT_OT_DEPTH_LIMIT) {
                setaddr(&work->quads[buffer][j * 8 + 7], getaddr(&ot[n]));
                setaddr(&ot[n], &work->quads[buffer][j * 8 + 7]);
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
        record->phase[byte_offset] = EFFECT_PHASE_IDLE;
        break;
    }
}
