/* Battle-condition banners use shaded BONUS.BIN quads for the general
 * wipe animation and for the named battle-result and party-change messages. */

#include "fft/after_battle.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/memory.h"
#include "fft/menu_input.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Flat view of both 40-packet buffers. The separate base binding preserves
 * conditions_special_thread's native address calculations. */
extern POLY_GT4 g_gfx_poly_gt4_flat_pool[80];

/* One color record per display-condition quad; the fourth byte of each
 * CVECTOR is not consumed by the banner renderer. */
typedef struct conditions_colors {
    u8 _unused_00[4];    /* 0x00 */
    CVECTOR vertices[4]; /* 0x04 */
} conditions_colors_t;
typedef char conditions_colors_size_must_be_20[sizeof(conditions_colors_t) == 20 ? 1 : -1];

typedef struct conditions_tpage {
    u8 _unused_00;
    u8 _unused_01;
    u8 page; /* Bits 5 and 6 encode the semi-transparency mode. */
    u8 _unused_03;
} conditions_tpage_t;

extern u8 g_conditions_general_quad_ranges[2];
extern sprite_sheet_frame_t g_conditions_general_quads[6];
extern conditions_tpage_t g_conditions_general_tpages[6];
extern conditions_colors_t g_conditions_general_vertex_colors[6];
extern sprite_sheet_frame_t g_conditions_special_quads[81];

/* Per-condition first-quad indices; entry n+1 bounds entry n. */
extern u8 g_conditions_special_quad_ranges[12];

/* 0x14-byte vertex-color records, one per quad. */
extern conditions_colors_t g_conditions_special_vertex_colors[81];
/* Scale each vertex color by scale / 128. */
static void gfx_scale_poly_gt4_colors(const conditions_colors_t* colors, s32 scale, POLY_GT4* poly) {
    s32 r;
    s32 g;
    s32 b;

    r = colors->vertices[0].r * scale / 128;
    g = colors->vertices[0].g * scale / 128;
    b = colors->vertices[0].b * scale / 128;
    setRGB0(poly, r, g, b);
    r = colors->vertices[1].r * scale / 128;
    g = colors->vertices[1].g * scale / 128;
    b = colors->vertices[1].b * scale / 128;
    setRGB1(poly, r, g, b);
    r = colors->vertices[2].r * scale / 128;
    g = colors->vertices[2].g * scale / 128;
    b = colors->vertices[2].b * scale / 128;
    setRGB2(poly, r, g, b);
    r = colors->vertices[3].r * scale / 128;
    g = colors->vertices[3].g * scale / 128;
    b = colors->vertices[3].b * scale / 128;
    setRGB3(poly, r, g, b);
}

/* Initialize the first 32 semi-transparent POLY_GT4s of buffer 0
 * with zeroed vertices, a one-texel texture cell, texture page 6 and the CLUT
 * at the bonus palette rectangle's origin offset by (x, y). */
static void gfx_init_poly_gt4_array_32(s32 x, s32 y) {
    s32 count = 0;
    s32 one = 1;
    s16* palette_origin;

    /* The y-centered pointer preserves the retail base address; RECT fields change it. */
    palette_origin = &g_conditions_banner_palette_rect.y;
    do {
        POLY_GT4* poly = &g_gfx_poly_gt4_flat_pool[count];
        SetPolyGT4(poly);
        SetShadeTex(poly, 0);
        SetSemiTrans(poly, 1);
        setUV4(poly, 0, 0, one, 0, 0, one, one, one);
        setXY4(poly, 0, 0, 0, 0, 0, 0, 0, 0);
        /* The graph type is read, and read again when it is not 1, but never used. */
        if (GetGraphType() != one) {
            GetGraphType();
        }
        count++;
        poly->tpage = 6;
        poly->clut = getClut(palette_origin[-1] + x, palette_origin[0] + y);
    } while (count < 32);
}

/* Initialize the final 8 semi-transparent POLY_GT4s of buffer 0
 * with zeroed vertices, a one-texel texture cell, texture page 6 and the CLUT
 * at the bonus palette rectangle's origin offset by (x, y). */
static void gfx_init_poly_gt4_array_8(s32 x, s32 y) {
    s32 count = 0;
    s32 one = 1;
    s16* palette_origin;

    /* The y-centered pointer preserves the retail base address; RECT fields change it. */
    palette_origin = &g_conditions_banner_palette_rect.y;
    do {
        POLY_GT4* poly = &g_gfx_poly_gt4_buffers[0][32 + count];
        SetPolyGT4(poly);
        SetShadeTex(poly, 0);
        SetSemiTrans(poly, 1);
        setUV4(poly, 0, 0, one, 0, 0, one, one, one);
        setXY4(poly, 0, 0, 0, 0, 0, 0, 0, 0);
        /* The graph type is read, and read again when it is not 1, but never used. */
        if (GetGraphType() != one) {
            GetGraphType();
        }
        count++;
        poly->tpage = 6;
        poly->clut = getClut(palette_origin[-1] + x, palette_origin[0] + y);
    } while (count < 8);
}

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
 * count read and its `addiu sN,t0,0` range rematerialization per phase;
 * `tpage` is a local copy so the page load keeps its +2 offset; `x + 0x100 +
 * width` / `y + (height + 0x78) - offset` / `y + 0x78 + offset` fix operand
 * order; declaring `frame` before `quad_count` fixes their stack slots.
 *
 * `buffer` is read before it is first written (the target's s7 is live out of
 * the prologue), so it is left uninitialized on purpose.
 */

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

