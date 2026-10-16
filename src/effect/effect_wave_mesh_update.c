/* An 8x8 textured quad grid distorted by horizontal and vertical sine waves. */

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

/* Distorts an 8x8 textured quad grid with horizontal and vertical sine waves
 * on a 9x9 scratchpad lattice that rests on a 48-pixel grid from draw
 * position (56, -64), larger than the screen. Each wave's amplitude is
 * cosine-modulated by row or column, and the four wave phases advance once
 * per update by their speed curves. The lattice-origin products, shared index
 * temporaries and non-void signature preserve register allocation and loop
 * scheduling. */
s32 effect_wave_mesh_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
    s16 state_index = record_index;
    VECTOR spread;
    effect_record_t* record;
    s32 translucent;
    u16 texture_page;
    effect_texture_page_config_t configuration;
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
    const effect_geometry_record_t* geometry;
    effect_wave_mesh_scratch_t* scratch;
    effect_wave_mesh_work_t* work;
    POLY_GT4* quad;
    /* Keep unsigned bindings separate from the signed curve-index scratch. */
    u32 selector;
    u32 phase_selector;
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

    scratch = (effect_wave_mesh_scratch_t*)getScratchAddr(0);
    record = &g_battle_effect_state_records[state_index];
    geometry = &g_battle_effect_geometry_table->entries[emitter_index];
    ot = main_gfx_get_otag();

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            battle_heap_free_block(record->work_slots[callback_slot]);
        }
        work = record->work_slots[callback_slot]
            = battle_heap_alloc_block(sizeof(effect_wave_mesh_work_t), state_index);

        configuration = geometry->textured_quad.texture_page_config;
        translucent = 1;
        if (configuration.clear_abe) {
            translucent = 0;
        }
        texture_page
            = (configuration.blend_mode << EFFECT_RENDER_TEXTURE_PAGE_BLEND_SHIFT) | EFFECT_RENDER_TEXTURE_PAGE_BASE;
        for (row = 0; row < 8; row++) {
            for (column = 0; column < 8; column++) {
                quad = &work->quads[0][row * 8 + column];
                SetPolyGT4(quad);
                setSemiTrans(quad, translucent);
                quad->tpage = texture_page;
                quad->clut = MAP_CLUT(MAP_CLUT_ROW_EFFECT, 0);
                work->quads[1][row * 8 + column] = *quad;
            }
        }

        work->scroll_v = 0;
        work->scroll_u = 0;
        work->frame = 0;
        work->buffer = 0;
        work->destroy_latched = 0;
        work->texture_u = geometry->textured_quad.texture_u;
        work->texture_v = geometry->textured_quad.texture_v;
        work->wave_x_amplitude_phase = 0;
        work->wave_x_phase = 0;
        work->wave_y_amplitude_phase = 0;
        work->wave_y_phase = 0;
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;

    case EFFECT_CALLBACK_PHASE_UPDATE:
        work = record->work_slots[callback_slot];
        texture_width = geometry->textured_quad.texture_width / 2;
        texture_height = geometry->textured_quad.texture_height / 2;

        /* Scroll the half-size texture window by the speed and angle curves, wrapping within it. */
        selector = geometry->textured_quad.scroll_curves.scroll_speed;
        EFFECT_READ_CURVE(row, column, selector, work->frame);
        speed = battle_lerp_fixed8(
                    geometry->textured_quad.scroll_speed_start, geometry->textured_quad.scroll_speed_end, column)
            << 4;
        selector = geometry->textured_quad.scroll_curves.scroll_angle;
        EFFECT_READ_CURVE(row, column, selector, work->frame);
        angle = battle_lerp_fixed8(
            geometry->textured_quad.scroll_angle_start, geometry->textured_quad.scroll_angle_end, column);
        work->scroll_u += (rcos(angle) * speed) >> 12;
        /* Comparison shifts preserve the original loop scheduling. */
        while (work->scroll_u >= texture_width << 12) {
            work->scroll_u -= texture_width * ONE;
        }
        while (work->scroll_u < 0) {
            work->scroll_u += texture_width * ONE;
        }
        work->scroll_v += (rsin(angle) * speed) >> 12;
        while (work->scroll_v >= texture_height << 12) {
            work->scroll_v -= texture_height * ONE;
        }
        while (work->scroll_v < 0) {
            work->scroll_v += texture_height * ONE;
        }

        if (geometry->textured_quad.flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
            row = (geometry->textured_quad.color_curves & 0xfff);
            red = g_battle_effect_animation_curve_data
                      ->curves[(row >> EFFECT_COLOR_CURVE_RED_SHIFT) & 0xf][work->frame];
            green = g_battle_effect_animation_curve_data
                        ->curves[(row >> EFFECT_COLOR_CURVE_GREEN_SHIFT) & 0xf][work->frame];
            blue = g_battle_effect_animation_curve_data
                       ->curves[(row >> EFFECT_COLOR_CURVE_BLUE_SHIFT) & 0xf][work->frame];
        } else {
            blue = EFFECT_COLOR_NEUTRAL;
            green = EFFECT_COLOR_NEUTRAL;
            red = EFFECT_COLOR_NEUTRAL;
        }

        /* Vertical wave: each point's y swings by amplitude * sin * cos about rest rows 48 pixels
         * apart; sin and cos are 4.12, hence the >> 4 and >> 20. */
        phase = work->wave_y_phase;
        amplitude_phase = work->wave_y_amplitude_phase;
        selector = geometry->textured_quad.wave_curves.shape;
        EFFECT_READ_CURVE(row, column, selector, frame);
        step = 0x20000
            / battle_lerp_fixed8(
                geometry->textured_quad.wave_y_start.period, geometry->textured_quad.wave_y_end.period, column);
        amplitude_step = 0x20000
            / battle_lerp_fixed8(geometry->textured_quad.wave_y_start.amplitude_period,
                geometry->textured_quad.wave_y_end.amplitude_period, column);
        amplitude = battle_lerp_fixed8(
            geometry->textured_quad.wave_y_start.amplitude, geometry->textured_quad.wave_y_end.amplitude, column);
        phase_step = battle_lerp_fixed8(
            geometry->textured_quad.wave_y_start.phase_step, geometry->textured_quad.wave_y_end.phase_step, column);
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

        /* Horizontal wave: the same along the columns, about rest columns 48 pixels apart. */
        phase = work->wave_x_phase;
        amplitude_phase = work->wave_x_amplitude_phase;
        step = 0x20000
            / battle_lerp_fixed8(
                geometry->textured_quad.wave_x_start.period, geometry->textured_quad.wave_x_end.period, column);
        amplitude_step = 0x20000
            / battle_lerp_fixed8(geometry->textured_quad.wave_x_start.amplitude_period,
                geometry->textured_quad.wave_x_end.amplitude_period, column);
        amplitude = battle_lerp_fixed8(
            geometry->textured_quad.wave_x_start.amplitude, geometry->textured_quad.wave_x_end.amplitude, column);
        phase_step = battle_lerp_fixed8(
            geometry->textured_quad.wave_x_start.phase_step, geometry->textured_quad.wave_x_end.phase_step, column);
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

        /* The vertical wave's phase speeds are read through the spawn-spread interpolator (x and y). */
        phase_selector = geometry->textured_quad.scroll_curves.wave_y_phase_speed;
        EFFECT_READ_CURVE(row, column, phase_selector, frame);
        battle_effect_interpolate_emitter_spawn_spread(geometry, column, &spread);
        work->wave_y_phase += spread.vx;
        work->wave_y_amplitude_phase += spread.vy;

        phase_selector = geometry->textured_quad.scroll_curves.wave_x_phase_speed;
        EFFECT_READ_CURVE(row, column, phase_selector, frame);
        work->wave_x_phase += battle_lerp_fixed8(
            geometry->textured_quad.wave_phase_speed_start[0], geometry->textured_quad.wave_phase_speed_end[0], column);
        work->wave_x_amplitude_phase += battle_lerp_fixed8(
            geometry->textured_quad.wave_phase_speed_start[1], geometry->textured_quad.wave_phase_speed_end[1], column);

        /* The whole grid sits at one ordering-table depth. */
        depth = geometry->textured_quad.ot_depth;
        brightness_row = geometry->textured_quad.brightness_row;
        if (depth < EFFECT_OT_DEPTH_MIN) {
            depth = EFFECT_OT_DEPTH_MIN;
        }
        if (depth >= EFFECT_OT_DEPTH_LIMIT) {
            depth = EFFECT_OT_DEPTH_MAX;
        }

        /* Lattice row r shades corners 0/1 of quad row r and corners 2/3 of quad row r - 1. */
        for (quad_row = 0; quad_row < 8; quad_row++) {
            for (quad_column = 0; quad_column < 8; quad_column++) {
                quad = &work->quads[work->buffer][quad_row * 8 + quad_column];
                setXY4(quad, scratch->points[quad_row][quad_column].x, scratch->points[quad_row][quad_column].y,
                    scratch->points[quad_row][quad_column + 1].x, scratch->points[quad_row][quad_column + 1].y,
                    scratch->points[quad_row + 1][quad_column].x, scratch->points[quad_row + 1][quad_column].y,
                    scratch->points[quad_row + 1][quad_column + 1].x, scratch->points[quad_row + 1][quad_column + 1].y);
                u = (work->scroll_u >> 12) + work->texture_u;
                quad->u0 = quad->u2 = u;
                quad->u1 = quad->u3 = u + texture_width;
                v = (work->scroll_v >> 12) + work->texture_v;
                quad->v0 = quad->v1 = v;
                quad->v2 = quad->v3 = v + texture_height;
                row = g_effect_wave_mesh_brightness_table[brightness_row][quad_row];
                setRGB0(
                    quad, quad->r1 = (row * red) / ONE, quad->g1 = (row * green) / ONE, quad->b1 = (row * blue) / ONE);
                row = g_effect_wave_mesh_brightness_table[brightness_row][quad_row + 1];
                setRGB2(
                    quad, quad->r3 = (row * red) / ONE, quad->g3 = (row * green) / ONE, quad->b3 = (row * blue) / ONE);
                AddPrim(ot + depth, quad);
            }
        }

        /* The next update reads the next curve column and fills the other buffer while this one is drawn. */
        work->frame++;
        work->buffer = 1 - work->buffer;
        break;

    case EFFECT_CALLBACK_PHASE_DESTROY:
        work = record->work_slots[callback_slot];
        /* Free one call late, as the last update's quads may still be drawing. The null
         * check below comes after this read through work (retail order). */
        if (work->destroy_latched == 0) {
            work->destroy_latched = 1;
        } else {
            if (work != 0) {
                battle_heap_free_block(work);
                record->work_slots[callback_slot] = 0;
            }
            record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_IDLE;
        }
        break;
    }
}
