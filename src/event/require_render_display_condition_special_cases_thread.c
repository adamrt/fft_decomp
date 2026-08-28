#include "fft/battle_gfx.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/require.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* The target re-reads the first-quad index for every field. */
#define QUAD (quads + parameters[0])[quad_index]

#define VERTEX_COLORS (&color_table[(parameters[0] + quad_index) * 0x14])

/* Fade a condition's sprites in over the two poly banks, hold them until
 * thread parameter 3 becomes 2, then fade them out.
 *
 * `table` and `color_table` are function-lifetime copies of the table
 * addresses; the target rematerialises them in each loop instead of folding
 * them into the loads. The bank copy and the small-array bank use
 * g_require_gfx_poly_gt4_banks, the second symbol at the address of
 * g_require_gfx_poly_gt4_array_32: sharing that base lets the hoisted
 * small-bank address derive from the copy source, which keeps it live in s0. */
void require_render_display_condition_special_cases_thread(void) {
    s32 parameter_index;
    s32 use_small_array;
    s32 quad_count;
    s32 buffer;
    s32 scale;
    s32 quad_index;
    require_display_condition_quad_t* quads;
    u8* table;
    u8* color_table;

    table = g_require_condition_special_quad_ranges;
    color_table = g_require_condition_special_vertex_colors;
    parameter_index = battle_thread_get_current_parameter_1();
    use_small_array = battle_thread_get_current_parameter_2();
    {
        u8* parameters = &g_require_condition_special_quad_ranges[parameter_index];

        quads = g_require_condition_special_quads;
        quad_count = parameters[1] - parameters[0];
    }
    if (use_small_array == 0) {
        require_gfx_init_poly_gt4_array_32(0x20, 0);
    } else {
        require_gfx_init_poly_gt4_array_8(0x20, 0);
    }

    for (quad_index = 0; quad_index < quad_count; quad_index++) {
        u8* parameters = &table[parameter_index];
        POLY_GT4* poly
            = &(use_small_array == 0 ? g_require_gfx_poly_gt4_array_32 : g_require_gfx_poly_gt4_array_8)[quad_index];

        poly->u0 = QUAD.u;
        poly->v0 = QUAD.v;
        poly->u1 = QUAD.u + QUAD.width;
        poly->v1 = QUAD.v;
        poly->u2 = QUAD.u;
        poly->v2 = QUAD.v + QUAD.height;
        poly->u3 = QUAD.u + QUAD.width;
        poly->v3 = QUAD.v + QUAD.height;
        poly->x0 = QUAD.x + 0x100;
        poly->y0 = QUAD.y + 0x78;
        poly->x1 = QUAD.x + 0x100 + QUAD.width;
        poly->y1 = QUAD.y + 0x78;
        poly->x2 = QUAD.x + 0x100;
        poly->y2 = QUAD.y + 0x78 + QUAD.height;
        poly->x3 = QUAD.x + 0x100 + QUAD.width;
        poly->y3 = QUAD.y + 0x78 + QUAD.height;
    }
    battle_copy_bytes(
        (POLY_GT4*)g_require_gfx_poly_gt4_banks + 40, g_require_gfx_poly_gt4_banks, 40 * sizeof(POLY_GT4));

    buffer = 0;
    scale = 0x30;
    /* Parameter 3 selects the next phase at any frame: 2 jumps from the
       fade-in past the hold loop to the fade-out, 1 ends the thread. */
    while (scale < 0x80) {
        if (battle_thread_get_current_parameter_3() == 2) {
            goto fade_out;
        }
        if (battle_thread_get_current_parameter_3() == 1) {
            goto done;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = (POLY_GT4*)g_require_gfx_poly_gt4_banks + 32 + buffer * 40;
            u8* parameters = &table[parameter_index];
            const u8* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_require_gfx_poly_gt4_array_32 + buffer * 40)[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, scale, poly);
                battle_gfx_draw_or_append_gpu_primitive((s32*)poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, scale, poly);
                battle_gfx_draw_or_append_gpu_primitive((s32*)poly);
            }
        }
        battle_thread_yield();
        scale += 8;
    }
    for (;;) {
        if (battle_thread_get_current_parameter_3() == 2) {
            break;
        }
        if (battle_thread_get_current_parameter_3() == 1) {
            goto done;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = (POLY_GT4*)g_require_gfx_poly_gt4_banks + 32 + buffer * 40;
            u8* parameters = &table[parameter_index];
            const u8* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_require_gfx_poly_gt4_array_32 + buffer * 40)[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, 0x80, poly);
                battle_gfx_draw_or_append_gpu_primitive((s32*)poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, 0x80, poly);
                battle_gfx_draw_or_append_gpu_primitive((s32*)poly);
            }
        }
        battle_thread_yield();
    }
fade_out:
    scale = 0x80;
    do {
        if (battle_thread_get_current_parameter_3() == 1) {
            break;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = (POLY_GT4*)g_require_gfx_poly_gt4_banks + 32 + buffer * 40;
            u8* parameters = &table[parameter_index];
            const u8* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_require_gfx_poly_gt4_array_32 + buffer * 40)[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, scale, poly);
                battle_gfx_draw_or_append_gpu_primitive((s32*)poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, scale, poly);
                battle_gfx_draw_or_append_gpu_primitive((s32*)poly);
            }
        }
        battle_thread_yield();
        scale -= 0xc;
    } while (scale >= 0x30);
done:
    battle_thread_yield();
    battle_thread_exit_current();
}
