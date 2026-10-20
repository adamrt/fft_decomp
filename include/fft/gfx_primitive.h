#ifndef GFX_PRIMITIVE_H
#define GFX_PRIMITIVE_H

/* Primitive submission: the event library's ordering-table hooks, the menu library's ordering table and primitive
 * pools, draw areas, the enqueue helpers, polygon builders and screen fades. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* submission */
/* Scene-local primitive submission state. */
extern u16 g_gfx_draw_primitives_immediately;
extern u32* g_gfx_current_otag_entry;

void gfx_draw_or_append_gpu_primitive(void* primitive);
void gfx_append_gpu_primitive_to_secondary_otag(void* primitive);
void gfx_set_draw_mode_for_texture_page(DR_MODE* mode, s32 page);
void gfx_set_draw_mode_from_origin(DR_MODE* mode, const point16_t* origin);

/* menu ordering table */
extern u32 g_gfx_otag_buffer[];
extern u16 g_gfx_otag_length;
extern u16 g_gfx_otag_count;
extern s8 g_gfx_otag_index_locked;

void gfx_set_otag_index(s32 value);

/* menu primitive pools */
/* The capacity of each primitive pool of the menu frame buffer
 * (menu_gfx_context_t), the entries used so far this frame, and the pools some
 * screens keep outside it. Each menu program defines the ones it uses. */
extern u16 g_gfx_poly_f3_capacity;
extern u16 g_gfx_poly_f4_capacity;
extern u16 g_gfx_poly_ft3_capacity;
extern u16 g_gfx_poly_ft4_capacity;
extern u16 g_gfx_poly_g3_capacity;
extern u16 g_gfx_poly_g4_capacity;
extern u16 g_gfx_poly_gt3_capacity;
extern u16 g_gfx_poly_gt4_capacity;
extern u16 g_gfx_line_f2_capacity;
extern u16 g_gfx_line_f3_capacity;
extern u16 g_gfx_line_f4_capacity;
extern u16 g_gfx_line_g4_capacity;
extern u16 g_gfx_tile_capacity;
extern u16 g_gfx_tile_1_capacity;
extern u16 g_gfx_tile_8_capacity;
extern u16 g_gfx_tile_16_capacity;
extern u16 g_gfx_sprite_capacity;
extern u16 g_gfx_sprite_8_capacity;
extern u16 g_gfx_sprite_16_capacity;
extern u16 g_gfx_draw_mode_capacity;
extern s16 g_gfx_poly_f3_count;
extern s16 g_gfx_poly_ft3_count;
extern s16 g_gfx_poly_g3_count;
extern u16 g_gfx_poly_g4_count;
extern s16 g_gfx_poly_gt3_count;
extern s16 g_gfx_poly_gt4_count;
extern u16 g_gfx_line_f2_count;
extern s16 g_gfx_line_f3_count;
extern s16 g_gfx_line_f4_count;
extern s16 g_gfx_line_g2_count;
extern s16 g_gfx_line_g3_count;
extern s16 g_gfx_line_g4_count;
extern u16 g_gfx_tile_count;
extern s16 g_gfx_tile_1_count;
extern s16 g_gfx_tile_8_count;
extern s16 g_gfx_tile_16_count;
extern s16 g_gfx_sprite_count;
extern s16 g_gfx_sprite_8_count;
extern s16 g_gfx_sprite_16_count;
extern POLY_FT4 g_gfx_poly_ft4_buffer[];
extern DR_AREA g_gfx_draw_area_buffer[];
extern DR_MOVE g_gfx_draw_move_buffer[];

/* draw areas */
extern RECT g_gfx_draw_area;
extern RECT g_gfx_tiled_rect;
extern u16 g_gfx_draw_offset_y;

/* Nonzero upper_half selects the buffer at y 0; zero selects y 240. */
void gfx_build_scaled_draw_area_pair_swapped(
    scaled_draw_area_pair_t* packets, const RECT* geometry, s32 step, s32 upper_half);

/* enqueue helpers */
extern oriented_quad_t g_gfx_blank_cell_rect;

void gfx_set_draw_color(const rgb24_t* color);
void gfx_enqueue_tile(const RECT* rect, const rgb24_t* color, u8 semi, s32 idx);
void gfx_enqueue_line_f2(const point16_t endpoints[2], const rgb24_t* color, u8 semitrans, s32 otag_index);
void gfx_enqueue_gouraud_quad(const upoint16_t* pts, const rgb24_t* color, s32 semi, s32 idx);
void gfx_enqueue_textured_quad_corners(const textured_quad_source_t* src, const rgb24_t* color, s32 semi, s32 idx);
void gfx_enqueue_gouraud_textured_quad(
    const textured_rect_source_t* rect, const rgb24_t* color, s32 semitrans, s32 otag_index);

/* polygon builders */
void gfx_set_scaled_poly_ft4_4bpp(POLY_FT4* poly, const point16_t* texture_page, const RECT* position,
    const sprite_sheet_frame_t* sprite_rect, const point16_t* scale, const sprite_draw_offset_t* base);
void gfx_set_scaled_poly_ft4_8bpp(POLY_FT4* poly, const point16_t* texture, const RECT* position,
    const sprite_sheet_frame_t* uv_rect, const point16_t* scale, const sprite_draw_offset_t* offset);
void gfx_quad_negative_uv_flip(POLY_FT4* primitive, s32 direction);
void gfx_set_formation_icon_uv(POLY_FT4* poly, s32 index);
void gfx_build_gradient_grid_primitives(POLY_GT4* poly);

/* screen fades */
/* Gray level the screen-covering fade tile subtracts: 0 is clear, 255 is black. */
extern s32 g_gfx_screen_fade;

void gfx_submit_screen_fade_overlay(fade_overlay_t* overlay, s32 level);
void gfx_screen_overlay_fade_out(void);
void gfx_screen_overlay_fade_in(void);

/* The menu library's fades, which BUNIT and CARD run. */
void gfx_start_increasing_fade(void);
s32 gfx_update_increasing_fade(void);
void gfx_start_decreasing_fade(void);
s32 gfx_update_decreasing_fade(void);
s32 gfx_get_fade_state(void);

#endif
