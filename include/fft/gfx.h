#ifndef GFX_H
#define GFX_H

/* Graphics records shared across programs: colors, points and rectangles, texture coordinates, sprites, textured
 * primitive sources and the primitive buffers. The routines are in gfx_image.h and gfx_primitive.h. */

#include "psx/libgpu.h"
#include "psx/types.h"

/* colors */
/* Three byte-sized RGB channels, used for colors and per-channel values.
 * CVECTOR adds a fourth byte; these records have a three-byte stride. */
typedef struct rgb24 {
    u8 r;
    u8 g;
    u8 b;
} rgb24_t;
typedef char assert_rgb24_size[sizeof(rgb24_t) == 3 ? 1 : -1];

/* Signed RGB channels for color tracks and per-channel offsets. */
typedef struct rgb_s8 {
    s8 r;
    s8 g;
    s8 b;
} rgb_s8_t;
typedef char rgb_s8_size_must_be_3[sizeof(rgb_s8_t) == 3 ? 1 : -1];

/* Packed PS1 color: the high bit enables semi-transparency for textured pixels. */
typedef struct rgba5551 {
    u16 red : 5;
    u16 green : 5;
    u16 blue : 5;
    u16 semi_transparent : 1;
} rgba5551_t;
typedef char assert_rgba5551_size[sizeof(rgba5551_t) == 2 ? 1 : -1];

enum {
    PALETTE_COLOR_COUNT = 16,
    CLUT_PALETTE_COUNT = 16,
};

/* Sixteen packed texture colors. */
typedef rgba5551_t palette_t[PALETTE_COLOR_COUNT];
typedef char assert_palette_size[sizeof(palette_t) == 32 ? 1 : -1];

/* Sixteen palette banks. */
typedef palette_t clut_t[CLUT_PALETTE_COUNT];
typedef char assert_clut_size[sizeof(clut_t) == 512 ? 1 : -1];

/* ranges */
/* Signed 16-bit lower and upper bounds. */
typedef struct ranges16 {
    s16 min;
    s16 max;
} ranges16_t;
typedef char ranges16_size_must_be_4[sizeof(ranges16_t) == 4 ? 1 : -1];

/* Signed 32-bit lower and upper bounds. */
typedef struct ranges32 {
    s32 min;
    s32 max;
} ranges32_t;
typedef char ranges32_size_must_be_8[sizeof(ranges32_t) == 8 ? 1 : -1];

/* geometry */
/* Use DVECTOR for GTE screen coordinates, SVECTOR/VECTOR for padded GTE
 * vectors, RECT for signed rectangles and CVECTOR for r/g/b plus a code byte.
 * The point types below cover other field widths and signedness. Battle world
 * positions use VECTOR for 20.12 values and SVECTOR for integer values
 * (vx = x, vy = height, vz = map depth). Rotations also use SVECTOR. */

/* Signed 16-bit x/y pair: screen offsets, sprite positions and per-axis
 * factors that are not GTE output. */
typedef struct point16 {
    s16 x;
    s16 y;
} point16_t;
typedef char assert_point16_size[sizeof(point16_t) == 4 ? 1 : -1];

/* Two signed 32-bit coordinates, without VECTOR's 32-bit z and padding fields. */
typedef struct point32 {
    s32 x;
    s32 y;
} point32_t;
typedef char assert_point32_size[sizeof(point32_t) == 8 ? 1 : -1];

/* Signed 32-bit width/height pair. */
typedef struct size32 {
    s32 width;
    s32 height;
} size32_t;
typedef char size32_size_must_be_8[sizeof(size32_t) == 8 ? 1 : -1];

/* Unsigned 16-bit x/y pair, e.g. VRAM or texture-page positions. */
typedef struct upoint16 {
    u16 x;
    u16 y;
} upoint16_t;
typedef char assert_upoint16_size[sizeof(upoint16_t) == 4 ? 1 : -1];

/* Unpadded signed 16-bit x/y/z triple (6 bytes; SVECTOR is 8). */
typedef struct vec3s16 {
    s16 x;
    s16 y;
    s16 z;
} vec3s16_t;
typedef char assert_vec3s16_size[sizeof(vec3s16_t) == 6 ? 1 : -1];

/* Three signed 32-bit components without VECTOR's trailing 32-bit padding field. */
typedef struct vec3s32 {
    s32 x;
    s32 y;
    s32 z;
} vec3s32_t;
typedef char assert_vec3s32_size[sizeof(vec3s32_t) == 12 ? 1 : -1];

/* RECT layout with unsigned fields, for rectangles the target reads with lhu. */
typedef struct urect16 {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} urect16_t;
typedef char assert_urect16_size[sizeof(urect16_t) == 8 ? 1 : -1];

/* texture coordinates */
/* Eight-bit texture coordinates within a PS1 texture page. */
typedef struct uv8 {
    u8 u;
    u8 v;
} uv8_t;
typedef char assert_uv8_size[sizeof(uv8_t) == 2 ? 1 : -1];

