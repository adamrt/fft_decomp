#include "fft/effect.h"
#include "psx/gpu.h"

/* Builds and draws a spinning dome of 8x16 textured quads.
 *
 * Nine lattice rows at 0x80 angle steps shrink the ring radius by the cosine
 * and raise the row by the scaled sine; seventeen points per row close the
 * ring at 0x100 steps. The lattice is written to the scratchpad, offset by the
 * placement origin, projected through the battle camera, and linked into the
 * ordering table by the average depth of each quad. The texture scrolls in v
 * and every lattice row takes its brightness from g_effect_e474_summon_mesh_brightness_table. The quads are
 * double-buffered in the work block.
 *
 * `i`, `j`, `k` and `n` are shared temporaries (`i`: curve factor, v scroll
 * and a loop index; `j`: curve index and a loop index; `k`: ring radius,
 * colour curves and brightness; `n`: row height, lattice row, brightness row
 * and quad depth): the target keeps each group in one register. The handler
 * returns no value, but the s32 return keeps $v0 live at the exit, which
 * leaves the phase dispatch's delay slots empty as in the target. */
s32 effect_e477_update_summon_mesh_state(s16 record_index, s32 byte_offset, s32 geometry_index, s32 frame) {
    SVECTOR vertex;
    SVECTOR unused_18[2]; /* frame padding the target reserves but never touches */
    VECTOR origin;
    VECTOR growth;      /* vx: radius speed step, vy: height scale of the current row */
    VECTOR growth_step; /* vx: radius acceleration, vy: height scale step per row */
    VECTOR screen[4];   /* projected quad corners; RotTrans writes its flag to pad */
    s16 depth[9][17];
    SVECTOR map_max;
    effect_record_t* record;
    effect_summon_mesh_geometry_view_t* entry;
    effect_summon_mesh_work_t* work;
    POLY_GT4* quad;
    POLY_GT4* other;
    effect_summon_mesh_point_t* lattice_point;
    map_tile_t* tile;
    u8* lattice_bytes;
    u16 placement;
    u8 target_index;
    s32 k;
    s32 j;
    u16 tpage;
    u16 texture_page_flags;
    s32 abe;
    s32 translucent;
    s32 radius;
    s32 u;
    s32 v;
    s32 u_span;
    s32 v_span;
    u32* ot;
    s32 i;
    u32 red;
    u32 green;
    u32 blue;
    u32 shade_r;
    u32 shade_g;
    u32 shade_b;
    s32 n;

    record = &g_effect_state_records[record_index];
    entry = (effect_summon_mesh_geometry_view_t*)&g_effect_geometry_table->entries[geometry_index];
    placement = entry->placement_flags;
    ot = main_gfx_get_otag();

    switch (record->phase[byte_offset]) {
    case EFFECT_PHASE_IDLE:
        break;

    case EFFECT_PHASE_INIT:
        if (record->work_slots[byte_offset] != 0) {
            battle_heap_free_block(record->work_slots[byte_offset]);
        }
        work = battle_heap_alloc_block(sizeof(effect_summon_mesh_work_t), record_index);
        record->work_slots[byte_offset] = work;
        texture_page_flags = entry->texture_page_flags;
        tpage = ((texture_page_flags & 3) << 5) | 0x86;
        abe = texture_page_flags & 4;
        translucent = abe == 0;
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 128; j++) {
                quad = &work->quads[i][j];
                SetPolyGT4(quad);
                if (translucent) {
                    quad->code |= 2;
                } else {
                    quad->code &= ~2;
                }
                quad->tpage = tpage;
                quad->clut = 0x7B00;
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
        target_index = g_effect_state_records[record_index].target_index;
        work = record->work_slots[byte_offset];
        READ_CURVE(i, j, entry->scroll_curves & 0xF, frame);
        origin.vx = battle_effect_lerp_linear(entry->origin_start[0], entry->origin_end[0], i);
        origin.vy = battle_effect_lerp_linear(entry->origin_start[1], entry->origin_end[1], i);
        origin.vz = battle_effect_lerp_linear(entry->origin_start[2], entry->origin_end[2], i);
        switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
        case EFFECT_EMITTER_ORIGIN_WORLD:
        case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
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
        case EFFECT_EMITTER_ORIGIN_MAP_CENTRE:
            battle_map_store_max_coordinates(&map_max);
            origin.vx += map_max.vx * 14;
            origin.vz += map_max.vz * 14;
            break;
        }

        READ_CURVE(i, j, entry->wave_curves.half.high & 0xF, frame);
        radius = battle_effect_lerp_linear(entry->radius_start, entry->radius_end, i);

        READ_CURVE(i, j, (entry->scroll_curves >> 8) & 0xF, frame);
        u = battle_effect_lerp_linear(entry->u_start, entry->u_end, i);
        v = battle_effect_lerp_linear(entry->v_start, entry->v_end, i);
        u_span = entry->u_span;
        v_span = entry->v_span;
        work->v_scroll += entry->v_scroll_speed;
        if ((s16)work->v_scroll > (v_span << 8)) {
            work->v_scroll -= v_span << 8;
        }
        if ((s16)work->v_scroll < 0) {
            work->v_scroll += v_span << 8;
        }

        READ_CURVE(i, j, entry->wave_curves.word & 0xF, frame);
        growth.vx = battle_effect_lerp_linear(entry->growth_start, entry->growth_end, i);
        growth.vy = battle_effect_lerp_linear(entry->height_start, entry->height_end, i);

        READ_CURVE(i, j, (entry->wave_curves.word >> 4) & 0xF, frame);
        growth_step.vx = battle_effect_lerp_linear(entry->growth_step_start, entry->growth_step_end, i);
        growth_step.vy = battle_effect_lerp_linear(entry->height_step_start, entry->height_step_end, i);

        READ_CURVE(i, j, entry->scroll_curves >> 28, frame);
        work->spin
            = (work->spin + battle_effect_lerp_linear(entry->spin_speed_start, entry->spin_speed_end, i)) & 0xFFF;

        /* The base stays in a variable: the target adds it to the row offset
         * after each call instead of folding it into the induction variable. */
        lattice_bytes = (u8*)0x1F800000;
        for (j = 0; j < 9; j++) {
            k = ((radius + (work->radius_offset >> 8)) * rcos(j << 7)) >> 12;
            n = ((((radius + (work->radius_offset >> 8)) * rsin(j << 7)) >> 12) * growth.vy) >> 8;
            for (i = 0; i < 17; i++) {
                lattice_point = (effect_summon_mesh_point_t*)(lattice_bytes + (j * 0x66 + i * 6));
                lattice_point->x = (rcos(work->spin + (i << 8)) * k) >> 12;
                lattice_point->z = (rsin(work->spin + (i << 8)) * k) >> 12;
                lattice_point->y = -n;
            }
            growth.vy += growth_step.vy;
        }

        work->radius_speed += growth_step.vx;
        work->radius_offset += growth.vx + work->radius_speed;
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);

        /* Row 0: quad column j - 1 takes lattice rows 1 (x0/x1) and 0 (x2/x3). */
        effect_add_scratchpad_xyz_components(0, 0, &origin, &vertex);
        RotTrans(&vertex, &screen[2], &screen[2].pad);
        quad = &work->quads[work->buffer][0];
        quad->x2 = screen[2].vx;
        quad->y2 = screen[2].vy;
        depth[0][0] = screen[2].vz;
        effect_add_scratchpad_xyz_components(0, 1, &origin, &vertex);
        RotTrans(&vertex, &screen[3], &screen[3].pad);
        quad->x3 = screen[3].vx;
        quad->y3 = screen[3].vy;
        depth[0][1] = screen[3].vz;
        effect_add_scratchpad_xyz_components(1, 0, &origin, &vertex);
        RotTrans(&vertex, &screen[0], &screen[0].pad);
        quad->x0 = screen[0].vx;
        quad->y0 = screen[0].vy;
        depth[1][0] = screen[0].vz;
        effect_add_scratchpad_xyz_components(1, 1, &origin, &vertex);
        RotTrans(&vertex, &screen[1], &screen[1].pad);
        quad->x1 = screen[1].vx;
        quad->y1 = screen[1].vy;
        depth[1][1] = screen[1].vz;
        for (j = 2; j < 17; j++) {
            quad = &work->quads[work->buffer][j - 1];
            quad->x0 = screen[1].vx;
            quad->y0 = screen[1].vy;
            quad->x2 = screen[3].vx;
            quad->y2 = screen[3].vy;
            effect_add_scratchpad_xyz_components(0, j, &origin, &vertex);
            RotTrans(&vertex, &screen[3], &screen[3].pad);
            quad->x3 = screen[3].vx;
            quad->y3 = screen[3].vy;
            depth[0][j] = screen[3].vz;
            effect_add_scratchpad_xyz_components(1, j, &origin, &vertex);
            RotTrans(&vertex, &screen[1], &screen[1].pad);
            quad->x1 = screen[1].vx;
            quad->y1 = screen[1].vy;
            depth[1][j] = screen[1].vz;
        }

        /* Rows 1-7 reuse the lower corners of the row above; the last column
         * closes the ring on column 0. */
        for (i = 1; i < 8; i++) {
            n = i + 1;
            effect_add_scratchpad_xyz_components(n, 0, &origin, &vertex);
            RotTrans(&vertex, &screen[0], &screen[0].pad);
            quad = &work->quads[work->buffer][i * 16];
            quad->x0 = screen[0].vx;
            quad->y0 = screen[0].vy;
            depth[n][0] = screen[0].vz;
            effect_add_scratchpad_xyz_components(n, 1, &origin, &vertex);
            RotTrans(&vertex, &screen[1], &screen[1].pad);
            quad->x1 = screen[1].vx;
            quad->y1 = screen[1].vy;
            depth[n][1] = screen[1].vz;
            other = &work->quads[work->buffer][(i - 1) * 16];
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            for (j = 2; j < 16; j++) {
                quad = &work->quads[work->buffer][i * 16 + j - 1];
                other = &work->quads[work->buffer][(i - 1) * 16 + j - 1];
                quad->x0 = screen[1].vx;
                quad->y0 = screen[1].vy;
                quad->x2 = other->x0;
                quad->y2 = other->y0;
                quad->x3 = other->x1;
                quad->y3 = other->y1;
                effect_add_scratchpad_xyz_components(n, j, &origin, &vertex);
                RotTrans(&vertex, &screen[1], &screen[1].pad);
                quad->x1 = screen[1].vx;
                quad->y1 = screen[1].vy;
                depth[n][j] = screen[1].vz;
            }
            quad = &work->quads[work->buffer][i * 16 + 15];
            other = &work->quads[work->buffer][(i - 1) * 16 + 15];
            quad->x0 = screen[1].vx;
            quad->y0 = screen[1].vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            other = &work->quads[work->buffer][i * 16];
            quad->x1 = other->x0;
            quad->y1 = other->y0;
        }

        i = (s16)work->v_scroll >> 8;
        for (j = 0; j < 128; j++) {
            quad = &work->quads[work->buffer][j];
            quad->u0 = quad->u2 = u;
            quad->v0 = quad->v1 = v + i;
            quad->u1 = quad->u3 = u + u_span;
            quad->v2 = quad->v3 = v + v_span + i;
        }

        n = entry->brightness_row;
        if (entry->flags & EFFECT_GEOMETRY_FLAG_COLOUR_CURVES) {
            red = g_effect_palette_table[entry->colour_curves & 0xF].factor[frame];
            k = entry->colour_curves & 0xFFF;
            green = g_effect_palette_table[(k >> 4) & 0xF].factor[frame];
            blue = g_effect_palette_table[(u32)k >> 8].factor[frame];
        } else {
            red = green = blue = 0x80;
        }

        k = g_effect_e474_summon_mesh_brightness_table[n][0];
        shade_r = (k * red) >> 12;
        shade_g = (k * green) >> 12;
        shade_b = (k * blue) >> 12;
        for (j = 0; j < 16; j++) {
            quad = &work->quads[work->buffer][j];
            quad->r2 = quad->r3 = shade_r;
            quad->g2 = quad->g3 = shade_g;
            quad->b2 = quad->b3 = shade_b;
        }
        for (i = 0; i < 7; i++) {
            k = g_effect_e474_summon_mesh_brightness_table[n][i + 1];
            shade_r = (k * red) >> 12;
            shade_g = (k * green) >> 12;
            shade_b = (k * blue) >> 12;
            for (j = 0; j < 16; j++) {
                quad = &work->quads[work->buffer][i * 16 + j];
                other = &work->quads[work->buffer][(i + 1) * 16 + j];
                quad->r0 = quad->r1 = other->r2 = other->r3 = shade_r;
                quad->g0 = quad->g1 = other->g2 = other->g3 = shade_g;
                quad->b0 = quad->b1 = other->b2 = other->b3 = shade_b;
            }
        }
        k = g_effect_e474_summon_mesh_brightness_table[n][8];
        shade_r = (k * red) >> 12;
        shade_g = (k * green) >> 12;
        shade_b = (k * blue) >> 12;
        for (j = 0; j < 16; j++) {
            quad = &work->quads[work->buffer][112 + j];
            quad->r0 = quad->r1 = shade_r;
            quad->g0 = quad->g1 = shade_g;
            quad->b0 = quad->b1 = shade_b;
        }

        for (i = 0; i < 8; i++) {
            for (j = 0; j < 15; j++) {
                n = (depth[i][j] + depth[i][j + 1] + depth[i + 1][j] + depth[i + 1][j + 1]) / 16;
                if ((u32)n < EFFECT_OT_DEPTH_LIMIT) {
                    addPrim(&ot[n], &work->quads[work->buffer][i * 16 + j]);
                }
            }
            n = (depth[i][0] + depth[i][15] + depth[i + 1][0] + depth[i + 1][15]) / 16;
            if ((u32)n < EFFECT_OT_DEPTH_LIMIT) {
                addPrim(&ot[n], &work->quads[work->buffer][i * 16 + 15]);
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