/* Show a general condition banner (DisplayConditions 8 and up) from its BONUS.BIN image: wipe in
 * the lower three pieces and then the upper three over 20 frames, hold for hold_frames, then wipe
 * it out over 10. Circle ends it at once. */
void conditions_show_general(s32 hold_frames) {
    sprite_sheet_frame_t* quads;
    u8* range = g_conditions_general_quad_ranges;
    conditions_colors_t* colors = g_conditions_general_vertex_colors;
    conditions_tpage_t* tpage = g_conditions_general_tpages;
    s32 condition = 0;
    s32 frame;
    s32 quad_count;
    s32 index;
    s32 buffer;
    s32 reveal_offset_bottom;
    s32 reveal_offset_top;
    s32 hide_offset;
    u32* input;

    conditions_load_banner_image(thread_current_parameter_1_get() - EVENT_DISPLAY_CONDITION_GENERAL_FIRST);
    {
        u8* parameters = &range[condition];

        quad_count = parameters[1] - parameters[0];
    }
    gfx_init_poly_gt4_array_32(0, 1);
    quads = g_conditions_general_quads;

    for (index = 0; index < quad_count; index++) {
        POLY_GT4* poly = &g_gfx_poly_gt4_buffers[0][index];
        u8* parameters = &range[condition];
        s32 page;

        STORE_QUAD_FULL(poly, index);
        /* Keep only the semi-transparency bits (0x60) on texture page 6; GPU types 1
         * and 2 take those bits two places higher. */
        if (GetGraphType() == 1 || GetGraphType() == 2)
            page = (tpage[index].page & 0x60) * 4;
        else
            page = tpage[index].page & 0x60;
        poly->tpage = page | 6;
    }

    /* Seed buffer 1 with all 40 quads of buffer 0. */
    {
        POLY_GT4* destination = &g_gfx_poly_gt4_buffers[1][0];
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
        /* Circle skips the remaining phases and ends the thread. */
        if (*input & PSX_PAD_CIRCLE) {
            thread_yield();
            thread_current_exit();
            return;
        }

        /* `buffer` is never initialized; only its parity matters, so either buffer may come first. */
        buffer = (buffer + 1) & 1;
        /* The upper three pieces start their wipe nine frames after the lower three. */
        if (frame >= 9) {
            for (index = 5; index >= 3; index--) {
                POLY_GT4* poly = &g_gfx_poly_gt4_buffers[buffer][index];

                /* The clamp lowers the shared offset itself, so a shorter piece also caps the
                 * pieces drawn after it. */
                if (QUAD(index).source.h < reveal_offset_top)
                    reveal_offset_top = QUAD(index).source.h;
                STORE_QUAD_REVEAL(poly, index, reveal_offset_top);
                gfx_scale_poly_gt4_colors(VERTEX_COLORS(index), 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        for (index = 2; index >= 0; index--) {
            POLY_GT4* poly = &g_gfx_poly_gt4_buffers[buffer][index];

            if (QUAD(index).source.h < reveal_offset_bottom)
                reveal_offset_bottom = QUAD(index).source.h;
            STORE_QUAD_REVEAL(poly, index, reveal_offset_bottom);
            gfx_scale_poly_gt4_colors(VERTEX_COLORS(index), 0x80, poly);
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
            if (*input & PSX_PAD_CIRCLE) {
                thread_yield();
                thread_current_exit();
                return;
            }
            buffer = (buffer + 1) & 1;
            for (index = quad_count - 1; index >= 0; index--) {
                POLY_GT4* poly = &g_gfx_poly_gt4_buffers[buffer][index];
                u8* parameters = &range[condition];
                gfx_scale_poly_gt4_colors(VERTEX_COLORS(index), 0x80, poly);
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
            POLY_GT4* poly = &g_gfx_poly_gt4_buffers[buffer][index];

            if (QUAD(index).source.h < hide_offset)
                hide_offset = QUAD(index).source.h;
            STORE_QUAD_HIDE(poly, index, hide_offset);
            gfx_scale_poly_gt4_colors(VERTEX_COLORS(index), 0x80, poly);
            gfx_draw_or_append_gpu_primitive(poly);
        }
        hide_offset += 8;
        frame++;
    } while (frame < 0xa);
    thread_yield();
    thread_current_exit();
}

/* Empty and unreferenced. */
void conditions_noop_1(void) {
}

/* The target re-reads the first-quad index for every field. */
#undef QUAD
#undef VERTEX_COLORS
#define QUAD (quads + parameters[0])[quad_index]

#define VERTEX_COLORS (&color_table[parameters[0] + quad_index])

/* Fade a condition's sprites in over the two primitive buffers, hold them until
 * thread parameter 3 becomes 2, then fade them out.
 *
 * `table` and `color_table` are function-lifetime copies of the table
 * addresses; the target rematerializes them in each loop instead of folding
 * them into the loads. The buffer copy and the small-array address use
 * g_gfx_poly_gt4_buffers, the second symbol at the address of
 * g_gfx_poly_gt4_flat_pool: sharing that base lets the hoisted
 * small-array address derive from the copy source, which keeps it live in s0. */
void conditions_special_thread(void) {
    s32 banner_id;
    s32 use_small_array;
    s32 quad_count;
    s32 buffer;
    s32 scale;
    s32 quad_index;
    sprite_sheet_frame_t* quads;
    u8* table = g_conditions_special_quad_ranges;
    conditions_colors_t* color_table = g_conditions_special_vertex_colors;

    banner_id = thread_current_parameter_1_get();
    use_small_array = thread_current_parameter_2_get();
    {
        u8* parameters = &g_conditions_special_quad_ranges[banner_id];

        quads = g_conditions_special_quads;
        quad_count = parameters[1] - parameters[0];
    }
    if (use_small_array == 0) {
        gfx_init_poly_gt4_array_32(0x20, 0);
    } else {
        gfx_init_poly_gt4_array_8(0x20, 0);
    }

    for (quad_index = 0; quad_index < quad_count; quad_index++) {
        u8* parameters = &table[banner_id];
        POLY_GT4* poly
            = &(use_small_array == 0 ? g_gfx_poly_gt4_flat_pool : &g_gfx_poly_gt4_buffers[0][32])[quad_index];

        setUV4(poly, QUAD.source.x, QUAD.source.y, QUAD.source.x + QUAD.source.w, QUAD.source.y, QUAD.source.x,
            QUAD.source.y + QUAD.source.h, QUAD.source.x + QUAD.source.w, QUAD.source.y + QUAD.source.h);
        setXY4(poly, QUAD.offset.x + 0x100, QUAD.offset.y + 0x78, QUAD.offset.x + 0x100 + QUAD.source.w,
            QUAD.offset.y + 0x78, QUAD.offset.x + 0x100, QUAD.offset.y + 0x78 + QUAD.source.h,
            QUAD.offset.x + 0x100 + QUAD.source.w, QUAD.offset.y + 0x78 + QUAD.source.h);
    }
    mem_copy_bytes_nonempty(g_gfx_poly_gt4_buffers[1], g_gfx_poly_gt4_buffers, 40 * sizeof(POLY_GT4));

    buffer = 0;
    scale = 0x30;
    /* Parameter 3 selects the next phase at any frame: 2 jumps from the
       fade-in past the hold loop to the fade-out, 1 ends the thread. */
    while (scale < 0x80) {
        if (thread_current_parameter_3_get() == 2) {
            goto fade_out;
        }
        if (thread_current_parameter_3_get() == 1) {
            thread_yield();
            thread_current_exit();
            return;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = &g_gfx_poly_gt4_buffers[buffer][32];
            u8* parameters = &table[banner_id];
            const conditions_colors_t* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_gfx_poly_gt4_flat_pool + buffer * 40)[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        thread_yield();
        scale += 8;
    }
    for (;;) {
        if (thread_current_parameter_3_get() == 2) {
            break;
        }
        if (thread_current_parameter_3_get() == 1) {
            thread_yield();
            thread_current_exit();
            return;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = &g_gfx_poly_gt4_buffers[buffer][32];
            u8* parameters = &table[banner_id];
            const conditions_colors_t* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_gfx_poly_gt4_flat_pool + buffer * 40)[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, 0x80, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        thread_yield();
    }
fade_out:
    scale = 0x80;
    do {
        if (thread_current_parameter_3_get() == 1) {
            break;
        }
        buffer ^= 1;
        for (quad_index = 0; quad_index < quad_count; quad_index++) {
            POLY_GT4* polys = &g_gfx_poly_gt4_buffers[buffer][32];
            u8* parameters = &table[banner_id];
            const conditions_colors_t* vertex_colors = VERTEX_COLORS;

            if (use_small_array == 0) {
                POLY_GT4* poly = &(g_gfx_poly_gt4_flat_pool + buffer * 40)[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            } else {
                POLY_GT4* poly = &polys[quad_index];

                gfx_scale_poly_gt4_colors(vertex_colors, scale, poly);
                gfx_draw_or_append_gpu_primitive(poly);
            }
        }
        thread_yield();
        scale -= 0xc;
    } while (scale >= 0x30);
    thread_yield();
    thread_current_exit();
}
