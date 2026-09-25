#include "fft/effect.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/*
 * Phase handler for the scrolling 8x8 textured quad grid.
 *
 * The 8x8 build of effect_e047_update_textured_quad_grid_4x4 (E047): 32-pixel
 * cells, nine brightness entries per row and an ordering-table depth of at
 * least 4. The shared row/column scratch variables are explained there.
 */
s32 effect_e475_update_textured_quad_grid_8x8(s16 record_index, s32 byte_offset, s32 geometry_index, s32 frame) {
    effect_record_t* record;
    s32 translucent;
    u16 texture_page;
    s32 texture_width;
    s32 texture_height;
    u32* ot;
    effect_geometry_entry_t* geometry;
    effect_textured_quad_grid_8x8_work_t* work;
    POLY_GT4* quad;
    s32 row;
    s32 column;
    s32 quad_column;
    s32 quad_row;
    s32 speed;
    s16 angle;
    u32 red;
    u32 green;
    u32 blue;
    s32 depth;
    s32 brightness_row;
    u8 u;
    u8 v;

    record = &g_effect_state_records[record_index];
    geometry = &g_effect_geometry_table->entries[geometry_index];
    ot = main_gfx_get_otag();

    switch (record->phase[byte_offset]) {
    case EFFECT_PHASE_IDLE:
        break;
    case EFFECT_PHASE_INIT:
        if (record->work_slots[byte_offset] != 0) {
            battle_heap_free_block(record->work_slots[byte_offset]);
        }
        work = battle_heap_alloc_block(sizeof(effect_textured_quad_grid_8x8_work_t), record_index);
        record->work_slots[byte_offset] = work;
        if (geometry->texture_page_flags & 4) {
            translucent = 0;
        } else {
            translucent = 1;
        }
        texture_page = ((geometry->texture_page_flags & 3) << 5) | 0x86;
        for (row = 0; row < 8; row++) {
            for (column = 0; column < 8; column++) {
                quad = &work->quads[0][row * 8 + column];
                SetPolyGT4(quad);
                if (translucent) {
                    quad->code |= 2;
                } else {
                    quad->code &= 0xfd;
                }
                quad->tpage = texture_page;
                quad->clut = 0x7b00;
                quad->x0 = quad->x2 = (column << 5) + 0x80;
                quad->y0 = quad->y1 = row << 5;
                quad->x1 = quad->x3 = (column << 5) + 0xa0;
                quad->y2 = quad->y3 = (row << 5) + 0x20;
                work->quads[1][row * 8 + column] = *quad;
            }
        }
        work->scroll_v = 0;
        work->scroll_u = 0;
        work->frame = 0;
        work->buffer = 0;
        work->destroy_latched = 0;
        work->texture_u = geometry->motion.textured_quad.texture_u;
        work->texture_v = geometry->motion.textured_quad.texture_v;
        record->phase[byte_offset] = EFFECT_PHASE_UPDATE;
        break;
    case EFFECT_PHASE_UPDATE:
        work = record->work_slots[byte_offset];
        texture_width = geometry->texture_width / 2;
        texture_height = geometry->texture_height / 2;
        /* Two statements keep the target's li -1 / beq compare. */
        row = geometry->curve_indices[0] >> 28;
        row -= 1;
        if (row != -1) {
            column = g_effect_palette_table[row].factor[work->frame];
        } else {
            column = 0;
        }
        speed = battle_effect_lerp_linear(geometry->scroll_speed_start, geometry->scroll_speed_end, column) << 4;
        row = (geometry->curve_indices[0] >> 8) & 0xf;
        row -= 1;
        if (row != -1) {
            column = g_effect_palette_table[row].factor[work->frame];
        } else {
            column = 0;
        }
        angle = battle_effect_lerp_linear(
            geometry->motion.textured_quad.scroll_angle_start, geometry->motion.textured_quad.scroll_angle_end, column);
        work->scroll_u += (rcos(angle) * speed) >> 12;
        while (work->scroll_u >= texture_width << 12) {
            work->scroll_u -= texture_width << 12;
        }
        while (work->scroll_u < 0) {
            work->scroll_u += texture_width << 12;
        }
        work->scroll_v += (rsin(angle) * speed) >> 12;
        while (work->scroll_v >= texture_height << 12) {
            work->scroll_v -= texture_height << 12;
        }
        while (work->scroll_v < 0) {
            work->scroll_v += texture_height << 12;
        }
        blue = 0x80;
        if (geometry->flags & 0x40) {
            row = geometry->colour_curves & 0xfff;
            red = g_effect_palette_table[row & 0xf].factor[work->frame];
            green = g_effect_palette_table[(row >> 4) & 0xf].factor[work->frame];
            blue = g_effect_palette_table[(row >> 8) & 0xf].factor[work->frame];
        } else {
            green = 0x80;
            red = 0x80;
        }
        depth = geometry->ot_depth;
        brightness_row = geometry->brightness_row;
        if (depth < EFFECT_OT_DEPTH_MIN) {
            depth = EFFECT_OT_DEPTH_MIN;
        }
        if (depth >= EFFECT_OT_DEPTH_LIMIT) {
            depth = EFFECT_OT_DEPTH_MAX;
        }
        for (quad_row = 0; quad_row < 8; quad_row++) {
            for (quad_column = 0; quad_column < 8; quad_column++) {
                quad = &work->quads[work->buffer][quad_row * 8 + quad_column];
                u = (work->scroll_u >> 12) + work->texture_u;
                quad->u0 = quad->u2 = u;
                quad->u1 = quad->u3 = u + texture_width;
                v = (work->scroll_v >> 12) + work->texture_v;
                quad->v0 = quad->v1 = v;
                quad->v2 = quad->v3 = v + texture_height;
                row = g_effect_textured_grid_brightness_table[brightness_row][quad_row];
                quad->r0 = quad->r1 = (row * red) >> 12;
                quad->g0 = quad->g1 = (row * green) >> 12;
                quad->b0 = quad->b1 = (row * blue) >> 12;
                row = g_effect_textured_grid_brightness_table[brightness_row][quad_row + 1];
                quad->r2 = quad->r3 = (row * red) >> 12;
                quad->g2 = quad->g3 = (row * green) >> 12;
                quad->b2 = quad->b3 = (row * blue) >> 12;
                AddPrim(ot + depth, quad);
            }
        }
        work->frame++;
        work->buffer = 1 - work->buffer;
        break;
    case EFFECT_PHASE_DESTROY:
        work = record->work_slots[byte_offset];
        if (work->destroy_latched == 0) {
            work->destroy_latched = 1;
        } else {
            if (work != 0) {
                battle_heap_free_block(work);
                record->work_slots[byte_offset] = 0;
            }
            record->phase[byte_offset] = EFFECT_PHASE_IDLE;
        }
        break;
    }
}
