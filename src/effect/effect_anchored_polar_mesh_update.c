/* A four-fold polar mesh drawn around an effect target with per-ring colors. */

#include "fft/battle_map_palette.h"
#include "fft/camera.h"
#include "fft/effect.h"
#include "fft/effect_file.h"
#include "fft/effect_geometry.h"
#include "fft/effect_heap.h"
#include "fft/effect_particle.h"
#include "fft/effect_script.h"
#include "fft/gfx.h"
#include "fft/unit_runtime.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Overlay-local vertex brightness rows (12-bit fixed): brightness_row selects the row, the ring (0-5) the column. */
extern u32 g_effect_anchored_polar_mesh_brightness_table[][6];

/* Scratchpad (0x1f800000) layout of the Zodiac Stone glow variant. Its fields shift from
 * effect_polar_mesh_scratch_t, so it is a separate layout, not a view. */
typedef struct effect_anchored_polar_mesh_scratch {
    s32 red_factor;   /* 0x00: color curve factors, 128 when curves are off */
    s32 green_factor; /* 0x04 */
    s32 blue_factor;  /* 0x08 */
    u8 r;             /* 0x0c: color bytes of the current ring */
    u8 _unused_00d;
    u8 g; /* 0x0e */
    u8 _unused_00f;
    u8 b; /* 0x10 */
    u8 _unused_011;
    u8 u0; /* 0x12 */
    u8 u1; /* 0x13 */
    u8 v0; /* 0x14 */
    u8 v1; /* 0x15 */
    u8 _unused_016[0x22];
    s32 translucent;       /* 0x38 */
    u16 tpage;             /* 0x3c */
    u8 _padding_03e[0x02]; /* aligns half_width */
    s32 half_width;        /* 0x40 */
    s32 half_height;       /* 0x44 */
    s32 brightness_row;    /* 0x48 */
    u8 _unused_04c[0x0c];
    s32 velocity_angle;     /* 0x58 */
    s32 acceleration_angle; /* 0x5c */
    VECTOR center;          /* 0x60: interpolated emitter spawn position */
    u8 _unused_070[0x30];
    point32_t velocity; /* 0xa0: screen-space center step per ring, Q12 */
    u8 _unused_0a8[0x08];
    point32_t acceleration; /* 0xb0: change in center step per ring, Q12 */
    u8 _unused_0b8[0x18];
    s32 target_index; /* 0xd0 */
    u8 _unused_0d4[0x14];
    s32 radius;                 /* 0xe8 */
    s32 radius_step;            /* 0xec */
    s32 radius_step_delta;      /* 0xf0 */
    s32 angle;                  /* 0xf4 */
    s32 angle_step;             /* 0xf8 */
    s32 angle_step_delta;       /* 0xfc */
    s32 ot_depth;               /* 0x100 */
    SVECTOR position;           /* 0x104 */
    const SVECTOR* unit_coords; /* 0x10c */
    VECTOR screen;              /* 0x110; RotTrans stores flags in the unused fourth 32-bit value */
    point16_t points[POLAR_MESH_RING_COUNT + 1][POLAR_MESH_SEGMENTS_PER_QUADRANT + 1]; /* 0x120 */
} effect_anchored_polar_mesh_scratch_t;

/* The polar mesh of effect_polar_mesh_update anchored to the effect's current
 * target: the target's world position plus its unit's event offset is
 * projected through the battle camera, and the spawn-position curve offsets
 * the mesh center from that screen point (projected coordinates already
 * include the draw offset, so no + GFX_BATTLE_SCREEN_X here). It stores each ring's color as
 * separate bytes instead of packed words, which shifts its scratchpad layout.
 * Shared index temporaries and the non-void signature preserve register
 * allocation and the phase-dispatch delay slots. */
