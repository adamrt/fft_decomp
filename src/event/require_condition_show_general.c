#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
/* 0x801c8f80-0x801c9fa4, 4132 bytes.
 *
 * General-case display-condition banner: builds one POLY_GT4 per quad into
 * buffer 0, mirrors it into buffer 1, then plays a 20-frame reveal (the lower
 * three quads from frame 0, the upper three from frame 9), an optional
 * `hold_frames` hold and a 10-frame hide. Circle aborts reveal and hold.
 *
 * Load-bearing spellings: the tables are function-lifetime locals and the
 * range row is `&range[condition]` with `condition = 0` (a spilled
 * constant-equivalent pseudo), which reproduces the target's constant-address
 * count read and its `addiu sN,t0,0` range rematerialisation per phase;
 * `tpage` is a local copy so the page load keeps its +2 offset; `x + 0x100 +
 * width` / `y + (height + 0x78) - offset` / `y + 0x78 + offset` fix operand
 * order; declaring `frame` before `quad_count` fixes their stack slots.
 *
 * `buffer` is read before it is first written (the target's s7 is live out of
 * the prologue), so it is left uninitialized on purpose.
 */
#include "fft/battle_script.h"
#include "fft/event_require.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

#define QUAD(index) (quads + parameters[0])[index]

#define VERTEX_COLORS(index) (&colors[parameters[0] + index])

/* Full, unanimated quad: the reveal offset is not yet applied. */
#define STORE_QUAD_FULL(poly, index)                                                                                   \
    poly->u0 = QUAD(index).source.x;                                                                                   \
    poly->v0 = QUAD(index).source.y - 0x80;                                                                            \
    poly->u1 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v1 = QUAD(index).source.y - 0x80;                                                                            \
    poly->u2 = QUAD(index).source.x;                                                                                   \
    poly->v2 = QUAD(index).source.y - 0x80 + QUAD(index).source.h;                                                     \
    poly->u3 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v3 = QUAD(index).source.y - 0x80 + QUAD(index).source.h;                                                     \
    poly->x0 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y0 = QUAD(index).offset.y + 0x78;                                                                            \
    poly->x1 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y1 = QUAD(index).offset.y + 0x78;                                                                            \
    poly->x2 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y2 = QUAD(index).offset.y + 0x78 + QUAD(index).source.h;                                                     \
    poly->x3 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y3 = QUAD(index).offset.y + 0x78 + QUAD(index).source.h

/* Reveal: keep the bottom edge fixed while raising the top edge. */
#define STORE_QUAD_REVEAL(poly, index, reveal_pixels)                                                                  \
    poly->u0 = QUAD(index).source.x;                                                                                   \
    poly->v0 = (QUAD(index).source.y + (QUAD(index).source.h - 0x80)) - reveal_pixels;                                 \
    poly->u1 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v1 = (QUAD(index).source.y + (QUAD(index).source.h - 0x80)) - reveal_pixels;                                 \
    poly->u2 = QUAD(index).source.x;                                                                                   \
    poly->v2 = QUAD(index).source.y + (QUAD(index).source.h - 0x80);                                                   \
    poly->u3 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v3 = QUAD(index).source.y + (QUAD(index).source.h - 0x80);                                                   \
    poly->x0 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y0 = QUAD(index).offset.y + (QUAD(index).source.h + 0x78) - reveal_pixels;                                   \
    poly->x1 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y1 = QUAD(index).offset.y + (QUAD(index).source.h + 0x78) - reveal_pixels;                                   \
    poly->x2 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y2 = QUAD(index).offset.y + 0x78 + QUAD(index).source.h;                                                     \
    poly->x3 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y3 = QUAD(index).offset.y + 0x78 + QUAD(index).source.h

/* Hide: keep the bottom edge fixed while lowering the top edge. */
#define STORE_QUAD_HIDE(poly, index, hide_pixels)                                                                      \
    poly->u0 = QUAD(index).source.x;                                                                                   \
    poly->v0 = QUAD(index).source.y - 0x80 + hide_pixels;                                                              \
    poly->u1 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v1 = QUAD(index).source.y - 0x80 + hide_pixels;                                                              \
    poly->u2 = QUAD(index).source.x;                                                                                   \
    poly->v2 = QUAD(index).source.y + ((hide_pixels) - 0x80) + (QUAD(index).source.h - hide_pixels);                   \
    poly->u3 = QUAD(index).source.x + QUAD(index).source.w;                                                            \
    poly->v3 = QUAD(index).source.y + ((hide_pixels) - 0x80) + (QUAD(index).source.h - hide_pixels);                   \
    poly->x0 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y0 = QUAD(index).offset.y + 0x78 + hide_pixels;                                                              \
    poly->x1 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y1 = QUAD(index).offset.y + 0x78 + hide_pixels;                                                              \
    poly->x2 = QUAD(index).offset.x + 0x100;                                                                           \
    poly->y2 = QUAD(index).offset.y + ((hide_pixels) + 0x78) + (QUAD(index).source.h - hide_pixels);                   \
    poly->x3 = QUAD(index).offset.x + 0x100 + QUAD(index).source.w;                                                    \
    poly->y3 = QUAD(index).offset.y + ((hide_pixels) + 0x78) + (QUAD(index).source.h - hide_pixels)