typedef enum texture_uv_flip {
    TEXTURE_UV_FLIP_NONE = 0,
    TEXTURE_UV_FLIP_X = 1,
    TEXTURE_UV_FLIP_Y = 2,
    TEXTURE_UV_FLIP_XY = TEXTURE_UV_FLIP_X | TEXTURE_UV_FLIP_Y,
} texture_uv_flip_e;

/* sprites */
/* Sprite-sheet frame: sampled rectangle and scaled screen offset. */
typedef struct sprite_sheet_frame {
    RECT source;
    point16_t offset;
} sprite_sheet_frame_t;
typedef char assert_sprite_sheet_frame_size[sizeof(sprite_sheet_frame_t) == 12 ? 1 : -1];

/* Screen translation used by the sprite geometry helpers. */
typedef struct sprite_draw_offset {
    u8 _unused_00[8];
    upoint16_t position;
} sprite_draw_offset_t;
typedef char sprite_draw_offset_size_must_be_12[sizeof(sprite_draw_offset_t) == 12 ? 1 : -1];

/* Draw state shared by the resident, formation and deployment ZODIAC renderers. */
typedef struct zodiac_draw_context {
    u32* ot;             /* 0x00: ordering table entry the primitives link into */
    s32 brightness;      /* 0x04: low byte is r0/g0/b0 of every sprite; WORLD stores g_screen_color_red */
    s32 link_primitives; /* 0x08: nonzero: link each sprite into the OT */
    s16 scale_x;         /* 0x0c: signed scale with 12 fractional bits; ONE = 1.0 */
    s16 scale_y;         /* 0x0e: signed scale with 12 fractional bits; ONE = 1.0 */
    u8 _unused_10[8];
    sprite_draw_offset_t offset; /* 0x18 */
} zodiac_draw_context_t;

/* glyph images */
/* The 0x10-byte glyph blit descriptor used by the window frame threads and filled
 * by screen_set_blit_source_rect, text_set_blit_source_rect and
 * text_set_blit_destination. The stride is the surface width in pixels. */
typedef struct glyph_blit {
    RECT rect;  /* 0x00 */
    s32 stride; /* 0x08 */
    u32 color;  /* 0x0c: 32-bit color value the decimal renderers set by value sign
                 * (0x77777777, 0xbbbbbbbb, ...) and clear after each draw */
} glyph_blit_t;
typedef char glyph_blit_size_must_be_16[sizeof(glyph_blit_t) == 16 ? 1 : -1];

/* textured primitive sources */
/* Texture primitive used by the WORLD/BATTLE image-loading and
 * texture-page allocation helpers: a SPRT
 * (code 0x64-0x67) or a POLY_FT4. Both carry the CLUT at 0x0e, which the icon
 * slot helpers overwrite. */
typedef union texture_primitive {
    SPRT sprt;
    POLY_FT4 ft4;
} texture_primitive_t;

/* Textured menu quad source for gfx_enqueue_textured_quad_corners:
 * four screen points, four texture points (only the low byte of each
 * 16-bit value is used), then the CLUT and texture page. */
typedef struct textured_quad_source {
    s16 x0, y0; /* 0x00 */
    s16 x1, y1; /* 0x04 */
    s16 x2, y2; /* 0x08 */
    s16 x3, y3; /* 0x0c */
    u16 u0;     /* 0x10 */
    u16 v0;     /* 0x12 */
    u16 u1;     /* 0x14 */
    u16 v1;     /* 0x16 */
    u16 u2;     /* 0x18 */
    u16 v2;     /* 0x1a */
    u16 u3;     /* 0x1c */
    u16 v3;     /* 0x1e */
    u16 clut;   /* 0x20 */
    u16 tpage;  /* 0x22 */
} textured_quad_source_t;

/* Twelve-byte texture cell copied by the formation portrait producers and
 * embedded in oriented menu quads. */
typedef struct texture_source {
    s16 u;     /* 0x00 */
    u16 v;     /* 0x02 */
    s16 w;     /* 0x04 */
    s16 h;     /* 0x06 */
    u16 clut;  /* 0x08 */
    s16 tpage; /* 0x0a */
} texture_source_t;
typedef char texture_source_size_must_be_12[sizeof(texture_source_t) == 12 ? 1 : -1];

/* Screen rectangle and texture source, passed to the oriented-quad readers.
 * Equipment-slot marker arrays have a 0x14-byte stride. */
typedef struct oriented_quad {
    point16_t position;      /* 0x00 */
    u16 w;                   /* 0x04 */
    u16 h;                   /* 0x06 */
    texture_source_t source; /* 0x08 */
} oriented_quad_t;
typedef char oriented_quad_size_must_be_20[sizeof(oriented_quad_t) == 20 ? 1 : -1];

/* Gouraud-textured rectangle source for gfx_enqueue_gouraud_textured_quad:
 * screen rectangle, texture origin (only the low byte of u/v is used; the
 * texture extent is the screen extent), CLUT and texture page. The caller also
 * passes four RGB triples, one per corner. */
