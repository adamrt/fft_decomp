#ifndef FFT_MAIN_GFX_H
#define FFT_MAIN_GFX_H

#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/types.h"

typedef enum texture_uv_flip {
    TEXTURE_UV_FLIP_NONE = 0,
    TEXTURE_UV_FLIP_X = 1,
    TEXTURE_UV_FLIP_Y = 2,
    TEXTURE_UV_FLIP_XY = TEXTURE_UV_FLIP_X | TEXTURE_UV_FLIP_Y,
} texture_uv_flip_e;

extern u32 g_main_boot_squaresoft_logo_image_words[];
extern s32 g_main_gfx_display_buffer_index;
extern DISPENV g_main_gfx_display_envs[2];
extern DRAWENV g_main_gfx_draw_envs[2];
extern s32 g_main_gfx_loading_display_frame_counter;
extern s32 g_main_gfx_screen_polarity;

/* boot */
void main_boot_build_and_draw_sceap_logo(void);
void main_boot_build_and_draw_squaresoft_logo(void);
int main_boot_draw_sceap_logo(void* otag, u32* image);
int main_boot_draw_squaresoft_logo(u32 otag, u32* image);
void main_boot_fade_out_squaresoft_logo(void);

/* gfx */
void main_gfx_add_now_loading_to_otag(u32* otag);
void main_gfx_build_now_loading(u32 visible, s32 x, s32 y);
void main_gfx_draw_now_loading_message(void);
u32* main_gfx_get_otag(void);
void main_gfx_load_efc_fnt(void);
void main_gfx_load_frame_bin_into_vram(void);
void main_gfx_load_zodiac_frame(void);
void main_gfx_reset_display(int width, int height, int projection, u8 red, u8 green, u8 blue);
void main_gfx_swap_and_clear_otag(void);
int main_gfx_swap_display_area(u32 otag);

/* set */
void main_set_display_draw(int width, int height, int projection, u8 red, u8 green, u8 blue);
void main_set_display_draw_new_game(int width, int height, int projection, u8 red, u8 green, u8 blue);

/* util */
void main_util_set_svector(SVECTOR* vector, s32 x, s32 y, s32 z);
void main_util_set_vector(VECTOR* vector, s32 x, s32 y, s32 z);

void main_gfx_build_now_loading_center(u32 visible);

#endif
