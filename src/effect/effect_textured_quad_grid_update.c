/* A grid of textured quads with a scrolling texture window and tints from color curves. Each effect overlay that
 * links the renderer builds this source with its grid size: VARIANT_QUAD_GRID_SIZE quads on a side (2, 4 or 8). */

#include "fft/battle.h"
#include "fft/battle_effect.h"
#include "fft/battle_effect_file.h"
#include "fft/battle_effect_geometry.h"
#include "fft/battle_effect_particle.h"
#include "fft/battle_effect_script.h"
#include "fft/battle_map_palette.h"
#include "fft/effect.h"
#include "fft/main_gfx.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* The grid is QUAD_GRID_SIZE quads on a side, each QUAD_GRID_CELL pixels of the 256x256 field. The 4x4 builds
 * clamp the ordering-table depth at 1, the 2x2 and 8x8 builds at EFFECT_OT_DEPTH_MIN. */
enum {
    QUAD_GRID_SIZE = VARIANT_QUAD_GRID_SIZE,
    QUAD_GRID_CELL = 256 / QUAD_GRID_SIZE,
    QUAD_GRID_DEPTH_MIN = QUAD_GRID_SIZE == 4 ? 1 : EFFECT_OT_DEPTH_MIN,
};

/* The renderer's work block, allocated in phase 1 and freed in phase 3. The 2x2 and 4x4 builds leave a 0x48-byte
 * gap after the quads that the 8x8 build does not have. */
typedef struct effect_textured_quad_grid_work {
    s32 buffer;                                         /* quads[buffer] is drawn this frame */
    POLY_GT4 quads[2][QUAD_GRID_SIZE * QUAD_GRID_SIZE]; /* row-major grid, double buffered */
#if VARIANT_QUAD_GRID_SIZE != 8
    u8 _unused_gap[0x48];
#endif
    s32 scroll_u; /* 20.12 texture scroll, wrapped to texture_width / 2 */
    s32 scroll_v; /* 20.12 texture scroll, wrapped to texture_height / 2 */
    u8 _unused_scroll_pad[4];
    u16 texture_u; /* from the emitter record; read back as u8 */
    u16 texture_v;
    s32 frame;           /* column of the animation curves */
    s32 destroy_latched; /* phase 3 frees the block on its second call */
} effect_textured_quad_grid_work_t;
typedef char assert_effect_textured_quad_grid_work_size[sizeof(effect_textured_quad_grid_work_t)
            == (QUAD_GRID_SIZE == 2       ? 516
                    : QUAD_GRID_SIZE == 4 ? 1764
                                          : 6684)
        ? 1
        : -1];

/* The overlay's vertex brightness rows (12-bit fixed): a row per brightness setting, a column per quad row edge.
 * Declared here because its width follows the build's grid size. */
extern u32 g_effect_textured_quad_grid_brightness_table[][QUAD_GRID_SIZE + 1];

/*
 * Phase handler for a scrolling textured quad grid.
 *
 * Phase 1 builds both POLY_GT4 buffers of a 256x256 grid; phase 2 scrolls the
 * texture window along the emitter record's speed and angle curves, tints each
 * quad from the color curves and the overlay-local vertex brightness rows, and
 * links the current buffer into the ordering table; phase 3 frees the work
 * block one call after it is first reached.
 *
 * row and column are also the update phase's curve scratch values. GCC 2.6.3
 * gives each C variable one pseudo, and that sharing is what places the curve
 * index, color curves and brightness in s2 and the curve factor in s3.
 */
