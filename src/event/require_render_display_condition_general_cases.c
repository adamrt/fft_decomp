/* 0x801c8f80-0x801c9fa4, 4132 bytes.
 *
 * General-case display-condition banner: builds one POLY_GT4 per quad into
 * bank 0, mirrors it into bank 1, then plays a 20-frame reveal (the lower
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
 * `bank` is read before it is first written (the target's s7 is live out of
 * the prologue), so it is left uninitialised on purpose.
 */
#include "fft/battle_gfx.h"
#include "fft/main_heap.h"
#include "fft/main_runtime.h"
#include "fft/require.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Four-byte record per general-case quad; only bit 5/6 of `page` is consumed,
 * shifted left by two for the 24bpp graph types. */
typedef struct require_render_condition_tpage {
    u8 unk0;
    u8 unk1;
    u8 page;
    u8 unk3;
} require_render_condition_tpage_t;

extern require_render_condition_tpage_t g_require_condition_general_tpages[];

#define QUAD(index) (quads + parameters[0])[index]

#define VERTEX_COLORS(index) (&colors[(parameters[0] + (index)) * 0x14])

/* Full, unanimated quad: the reveal offset is not yet applied. */
#define STORE_QUAD_FULL(poly, index)                                                                                   \
    (poly)->u0 = QUAD(index).u;                                                                                        \
    (poly)->v0 = QUAD(index).v - 0x80;                                                                                 \
    (poly)->u1 = QUAD(index).u + QUAD(index).width;                                                                    \
    (poly)->v1 = QUAD(index).v - 0x80;                                                                                 \
    (poly)->u2 = QUAD(index).u;                                                                                        \
    (poly)->v2 = QUAD(index).v - 0x80 + QUAD(index).height;                                                            \
    (poly)->u3 = QUAD(index).u + QUAD(index).width;                                                                    \
    (poly)->v3 = QUAD(index).v - 0x80 + QUAD(index).height;                                                            \
    (poly)->x0 = QUAD(index).x + 0x100;                                                                                \
    (poly)->y0 = QUAD(index).y + 0x78;                                                                                 \
    (poly)->x1 = QUAD(index).x + 0x100 + QUAD(index).width;                                                            \
    (poly)->y1 = QUAD(index).y + 0x78;                                                                                 \
    (poly)->x2 = QUAD(index).x + 0x100;                                                                                \
    (poly)->y2 = QUAD(index).y + 0x78 + QUAD(index).height;                                                            \
    (poly)->x3 = QUAD(index).x + 0x100 + QUAD(index).width;                                                            \
    (poly)->y3 = QUAD(index).y + 0x78 + QUAD(index).height

/* Reveal: the bottom edge is fixed and the top edge rises by `offset`. */
#define STORE_QUAD_REVEAL(poly, index, offset)                                                                         \
    (poly)->u0 = QUAD(index).u;                                                                                        \
    (poly)->v0 = (QUAD(index).v + (QUAD(index).height - 0x80)) - (offset);                                             \
    (poly)->u1 = QUAD(index).u + QUAD(index).width;                                                                    \
    (poly)->v1 = (QUAD(index).v + (QUAD(index).height - 0x80)) - (offset);                                             \
    (poly)->u2 = QUAD(index).u;                                                                                        \
    (poly)->v2 = QUAD(index).v + (QUAD(index).height - 0x80);                                                          \
    (poly)->u3 = QUAD(index).u + QUAD(index).width;                                                                    \
    (poly)->v3 = QUAD(index).v + (QUAD(index).height - 0x80);                                                          \
    (poly)->x0 = QUAD(index).x + 0x100;                                                                                \
    (poly)->y0 = QUAD(index).y + (QUAD(index).height + 0x78) - (offset);                                               \
    (poly)->x1 = QUAD(index).x + 0x100 + QUAD(index).width;                                                            \
    (poly)->y1 = QUAD(index).y + (QUAD(index).height + 0x78) - (offset);                                               \
    (poly)->x2 = QUAD(index).x + 0x100;                                                                                \
    (poly)->y2 = QUAD(index).y + 0x78 + QUAD(index).height;                                                            \
    (poly)->x3 = QUAD(index).x + 0x100 + QUAD(index).width;                                                            \
    (poly)->y3 = QUAD(index).y + 0x78 + QUAD(index).height

/* Hide: the bottom edge is fixed and the top edge sinks by `offset`. */
#define STORE_QUAD_HIDE(poly, index, offset)                                                                           \
    (poly)->u0 = QUAD(index).u;                                                                                        \
    (poly)->v0 = QUAD(index).v - 0x80 + (offset);                                                                      \
    (poly)->u1 = QUAD(index).u + QUAD(index).width;                                                                    \
    (poly)->v1 = QUAD(index).v - 0x80 + (offset);                                                                      \
    (poly)->u2 = QUAD(index).u;                                                                                        \
    (poly)->v2 = QUAD(index).v + ((offset) - 0x80) + (QUAD(index).height - (offset));                                  \
    (poly)->u3 = QUAD(index).u + QUAD(index).width;                                                                    \
    (poly)->v3 = QUAD(index).v + ((offset) - 0x80) + (QUAD(index).height - (offset));                                  \
    (poly)->x0 = QUAD(index).x + 0x100;                                                                                \
    (poly)->y0 = QUAD(index).y + 0x78 + (offset);                                                                      \
    (poly)->x1 = QUAD(index).x + 0x100 + QUAD(index).width;                                                            \
    (poly)->y1 = QUAD(index).y + 0x78 + (offset);                                                                      \
    (poly)->x2 = QUAD(index).x + 0x100;                                                                                \
    (poly)->y2 = QUAD(index).y + ((offset) + 0x78) + (QUAD(index).height - (offset));                                  \
    (poly)->x3 = QUAD(index).x + 0x100 + QUAD(index).width;                                                            \
    (poly)->y3 = QUAD(index).y + ((offset) + 0x78) + (QUAD(index).height - (offset))

