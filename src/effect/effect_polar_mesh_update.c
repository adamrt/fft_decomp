/* A four-fold polar mesh built from radius and angle curves, drawn with a drifting center and scrolling texture. */

#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_effect_file.h"
#include "fft/battle_effect_geometry.h"
#include "fft/battle_effect_particle.h"
#include "fft/battle_effect_script.h"
#include "fft/battle_map_palette.h"
#include "fft/effect.h"
#include "fft/main_gfx.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Pack one primitive color and code word after applying 4.12 brightness. */
#define PACK_RGB(scratch, brightness)                                                                                  \
    ((((brightness) * (scratch)->red_factor) / ONE) | (scratch)->code                                                  \
        | ((((brightness) * (scratch)->green_factor) / ONE) << 8)                                                      \
        | ((((brightness) * (scratch)->blue_factor) / ONE) << 16))

#define POLY_RGB(poly, n) (*(u32*)&(poly)->r##n)

/* Callback handler for the polar mesh, a flat screen-space disc: six rings of
 * points around a center, each ring a radius and a start angle that step from
 * ring to ring by the curves, cut into four quadrants of 5 x 4 quads that are
 * the first quadrant turned by quarter turns. The center drifts from ring to
 * ring by a velocity that the acceleration curves bend, and the texture window
 * scrolls. Screen x carries + 0x80 because BATTLE's draw areas begin at x
 * -0x80. Shared index temporaries and the non-void signature preserve register
 * allocation and phase-dispatch delay slots. */
