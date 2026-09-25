#include "fft/effect.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

/* Phase handler for the 8x8 wave-distorted textured quad mesh.
 *
 * Allocation, texture scroll and colour curves follow
 * effect_e230_update_textured_quad_grid_8x8; in addition, phase 2 displaces a 9x9
 * scratchpad lattice (48-pixel cells from (0x38, -0x40)) by a vertical and a
 * horizontal sine wave whose amplitude is itself modulated by a cosine per
 * row or column, then uses the lattice as the quads' corners.
 *
 * The lattice origins are spelled `(quad_row * 3 - 4) * 16` and
 * `(quad_column * 6 + 7) * 8` so each stays one product that the inner loop
 * hoists and the outer loop strength-reduces (spilled, copied into s7/s4 per
 * row or column, in the target's slot order); `quad_row * 0x30 - 0x40` is
 * reassociated by fold into a per-point `addiu -0x40`. `column` doubles as the
 * curve factor, as `row` does as the curve index and brightness. The s32
 * return with no value keeps $v0 live at the exit. */
s32 effect_e079_update_wave_mesh_state(s16 record_index, s32 byte_offset, s32 geometry_index, s32 frame) {
    s32 spread[3];
    effect_record_t* record;
    s32 translucent;
    u16 texture_page;
    u32 red;
    u32 green;
    u32 blue;
    s32 texture_width;
    s32 texture_height;
    u32* ot;
    s32 phase;
    s32 amplitude_phase;
    s32 step;
    s32 amplitude_step;
    s32 amplitude;
    s32 phase_step;
    effect_geometry_entry_t* geometry;
    effect_wave_mesh_scratch_t* scratch;
    effect_wave_mesh_work_t* work;
    POLY_GT4* quad;
    s32 row;
    s32 column;
    s32 quad_column;
    s32 quad_row;
    s32 speed;
    s16 angle;
    s32 depth;
    s32 brightness_row;
    s32 scaled;
    s32 wave;
    u8 u;
    u8 v;

    scratch = (effect_wave_mesh_scratch_t*)0x1F800000;
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
        work = record->work_slots[byte_offset] = battle_heap_alloc_block(sizeof(effect_wave_mesh_work_t), record_index);
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
        work->wave_x_amplitude_phase = 0;
        work->wave_x_phase = 0;
        work->wave_y_amplitude_phase = 0;
        work->wave_y_phase = 0;
        record->phase[byte_offset] = EFFECT_PHASE_UPDATE;
        break;
    case EFFECT_PHASE_UPDATE:
        work = record->work_slots[byte_offset];
        texture_width = geometry->texture_width / 2;
        texture_height = geometry->texture_height / 2;
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
        if (geometry->flags & 0x40) {
            row = geometry->colour_curves & 0xfff;
            red = g_effect_palette_table[row & 0xf].factor[work->frame];
            green = g_effect_palette_table[(row >> 4) & 0xf].factor[work->frame];
            blue = g_effect_palette_table[(row >> 8) & 0xf].factor[work->frame];
        } else {
            blue = 0x80;
            green = 0x80;
            red = 0x80;
        }

        phase = work->wave_y_phase;
        amplitude_phase = work->wave_y_amplitude_phase;
        row = geometry->curve_indices[1] & 0xf;
        row -= 1;
        if (row != -1) {
            column = g_effect_palette_table[row].factor[frame];
        } else {
            column = 0;
        }
        step = 0x20000 / battle_effect_lerp_linear(geometry->wave_y_start[0], geometry->wave_y_end[0], column);
        amplitude_step
            = 0x20000 / battle_effect_lerp_linear(geometry->wave_y_start[1], geometry->wave_y_end[1], column);
        amplitude = battle_effect_lerp_linear(geometry->wave_y_start[2], geometry->wave_y_end[2], column);
        phase_step = battle_effect_lerp_linear(geometry->wave_y_start[4], geometry->wave_y_end[4], column);
        for (quad_row = 0; quad_row < 9; quad_row++) {
            scaled = (amplitude * rcos(amplitude_phase)) >> 4;
            wave = phase;
            for (quad_column = 0; quad_column < 9; quad_column++) {
                scratch->points[quad_row][quad_column].y = ((rsin(wave) * scaled) >> 20) + (quad_row * 3 - 4) * 16;
                wave += step;
            }
            phase += phase_step;
            amplitude_phase += amplitude_step;
        }

        phase = work->wave_x_phase;
        amplitude_phase = work->wave_x_amplitude_phase;
        step = 0x20000 / battle_effect_lerp_linear(geometry->wave_x_start[0], geometry->wave_x_end[0], column);
        amplitude_step
            = 0x20000 / battle_effect_lerp_linear(geometry->wave_x_start[1], geometry->wave_x_end[1], column);
        amplitude = battle_effect_lerp_linear(geometry->wave_x_start[2], geometry->wave_x_end[2], column);
        phase_step = battle_effect_lerp_linear(geometry->wave_x_start[4], geometry->wave_x_end[4], column);
        for (quad_column = 0; quad_column < 9; quad_column++) {
            scaled = (amplitude * rcos(amplitude_phase)) >> 4;
            wave = phase;
            for (quad_row = 0; quad_row < 9; quad_row++) {
                scratch->points[quad_row][quad_column].x = ((rsin(wave) * scaled) >> 20) + (quad_column * 6 + 7) * 8;
                wave += step;
            }
            phase += phase_step;
            amplitude_phase += amplitude_step;
        }

        row = (geometry->curve_indices[0] >> 4) & 0xf;
        row -= 1;
        if (row != -1) {
            column = g_effect_palette_table[row].factor[frame];
        } else {
            column = 0;
        }
        battle_effect_interpolate_emitter_spawn_spread(geometry, column, spread);
        work->wave_y_phase += spread[0];
        work->wave_y_amplitude_phase += spread[1];
        row = (geometry->curve_indices[0] >> 12) & 0xf;
        row -= 1;
        if (row != -1) {
            column = g_effect_palette_table[row].factor[frame];
        } else {
            column = 0;
        }
        work->wave_x_phase += battle_effect_lerp_linear(geometry->motion.textured_quad.wave_phase_speed_start[0],
            geometry->motion.textured_quad.wave_phase_speed_end[0], column);
        work->wave_x_amplitude_phase
            += battle_effect_lerp_linear(geometry->motion.textured_quad.wave_phase_speed_start[1],
                geometry->motion.textured_quad.wave_phase_speed_end[1], column);

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
                quad->x0 = scratch->points[quad_row][quad_column].x;
                quad->y0 = scratch->points[quad_row][quad_column].y;
                quad->x1 = scratch->points[quad_row][quad_column + 1].x;
                quad->y1 = scratch->points[quad_row][quad_column + 1].y;
                quad->x2 = scratch->points[quad_row + 1][quad_column].x;
                quad->y2 = scratch->points[quad_row + 1][quad_column].y;
                quad->x3 = scratch->points[quad_row + 1][quad_column + 1].x;
                quad->y3 = scratch->points[quad_row + 1][quad_column + 1].y;
                u = (work->scroll_u >> 12) + work->texture_u;
                quad->u0 = quad->u2 = u;
                quad->u1 = quad->u3 = u + texture_width;
                v = (work->scroll_v >> 12) + work->texture_v;
                quad->v0 = quad->v1 = v;
                quad->v2 = quad->v3 = v + texture_height;
                row = g_effect_e073_wave_mesh_brightness_table[brightness_row][quad_row];
                quad->r0 = quad->r1 = (row * red) >> 12;
                quad->g0 = quad->g1 = (row * green) >> 12;
                quad->b0 = quad->b1 = (row * blue) >> 12;
                row = g_effect_e073_wave_mesh_brightness_table[brightness_row][quad_row + 1];
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