void require_render_display_condition_general_cases(s32 hold_frames) {
    require_display_condition_quad_t* quads;
    u8* range;
    u8* colors;
    require_render_condition_tpage_t* tpage;
    s32 condition;
    s32 frame;
    s32 quad_count;
    s32 index;
    s32 bank;
    s32 reveal_offset_bottom;
    s32 reveal_offset_top;
    s32 hide_offset;
    u32* input;

    range = g_require_condition_general_quad_ranges;
    colors = g_require_condition_general_vertex_colors;
    tpage = g_require_condition_general_tpages;
    condition = 0;
    require_reward_load_bonus_image(battle_thread_get_current_parameter_1() - 8);
    {
        u8* parameters = &range[condition];

        quad_count = parameters[1] - parameters[0];
    }
    require_gfx_init_poly_gt4_array_32(0, 1);
    quads = g_require_condition_general_quads;

    for (index = 0; index < quad_count; index++) {
        POLY_GT4* poly = &g_require_gfx_poly_gt4_banks[0][index];
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
        POLY_GT4* destination = &g_require_gfx_poly_gt4_banks[1][0];
        POLY_GT4* source = destination - 40;

        battle_copy_bytes(destination, source, 0x820);
    }

    reveal_offset_top = 0;
    reveal_offset_bottom = 0;
    frame = 0;
    for (;;) {
        u8* parameters = &range[condition];

        battle_thread_yield();
        input = battle_script_get_controller_input_pointer(0);
        g_require_input_controller = input;
        /* Circle skips the remaining phases to the shared exit. */
        if (*input & PSX_PAD_CIRCLE)
            goto done;
        bank = (bank + 1) & 1;
        if (frame >= 9) {
            for (index = 5; index >= 3; index--) {
                POLY_GT4* poly = &g_require_gfx_poly_gt4_banks[bank][index];

                if (QUAD(index).height < reveal_offset_top)
                    reveal_offset_top = QUAD(index).height;
                STORE_QUAD_REVEAL(poly, index, reveal_offset_top);
                require_gfx_scale_poly_gt4_vertex_colors(VERTEX_COLORS(index), 0x80, poly);
                battle_gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        for (index = 2; index >= 0; index--) {
            POLY_GT4* poly = &g_require_gfx_poly_gt4_banks[bank][index];

            if (QUAD(index).height < reveal_offset_bottom)
                reveal_offset_bottom = QUAD(index).height;
            STORE_QUAD_REVEAL(poly, index, reveal_offset_bottom);
            require_gfx_scale_poly_gt4_vertex_colors(VERTEX_COLORS(index), 0x80, poly);
            battle_gfx_draw_or_append_gpu_primitive(poly);
        }
        reveal_offset_bottom += 8;
        /* The one-trip loop emits nothing; its loop note weights these two
         * references of reveal_offset_top one level deeper, which keeps the
         * top offset ahead of the bottom one in global allocation (s3/s6)
         * while both are initialised top first like the target. */
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
            battle_thread_yield();
            input = battle_script_get_controller_input_pointer(0);
            g_require_input_controller = input;
            if (*input & PSX_PAD_CIRCLE)
                goto done;
            bank = (bank + 1) & 1;
            for (index = quad_count - 1; index >= 0; index--) {
                POLY_GT4* poly = &g_require_gfx_poly_gt4_banks[bank][index];
                u8* parameters = &range[condition];
                require_gfx_scale_poly_gt4_vertex_colors(VERTEX_COLORS(index), 0x80, poly);
                battle_gfx_draw_or_append_gpu_primitive(poly);
            }
            frame++;
        } while (frame < hold_frames);
    }

    hide_offset = 0;
    frame = 0;
    do {
        u8* parameters = &range[condition];

        battle_thread_yield();
        bank = (bank + 1) & 1;
        for (index = quad_count - 1; index >= 0; index--) {
            POLY_GT4* poly = &g_require_gfx_poly_gt4_banks[bank][index];

            if (QUAD(index).height < hide_offset)
                hide_offset = QUAD(index).height;
            STORE_QUAD_HIDE(poly, index, hide_offset);
            require_gfx_scale_poly_gt4_vertex_colors(VERTEX_COLORS(index), 0x80, poly);
            battle_gfx_draw_or_append_gpu_primitive(poly);
        }
        hide_offset += 8;
        frame++;
    } while (frame < 0xa);
done:
    battle_thread_yield();
    battle_thread_exit_current();
}
