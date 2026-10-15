#include "fft/event_require.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* The target re-reads the first-quad index for every field. */
#define QUAD (quads + parameters[0])[quad_index]

#define VERTEX_COLORS (&color_table[parameters[0] + quad_index])

/* Fade a condition's sprites in over the two primitive buffers, hold them until
 * thread parameter 3 becomes 2, then fade them out.
 *
 * `table` and `color_table` are function-lifetime copies of the table
 * addresses; the target rematerialises them in each loop instead of folding
 * them into the loads. The buffer copy and the small-array address use
 * g_require_gfx_poly_gt4_buffers, the second symbol at the address of
 * g_require_gfx_poly_gt4_array_32: sharing that base lets the hoisted
 * small-array address derive from the copy source, which keeps it live in s0. */
void require_condition_run_special_thread(void) {
    s32 parameter_index;
    s32 use_small_array;
    s32 quad_count;
    s32 buffer;
    s32 scale;
    s32 quad_index;
    sprite_sheet_frame_t* quads;
    u8* table;
    require_display_condition_colors_t* color_table;

    table = g_require_condition_special_quad_ranges;
    color_table = g_require_condition_special_vertex_colors;
    parameter_index = thread_get_current_parameter_1_variant();
    use_small_array = thread_get_current_parameter_2_variant();
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

        setUV4(poly, QUAD.source.x, QUAD.source.y, QUAD.source.x + QUAD.source.w, QUAD.source.y, QUAD.source.x,
            QUAD.source.y + QUAD.source.h, QUAD.source.x + QUAD.source.w, QUAD.source.y + QUAD.source.h);
        setXY4(poly, QUAD.offset.x + 0x100, QUAD.offset.y + 0x78, QUAD.offset.x + 0x100 + QUAD.source.w,
            QUAD.offset.y + 0x78, QUAD.offset.x + 0x100, QUAD.offset.y + 0x78 + QUAD.source.h,
            QUAD.offset.x + 0x100 + QUAD.source.w, QUAD.offset.y + 0x78 + QUAD.source.h);
    }
    mem_copy_bytes_nonempty(g_require_gfx_poly_gt4_buffers[1], g_require_gfx_poly_gt4_buffers, 40 * sizeof(POLY_GT4));

    buffer = 0;
    scale = 0x30;
    /* Parameter 3 selects the next phase at any frame: 2 jumps from the
       fade-in past the hold loop to the fade-out, 1 ends the thread. */
    while (scale < 0x80) {
        if (thread_get_current_parameter_3_variant() == 2) {
            goto fade_out;
        }
        if (thread_get_current_parameter_3_variant() == 1) {
            goto done;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = &g_require_gfx_poly_gt4_buffers[buffer][32];
            u8* parameters = &table[parameter_index];
            const require_display_condition_colors_t* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_require_gfx_poly_gt4_array_32 + buffer * 40)[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        thread_yield();
        scale += 8;
    }
    for (;;) {
        if (thread_get_current_parameter_3_variant() == 2) {
            break;
        }
        if (thread_get_current_parameter_3_variant() == 1) {
            goto done;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = &g_require_gfx_poly_gt4_buffers[buffer][32];
            u8* parameters = &table[parameter_index];
            const require_display_condition_colors_t* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_require_gfx_poly_gt4_array_32 + buffer * 40)[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        thread_yield();
    }
fade_out:
    scale = 0x80;
    do {
        if (thread_get_current_parameter_3_variant() == 1) {
            break;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = &g_require_gfx_poly_gt4_buffers[buffer][32];
            u8* parameters = &table[parameter_index];
            const require_display_condition_colors_t* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_require_gfx_poly_gt4_array_32 + buffer * 40)[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                require_gfx_scale_poly_gt4_vertex_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        thread_yield();
        scale -= 0xc;
    } while (scale >= 0x30);
done:
    thread_yield();
    thread_exit_current();
}
