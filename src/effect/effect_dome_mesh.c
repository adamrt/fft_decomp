/* Spinning dome mesh vertex load and update shared by twelve effect overlays. */

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

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects
 * the row, the lattice row (0-8) the column. */
extern u32 g_effect_dome_mesh_brightness_table[][9];

/* Work block: two POLY_GT4 buffers of 8 rows x 16 columns. */
typedef struct effect_dome_mesh_work {
    s32 buffer;                              /* 0x0000: 0 or 1, flipped after each update */
    POLY_GT4 quads[2][DOME_MESH_QUAD_COUNT]; /* 0x0004: row * 16 + column */
    s16 spin;                                /* 0x3404: lattice rotation, modulo one turn */
    s16 v_scroll;                            /* 0x3406: 8.8 fixed, wrapped to v_span */
    s32 radius_offset;                       /* 0x3408: 24.8 fixed, added to the radius */
    s32 radius_speed;                        /* 0x340c */
    u8 _unused_3410[0x08];
    s32 destroy_delay; /* 0x3418: the destroy phase waits one call before freeing */
} effect_dome_mesh_work_t;
typedef char effect_dome_mesh_work_size_must_be_13340[sizeof(effect_dome_mesh_work_t) == 13340 ? 1 : -1];

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

enum { DOME_MESH_LATITUDE_STEP = ONE / (DOME_MESH_ROWS * 4), DOME_MESH_LONGITUDE_STEP = ONE / DOME_MESH_SEGMENTS };