typedef struct textured_rect_source {
    s16 x, y;  /* 0x00 */
    s16 w, h;  /* 0x04 */
    s16 u;     /* 0x08 */
    u16 v;     /* 0x0a */
    u16 clut;  /* 0x0c */
    u16 tpage; /* 0x0e */
} textured_rect_source_t;
typedef char textured_rect_source_size_must_be_16[sizeof(textured_rect_source_t) == 16 ? 1 : -1];

/* Texture and screen placement for gfx_init_image_loading (12 bytes).
 * g_menu_sprite_page_image_params holds one per menu sprite. */
typedef struct image_load_parameters {
    s16 x_load;
    s16 y_load;
    u16 width;
    u16 height;
    u16 x_screen_offset;
    u16 y_screen_offset;
} image_load_parameters_t;

/* primitive buffers */
/* Two DR_AREA packets and their corresponding rectangles.
 * Builders choose scaling percentages and rectangle placement. */
typedef struct scaled_draw_area_pair {
    DR_AREA areas[2]; /* 0x00 */
    RECT rects[2];    /* 0x18 */
} scaled_draw_area_pair_t;
typedef char scaled_draw_area_pair_size_must_be_40[sizeof(scaled_draw_area_pair_t) == 40 ? 1 : -1];

/* Menu frame buffer: primitive pools followed by drawing/display state.
 * BUNIT uses the environments; EQUIP owns the same 236-byte buffers without
 * reading or writing the environment suffix. */
typedef struct menu_gfx_context {
    u32* otag;                            /* 0x00 */
    POLY_F3* flat_triangles;              /* 0x04 */
    POLY_FT3* textured_triangles;         /* 0x08 */
    POLY_F4* flat_quads;                  /* 0x0c */
    POLY_FT4* textured_quads;             /* 0x10 */
    POLY_G3* gouraud_triangles;           /* 0x14 */
    POLY_GT3* gouraud_textured_triangles; /* 0x18 */
    POLY_G4* gouraud_quads;               /* 0x1c */
    POLY_GT4* gouraud_textured_quads;     /* 0x20 */
    LINE_F2* flat_lines;                  /* 0x24 */
    LINE_F3* flat_lines_3;                /* 0x28 */
    LINE_F4* flat_lines_4;                /* 0x2c */
    LINE_G2* gouraud_lines;               /* 0x30 */
    LINE_G3* gouraud_lines_3;             /* 0x34 */
    LINE_G4* gouraud_lines_4;             /* 0x38 */
    TILE* tiles;                          /* 0x3c */
    TILE_1* tiles_1;                      /* 0x40 */
    TILE_8* tiles_8;                      /* 0x44 */
    TILE_16* tiles_16;                    /* 0x48 */
    SPRT* sprites;                        /* 0x4c */
    SPRT_8* sprites_8;                    /* 0x50 */
    SPRT_16* sprites_16;                  /* 0x54 */
    DR_MOVE* draw_moves;                  /* 0x58 */
    DR_AREA* draw_areas;                  /* 0x5c */
    DR_MODE* draw_modes;                  /* 0x60 */
    DRAWENV draw_env;                     /* 0x64 */
    DISPENV disp_env;                     /* 0xc0 */
    u8 _unused_d4[0x18];                  /* 0xd4 */
} menu_gfx_context_t;
typedef char menu_gfx_context_size_must_be_236[(sizeof(menu_gfx_context_t) == 236) ? 1 : -1];

/* Full-screen fade primitive pair: a TILE followed by the DR_MODE that
 * selects its texture page and window. */
typedef struct fade_overlay {
    TILE tile;    /* 0x00 */
    DR_MODE mode; /* 0x10 */
} fade_overlay_t;
typedef char fade_overlay_size_must_be_28[(sizeof(fade_overlay_t) == 28) ? 1 : -1];

/* event character graphics */
/* One of the 137 fixed-size blocks in EVTCHR.BIN. No native pointer tables
 * or widened 32-bit unit-animation headers are present in the file. */
typedef struct evtchr_file_record {
    u8 sequence_offsets[0x40][4]; /* 0x0000; signed 32-bit offsets, -1 resolves to zero */
    u8 sequence_data[0x400];      /* 0x0100 */
    u8 frame_offsets[0x28][4];    /* 0x0500; signed 32-bit offsets, -1 resolves to zero */
    u8 frame_data[0x1e0];         /* 0x05a0; copied into both native frame sets */
    u8 cluts[16 * 16 * 2];        /* 0x0780; 16 palettes of 16 little-endian colors */
    u8 pixels[200 * 128];         /* 0x0980; 256x200, 4bpp */
    u8 _unused_6d80[0xa80];       /* unread tail of the 0x7800-byte block */
} evtchr_file_record_t;
typedef char evtchr_file_record_size_must_be_30720[sizeof(evtchr_file_record_t) == 30720 ? 1 : -1];

#endif