void require_condition_show_general(s32 hold_frames) {
    sprite_sheet_frame_t* quads;
    u8* range;
    require_display_condition_colors_t* colors;
    require_render_condition_tpage_t* tpage;
    s32 condition;
    s32 frame;
    s32 quad_count;
    s32 index;
    s32 buffer;
    s32 reveal_offset_bottom;
    s32 reveal_offset_top;
    s32 hide_offset;
    u32* input;

    range = g_require_condition_general_quad_ranges;
    colors = g_require_condition_general_vertex_colors;
    tpage = g_require_condition_general_tpages;
    condition = 0;
    require_reward_load_bonus_image(thread_get_current_parameter_1_variant() - EVENT_DISPLAY_CONDITION_GENERAL_FIRST);
    {
        u8* parameters = &range[condition];

        quad_count = parameters[1] - parameters[0];
    }
    require_gfx_init_poly_gt4_array_32(0, 1);
    quads = g_require_condition_general_quads;

    for (index = 0; index < quad_count; index++) {
        POLY_GT4* poly = &g_require_gfx_poly_gt4_buffers[0][index];
        u8* parameters = &range[condition];
        s32 page;

        STORE_QUAD_FULL(poly, index);
        if (GetGraphType() == 1 || GetGraphType() == 2)
            page = (tpage[index].page & 0x60) * 4;
        else
            page = tpage[index].page & 0x60;
        poly->tpage = page | 6;
    }
    {
        POLY_GT4* destination = &g_require_gfx_poly_gt4_buffers[1][0];
        POLY_GT4* source = destination - 40;

        mem_copy_bytes_nonempty(destination, source, 0x820);
    }

    reveal_offset_top = 0;
    reveal_offset_bottom = 0;
    frame = 0;
    for (;;) {
        u8* parameters = &range[condition];

        thread_yield();
        input = script_get_controller_input_pointer(0);
        g_menu_input_controller = input;
        /* Circle skips the remaining phases to the shared exit. */
        if (*input & PSX_PAD_CIRCLE)
            goto done;
        buffer = (buffer + 1) & 1;
        if (frame >= 9) {
            for (index = 5; index >= 3; index--) {
                POLY_GT4* poly = &g_require_gfx_poly_gt4_buffers[buffer][index];

                if (QUAD(index).source.h < reveal_offset_top)
                    reveal_offset_top = QUAD(index).source.h;
                STORE_QUAD_REVEAL(poly, index, reveal_offset_top);
                require_gfx_scale_poly_gt4_vertex_colors(VERTEX_COLORS(index), 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        for (index = 2; index >= 0; index--) {
            POLY_GT4* poly = &g_require_gfx_poly_gt4_buffers[buffer][index];

            if (QUAD(index).source.h < reveal_offset_bottom)
                reveal_offset_bottom = QUAD(index).source.h;
            STORE_QUAD_REVEAL(poly, index, reveal_offset_bottom);
            require_gfx_scale_poly_gt4_vertex_colors(VERTEX_COLORS(index), 0x80, poly);
            gfx_draw_or_append_gpu_primitive(poly);
        }
        reveal_offset_bottom += 8;
        /* The one-trip loop emits nothing; its loop note weights these two
         * references of reveal_offset_top one level deeper, which keeps the
         * top offset ahead of the bottom one in global allocation (s3/s6)
         * while both are initialized top first like the target. */
        if (frame >= 9)
            do {
                reveal_offset_top += 8;
            } while (0);
        frame++;
        if (frame >= 0x14)
            break;
    }

    frame = 0;
    if (hold_frames > 0) {
        do {
            thread_yield();
            input = script_get_controller_input_pointer(0);
            g_menu_input_controller = input;
            if (*input & PSX_PAD_CIRCLE)
                goto done;
            buffer = (buffer + 1) & 1;
            for (index = quad_count - 1; index >= 0; index--) {
                POLY_GT4* poly = &g_require_gfx_poly_gt4_buffers[buffer][index];
                u8* parameters = &range[condition];
                require_gfx_scale_poly_gt4_vertex_colors(VERTEX_COLORS(index), 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
            frame++;
        } while (frame < hold_frames);
    }

    hide_offset = 0;
    frame = 0;
    do {
        u8* parameters = &range[condition];

        thread_yield();
        buffer = (buffer + 1) & 1;
        for (index = quad_count - 1; index >= 0; index--) {
            POLY_GT4* poly = &g_require_gfx_poly_gt4_buffers[buffer][index];

            if (QUAD(index).source.h < hide_offset)
                hide_offset = QUAD(index).source.h;
            STORE_QUAD_HIDE(poly, index, hide_offset);
            require_gfx_scale_poly_gt4_vertex_colors(VERTEX_COLORS(index), 0x80, poly);
            gfx_draw_or_append_gpu_primitive(poly);
        }
        hide_offset += 8;
        frame++;
    } while (frame < 0xa);
done:
    thread_yield();
    thread_exit_current();
}