/* Callback handler for the dome mesh: a hemisphere of 8 rows of 16 textured
 * quads over the emitter origin, spinning about the vertical axis. Its nine
 * lattice rows run in equal latitude steps from the rim to the pole, and the
 * whole radius grows from frame to frame. Each update builds the lattice in
 * the scratchpad, projects it through the battle camera into the current quad
 * buffer, shades the rows, depth-sorts the quads and flips buffers. The shared
 * temporaries and non-void signature preserve the renderer's register
 * allocation and phase-dispatch delay slots. */
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
    effect_record_t* record = &g_effect_state_records[state_index];
    const effect_radial_mesh_geometry_t* entry = &g_effect_geometry_table->entries[emitter_index].radial_mesh;
    effect_dome_mesh_work_t* work;
    POLY_GT4* quad;
    POLY_GT4* other;
    vec3s16_t* lattice_point;
    battle_map_tile_t* tile;
    u8* lattice_bytes;
    u16 placement = entry->placement_flags;
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
    u32* ot = gfx_get_battle_otag();
    s32 i;
    u32 red;
    u32 green;
    u32 blue;
    u32 shade_r;
    u32 shade_g;
    u32 shade_b;
    s32 n;

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            effect_heap_free_block(record->work_slots[callback_slot]);
        }
        work = effect_heap_alloc_block(sizeof(effect_dome_mesh_work_t), state_index);
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
        /* Indexed again rather than read through record, as the target does. */
        target_index = g_effect_state_records[state_index].timeline.target_index;
        work = record->work_slots[callback_slot];

        /* effect_interpolate_emitter_spawn_position's lerp, done inline on the same fields. */
        EFFECT_READ_CURVE(j, i, entry->motion_curves.origin, frame);
        origin.vx = effect_lerp_q8(entry->origin_start.x, entry->origin_end.x, i);
        origin.vy = effect_lerp_q8(entry->origin_start.y, entry->origin_end.y, i);
        origin.vz = effect_lerp_q8(entry->origin_start.z, entry->origin_end.z, i);
        switch (placement & EFFECT_EMITTER_ORIGIN_MASK) {
        case EFFECT_EMITTER_ORIGIN_WORLD:
        case EFFECT_EMITTER_ORIGIN_PARENT_PARTICLE:
            break;
        case EFFECT_EMITTER_ORIGIN_CURSOR_TILE:
            tile = battle_map_tile_get_pointer(
                g_effect_context.target_tile.vx, g_effect_context.target_tile.vz, g_effect_context.target_tile.vy);
            origin.vx += g_effect_context.target_tile.vx * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
            origin.vy -= tile->height * BATTLE_MAP_HEIGHT_STEP;
            origin.vz += g_effect_context.target_tile.vz * BATTLE_MAP_TILE_SIZE + (BATTLE_MAP_TILE_SIZE / 2);
            break;
        case EFFECT_EMITTER_ORIGIN_CASTER:
            effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
                EFFECT_TARGET_INDEX_CASTER, &g_effect_context, &vertex);
            origin.vx += vertex.vx;
            origin.vy += vertex.vy;
            origin.vz += vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_TARGET:
            effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
                target_index, &g_effect_context, &vertex);
            origin.vx += vertex.vx;
            origin.vy += vertex.vy;
            origin.vz += vertex.vz;
            break;
        case EFFECT_EMITTER_ORIGIN_MAP_CENTER:
            battle_map_store_dimensions(&map_max);
            origin.vx += map_max.vx * (BATTLE_MAP_TILE_SIZE / 2);
            origin.vz += map_max.vz * (BATTLE_MAP_TILE_SIZE / 2);
            break;
        }

        radius_binding = entry->shape_curves.radius;
        EFFECT_READ_CURVE(j, i, radius_binding, frame);
        radius = effect_lerp_q8(entry->radius_start, entry->radius_end, i);

        texture_selector = entry->motion_curves.texture;
        EFFECT_READ_CURVE(j, i, texture_selector, frame);
        u = effect_lerp_q8(entry->u_start, entry->u_end, i);
        v = effect_lerp_q8(entry->v_start, entry->v_end, i);
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
        growth.vx = effect_lerp_q8(entry->growth_start, entry->growth_end, i);
        growth.vy = effect_lerp_q8(entry->height_start, entry->height_end, i);

        growth_step_selector = entry->shape_curves.growth_step;
        EFFECT_READ_CURVE(j, i, growth_step_selector, frame);
        growth_step.vx = effect_lerp_q8(entry->growth_step_start, entry->growth_step_end, i);
        growth_step.vy = effect_lerp_q8(entry->height_step_start, entry->height_step_end, i);

        EFFECT_READ_CURVE(j, i, entry->motion_curves.spin, frame);
        work->spin = (work->spin + effect_lerp_q8(entry->spin_speed_start, entry->spin_speed_end, i)) & (ONE - 1);

        /* Rows climb from the rim (row 0) to the pole (row 8) in equal latitude steps, each row's
         * height scaled by growth.vy. The base stays in a variable: the target adds it to the row
         * offset after each call instead of folding it into the induction variable. */
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

        /* Grow the radius for the next frame; growth_step.vx accelerates the growth. */
        work->radius_speed += growth_step.vx;
        work->radius_offset += growth.vx + work->radius_speed;

        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);

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

        /* Row 0 runs to lattice column 16, a full turn that lands back on column 0. */
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

        /* Rows 1-7: corners 2/3 repeat the previous row's corners 0/1, and lattice row i + 1
         * gives corners 0/1. */
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

            /* The last quad closes the ring on the row's first quad. */
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

        /* Every quad maps the same texture window, scrolled by v_scroll (8.8 fixed). */
        i = work->v_scroll >> 8;
        for (j = 0; j < DOME_MESH_QUAD_COUNT; j++) {
            quad = &work->quads[work->buffer][j];
            quad->u0 = quad->u2 = u;
            quad->v0 = quad->v1 = v + i;
            quad->u1 = quad->u3 = u + u_span;
            quad->v2 = quad->v3 = v + v_span + i;
        }

        /* Vertex colors: the color curves (neutral without them) scaled by each lattice row's brightness. */
        n = entry->brightness_row;
        if (entry->flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
            k = (entry->color_curves & 0xfff);
            red = g_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_RED_SHIFT) & 0xf][frame];
            green = g_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_GREEN_SHIFT) & 0xf][frame];
            blue = g_effect_animation_curve_data->curves[(k >> EFFECT_COLOR_CURVE_BLUE_SHIFT) & 0xf][frame];
        } else {
            red = green = blue = EFFECT_COLOR_NEUTRAL;
        }

        /* Lattice row r shades corners 2/3 of quad row r and corners 0/1 of quad row r - 1. */
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

        /* Sort each quad by a quarter of its mean corner depth; quads outside the ordering table are skipped. */
        for (i = 0; i < DOME_MESH_ROWS; i++) {
            for (j = 0; j < DOME_MESH_SEGMENTS - 1; j++) {
                n = (depth[i][j] + depth[i][j + 1] + depth[i + 1][j] + depth[i + 1][j + 1]) / 16;
                if (n >= 0 && n < EFFECT_OT_DEPTH_LIMIT) {
                    addPrim(&ot[n], &work->quads[work->buffer][i * DOME_MESH_SEGMENTS + j]);
                }
            }

            /* The closing quad spans lattice columns 15 and 0. */
            n = (depth[i][0] + depth[i][DOME_MESH_SEGMENTS - 1] + depth[i + 1][0]
                    + depth[i + 1][DOME_MESH_SEGMENTS - 1])
                / 16;
            if (n >= 0 && n < EFFECT_OT_DEPTH_LIMIT) {
                addPrim(&ot[n], &work->quads[work->buffer][i * DOME_MESH_SEGMENTS + DOME_MESH_SEGMENTS - 1]);
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
