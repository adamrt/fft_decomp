#ifndef WORLD_GFX_H
#define WORLD_GFX_H

/* WORLD's graphics: packet buffers, fades and the portrait faces. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Separate decreasing/increasing RGB-intensity flags.
 * The getter packs them into bits 0/1 and returns 0 when disabled. */
typedef enum world_fade_state {
    WORLD_FADE_STATE_NONE = 0,
    WORLD_FADE_STATE_DECREASING = 1,
    WORLD_FADE_STATE_INCREASING = 2
} world_fade_state_e;

/* gfx */
extern u8 g_world_gfx_fade_decreasing;
extern u8 g_world_gfx_fade_disabled;
extern u8 g_world_gfx_fade_increasing;
extern s16 g_world_gfx_fade_intensity;
extern glyph_blit_t g_world_gfx_image_blit_source;
extern RECT g_world_gfx_temporary_draw_area;
extern u16 g_world_gfx_draw_area_scale_percent[];
/* A full-screen texture window (x=0, y=0, w=0x100, h=0x100 in the WORLD data
 * at 0x8016e44c), copied verbatim into a menu record's DR_MODE by
 * world_screen_gfx_init_scaled_draw_area_packets; the formation view reads the same
 * bytes as a zero screen point. Same 8-byte RECT shape as
 * g_gfx_texture_window, whose reset sibling is byte-for-byte identical. */
extern const RECT g_world_gfx_full_texture_window;
/* Frame-buffer rectangles for the four WLDFACE pages; four separate objects,
 * since as RECT[4] world_gfx_load_wldface_to_frame_buffer shares one base. */
extern RECT g_world_wldface_vram_rects;
extern RECT g_world_wldface_vram_rect_1;
extern RECT g_world_wldface_vram_rect_2;
extern RECT g_world_wldface_vram_rect_3;
extern rgb24_t g_world_gfx_black_tile_color;
extern RECT g_world_gfx_fade_tile_rect;
/* Fade tile RGB channels, set to equal levels by the fade updaters. */
extern rgb24_t g_world_fade_tile_color;
extern u16 g_world_gfx_flat_line_capacity;     /* +0x24 line count */
extern u16 g_world_gfx_gradient_line_capacity; /* gradient line count */
extern u16 g_world_gfx_line_g3_capacity;
extern u16 g_world_gfx_textured_quad_capacity; /* textured quad count */
extern u16 g_world_gfx_gradient_line_count;
extern u16 g_world_gfx_textured_gradient_quad_capacity; /* textured gradient quad count */
extern u16 g_world_gfx_draw_buffer_clip_y;
extern u16 g_world_ot_length;
/* Volatile: world_gfx_init_double_packet_buffers must read the draw-area count
 * after the draw-move count, as the target does. */
extern volatile u16 g_world_gfx_draw_area_capacity;
extern u16 g_world_gfx_draw_area_count;
/* Draw move count; volatile for the same read order as g_world_gfx_draw_area_capacity. */
extern volatile u16 g_world_gfx_draw_move_capacity;
extern menu_gfx_context_t* g_world_gfx_packet_buffers;
extern u16 g_world_gfx_gradient_quad_capacity; /* gradient quad count */
extern u16 g_world_gfx_poly_f4_count;
void world_gfx_add_draw_area_primitive(RECT* rect, s32 otag_index);
void world_gfx_append_poly_f4_to_otag(point16_t* xy, rgb24_t* rgb, s32 semi_trans, s32 ot_index);
void world_gfx_init_double_packet_buffers(menu_gfx_context_t* buffers, u32* otags, void* unused_2, void* unused_3,
    void* unused_4, POLY_FT4* textured_quads, void* unused_6, void* unused_7, POLY_G4* gradient_quads,
    POLY_GT4* textured_gradient_quads, LINE_F2* flat_lines, void* unused_11, void* unused_12, LINE_G2* gradient_lines,
    void* unused_14, void* unused_15, TILE* tiles, void* unused_17, void* unused_18, void* unused_19, void* unused_20,
    void* unused_21, void* unused_22, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes);
void world_gfx_set_blit_dest_position(s32 x, s32 y);
void world_gfx_set_fade_disabled(u8 disabled);
s32 world_gfx_get_fade_state(void);
void world_gfx_present_frame_and_swap_packet_buffer(s32 first_otag, s32 draw_otag);
void world_gfx_set_image_blit_source_rect(s16 x, s16 y, s16 width, s16 height);
void world_gfx_start_decreasing_fade(void);
void world_gfx_start_increasing_fade(void);
s32 world_gfx_update_fade_in_tile(void);
s32 world_gfx_update_fade_out_tile(void);

#endif
