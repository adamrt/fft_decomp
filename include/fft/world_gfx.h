#ifndef WORLD_GFX_H
#define WORLD_GFX_H

/* WORLD's graphics: the double packet buffers, draw areas and blits, the screen fades and the portrait faces. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* packet buffers */
extern menu_gfx_context_t* g_screen_packet_buffers;
extern u16 g_screen_ot_length;
extern u16 g_screen_draw_buffer_clip_y;
extern u16 g_screen_textured_quad_capacity;          /* textured quad count */
extern u16 g_screen_gradient_quad_capacity;          /* gradient quad count */
extern u16 g_screen_textured_gradient_quad_capacity; /* textured gradient quad count */
extern u16 g_screen_flat_line_capacity;              /* +0x24 line count */
extern u16 g_screen_gradient_line_capacity;          /* gradient line count */
extern u16 g_screen_gradient_line_count;
extern u16 g_screen_line_g3_capacity;
extern u16 g_screen_poly_f4_count;
/* Volatile: screen_init_packet_buffers must read the draw-area count
 * after the draw-move count, as the target does. */
extern volatile u16 g_screen_draw_area_capacity;
extern u16 g_screen_draw_area_count;
/* Draw move count; volatile for the same read order as g_screen_draw_area_capacity. */
extern volatile u16 g_screen_draw_move_capacity;
void screen_init_packet_buffers(menu_gfx_context_t* buffers, u32* otags, void* unused_2, void* unused_3, void* unused_4,
    POLY_FT4* textured_quads, void* unused_6, void* unused_7, POLY_G4* gradient_quads,
    POLY_GT4* textured_gradient_quads, LINE_F2* flat_lines, void* unused_11, void* unused_12, LINE_G2* gradient_lines,
    void* unused_14, void* unused_15, TILE* tiles, void* unused_17, void* unused_18, void* unused_19, void* unused_20,
    void* unused_21, void* unused_22, DR_MOVE* draw_moves, DR_AREA* draw_areas, DR_MODE* draw_modes);
void screen_present_frame(s32 first_otag, s32 draw_otag);
void screen_append_poly_f4(point16_t* xy, rgb24_t* rgb, s32 semi_trans, s32 ot_index);
void screen_add_draw_area(RECT* rect, s32 otag_index);

/* draw areas and blits */
extern RECT g_screen_temporary_draw_area;
extern u16 g_screen_draw_area_scale_percent[];
/* A full-screen texture window (x=0, y=0, w=0x100, h=0x100), copied verbatim into a menu record's DR_MODE by
 * screen_init_scaled_draw_area_packets; the formation view reads the same bytes as a zero screen point.
 * Same 8-byte RECT shape as g_gfx_texture_window, whose reset sibling is byte-for-byte identical. */
extern const RECT g_screen_full_texture_window;
extern glyph_blit_t g_screen_image_blit_source;
void screen_set_blit_dest_position(s32 x, s32 y);
void screen_set_blit_source_rect(s16 x, s16 y, s16 width, s16 height);

/* fades */
/* Separate decreasing/increasing RGB-intensity flags.
 * The getter packs them into bits 0/1 and returns 0 when disabled. */
typedef enum screen_fade_state {
    SCREEN_FADE_STATE_NONE = 0,
    SCREEN_FADE_STATE_DECREASING = 1,
    SCREEN_FADE_STATE_INCREASING = 2
} screen_fade_state_e;

extern s16 g_screen_fade_intensity;
/* Fade tile RGB channels, set to equal levels by the fade updaters. */
extern rgb24_t g_screen_fade_tile_color;
extern rgb24_t g_screen_black_tile_color;
void screen_fade_set_disabled(u8 disabled);
s32 screen_fade_get_state(void);
void screen_fade_start_decreasing(void);
void screen_fade_start_increasing(void);
s32 screen_fade_update_in(void);
s32 screen_fade_update_out(void);

/* portrait faces */
/* Frame-buffer rectangles for the four WLDFACE pages; four separate objects,
 * since as RECT[4] screen_load_wldface_portraits shares one base. */
extern RECT g_screen_wldface_vram_rects;
extern RECT g_screen_wldface_vram_rect_1;
extern RECT g_screen_wldface_vram_rect_2;
extern RECT g_screen_wldface_vram_rect_3;

#endif
