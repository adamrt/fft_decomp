#ifndef MAIN_GFX_H
#define MAIN_GFX_H

/* Resident graphics: display, ordering tables, boot logos and the Now Loading message. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* boot */
extern s32 g_main_boot_startup2_has_run;
extern u32 g_main_boot_squaresoft_logo_image_words[];
void main_boot_run_startup(void);
void main_boot_reset_game_state(void);
void main_boot_build_and_draw_sceap_logo(void);
void main_boot_build_and_draw_squaresoft_logo(void);
void main_boot_fade_out_squaresoft_logo(void);

/* gfx */
extern RECT g_main_gfx_frame_image_rect;
extern RECT g_main_gfx_frame_palette_rect;
extern RECT g_main_gfx_frame_palette_tail_rect;
extern palette_t g_main_gfx_now_loading_palette;
extern u32 g_main_gfx_now_loading_frame_counter;
extern u32 g_main_gfx_now_loading_otags[2][2];
/* Seven textured quads for each display buffer. */
extern POLY_FT4 g_main_gfx_now_loading_primitives[2][7];
extern u32 g_main_gfx_now_loading_visible;
extern u32 g_main_gfx_battle_otags[2][0x180];
extern main_gfx_unit_graphics_load_queue_t g_main_gfx_unit_graphics_load_queue;
extern upoint16_t g_main_gfx_zodiac_screen_position;
extern DISPENV g_main_gfx_display_envs[2];
extern DRAWENV g_main_gfx_draw_envs[2];
extern s32 g_main_gfx_loading_display_frame_counter;
extern s32 g_main_gfx_screen_polarity;
void main_gfx_add_now_loading_to_otag(u32* otag);
void main_gfx_build_now_loading(u32 visible, s32 x, s32 y);
void main_gfx_draw_now_loading_message(void);
u32* main_gfx_get_otag(void);
void main_gfx_load_efc_fnt(void);
void main_gfx_load_frame_bin_into_vram(void);
void main_gfx_load_zodiac_frame(void);
void main_gfx_reset_display(int width, int height, int projection, u8 red, u8 green, u8 blue);
void main_gfx_swap_and_clear_otag(void);
void main_gfx_build_now_loading_center(u32 visible);
void main_gfx_call_build_zodiac_bin(zodiac_draw_context_t* context, POLY_FT4* poly);

#endif
