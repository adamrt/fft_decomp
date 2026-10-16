/* Spinning dome mesh vertex load and update shared by twelve effect overlays. */

#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/effect.h"
#include "fft/gfx.h"
#include "fft/main_gfx.h"
#include "fft/map.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Read lattice point `point` of dome row `group` from the scratchpad and add `offset`
 * to give a vertex ready for projection. */
static void effect_dome_mesh_load_translated_vertex(s32 group, s32 point, const VECTOR* offset, SVECTOR* output) {
    int point_element = point * DOME_MESH_LATTICE_COMPONENTS;
    int element = group * ((DOME_MESH_SEGMENTS + 1) * DOME_MESH_LATTICE_COMPONENTS) + point_element;
    /* Typed point indexing caches the scratchpad base and changes the bytes. */
    u16* scratchpad = (u16*)getScratchAddr(0);

    output->vx = scratchpad[element] + offset->vx;
    output->vy = scratchpad[element + 1] + offset->vy;
    output->vz = scratchpad[element + 2] + offset->vz;
}

enum {
    DOME_MESH_LATITUDE_STEP = ONE / (DOME_MESH_ROWS * 4),
    DOME_MESH_LONGITUDE_STEP = ONE / DOME_MESH_SEGMENTS,
};

/* Builds a spinning 8x16 dome in scratchpad, projects it through the battle
 * camera and draws alternating quad buffers with scrolling texture and row
 * brightness. The shared temporaries and non-void signature preserve the
 * renderer's register allocation and phase-dispatch delay slots. */