s32 effect_polar_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
    s16 state_index = record_index;
    effect_polar_mesh_scratch_t* scratch = (effect_polar_mesh_scratch_t*)getScratchAddr(0);
    effect_record_t* record = &g_battle_effect_state_records[state_index];
    const effect_geometry_record_t* entry = &g_battle_effect_geometry_table->entries[emitter_index];
    u32* ot = main_gfx_get_otag();
    effect_polar_mesh_work_t* work;
    POLY_GT4* quad;
    POLY_GT4* inner;
    s32 j;
    s32 i;
    s32 ring;
    s32 selector; /* Keeps curve-field loads in the matched register order. */
    u16 angle_binding;
    u8 delta_binding;
    u32 brightness;

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            battle_heap_free_block(record->work_slots[callback_slot]);
        }
        work = record->work_slots[callback_slot]
            = battle_heap_alloc_block(sizeof(effect_polar_mesh_work_t), state_index);

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
            quad->clut = MAP_CLUT(MAP_CLUT_ROW_EFFECT, 0);
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

        /* Ring shape: a radius and an angle, each with a per-ring step and a change of that step. */
        selector = entry->polar_mesh.curve_bindings.radius;
        EFFECT_READ_CURVE(j, i, selector, work->frame);
        scratch->radius = battle_lerp_fixed8(entry->polar_mesh.radius_start, entry->polar_mesh.radius_end, i) * ONE;
        EFFECT_READ_CURVE(j, i, entry->polar_mesh.curve_bindings.radius_step, work->frame);
        scratch->radius_step
            = battle_lerp_fixed8(entry->polar_mesh.radius_step_start, entry->polar_mesh.radius_step_end, i) << 4;
        EFFECT_READ_CURVE(j, i, entry->polar_mesh.curve_bindings.radius_step_delta, work->frame);
        scratch->radius_step_delta
            = battle_lerp_fixed8(entry->polar_mesh.radius_step_delta_start, entry->polar_mesh.radius_step_delta_end, i)
            << 4;

        angle_binding = entry->polar_mesh.curve_bindings.angle;
        EFFECT_READ_CURVE(j, i, angle_binding, work->frame);
        scratch->angle = battle_lerp_fixed8(entry->polar_mesh.angle_start, entry->polar_mesh.angle_end, i) << 4;
        selector = entry->polar_mesh.curve_bindings.angle_step;
        EFFECT_READ_CURVE(j, i, selector, work->frame);
        scratch->angle_step
            = battle_lerp_fixed8(entry->polar_mesh.angle_step_start, entry->polar_mesh.angle_step_end, i);
        delta_binding = entry->polar_mesh.curve_bindings.angle_step_delta;
        EFFECT_READ_CURVE(j, i, delta_binding, work->frame);
        scratch->angle_step_delta
            = battle_lerp_fixed8(entry->polar_mesh.angle_step_delta_start, entry->polar_mesh.angle_step_delta_end, i);

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

        /* The center moves by velocity from ring to ring, and velocity changes by acceleration. */
        EFFECT_READ_CURVE(j, i, entry->polar_mesh.curve_bindings.center, work->frame);
        battle_effect_interpolate_emitter_spawn_position(entry, i, &scratch->center);

        selector = entry->polar_mesh.curve_bindings.velocity;
        EFFECT_READ_CURVE(j, i, selector, work->frame);
        scratch->velocity_angle
            = battle_lerp_fixed8(entry->polar_mesh.velocity_angle_start, entry->polar_mesh.velocity_angle_end, i) << 4;
        j = battle_lerp_fixed8(entry->polar_mesh.velocity_speed_start, entry->polar_mesh.velocity_speed_end, i) << 4;
        scratch->velocity.x = rcos(scratch->velocity_angle) * j >> 12;
        scratch->velocity.y = rsin(scratch->velocity_angle) * j >> 12;

        selector = entry->polar_mesh.curve_bindings.acceleration;
        EFFECT_READ_CURVE(j, i, selector, work->frame);
        scratch->acceleration_angle = battle_lerp_fixed8(entry->polar_mesh.acceleration_angle_start,
                                          entry->polar_mesh.acceleration_angle_end, i)
            << 4;
        j = battle_lerp_fixed8(entry->polar_mesh.acceleration_speed_start, entry->polar_mesh.acceleration_speed_end, i)
            << 4;
        scratch->acceleration.x = rcos(scratch->acceleration_angle) * j >> 12;
        scratch->acceleration.y = rsin(scratch->acceleration_angle) * j >> 12;

        /* The scroll curve alone is read at the caller's frame. */
        selector = entry->polar_mesh.curve_bindings.scroll;
        EFFECT_READ_CURVE(j, i, selector, frame);
        j = battle_lerp_fixed8(entry->polar_mesh.scroll_u_speed_start, entry->polar_mesh.scroll_u_speed_end, i) << 4;
        work->scroll_u += j;
        while (work->scroll_u >= scratch->half_width * ONE) {
            work->scroll_u -= scratch->half_width * ONE;
        }
        while (work->scroll_u < 0) {
            work->scroll_u += scratch->half_width * ONE;
        }

        j = battle_lerp_fixed8(entry->polar_mesh.scroll_v_speed_start, entry->polar_mesh.scroll_v_speed_end, i) << 4;
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
                = g_battle_effect_animation_curve_data->curves[(j >> EFFECT_COLOR_CURVE_RED_SHIFT) & 0xf][work->frame];
            scratch->green_factor = g_battle_effect_animation_curve_data
                                        ->curves[(j >> EFFECT_COLOR_CURVE_GREEN_SHIFT) & 0xf][work->frame];
            scratch->blue_factor
                = g_battle_effect_animation_curve_data->curves[(j >> EFFECT_COLOR_CURVE_BLUE_SHIFT) & 0xf][work->frame];
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

        /* Colors are stored as whole words, so keep the code byte that shares a word with r0. */
        scratch->code = POLY_RGB(&work->quads[0][0], 0) & 0xFF000000;

        /* Ring 0: the inner edge of the first quad row, which also sets the UVs. Quadrants 1-3
         * turn each point (x, y) to (-y, x), (-x, -y) and (y, -x). */
        for (i = 0; i < POLAR_MESH_SEGMENTS_PER_QUADRANT; i++) {
            quad = &work->quads[work->buffer][i];
            setXY2(quad, scratch->center.vx + scratch->points[0][i].x + 0x80,
                scratch->center.vy + scratch->points[0][i].y, scratch->center.vx + scratch->points[0][i + 1].x + 0x80,
                scratch->center.vy + scratch->points[0][i + 1].y);
            scratch->u0 = quad->u0 = quad->u2 = (work->scroll_u >> 12) + work->base_u;
            scratch->u1 = quad->u1 = quad->u3 = scratch->u0 + scratch->half_width;
            scratch->v0 = quad->v0 = quad->v1 = (work->scroll_v >> 12) + work->base_v;
            scratch->v1 = quad->v2 = quad->v3 = scratch->v0 + scratch->half_height;
            brightness = g_effect_polar_mesh_brightness_table[scratch->brightness_row][0];
            scratch->rgb = PACK_RGB(scratch, brightness);
            POLY_RGB(quad, 0) = scratch->rgb;
            POLY_RGB(quad, 1) = scratch->rgb;

            quad = &work->quads[work->buffer][i + POLAR_MESH_QUADS_PER_QUADRANT];
            setXY2(quad, scratch->center.vx - scratch->points[0][i].y + 0x80,
                scratch->center.vy + scratch->points[0][i].x, scratch->center.vx - scratch->points[0][i + 1].y + 0x80,
                scratch->center.vy + scratch->points[0][i + 1].x);
            quad->u0 = quad->u2 = scratch->u0;
            quad->u1 = quad->u3 = scratch->u1;
            quad->v0 = quad->v1 = scratch->v0;
            quad->v2 = quad->v3 = scratch->v1;
            POLY_RGB(quad, 0) = scratch->rgb;
            POLY_RGB(quad, 1) = scratch->rgb;

            quad = &work->quads[work->buffer][i + 2 * POLAR_MESH_QUADS_PER_QUADRANT];
            setXY2(quad, scratch->center.vx - scratch->points[0][i].x + 0x80,
                scratch->center.vy - scratch->points[0][i].y, scratch->center.vx - scratch->points[0][i + 1].x + 0x80,
                scratch->center.vy - scratch->points[0][i + 1].y);
            quad->u0 = quad->u2 = scratch->u0;
            quad->u1 = quad->u3 = scratch->u1;
            quad->v0 = quad->v1 = scratch->v0;
            quad->v2 = quad->v3 = scratch->v1;
            POLY_RGB(quad, 0) = scratch->rgb;
            POLY_RGB(quad, 1) = scratch->rgb;

            quad = &work->quads[work->buffer][i + 3 * POLAR_MESH_QUADS_PER_QUADRANT];
            setXY2(quad, scratch->center.vx + scratch->points[0][i].y + 0x80,
                scratch->center.vy - scratch->points[0][i].x, scratch->center.vx + scratch->points[0][i + 1].y + 0x80,
                scratch->center.vy - scratch->points[0][i + 1].x);
            quad->u0 = quad->u2 = scratch->u0;
            quad->u1 = quad->u3 = scratch->u1;
            quad->v0 = quad->v1 = scratch->v0;
            quad->v2 = quad->v3 = scratch->v1;
            POLY_RGB(quad, 0) = scratch->rgb;
            POLY_RGB(quad, 1) = scratch->rgb;
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
                setXY2(quad, inner->x2 = scratch->center.vx + scratch->points[j + 1][i].x + 0x80,
                    inner->y2 = scratch->center.vy + scratch->points[j + 1][i].y,
                    inner->x3 = scratch->center.vx + scratch->points[j + 1][i + 1].x + 0x80,
                    inner->y3 = scratch->center.vy + scratch->points[j + 1][i + 1].y);
                brightness = g_effect_polar_mesh_brightness_table[scratch->brightness_row][j + 1];
                POLY_RGB(quad, 0) = POLY_RGB(quad, 1) = POLY_RGB(inner, 2) = POLY_RGB(inner, 3) = scratch->rgb
                    = PACK_RGB(scratch, brightness);
                AddPrim(ot + scratch->ot_depth, inner);
                scratch->u0 = quad->u0 = quad->u2 = (work->scroll_u >> 12) + work->base_u;
                scratch->u1 = quad->u1 = quad->u3 = scratch->u0 + scratch->half_width;
                scratch->v0 = quad->v0 = quad->v1 = (work->scroll_v >> 12) + work->base_v;
                scratch->v1 = quad->v2 = quad->v3 = scratch->v0 + scratch->half_height;

                inner = &work->quads[work->buffer]
                                    [j * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + POLAR_MESH_QUADS_PER_QUADRANT];
                quad = &work->quads[work->buffer]
                                   [(j + 1) * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + POLAR_MESH_QUADS_PER_QUADRANT];
                setXY2(quad, inner->x2 = scratch->center.vx - scratch->points[j + 1][i].y + 0x80,
                    inner->y2 = scratch->center.vy + scratch->points[j + 1][i].x,
                    inner->x3 = scratch->center.vx - scratch->points[j + 1][i + 1].y + 0x80,
                    inner->y3 = scratch->center.vy + scratch->points[j + 1][i + 1].x);
                POLY_RGB(quad, 0) = POLY_RGB(quad, 1) = POLY_RGB(inner, 2) = POLY_RGB(inner, 3) = scratch->rgb;
                AddPrim(ot + scratch->ot_depth, inner);
                quad->u0 = quad->u2 = scratch->u0;
                quad->u1 = quad->u3 = scratch->u1;
                quad->v0 = quad->v1 = scratch->v0;
                quad->v2 = quad->v3 = scratch->v1;

                inner = &work->quads[work->buffer]
                                    [j * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + 2 * POLAR_MESH_QUADS_PER_QUADRANT];
                quad = &work->quads[work->buffer]
                                   [(j + 1) * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + 2 * POLAR_MESH_QUADS_PER_QUADRANT];
                setXY2(quad, inner->x2 = scratch->center.vx - scratch->points[j + 1][i].x + 0x80,
                    inner->y2 = scratch->center.vy - scratch->points[j + 1][i].y,
                    inner->x3 = scratch->center.vx - scratch->points[j + 1][i + 1].x + 0x80,
                    inner->y3 = scratch->center.vy - scratch->points[j + 1][i + 1].y);
                POLY_RGB(quad, 0) = POLY_RGB(quad, 1) = POLY_RGB(inner, 2) = POLY_RGB(inner, 3) = scratch->rgb;
                AddPrim(ot + scratch->ot_depth, inner);
                quad->u0 = quad->u2 = scratch->u0;
                quad->u1 = quad->u3 = scratch->u1;
                quad->v0 = quad->v1 = scratch->v0;
                quad->v2 = quad->v3 = scratch->v1;

                inner = &work->quads[work->buffer]
                                    [j * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + 3 * POLAR_MESH_QUADS_PER_QUADRANT];
                quad = &work->quads[work->buffer]
                                   [(j + 1) * POLAR_MESH_SEGMENTS_PER_QUADRANT + i + 3 * POLAR_MESH_QUADS_PER_QUADRANT];
                setXY2(quad, inner->x2 = scratch->center.vx + scratch->points[j + 1][i].y + 0x80,
                    inner->y2 = scratch->center.vy - scratch->points[j + 1][i].x,
                    inner->x3 = scratch->center.vx + scratch->points[j + 1][i + 1].y + 0x80,
                    inner->y3 = scratch->center.vy - scratch->points[j + 1][i + 1].x);
                POLY_RGB(quad, 0) = POLY_RGB(quad, 1) = POLY_RGB(inner, 2) = POLY_RGB(inner, 3) = scratch->rgb;
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
            quad->x2 = scratch->center.vx + scratch->points[POLAR_MESH_RING_COUNT][i].x + 0x80;
            quad->y2 = scratch->center.vy + scratch->points[POLAR_MESH_RING_COUNT][i].y;
            quad->x3 = scratch->center.vx + scratch->points[POLAR_MESH_RING_COUNT][i + 1].x + 0x80;
            quad->y3 = scratch->center.vy + scratch->points[POLAR_MESH_RING_COUNT][i + 1].y;
            brightness = g_effect_polar_mesh_brightness_table[scratch->brightness_row][POLAR_MESH_RING_COUNT];
            POLY_RGB(quad, 2) = POLY_RGB(quad, 3) = scratch->rgb = PACK_RGB(scratch, brightness);
            AddPrim(ot + scratch->ot_depth, quad);

            quad = &work->quads[work->buffer][i + POLAR_MESH_OUTER_RING_OFFSET + POLAR_MESH_QUADS_PER_QUADRANT];
            quad->x2 = scratch->center.vx - scratch->points[POLAR_MESH_RING_COUNT][i].y + 0x80;
            quad->y2 = scratch->center.vy + scratch->points[POLAR_MESH_RING_COUNT][i].x;
            quad->x3 = scratch->center.vx - scratch->points[POLAR_MESH_RING_COUNT][i + 1].y + 0x80;
            quad->y3 = scratch->center.vy + scratch->points[POLAR_MESH_RING_COUNT][i + 1].x;
            POLY_RGB(quad, 2) = POLY_RGB(quad, 3) = scratch->rgb;
            AddPrim(ot + scratch->ot_depth, quad);

            quad = &work->quads[work->buffer][i + POLAR_MESH_OUTER_RING_OFFSET + 2 * POLAR_MESH_QUADS_PER_QUADRANT];
            quad->x2 = scratch->center.vx - scratch->points[POLAR_MESH_RING_COUNT][i].x + 0x80;
            quad->y2 = scratch->center.vy - scratch->points[POLAR_MESH_RING_COUNT][i].y;
            quad->x3 = scratch->center.vx - scratch->points[POLAR_MESH_RING_COUNT][i + 1].x + 0x80;
            quad->y3 = scratch->center.vy - scratch->points[POLAR_MESH_RING_COUNT][i + 1].y;
            POLY_RGB(quad, 2) = POLY_RGB(quad, 3) = scratch->rgb;
            AddPrim(ot + scratch->ot_depth, quad);

            quad = &work->quads[work->buffer][i + POLAR_MESH_OUTER_RING_OFFSET + 3 * POLAR_MESH_QUADS_PER_QUADRANT];
            quad->x2 = scratch->center.vx + scratch->points[POLAR_MESH_RING_COUNT][i].y + 0x80;
            quad->y2 = scratch->center.vy - scratch->points[POLAR_MESH_RING_COUNT][i].x;
            quad->x3 = scratch->center.vx + scratch->points[POLAR_MESH_RING_COUNT][i + 1].y + 0x80;
            quad->y3 = scratch->center.vy - scratch->points[POLAR_MESH_RING_COUNT][i + 1].x;
            POLY_RGB(quad, 2) = POLY_RGB(quad, 3) = scratch->rgb;
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
            battle_heap_free_block(work);
            record->work_slots[callback_slot] = 0;
        }
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        break;
    }
}
