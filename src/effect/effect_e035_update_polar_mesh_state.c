#include "fft/effect_polar_mesh.h"

/* Builds and draws a four-fold symmetric polar mesh of textured quads.
 *
 * Six rings of five points are generated from interpolated radius and angle
 * curves, then mirrored into four quadrants of 5x4 POLY_GT4s; the centre drifts
 * with a velocity and acceleration per ring, and the texture scrolls within
 * half the texture size. The quads are double-buffered in the work block.
 *
 * `i` doubles as the curve factor and segment index, and `j` as the curve
 * index, scalar temporary and ring counter: the target keeps each pair in one
 * register. The handler returns no value, but the s32 return keeps $v0 live at
 * the exit, which leaves the phase dispatch's delay slots empty as in the
 * target. */
s32 effect_e035_update_polar_mesh_state(s16 record_index, s32 byte_offset, s32 geometry_index, s32 frame) {
    effect_polar_mesh_scratch_t* scratch;
    effect_record_t* record;
    effect_polar_mesh_geometry_view_t* entry;
    u32* ot;
    effect_polar_mesh_work_t* work;
    POLY_GT4* quad;
    POLY_GT4* inner;
    s32 j;
    s32 i;
    s32 ring;
    u32 brightness;

    scratch = (effect_polar_mesh_scratch_t*)0x1F800000;
    record = &g_effect_state_records[record_index];
    entry = (effect_polar_mesh_geometry_view_t*)&g_effect_geometry_table->entries[geometry_index];
    ot = main_gfx_get_otag();

    switch (record->phase[byte_offset]) {
    case EFFECT_PHASE_IDLE:
        break;

    case EFFECT_PHASE_INIT:
        if (record->work_slots[byte_offset] != 0) {
            battle_heap_free_block(record->work_slots[byte_offset]);
        }
        work = record->work_slots[byte_offset]
            = battle_heap_alloc_block(sizeof(effect_polar_mesh_work_t), record_index);
        if (entry->texture_page_flags & 4) {
            scratch->translucent = 0;
        } else {
            scratch->translucent = 1;
        }
        scratch->tpage = ((entry->texture_page_flags & 3) << 5) | 0x86;
        for (i = 0; i < 80; i++) {
            quad = &work->quads[0][i];
            SetPolyGT4(quad);
            if (scratch->translucent) {
                quad->code |= 2;
            } else {
                quad->code &= ~2;
            }
            quad->tpage = scratch->tpage;
            quad->clut = 0x7B00;
            work->quads[1][i] = *quad;
        }
        work->scroll_v = 0;
        work->scroll_u = 0;
        work->frame = 0;
        work->buffer = 0;
        work->destroy_delay = 0;
        work->base_u = entry->texture_u;
        work->base_v = entry->texture_v;
        record->phase[byte_offset] = EFFECT_PHASE_UPDATE;
        break;

    case EFFECT_PHASE_UPDATE:
        work = record->work_slots[byte_offset];
        scratch->half_width = entry->texture_width / 2;
        scratch->half_height = entry->texture_height / 2;

        READ_CURVE(i, j, (entry->curve_indices[0] >> 4) & 0xF, work->frame);
        scratch->radius = battle_effect_lerp_linear(entry->radius_start, entry->radius_end, i) << 12;
        READ_CURVE(i, j, entry->curve_indices[0] >> 28, work->frame);
        scratch->radius_step = battle_effect_lerp_linear(entry->radius_step_start, entry->radius_step_end, i) << 4;
        READ_CURVE(i, j, entry->curve_indices[1] & 0xF, work->frame);
        scratch->radius_step_delta
            = battle_effect_lerp_linear(entry->radius_step_delta_start, entry->radius_step_delta_end, i) << 4;
        READ_CURVE(i, j, (entry->curve_indices[0] >> 16) & 0xF, work->frame);
        scratch->angle = battle_effect_lerp_linear(entry->angle_start, entry->angle_end, i) << 4;
        READ_CURVE(i, j, (entry->curve_indices[1] >> 12) & 0xF, work->frame);
        scratch->angle_step = battle_effect_lerp_linear(entry->angle_step_start, entry->angle_step_end, i);
        READ_CURVE(i, j, (entry->curve_indices[1] >> 24) & 0xF, work->frame);
        scratch->angle_step_delta
            = battle_effect_lerp_linear(entry->angle_step_delta_start, entry->angle_step_delta_end, i);

        for (ring = 0; ring < 6; ring++) {
            for (i = 0; i < 4; i++) {
                scratch->points[ring][i].x = rcos(scratch->angle + (i << 8)) * (scratch->radius >> 12) >> 12;
                scratch->points[ring][i].y = rsin(scratch->angle + (i << 8)) * (scratch->radius >> 12) >> 12;
            }
            scratch->points[ring][4].x = -scratch->points[ring][0].y;
            scratch->points[ring][4].y = scratch->points[ring][0].x;
            scratch->radius += scratch->radius_step;
            scratch->radius_step += scratch->radius_step_delta;
            scratch->angle += scratch->angle_step;
            scratch->angle_step += scratch->angle_step_delta;
        }

        READ_CURVE(i, j, entry->curve_indices[0] & 0xF, work->frame);
        battle_effect_interpolate_emitter_spawn_position(entry, i, scratch->center);

        READ_CURVE(i, j, (entry->curve_indices[0] >> 8) & 0xF, work->frame);
        scratch->velocity_angle = battle_effect_lerp_linear(entry->velocity_angle_start, entry->velocity_angle_end, i)
            << 4;
        j = battle_effect_lerp_linear(entry->velocity_speed_start, entry->velocity_speed_end, i) << 4;
        scratch->velocity[0] = rcos(scratch->velocity_angle) * j >> 12;
        scratch->velocity[1] = rsin(scratch->velocity_angle) * j >> 12;

        READ_CURVE(i, j, (entry->curve_indices[0] >> 12) & 0xF, work->frame);
        scratch->acceleration_angle
            = battle_effect_lerp_linear(entry->acceleration_angle_start, entry->acceleration_angle_end, i) << 4;
        j = battle_effect_lerp_linear(entry->acceleration_speed_start, entry->acceleration_speed_end, i) << 4;
        scratch->acceleration[0] = rcos(scratch->acceleration_angle) * j >> 12;
        scratch->acceleration[1] = rsin(scratch->acceleration_angle) * j >> 12;

        /* The scroll curve alone is read at the caller's frame. */
        READ_CURVE(i, j, (entry->curve_indices[1] >> 4) & 0xF, frame);
        j = battle_effect_lerp_linear(entry->scroll_u_speed_start, entry->scroll_u_speed_end, i) << 4;
        work->scroll_u += j;
        while (work->scroll_u >= scratch->half_width << 12) {
            work->scroll_u -= scratch->half_width << 12;
        }
        while (work->scroll_u < 0) {
            work->scroll_u += scratch->half_width << 12;
        }
        j = battle_effect_lerp_linear(entry->scroll_v_speed_start, entry->scroll_v_speed_end, i) << 4;
        work->scroll_v += j;
        while (work->scroll_v >= scratch->half_height << 12) {
            work->scroll_v -= scratch->half_height << 12;
        }
        while (work->scroll_v < 0) {
            work->scroll_v += scratch->half_height << 12;
        }

        if (entry->flags & 0x40) {
            j = entry->colour_curves & 0xFFF;
            scratch->colour[0] = g_effect_palette_table[entry->colour_curves & 0xF].factor[work->frame];
            scratch->colour[1] = g_effect_palette_table[(j >> 4) & 0xF].factor[work->frame];
            scratch->colour[2] = g_effect_palette_table[(u32)j >> 8].factor[work->frame];
        } else {
            scratch->colour[2] = 0x80;
            scratch->colour[1] = 0x80;
            scratch->colour[0] = 0x80;
        }
        scratch->brightness_row = entry->brightness_row;
        scratch->ot_depth = entry->ot_depth;
        if (scratch->ot_depth < EFFECT_OT_DEPTH_MIN) {
            scratch->ot_depth = EFFECT_OT_DEPTH_MIN;
        }
        if (scratch->ot_depth >= EFFECT_OT_DEPTH_LIMIT) {
            scratch->ot_depth = EFFECT_OT_DEPTH_MAX;
        }
        scratch->code = POLY_RGB(&work->quads[0][0], 0) & 0xFF000000;

        /* Ring 0: the inner edge of the first quad row, which also sets the UVs. */
        for (i = 0; i < 4; i++) {
            quad = &work->quads[work->buffer][i];
            quad->x0 = scratch->center[0] + scratch->points[0][i].x + 0x80;
            quad->y0 = scratch->center[1] + scratch->points[0][i].y;
            quad->x1 = scratch->center[0] + scratch->points[0][i + 1].x + 0x80;
            quad->y1 = scratch->center[1] + scratch->points[0][i + 1].y;
            scratch->u0 = quad->u0 = quad->u2 = (work->scroll_u >> 12) + work->base_u;
            scratch->u1 = quad->u1 = quad->u3 = scratch->u0 + scratch->half_width;
            scratch->v0 = quad->v0 = quad->v1 = (work->scroll_v >> 12) + work->base_v;
            scratch->v1 = quad->v2 = quad->v3 = scratch->v0 + scratch->half_height;
            brightness = g_effect_e033_polar_mesh_brightness_table[scratch->brightness_row][0];
            scratch->rgb = PACK_RGB(scratch, brightness);
            POLY_RGB(quad, 0) = scratch->rgb;
            POLY_RGB(quad, 1) = scratch->rgb;

            quad = &work->quads[work->buffer][i + 20];
            quad->x0 = scratch->center[0] - scratch->points[0][i].y + 0x80;
            quad->y0 = scratch->center[1] + scratch->points[0][i].x;
            quad->x1 = scratch->center[0] - scratch->points[0][i + 1].y + 0x80;
            quad->y1 = scratch->center[1] + scratch->points[0][i + 1].x;
            quad->u0 = quad->u2 = scratch->u0;
            quad->u1 = quad->u3 = scratch->u1;
            quad->v0 = quad->v1 = scratch->v0;
            quad->v2 = quad->v3 = scratch->v1;
            POLY_RGB(quad, 0) = scratch->rgb;
            POLY_RGB(quad, 1) = scratch->rgb;

            quad = &work->quads[work->buffer][i + 40];
            quad->x0 = scratch->center[0] - scratch->points[0][i].x + 0x80;
            quad->y0 = scratch->center[1] - scratch->points[0][i].y;
            quad->x1 = scratch->center[0] - scratch->points[0][i + 1].x + 0x80;
            quad->y1 = scratch->center[1] - scratch->points[0][i + 1].y;
            quad->u0 = quad->u2 = scratch->u0;
            quad->u1 = quad->u3 = scratch->u1;
            quad->v0 = quad->v1 = scratch->v0;
            quad->v2 = quad->v3 = scratch->v1;
            POLY_RGB(quad, 0) = scratch->rgb;
            POLY_RGB(quad, 1) = scratch->rgb;

            quad = &work->quads[work->buffer][i + 60];
            quad->x0 = scratch->center[0] + scratch->points[0][i].y + 0x80;
            quad->y0 = scratch->center[1] - scratch->points[0][i].x;
            quad->x1 = scratch->center[0] + scratch->points[0][i + 1].y + 0x80;
            quad->y1 = scratch->center[1] - scratch->points[0][i + 1].x;
            quad->u0 = quad->u2 = scratch->u0;
            quad->u1 = quad->u3 = scratch->u1;
            quad->v0 = quad->v1 = scratch->v0;
            quad->v2 = quad->v3 = scratch->v1;
            POLY_RGB(quad, 0) = scratch->rgb;
            POLY_RGB(quad, 1) = scratch->rgb;
        }

        /* Rings 1-4 close the inner quad row and open the next one. */
        for (j = 0; j < 4; j++) {
            scratch->center[0] += scratch->velocity[0] >> 12;
            scratch->center[1] += scratch->velocity[1] >> 12;
            scratch->velocity[0] += scratch->acceleration[0];
            scratch->velocity[1] += scratch->acceleration[1];
            for (i = 0; i < 4; i++) {
                inner = &work->quads[work->buffer][j * 4 + i];
                quad = &work->quads[work->buffer][(j + 1) * 4 + i];
                quad->x0 = inner->x2 = scratch->center[0] + scratch->points[j + 1][i].x + 0x80;
                quad->y0 = inner->y2 = scratch->center[1] + scratch->points[j + 1][i].y;
                quad->x1 = inner->x3 = scratch->center[0] + scratch->points[j + 1][i + 1].x + 0x80;
                quad->y1 = inner->y3 = scratch->center[1] + scratch->points[j + 1][i + 1].y;
                brightness = g_effect_e033_polar_mesh_brightness_table[scratch->brightness_row][j + 1];
                POLY_RGB(quad, 0) = POLY_RGB(quad, 1) = POLY_RGB(inner, 2) = POLY_RGB(inner, 3) = scratch->rgb
                    = PACK_RGB(scratch, brightness);
                AddPrim(ot + scratch->ot_depth, inner);
                scratch->u0 = quad->u0 = quad->u2 = (work->scroll_u >> 12) + work->base_u;
                scratch->u1 = quad->u1 = quad->u3 = scratch->u0 + scratch->half_width;
                scratch->v0 = quad->v0 = quad->v1 = (work->scroll_v >> 12) + work->base_v;
                scratch->v1 = quad->v2 = quad->v3 = scratch->v0 + scratch->half_height;

                inner = &work->quads[work->buffer][j * 4 + i + 20];
                quad = &work->quads[work->buffer][(j + 1) * 4 + i + 20];
                quad->x0 = inner->x2 = scratch->center[0] - scratch->points[j + 1][i].y + 0x80;
                quad->y0 = inner->y2 = scratch->center[1] + scratch->points[j + 1][i].x;
                quad->x1 = inner->x3 = scratch->center[0] - scratch->points[j + 1][i + 1].y + 0x80;
                quad->y1 = inner->y3 = scratch->center[1] + scratch->points[j + 1][i + 1].x;
                POLY_RGB(quad, 0) = POLY_RGB(quad, 1) = POLY_RGB(inner, 2) = POLY_RGB(inner, 3) = scratch->rgb;
                AddPrim(ot + scratch->ot_depth, inner);
                quad->u0 = quad->u2 = scratch->u0;
                quad->u1 = quad->u3 = scratch->u1;
                quad->v0 = quad->v1 = scratch->v0;
                quad->v2 = quad->v3 = scratch->v1;

                inner = &work->quads[work->buffer][j * 4 + i + 40];
                quad = &work->quads[work->buffer][(j + 1) * 4 + i + 40];
                quad->x0 = inner->x2 = scratch->center[0] - scratch->points[j + 1][i].x + 0x80;
                quad->y0 = inner->y2 = scratch->center[1] - scratch->points[j + 1][i].y;
                quad->x1 = inner->x3 = scratch->center[0] - scratch->points[j + 1][i + 1].x + 0x80;
                quad->y1 = inner->y3 = scratch->center[1] - scratch->points[j + 1][i + 1].y;
                POLY_RGB(quad, 0) = POLY_RGB(quad, 1) = POLY_RGB(inner, 2) = POLY_RGB(inner, 3) = scratch->rgb;
                AddPrim(ot + scratch->ot_depth, inner);
                quad->u0 = quad->u2 = scratch->u0;
                quad->u1 = quad->u3 = scratch->u1;
                quad->v0 = quad->v1 = scratch->v0;
                quad->v2 = quad->v3 = scratch->v1;

                inner = &work->quads[work->buffer][j * 4 + i + 60];
                quad = &work->quads[work->buffer][(j + 1) * 4 + i + 60];
                quad->x0 = inner->x2 = scratch->center[0] + scratch->points[j + 1][i].y + 0x80;
                quad->y0 = inner->y2 = scratch->center[1] - scratch->points[j + 1][i].x;
                quad->x1 = inner->x3 = scratch->center[0] + scratch->points[j + 1][i + 1].y + 0x80;
                quad->y1 = inner->y3 = scratch->center[1] - scratch->points[j + 1][i + 1].x;
                POLY_RGB(quad, 0) = POLY_RGB(quad, 1) = POLY_RGB(inner, 2) = POLY_RGB(inner, 3) = scratch->rgb;
                AddPrim(ot + scratch->ot_depth, inner);
                quad->u0 = quad->u2 = scratch->u0;
                quad->u1 = quad->u3 = scratch->u1;
                quad->v0 = quad->v1 = scratch->v0;
                quad->v2 = quad->v3 = scratch->v1;
            }
        }

        /* Ring 5: the outer edge of the last quad row. */
        scratch->center[0] += scratch->velocity[0] >> 12;
        scratch->center[1] += scratch->velocity[1] >> 12;
        scratch->velocity[0] += scratch->acceleration[0];
        scratch->velocity[1] += scratch->acceleration[1];
        for (i = 0; i < 4; i++) {
            quad = &work->quads[work->buffer][i + 16];
            quad->x2 = scratch->center[0] + scratch->points[5][i].x + 0x80;
            quad->y2 = scratch->center[1] + scratch->points[5][i].y;
            quad->x3 = scratch->center[0] + scratch->points[5][i + 1].x + 0x80;
            quad->y3 = scratch->center[1] + scratch->points[5][i + 1].y;
            brightness = g_effect_e033_polar_mesh_brightness_table[scratch->brightness_row][5];
            POLY_RGB(quad, 2) = POLY_RGB(quad, 3) = scratch->rgb = PACK_RGB(scratch, brightness);
            AddPrim(ot + scratch->ot_depth, quad);

            quad = &work->quads[work->buffer][i + 16 + 20];
            quad->x2 = scratch->center[0] - scratch->points[5][i].y + 0x80;
            quad->y2 = scratch->center[1] + scratch->points[5][i].x;
            quad->x3 = scratch->center[0] - scratch->points[5][i + 1].y + 0x80;
            quad->y3 = scratch->center[1] + scratch->points[5][i + 1].x;
            POLY_RGB(quad, 2) = POLY_RGB(quad, 3) = scratch->rgb;
            AddPrim(ot + scratch->ot_depth, quad);

            quad = &work->quads[work->buffer][i + 16 + 40];
            quad->x2 = scratch->center[0] - scratch->points[5][i].x + 0x80;
            quad->y2 = scratch->center[1] - scratch->points[5][i].y;
            quad->x3 = scratch->center[0] - scratch->points[5][i + 1].x + 0x80;
            quad->y3 = scratch->center[1] - scratch->points[5][i + 1].y;
            POLY_RGB(quad, 2) = POLY_RGB(quad, 3) = scratch->rgb;
            AddPrim(ot + scratch->ot_depth, quad);

            quad = &work->quads[work->buffer][i + 16 + 60];
            quad->x2 = scratch->center[0] + scratch->points[5][i].y + 0x80;
            quad->y2 = scratch->center[1] - scratch->points[5][i].x;
            quad->x3 = scratch->center[0] + scratch->points[5][i + 1].y + 0x80;
            quad->y3 = scratch->center[1] - scratch->points[5][i + 1].x;
            POLY_RGB(quad, 2) = POLY_RGB(quad, 3) = scratch->rgb;
            AddPrim(ot + scratch->ot_depth, quad);
        }
        work->frame++;
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