s32 effect_dome_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
    s16 state_index = record_index;
    SVECTOR vertex;
    SVECTOR unused_18[2]; /* frame padding the target reserves but never touches */
    VECTOR origin;
    VECTOR growth;      /* vx: radius speed step, vy: height scale of the current row */
    VECTOR growth_step; /* vx: radius acceleration, vy: height scale step per row */
    VECTOR screen[4];   /* projected quad corners; RotTrans writes its flag to pad */
    s16 depth[DOME_MESH_ROWS + 1][DOME_MESH_SEGMENTS + 1];
    SVECTOR map_max;
    effect_record_t* record;
    const effect_radial_mesh_geometry_t* entry;
    effect_dome_mesh_work_t* work;
    POLY_GT4* quad;
    POLY_GT4* other;
    vec3s16_t* lattice_point;
    map_tile_t* tile;
    u8* lattice_bytes;
    u16 placement;
    u8 target_index;
    s32 k;
    s32 j;
    s32 texture_selector;
    s32 growth_step_selector;
    u16 radius_binding;
    u16 tpage;
    effect_texture_page_config_t texture_page_config;
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

    record = &g_battle_effect_state_records[state_index];
    entry = &g_battle_effect_geometry_table->entries[emitter_index].radial_mesh;
    placement = entry->placement_flags;
    ot = main_gfx_get_otag();

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            battle_heap_free_block(record->work_slots[callback_slot]);
        }
        work = battle_heap_alloc_block(sizeof(effect_dome_mesh_work_t), state_index);
        record->work_slots[callback_slot] = work;
        texture_page_config = entry->texture_page_config;
        tpage = (texture_page_config.blend_mode << EFFECT_RENDER_TEXTURE_PAGE_BLEND_SHIFT)
            | EFFECT_RENDER_TEXTURE_PAGE_BASE;
        translucent = 1;
        if (texture_page_config.clear_abe) {
            translucent = 0;
        }
        for (i = 0; i < 2; i++) {
            for (j = 0; j < DOME_MESH_QUAD_COUNT; j++) {
                quad = &work->quads[i][j];
                SetPolyGT4(quad);
                setSemiTrans(quad, translucent);
                quad->tpage = tpage;
                quad->clut = MAP_CLUT(MAP_CLUT_ROW_EFFECT, 0);
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
        target_index = g_battle_effect_state_records[state_index].timeline.target_index;
        work = record->work_slots[callback_slot];
        EFFECT_READ_CURVE(j, i, entry->motion_curves.origin, frame);
        origin.vx = battle_lerp_fixed8(entry->origin_start.x, entry->origin_end.x, i);
        origin.vy = battle_lerp_fixed8(entry->origin_start.y, entry->origin_end.y, i);
        origin.vz = battle_lerp_fixed8(entry->origin_start.z, entry->origin_end.z, i);
        switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
        case EFFECT_EMITTER_ORIGIN_WORLD:
        case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
            break;
        case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
            tile = battle_map_get_tile_data_pointer(g_battle_effect_target_tile.data.tile.vx,
                g_battle_effect_target_tile.data.tile.vz, g_battle_effect_target_tile.data.tile.vy);
            origin.vx += g_battle_effect_target_tile.data.tile.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            origin.vy -= tile->height * MAP_HEIGHT_STEP;
            origin.vz += g_battle_effect_target_tile.data.tile.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            break;
        case EFFECT_EMITTER_ORIGIN_CASTER:
            battle_effect_resolve_target_world_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, EFFECT_TARGET_INDEX_CASTER,
                &g_battle_effect_context, &vertex);
            origin.vx += vertex.vx;
            origin.vy += vertex.vy;
            origin.vz += vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_TARGET:
            battle_effect_resolve_target_world_coords(
                g_battle_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST, target_index,
                &g_battle_effect_context, &vertex);
            origin.vx += vertex.vx;
            origin.vy += vertex.vy;
            origin.vz += vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_MAP_CENTER:
            battle_map_store_max_coordinates(&map_max);
            origin.vx += map_max.vx * (MAP_TILE_SIZE / 2);
            origin.vz += map_max.vz * (MAP_TILE_SIZE / 2);
            break;
        }

        radius_binding = entry->shape_curves.radius;
        EFFECT_READ_CURVE(j, i, radius_binding, frame);
        radius = battle_lerp_fixed8(entry->radius_start, entry->radius_end, i);

        texture_selector = entry->motion_curves.texture;
        EFFECT_READ_CURVE(j, i, texture_selector, frame);
        u = battle_lerp_fixed8(entry->u_start, entry->u_end, i);
        v = battle_lerp_fixed8(entry->v_start, entry->v_end, i);
        u_span = entry->u_span;
        v_span = entry->v_span;
        work->v_scroll += entry->v_scroll_speed;
        if (work->v_scroll > (v_span << 8)) {
            work->v_scroll -= v_span << 8;
        }
        if (work->v_scroll < 0) {
            work->v_scroll += v_span << 8;
        }

        EFFECT_READ_CURVE(j, i, entry->shape_curves.growth, frame);
        growth.vx = battle_lerp_fixed8(entry->growth_start, entry->growth_end, i);
        growth.vy = battle_lerp_fixed8(entry->height_start, entry->height_end, i);

        growth_step_selector = entry->shape_curves.growth_step;
        EFFECT_READ_CURVE(j, i, growth_step_selector, frame);
        growth_step.vx = battle_lerp_fixed8(entry->growth_step_start, entry->growth_step_end, i);
        growth_step.vy = battle_lerp_fixed8(entry->height_step_start, entry->height_step_end, i);

        EFFECT_READ_CURVE(j, i, entry->motion_curves.spin, frame);
        work->spin = (work->spin + battle_lerp_fixed8(entry->spin_speed_start, entry->spin_speed_end, i)) & (ONE - 1);

        /* The base stays in a variable: the target adds it to the row offset
         * after each call instead of folding it into the induction variable. */
        lattice_bytes = (u8*)getScratchAddr(0);
        for (j = 0; j < DOME_MESH_ROWS + 1; j++) {
            k = ((radius + (work->radius_offset >> 8)) * rcos(j * DOME_MESH_LATITUDE_STEP)) >> 12;
            n = ((((radius + (work->radius_offset >> 8)) * rsin(j * DOME_MESH_LATITUDE_STEP)) >> 12) * growth.vy) >> 8;
            for (i = 0; i < DOME_MESH_SEGMENTS + 1; i++) {
                /* Typed lattice indexing changes the loop induction and register allocation. */
                lattice_point = (vec3s16_t*)(lattice_bytes
                    + (j * DOME_MESH_LATTICE_ROW_BYTES + i * DOME_MESH_LATTICE_POINT_BYTES));
                lattice_point->x = (rcos(work->spin + i * DOME_MESH_LONGITUDE_STEP) * k) >> 12;
                lattice_point->z = (rsin(work->spin + i * DOME_MESH_LONGITUDE_STEP) * k) >> 12;
                lattice_point->y = -n;
            }
            growth.vy += growth_step.vy;
        }

        work->radius_speed += growth_step.vx;
        work->radius_offset += growth.vx + work->radius_speed;
        SetRotMatrix(&g_battle_camera_matrix);
        SetTransMatrix(&g_battle_camera_matrix);

        /* Row 0: quad column j - 1 takes lattice rows 1 (x0/x1) and 0 (x2/x3). */
        effect_dome_mesh_load_translated_vertex(0, 0, &origin, &vertex);
        RotTrans(&vertex, &screen[2], &screen[2].pad);
        quad = &work->quads[work->buffer][0];
        quad->x2 = screen[2].vx;
        quad->y2 = screen[2].vy;
        depth[0][0] = screen[2].vz;
        effect_dome_mesh_load_translated_vertex(0, 1, &origin, &vertex);
        RotTrans(&vertex, &screen[3], &screen[3].pad);
        quad->x3 = screen[3].vx;
        quad->y3 = screen[3].vy;
        depth[0][1] = screen[3].vz;
        effect_dome_mesh_load_translated_vertex(1, 0, &origin, &vertex);
        RotTrans(&vertex, &screen[0], &screen[0].pad);
        quad->x0 = screen[0].vx;
        quad->y0 = screen[0].vy;
        depth[1][0] = screen[0].vz;
        effect_dome_mesh_load_translated_vertex(1, 1, &origin, &vertex);
        RotTrans(&vertex, &screen[1], &screen[1].pad);
        quad->x1 = screen[1].vx;
        quad->y1 = screen[1].vy;
        depth[1][1] = screen[1].vz;
        for (j = 2; j < DOME_MESH_SEGMENTS + 1; j++) {
            quad = &work->quads[work->buffer][j - 1];
            quad->x0 = screen[1].vx;
            quad->y0 = screen[1].vy;
            quad->x2 = screen[3].vx;
            quad->y2 = screen[3].vy;
            effect_dome_mesh_load_translated_vertex(0, j, &origin, &vertex);
            RotTrans(&vertex, &screen[3], &screen[3].pad);
            quad->x3 = screen[3].vx;
            quad->y3 = screen[3].vy;
            depth[0][j] = screen[3].vz;
            effect_dome_mesh_load_translated_vertex(1, j, &origin, &vertex);
            RotTrans(&vertex, &screen[1], &screen[1].pad);
            quad->x1 = screen[1].vx;
            quad->y1 = screen[1].vy;
            depth[1][j] = screen[1].vz;
        }

        /* Rows 1-7 reuse the lower corners of the row above; the last column
         * closes the ring on column 0. */
        for (i = 1; i < DOME_MESH_ROWS; i++) {
            n = i + 1;
            effect_dome_mesh_load_translated_vertex(n, 0, &origin, &vertex);
            RotTrans(&vertex, &screen[0], &screen[0].pad);
            quad = &work->quads[work->buffer][i * DOME_MESH_SEGMENTS];
            quad->x0 = screen[0].vx;
            quad->y0 = screen[0].vy;
            depth[n][0] = screen[0].vz;
            effect_dome_mesh_load_translated_vertex(n, 1, &origin, &vertex);
            RotTrans(&vertex, &screen[1], &screen[1].pad);
            quad->x1 = screen[1].vx;
            quad->y1 = screen[1].vy;
            depth[n][1] = screen[1].vz;
            other = &work->quads[work->buffer][(i - 1) * DOME_MESH_SEGMENTS];
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            for (j = 2; j < DOME_MESH_SEGMENTS; j++) {
                quad = &work->quads[work->buffer][i * DOME_MESH_SEGMENTS + j - 1];
                other = &work->quads[work->buffer][(i - 1) * DOME_MESH_SEGMENTS + j - 1];
                quad->x0 = screen[1].vx;
                quad->y0 = screen[1].vy;
                quad->x2 = other->x0;
                quad->y2 = other->y0;
                quad->x3 = other->x1;
                quad->y3 = other->y1;
                effect_dome_mesh_load_translated_vertex(n, j, &origin, &vertex);
                RotTrans(&vertex, &screen[1], &screen[1].pad);
                quad->x1 = screen[1].vx;
                quad->y1 = screen[1].vy;
                depth[n][j] = screen[1].vz;
            }
            quad = &work->quads[work->buffer][i * DOME_MESH_SEGMENTS + DOME_MESH_SEGMENTS - 1];
            other = &work->quads[work->buffer][(i - 1) * DOME_MESH_SEGMENTS + DOME_MESH_SEGMENTS - 1];
            quad->x0 = screen[1].vx;
            quad->y0 = screen[1].vy;
            quad->x2 = other->x0;
            quad->y2 = other->y0;
            quad->x3 = other->x1;
            quad->y3 = other->y1;
            other = &work->quads[work->buffer][i * DOME_MESH_SEGMENTS];
            quad->x1 = other->x0;
            quad->y1 = other->y0;
        }

        i = work->v_scroll >> 8;
        for (j = 0; j < DOME_MESH_QUAD_COUNT; j++) {
            quad = &work->quads[work->buffer][j];
            quad->u0 = quad->u2 = u;
            quad->v0 = quad->v1 = v + i;
            quad->u1 = quad->u3 = u + u_span;
            quad->v2 = quad->v3 = v + v_span + i;
        }

        n = entry->brightness_row;
        if (entry->flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
            red = g_battle_effect_animation_curve_data
                      ->curves[((entry->color_curves & 0xfff) >> EFFECT_COLOR_CURVE_RED_SHIFT) & 0xf][frame];
            k = (entry->color_curves & 0xfff);
            green = g_battle_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_GREEN_SHIFT) & 0xf][frame];
            blue = g_battle_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_BLUE_SHIFT) & 0xf][frame];
        } else {
            red = green = blue = EFFECT_COLOR_NEUTRAL;
        }

        k = g_effect_dome_mesh_brightness_table[n][0];
        shade_r = (k * red) / ONE;
        shade_g = (k * green) / ONE;
        shade_b = (k * blue) / ONE;
        for (j = 0; j < DOME_MESH_SEGMENTS; j++) {
            quad = &work->quads[work->buffer][j];
            setRGB2(quad, quad->r3 = shade_r, quad->g3 = shade_g, quad->b3 = shade_b);
        }
        for (i = 0; i < DOME_MESH_ROWS - 1; i++) {
            k = g_effect_dome_mesh_brightness_table[n][i + 1];
            shade_r = (k * red) / ONE;
            shade_g = (k * green) / ONE;
            shade_b = (k * blue) / ONE;
            for (j = 0; j < DOME_MESH_SEGMENTS; j++) {
                quad = &work->quads[work->buffer][i * DOME_MESH_SEGMENTS + j];
                other = &work->quads[work->buffer][(i + 1) * DOME_MESH_SEGMENTS + j];
                setRGB0(quad, quad->r1 = other->r2 = other->r3 = shade_r, quad->g1 = other->g2 = other->g3 = shade_g,
                    quad->b1 = other->b2 = other->b3 = shade_b);
            }
        }
        k = g_effect_dome_mesh_brightness_table[n][DOME_MESH_ROWS];
        shade_r = (k * red) / ONE;
        shade_g = (k * green) / ONE;
        shade_b = (k * blue) / ONE;
        for (j = 0; j < DOME_MESH_SEGMENTS; j++) {
            quad = &work->quads[work->buffer][(DOME_MESH_ROWS - 1) * DOME_MESH_SEGMENTS + j];
            setRGB0(quad, quad->r1 = shade_r, quad->g1 = shade_g, quad->b1 = shade_b);
        }

        for (i = 0; i < DOME_MESH_ROWS; i++) {
            for (j = 0; j < DOME_MESH_SEGMENTS - 1; j++) {
                n = (depth[i][j] + depth[i][j + 1] + depth[i + 1][j] + depth[i + 1][j + 1]) / 16;
                if (n >= 0 && n < EFFECT_OT_DEPTH_LIMIT) {
                    addPrim(&ot[n], &work->quads[work->buffer][i * DOME_MESH_SEGMENTS + j]);
                }
            }
            n = (depth[i][0] + depth[i][DOME_MESH_SEGMENTS - 1] + depth[i + 1][0]
                    + depth[i + 1][DOME_MESH_SEGMENTS - 1])
                / 16;
            if (n >= 0 && n < EFFECT_OT_DEPTH_LIMIT) {
                addPrim(&ot[n], &work->quads[work->buffer][i * DOME_MESH_SEGMENTS + DOME_MESH_SEGMENTS - 1]);
            }
        }
        work->buffer = 1 - work->buffer;
        break;

    case EFFECT_CALLBACK_PHASE_DESTROY:
        work = record->work_slots[callback_slot];
        if (work->destroy_delay == 0) {
            work->destroy_delay = 1;
            break;
        }
        if (work != 0) {
            battle_heap_free_block(work);
            record->work_slots[callback_slot] = 0;
        }
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