s32 effect_textured_quad_grid_update(s32 record_index, s32 callback_slot, s32 emitter_index, s32 frame) {
    s16 state_index = record_index;
    effect_record_t* record = &g_battle_effect_state_records[state_index];
    s32 translucent;
    u16 texture_page;
    effect_texture_page_config_t configuration;
    s32 texture_width;
    s32 texture_height;
    u32* ot;
    const effect_textured_quad_geometry_view_t* geometry
        = &g_battle_effect_geometry_table->entries[emitter_index].textured_quad;
    effect_textured_quad_grid_work_t* work;
    POLY_GT4* quad;
    /* Keep unsigned bindings separate from the signed curve-index scratch. */
    u32 selector;
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

    ot = gfx_get_battle_otag();

    switch (record->callback_phase[callback_slot]) {
    case EFFECT_CALLBACK_PHASE_IDLE:
        break;

    case EFFECT_CALLBACK_PHASE_INIT:
        if (record->work_slots[callback_slot] != 0) {
            battle_heap_free_block(record->work_slots[callback_slot]);
        }
        work = battle_heap_alloc_block(sizeof(effect_textured_quad_grid_work_t), state_index);
        record->work_slots[callback_slot] = work;

        configuration = geometry->texture_page_config;
        translucent = 1;
        if (configuration.clear_abe) {
            translucent = 0;
        }
        texture_page
            = (configuration.blend_mode << EFFECT_RENDER_TEXTURE_PAGE_BLEND_SHIFT) | EFFECT_RENDER_TEXTURE_PAGE_BASE;

        /* The quads never move, so their positions are set here once, in both buffers. The grid
         * starts at the left screen edge, MAIN_GFX_BATTLE_SCREEN_X. */
        for (row = 0; row < QUAD_GRID_SIZE; row++) {
            for (column = 0; column < QUAD_GRID_SIZE; column++) {
                quad = &work->quads[0][row * QUAD_GRID_SIZE + column];
                SetPolyGT4(quad);
                setSemiTrans(quad, translucent);
                quad->tpage = texture_page;
                quad->clut = MAP_CLUT(MAP_CLUT_ROW_EFFECT, 0);
                quad->x0 = quad->x2 = column * QUAD_GRID_CELL + MAIN_GFX_BATTLE_SCREEN_X;
                quad->y0 = quad->y1 = row * QUAD_GRID_CELL;
                quad->x1 = quad->x3 = column * QUAD_GRID_CELL + MAIN_GFX_BATTLE_SCREEN_X + QUAD_GRID_CELL;
                quad->y2 = quad->y3 = row * QUAD_GRID_CELL + QUAD_GRID_CELL;
                work->quads[1][row * QUAD_GRID_SIZE + column] = *quad;
            }
        }

        work->scroll_v = 0;
        work->scroll_u = 0;
        work->frame = 0;
        work->buffer = 0;
        work->destroy_latched = 0;
        work->texture_u = geometry->texture_u;
        work->texture_v = geometry->texture_v;
        record->callback_phase[callback_slot] = EFFECT_CALLBACK_PHASE_UPDATE;
        break;

    case EFFECT_CALLBACK_PHASE_UPDATE:
        work = record->work_slots[callback_slot];
        texture_width = geometry->texture_width / 2;
        texture_height = geometry->texture_height / 2;

        /* Scroll the half-size texture window by the speed and angle curves, wrapping within it. */
        selector = geometry->scroll_curves.scroll_speed;
        EFFECT_READ_CURVE(row, column, selector, work->frame);
        speed = battle_lerp_q8(geometry->scroll_speed_start, geometry->scroll_speed_end, column) << 4;
        selector = geometry->scroll_curves.scroll_angle;
        EFFECT_READ_CURVE(row, column, selector, work->frame);
        angle = battle_lerp_q8(geometry->scroll_angle_start, geometry->scroll_angle_end, column);
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

        blue = EFFECT_COLOR_NEUTRAL;
        if (geometry->flags & EFFECT_EMITTER_FLAG_COLOR_CURVES) {
            row = (geometry->color_curves & 0xfff);
            red = g_battle_effect_animation_curve_data
                      ->curves[(row >> EFFECT_COLOR_CURVE_RED_SHIFT) & 0xf][work->frame];
            green = g_battle_effect_animation_curve_data
                        ->curves[(row >> EFFECT_COLOR_CURVE_GREEN_SHIFT) & 0xf][work->frame];
            blue = g_battle_effect_animation_curve_data
                       ->curves[(row >> EFFECT_COLOR_CURVE_BLUE_SHIFT) & 0xf][work->frame];
        } else {
            green = EFFECT_COLOR_NEUTRAL;
            red = EFFECT_COLOR_NEUTRAL;
        }

        /* The whole grid sits at one ordering-table depth. */
        depth = geometry->ot_depth;
        brightness_row = geometry->brightness_row;
        if (depth < QUAD_GRID_DEPTH_MIN) {
            depth = QUAD_GRID_DEPTH_MIN;
        }
        if (depth >= EFFECT_OT_DEPTH_LIMIT) {
            depth = EFFECT_OT_DEPTH_MAX;
        }

        /* Lattice row r shades corners 0/1 of quad row r and corners 2/3 of quad row r - 1. */
        for (quad_row = 0; quad_row < QUAD_GRID_SIZE; quad_row++) {
            for (quad_column = 0; quad_column < QUAD_GRID_SIZE; quad_column++) {
                quad = &work->quads[work->buffer][quad_row * QUAD_GRID_SIZE + quad_column];
                u = (work->scroll_u >> 12) + work->texture_u;
                quad->u0 = quad->u2 = u;
                quad->u1 = quad->u3 = u + texture_width;
                v = (work->scroll_v >> 12) + work->texture_v;
                quad->v0 = quad->v1 = v;
                quad->v2 = quad->v3 = v + texture_height;
                row = g_effect_textured_quad_grid_brightness_table[brightness_row][quad_row];
                setRGB0(
                    quad, quad->r1 = (row * red) / ONE, quad->g1 = (row * green) / ONE, quad->b1 = (row * blue) / ONE);
                row = g_effect_textured_quad_grid_brightness_table[brightness_row][quad_row + 1];
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