s32 effect_anchored_polar_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
    s16 state_index = record_index;
    effect_anchored_polar_mesh_scratch_t* scratch = (effect_anchored_polar_mesh_scratch_t*)getScratchAddr(0);
    effect_record_t* record = &g_effect_state_records[state_index];
    const effect_geometry_record_t* entry = &g_effect_geometry_table->entries[emitter_index];
    u32* ot = gfx_get_battle_otag();
    effect_polar_mesh_work_t* work;
    POLY_GT4* quad;
    POLY_GT4* inner;
    s32 j;
    s32 i;
    s32 ring;
    s32 selector;
    u16 angle_binding;
    u8 delta_binding;
    u32 brightness;

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            effect_heap_free_block(record->work_slots[callback_slot]);
        }
        work = record->work_slots[callback_slot]
            = effect_heap_alloc_block(sizeof(effect_polar_mesh_work_t), state_index);

        if (entry->polar_mesh.texture_page_flags & EFFECT_TEXTURE_PAGE_FLAG_CLEAR_ABE) {
            scratch->translucent = 0;
        } else {
            scratch->translucent = 1;
        }
        scratch->tpage = ((entry->polar_mesh.texture_page_flags & EFFECT_TEXTURE_PAGE_ABR_MASK)
                             << EFFECT_RENDER_TEXTURE_PAGE_BLEND_SHIFT)
            | EFFECT_RENDER_TEXTURE_PAGE_BASE;
        for (i = 0; i < POLAR_MESH_QUAD_COUNT; i++) {
            quad = &work->quads[0][i];
            SetPolyGT4(quad);
            setSemiTrans(quad, scratch->translucent);
            quad->tpage = scratch->tpage;
            quad->clut = BATTLE_MAP_CLUT(BATTLE_MAP_CLUT_ROW_EFFECT, 0);
            work->quads[1][i] = *quad;
        }

        work->scroll_v = 0;
        work->scroll_u = 0;
        work->frame = 0;
        work->buffer = 0;
        work->destroy_delay = 0;
        work->base_u = entry->polar_mesh.texture_u;
        work->base_v = entry->polar_mesh.texture_v;
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;

    case EFFECT_CALLBACK_PHASE_UPDATE:
        work = record->work_slots[callback_slot];
        scratch->half_width = entry->polar_mesh.texture_width / 2;
        scratch->half_height = entry->polar_mesh.texture_height / 2;

        /* The anchor: the target's position plus its unit's event offset, projected to the screen.
         * The record array is indexed again rather than read through record, as the target does. */
        scratch->target_index = g_effect_state_records[state_index].timeline.target_index;
        effect_resolve_target_world_coords(g_effect_flags_section->flags & EFFECT_FLAG_TERRAIN_HEIGHT_ADJUST,
            scratch->target_index, &g_effect_context, &scratch->position);
        scratch->unit_coords
            = unit_get_event_offset_by_runtime_id(g_effect_context.targets[scratch->target_index].data.unit.runtime_id);
        scratch->position.vx += scratch->unit_coords->vx;
        scratch->position.vy += scratch->unit_coords->vy;
        scratch->position.vz += scratch->unit_coords->vz;
        SetRotMatrix(&g_camera_matrix);
        SetTransMatrix(&g_camera_matrix);
        RotTrans(&scratch->position, &scratch->screen, &scratch->screen.pad);

        /* Ring shape: a radius and an angle, each with a per-ring step and a change of that step. */
        selector = entry->polar_mesh.curve_bindings.radius;
        EFFECT_READ_CURVE(j, i, selector, work->frame);
        scratch->radius = effect_lerp_q8(entry->polar_mesh.radius_start, entry->polar_mesh.radius_end, i) * ONE;
        EFFECT_READ_CURVE(j, i, entry->polar_mesh.curve_bindings.radius_step, work->frame);
        scratch->radius_step = effect_lerp_q8(entry->polar_mesh.radius_step_start, entry->polar_mesh.radius_step_end, i)
            << 4;
        EFFECT_READ_CURVE(j, i, entry->polar_mesh.curve_bindings.radius_step_delta, work->frame);
        scratch->radius_step_delta
            = effect_lerp_q8(entry->polar_mesh.radius_step_delta_start, entry->polar_mesh.radius_step_delta_end, i)
            << 4;

        angle_binding = entry->polar_mesh.curve_bindings.angle;
        EFFECT_READ_CURVE(j, i, angle_binding, work->frame);
        scratch->angle = effect_lerp_q8(entry->polar_mesh.angle_start, entry->polar_mesh.angle_end, i) << 4;
        selector = entry->polar_mesh.curve_bindings.angle_step;
        EFFECT_READ_CURVE(j, i, selector, work->frame);
        scratch->angle_step = effect_lerp_q8(entry->polar_mesh.angle_step_start, entry->polar_mesh.angle_step_end, i);
        delta_binding = entry->polar_mesh.curve_bindings.angle_step_delta;
        EFFECT_READ_CURVE(j, i, delta_binding, work->frame);
        scratch->angle_step_delta
            = effect_lerp_q8(entry->polar_mesh.angle_step_delta_start, entry->polar_mesh.angle_step_delta_end, i);

        /* Each ring sweeps one quadrant; its last point is the first turned a quarter turn. */
        for (ring = 0; ring < POLAR_MESH_RING_COUNT + 1; ring++) {
            for (i = 0; i < POLAR_MESH_SEGMENTS_PER_QUADRANT; i++) {
                scratch->points[ring][i].x
                    = rcos(scratch->angle + (i * POLAR_MESH_ANGLE_STEP)) * (scratch->radius >> 12) >> 12;
                scratch->points[ring][i].y
                    = rsin(scratch->angle + (i * POLAR_MESH_ANGLE_STEP)) * (scratch->radius >> 12) >> 12;
            }
            scratch->points[ring][POLAR_MESH_SEGMENTS_PER_QUADRANT].x = -scratch->points[ring][0].y;
            scratch->points[ring][POLAR_MESH_SEGMENTS_PER_QUADRANT].y = scratch->points[ring][0].x;
            scratch->radius += scratch->radius_step;
            scratch->radius_step += scratch->radius_step_delta;
            scratch->angle += scratch->angle_step;
            scratch->angle_step += scratch->angle_step_delta;
        }

        /* The center moves by velocity from ring to ring, and velocity changes by acceleration.
         * Its curve position is an offset from the projected anchor. */
        EFFECT_READ_CURVE(j, i, entry->polar_mesh.curve_bindings.center, work->frame);
        effect_interpolate_emitter_spawn_position(entry, i, &scratch->center);
        scratch->center.vx += scratch->screen.vx;
        scratch->center.vy += scratch->screen.vy;

        selector = entry->polar_mesh.curve_bindings.velocity;
        EFFECT_READ_CURVE(j, i, selector, work->frame);
        scratch->velocity_angle
            = effect_lerp_q8(entry->polar_mesh.velocity_angle_start, entry->polar_mesh.velocity_angle_end, i) << 4;
        j = effect_lerp_q8(entry->polar_mesh.velocity_speed_start, entry->polar_mesh.velocity_speed_end, i) << 4;
        scratch->velocity.x = rcos(scratch->velocity_angle) * j >> 12;
        scratch->velocity.y = rsin(scratch->velocity_angle) * j >> 12;

        selector = entry->polar_mesh.curve_bindings.acceleration;
        EFFECT_READ_CURVE(j, i, selector, work->frame);
        scratch->acceleration_angle
            = effect_lerp_q8(entry->polar_mesh.acceleration_angle_start, entry->polar_mesh.acceleration_angle_end, i)
            << 4;
        j = effect_lerp_q8(entry->polar_mesh.acceleration_speed_start, entry->polar_mesh.acceleration_speed_end, i)
            << 4;
        scratch->acceleration.x = rcos(scratch->acceleration_angle) * j >> 12;
        scratch->acceleration.y = rsin(scratch->acceleration_angle) * j >> 12;

        /* The scroll curve alone is read at the caller's frame. */
        selector = entry->polar_mesh.curve_bindings.scroll;
        EFFECT_READ_CURVE(j, i, selector, frame);
        j = effect_lerp_q8(entry->polar_mesh.scroll_u_speed_start, entry->polar_mesh.scroll_u_speed_end, i) << 4;
        work->scroll_u += j;
        while (work->scroll_u >= scratch->half_width * ONE) {
            work->scroll_u -= scratch->half_width * ONE;
        }
        while (work->scroll_u < 0) {
            work->scroll_u += scratch->half_width * ONE;
        }

        j = effect_lerp_q8(entry->polar_mesh.scroll_v_speed_start, entry->polar_mesh.scroll_v_speed_end, i) << 4;
        work->scroll_v += j;
        while (work->scroll_v >= scratch->half_height * ONE) {
            work->scroll_v -= scratch->half_height * ONE;
        }
        while (work->scroll_v < 0) {
            work->scroll_v += scratch->half_height * ONE;
        }

        if (entry->polar_mesh.flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
            j = (entry->polar_mesh.color_curves & 0xfff);
            scratch->red_factor
                = g_effect_animation_curve_data->curves[(j >> EFFECT_COLOR_CURVE_RED_SHIFT) & 0xf][work->frame];
            scratch->green_factor
                = g_effect_animation_curve_data->curves[(j >> EFFECT_COLOR_CURVE_GREEN_SHIFT) & 0xf][work->frame];
            scratch->blue_factor
                = g_effect_animation_curve_data->curves[(j >> EFFECT_COLOR_CURVE_BLUE_SHIFT) & 0xf][work->frame];
        } else {
            scratch->blue_factor = EFFECT_COLOR_NEUTRAL;
            scratch->green_factor = EFFECT_COLOR_NEUTRAL;
            scratch->red_factor = EFFECT_COLOR_NEUTRAL;
        }

        /* The whole mesh sits at one ordering-table depth. */
        scratch->brightness_row = entry->polar_mesh.brightness_row;
        scratch->ot_depth = entry->polar_mesh.ot_depth;
        if (scratch->ot_depth < EFFECT_OT_DEPTH_MIN) {
            scratch->ot_depth = EFFECT_OT_DEPTH_MIN;
        }
        if (scratch->ot_depth >= EFFECT_OT_DEPTH_LIMIT) {
            scratch->ot_depth = EFFECT_OT_DEPTH_MAX;
        }

        /* Ring 0: the inner edge of the first quad row, which also sets the UVs. Quadrants 1-3
         * turn each point (x, y) to (-y, x), (-x, -y) and (y, -x). */
        for (i = 0; i < POLAR_MESH_SEGMENTS_PER_QUADRANT; i++) {
            quad = &work->quads[work->buffer][i];
            setXY2(quad, scratch->center.vx + scratch->points[0][i].x, scratch->center.vy + scratch->points[0][i].y,
                scratch->center.vx + scratch->points[0][i + 1].x, scratch->center.vy + scratch->points[0][i + 1].y);
            scratch->u0 = quad->u0 = quad->u2 = (work->scroll_u >> 12) + work->base_u;
            scratch->u1 = quad->u1 = quad->u3 = scratch->u0 + scratch->half_width;
            scratch->v0 = quad->v0 = quad->v1 = (work->scroll_v >> 12) + work->base_v;
            scratch->v1 = quad->v2 = quad->v3 = scratch->v0 + scratch->half_height;
            brightness = g_effect_anchored_polar_mesh_brightness_table[scratch->brightness_row][0];
            scratch->r = quad->r0 = quad->r1 = brightness * scratch->red_factor / ONE;
            scratch->g = quad->g0 = quad->g1 = brightness * scratch->green_factor / ONE;
            scratch->b = quad->b0 = quad->b1 = brightness * scratch->blue_factor / ONE;

            quad = &work->quads[work->buffer][i + POLAR_MESH_QUADS_PER_QUADRANT];
            setXY2(quad, scratch->center.vx - scratch->points[0][i].y, scratch->center.vy + scratch->points[0][i].x,
                scratch->center.vx - scratch->points[0][i + 1].y, scratch->center.vy + scratch->points[0][i + 1].x);
            quad->u0 = quad->u2 = scratch->u0;
            quad->u1 = quad->u3 = scratch->u1;
            quad->v0 = quad->v1 = scratch->v0;
            quad->v2 = quad->v3 = scratch->v1;
            setRGB0(quad, quad->r1 = scratch->r, quad->g1 = scratch->g, quad->b1 = scratch->b);

            quad = &work->quads[work->buffer][i + 2 * POLAR_MESH_QUADS_PER_QUADRANT];
            setXY2(quad, scratch->center.vx - scratch->points[0][i].x, scratch->center.vy - scratch->points[0][i].y,
                scratch->center.vx - scratch->points[0][i + 1].x, scratch->center.vy - scratch->points[0][i + 1].y);
            quad->u0 = quad->u2 = scratch->u0;
            quad->u1 = quad->u3 = scratch->u1;
            quad->v0 = quad->v1 = scratch->v0;
            quad->v2 = quad->v3 = scratch->v1;
            setRGB0(quad, quad->r1 = scratch->r, quad->g1 = scratch->g, quad->b1 = scratch->b);

            quad = &work->quads[work->buffer][i + 3 * POLAR_MESH_QUADS_PER_QUADRANT];
            setXY2(quad, scratch->center.vx + scratch->points[0][i].y, scratch->center.vy - scratch->points[0][i].x,
                scratch->center.vx + scratch->points[0][i + 1].y, scratch->center.vy - scratch->points[0][i + 1].x);
            quad->u0 = quad->u2 = scratch->u0;
            quad->u1 = quad->u3 = scratch->u1;
            quad->v0 = quad->v1 = scratch->v0;
            quad->v2 = quad->v3 = scratch->v1;
            setRGB0(quad, quad->r1 = scratch->r, quad->g1 = scratch->g, quad->b1 = scratch->b);
        }

        /* Rings 1-4 close the inner quad row and open the next one. */
        for (j = 0; j < POLAR_MESH_RING_COUNT - 1; j++) {
            scratch->center.vx += scratch->velocity.x >> 12;
            scratch->center.vy += scratch->velocity.y >> 12;
            scratch->velocity.x += scratch->acceleration.x;
            scratch->velocity.y += scratch->acceleration.y;

            for (i = 0; i < POLAR_MESH_SEGMENTS_PER_QUADRANT; i++) {
                /* The ring is inner's outer edge and quad's inner edge; inner is then complete. */
                inner = &work->quads[work->buffer][j * POLAR_MESH_SEGMENTS_PER_QUADRANT + i];
                quad = &work->quads[work->buffer][(j + 1) * POLAR_MESH_SEGMENTS_PER_QUADRANT + i];
                setXY2(quad, inner->x2 = scratch->center.vx + scratch->points[j + 1][i].x,
                    inner->y2 = scratch->center.vy + scratch->points[j + 1][i].y,
                    inner->x3 = scratch->center.vx + scratch->points[j + 1][i + 1].x,
                    inner->y3 = scratch->center.vy + scratch->points[j + 1][i + 1].y);
                brightness = g_effect_anchored_polar_mesh_brightness_table[scratch->brightness_row][j + 1];
                scratch->r = quad->r0 = quad->r1 = inner->r2 = inner->r3 = brightness * scratch->red_factor / ONE;
                scratch->g = quad->g0 = quad->g1 = inner->g2 = inner->g3 = brightness * scratch->green_factor / ONE;
                scratch->b = quad->b0 = quad->b1 = inner->b2 = inner->b3 = brightness * scratch->blue_factor / ONE;
                AddPrim(ot + scratch->ot_depth, inner);
                scratch->u0 = quad->u0 = quad->u2 = (work->scroll_u >> 12) + work->base_u;
                scratch->u1 = quad->u1 = quad->u3 = scratch->u0 + scratch->half_width;
                scratch->v0 = quad->v0 = quad->v1 = (work->scroll_v >> 12) + work->base_v;
                scratch->v1 = quad->v2 = quad->v3 = scratch->v0 + scratch->half_height;

                inner = &work->quads[work->buffer]
                                    [j * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + POLAR_MESH_QUADS_PER_QUADRANT];
                quad = &work->quads[work->buffer]
                                   [(j + 1) * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + POLAR_MESH_QUADS_PER_QUADRANT];
                setXY2(quad, inner->x2 = scratch->center.vx - scratch->points[j + 1][i].y,
                    inner->y2 = scratch->center.vy + scratch->points[j + 1][i].x,
                    inner->x3 = scratch->center.vx - scratch->points[j + 1][i + 1].y,
                    inner->y3 = scratch->center.vy + scratch->points[j + 1][i + 1].x);
                setRGB0(quad, quad->r1 = inner->r2 = inner->r3 = scratch->r,
                    quad->g1 = inner->g2 = inner->g3 = scratch->g, quad->b1 = inner->b2 = inner->b3 = scratch->b);
                AddPrim(ot + scratch->ot_depth, inner);
                quad->u0 = quad->u2 = scratch->u0;
                quad->u1 = quad->u3 = scratch->u1;
                quad->v0 = quad->v1 = scratch->v0;
                quad->v2 = quad->v3 = scratch->v1;

                inner = &work->quads[work->buffer]
                                    [j * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + 2 * POLAR_MESH_QUADS_PER_QUADRANT];
                quad = &work->quads[work->buffer]
                                   [(j + 1) * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + 2 * POLAR_MESH_QUADS_PER_QUADRANT];
                setXY2(quad, inner->x2 = scratch->center.vx - scratch->points[j + 1][i].x,
                    inner->y2 = scratch->center.vy - scratch->points[j + 1][i].y,
                    inner->x3 = scratch->center.vx - scratch->points[j + 1][i + 1].x,
                    inner->y3 = scratch->center.vy - scratch->points[j + 1][i + 1].y);
                setRGB0(quad, quad->r1 = inner->r2 = inner->r3 = scratch->r,
                    quad->g1 = inner->g2 = inner->g3 = scratch->g, quad->b1 = inner->b2 = inner->b3 = scratch->b);
                AddPrim(ot + scratch->ot_depth, inner);
                quad->u0 = quad->u2 = scratch->u0;
                quad->u1 = quad->u3 = scratch->u1;
                quad->v0 = quad->v1 = scratch->v0;
                quad->v2 = quad->v3 = scratch->v1;

                inner = &work->quads[work->buffer]
                                    [j * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + 3 * POLAR_MESH_QUADS_PER_QUADRANT];
                quad = &work->quads[work->buffer]
                                   [(j + 1) * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + 3 * POLAR_MESH_QUADS_PER_QUADRANT];
                setXY2(quad, inner->x2 = scratch->center.vx + scratch->points[j + 1][i].y,
                    inner->y2 = scratch->center.vy - scratch->points[j + 1][i].x,
                    inner->x3 = scratch->center.vx + scratch->points[j + 1][i + 1].y,
                    inner->y3 = scratch->center.vy - scratch->points[j + 1][i + 1].x);
                setRGB0(quad, quad->r1 = inner->r2 = inner->r3 = scratch->r,
                    quad->g1 = inner->g2 = inner->g3 = scratch->g, quad->b1 = inner->b2 = inner->b3 = scratch->b);
                AddPrim(ot + scratch->ot_depth, inner);
                quad->u0 = quad->u2 = scratch->u0;
                quad->u1 = quad->u3 = scratch->u1;
                quad->v0 = quad->v1 = scratch->v0;
                quad->v2 = quad->v3 = scratch->v1;
            }
        }

        /* Ring 5: the outer edge of the last quad row. */
        scratch->center.vx += scratch->velocity.x >> 12;
        scratch->center.vy += scratch->velocity.y >> 12;
        scratch->velocity.x += scratch->acceleration.x;
        scratch->velocity.y += scratch->acceleration.y;

        for (i = 0; i < POLAR_MESH_SEGMENTS_PER_QUADRANT; i++) {
            quad = &work->quads[work->buffer][i + POLAR_MESH_OUTER_RING_OFFSET];
            quad->x2 = scratch->center.vx + scratch->points[POLAR_MESH_RING_COUNT][i].x;
            quad->y2 = scratch->center.vy + scratch->points[POLAR_MESH_RING_COUNT][i].y;
            quad->x3 = scratch->center.vx + scratch->points[POLAR_MESH_RING_COUNT][i + 1].x;
            quad->y3 = scratch->center.vy + scratch->points[POLAR_MESH_RING_COUNT][i + 1].y;
            brightness = g_effect_anchored_polar_mesh_brightness_table[scratch->brightness_row][POLAR_MESH_RING_COUNT];
            scratch->r = quad->r2 = quad->r3 = brightness * scratch->red_factor / ONE;
            scratch->g = quad->g2 = quad->g3 = brightness * scratch->green_factor / ONE;
            scratch->b = quad->b2 = quad->b3 = brightness * scratch->blue_factor / ONE;
            AddPrim(ot + scratch->ot_depth, quad);

            quad = &work->quads[work->buffer][i + POLAR_MESH_OUTER_RING_OFFSET + POLAR_MESH_QUADS_PER_QUADRANT];
            quad->x2 = scratch->center.vx - scratch->points[POLAR_MESH_RING_COUNT][i].y;
            quad->y2 = scratch->center.vy + scratch->points[POLAR_MESH_RING_COUNT][i].x;
            quad->x3 = scratch->center.vx - scratch->points[POLAR_MESH_RING_COUNT][i + 1].y;
            quad->y3 = scratch->center.vy + scratch->points[POLAR_MESH_RING_COUNT][i + 1].x;
            setRGB2(quad, quad->r3 = scratch->r, quad->g3 = scratch->g, quad->b3 = scratch->b);
            AddPrim(ot + scratch->ot_depth, quad);

            quad = &work->quads[work->buffer][i + POLAR_MESH_OUTER_RING_OFFSET + 2 * POLAR_MESH_QUADS_PER_QUADRANT];
            quad->x2 = scratch->center.vx - scratch->points[POLAR_MESH_RING_COUNT][i].x;
            quad->y2 = scratch->center.vy - scratch->points[POLAR_MESH_RING_COUNT][i].y;
            quad->x3 = scratch->center.vx - scratch->points[POLAR_MESH_RING_COUNT][i + 1].x;
            quad->y3 = scratch->center.vy - scratch->points[POLAR_MESH_RING_COUNT][i + 1].y;
            setRGB2(quad, quad->r3 = scratch->r, quad->g3 = scratch->g, quad->b3 = scratch->b);
            AddPrim(ot + scratch->ot_depth, quad);

            quad = &work->quads[work->buffer][i + POLAR_MESH_OUTER_RING_OFFSET + 3 * POLAR_MESH_QUADS_PER_QUADRANT];
            quad->x2 = scratch->center.vx + scratch->points[POLAR_MESH_RING_COUNT][i].y;
            quad->y2 = scratch->center.vy - scratch->points[POLAR_MESH_RING_COUNT][i].x;
            quad->x3 = scratch->center.vx + scratch->points[POLAR_MESH_RING_COUNT][i + 1].y;
            quad->y3 = scratch->center.vy - scratch->points[POLAR_MESH_RING_COUNT][i + 1].x;
            setRGB2(quad, quad->r3 = scratch->r, quad->g3 = scratch->g, quad->b3 = scratch->b);
            AddPrim(ot + scratch->ot_depth, quad);
        }

        /* The next update reads the next curve column and fills the other buffer while this one is drawn. */
        work->frame++;
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
