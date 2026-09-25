#ifndef FFT_GFX_H
#define FFT_GFX_H

/* Graphics records shared across modules: points, rectangles, textures and sprites. */

#include "psx/gpu.h"
#include "psx/types.h"

/* geometry */
/* Neutral 2D/3D point, rectangle and colour records for shapes the Psy-Q SDK
 * does not define.
 *
 * Use the SDK type whenever its layout and signedness fit exactly: DVECTOR
 * for GTE screen coordinates (SXY), SVECTOR/VECTOR for padded GTE vectors,
 * RECT for signed x/y/w/h rectangles and CVECTOR for r/g/b plus a code or
 * padding byte. These types cover the remaining cases. Field signedness is
 * part of the choice, not a detail: s16 fields load with lh and u16 fields
 * with lhu, so pick the variant whose loads match the target.
 *
 * Battle real positions are VECTORs (vx = x, vy = height, vz = map depth) and
 * screen positions are battle_screen_coords_t in X, Z, Y order; keep those in
 * their named types rather than vec3s16_t/vec3u16_t. */
/* Signed 16-bit x/y pair: screen offsets, sprite positions and per-axis
 * factors that are not GTE output. */
typedef struct point16 {
    s16 x;
    s16 y;
} point16_t;
typedef char assert_point16_size[sizeof(point16_t) == 4 ? 1 : -1];

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

/* RECT layout with unsigned fields, for rectangles the target reads with lhu. */
typedef struct urect16 {
    u16 x;
    u16 y;
    u16 w;
    u16 h;
} urect16_t;
typedef char assert_urect16_size[sizeof(urect16_t) == 8 ? 1 : -1];

/* gfx */
typedef enum texture_uv_flip {
    TEXTURE_UV_FLIP_NONE = 0,
    TEXTURE_UV_FLIP_X = 1,
    TEXTURE_UV_FLIP_Y = 2,
    TEXTURE_UV_FLIP_XY = TEXTURE_UV_FLIP_X | TEXTURE_UV_FLIP_Y,
} texture_uv_flip_e;

/* Image load parameters consumed by world_gfx_init_image_loading (0xc bytes;
 * the table at 0x80155838 holds one per menu sprite). */
typedef struct world_gfx_image_load_parameters {
    s16 x_load;
    s16 y_load;
    u16 width;
    u16 height;
    u16 x_screen_offset;
    u16 y_screen_offset;
} world_gfx_image_load_parameters_t;

/* 0x800ef2fc initializes this 0x13c-byte packet group and copies it to the
 * second frame buffer. Each side contains nine tiles, ordered left to right. */
typedef struct world_fade_tile_frame {
    DR_MODE draw_mode;
    TILE center;
    TILE sides[18];
} world_fade_tile_frame_t;
typedef char world_fade_tile_frame_size_must_be_0x13c[sizeof(world_fade_tile_frame_t) == 0x13c ? 1 : -1];

/* The fade routine reads the low byte for RGB and the full word for stepping. */
typedef union world_fade_intensity {
    s32 value;
    u8 bytes[4];
} world_fade_intensity_t;

/* Draw-area pair built by world_menu_build_zoom_draw_area_pair: two DR_AREA packets whose
 * rectangles are scaled around their centre by a percentage from
 * g_world_menu_zoom_percentages (indexed by g_world_event_speed-adjusted step). */
typedef struct world_gfx_scaled_draw_area_pair {
    DR_AREA areas[2]; /* 0x00 */
    RECT rects[2];    /* 0x18 */
} world_gfx_scaled_draw_area_pair_t;
typedef char world_scaled_draw_area_pair_size_must_be_0x28[sizeof(world_gfx_scaled_draw_area_pair_t) == 0x28 ? 1 : -1];

/* Provisional: rectangle inside a 4bpp image, used by the horizontal mirror
 * routine at 0x800ff478. Coordinates and stride are in pixels. */
typedef struct world_gfx_4bpp_rect {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
    s32 stride;
} world_gfx_4bpp_rect_t;

/* View written by world_gfx_reset_record_texture_window{,_2,_3} and
 * world_menu_init_record: the cursor mode/sprite/window tail of
 * world_panel_frame_t. Callers also pass other records with the same offsets
 * (e.g. world_gfx_scaled_draw_area_pair_t, whose rects[0].w and rects[1] land
 * on sprite.w and texture_window). */
typedef struct world_gfx_texture_window_record {
    DR_MODE mode;        /* 0x00 */
    SPRT sprite;         /* 0x0c: sprite.w (0x1c) is cleared */
    RECT texture_window; /* 0x20: h cleared, then the 8-byte template copied in */
} world_gfx_texture_window_record_t;

/* other */
enum {
    BATTLE_CAMERA_PITCH_STEEP_MASK_THRESHOLD = 0x178,
    BATTLE_CAMERA_FUSION_HOLD_SENTINEL = 0x2710,
};

/* Base texture/screen location paired with the parameters above by
 * world_gfx_init_image_loading: the x is sign-tested (an s16) while the y is
 * only ever added as an unsigned VRAM row. Hoisted here from the three
 * identical local copies in the WORLD and BATTLE unit-editor panels. */
typedef struct world_image_location {
    s16 x; /* 0x00 */
    u16 y; /* 0x02 */
} world_image_location_t;

#endif
