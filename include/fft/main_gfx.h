#ifndef MAIN_GFX_H
#define MAIN_GFX_H

/* Resident graphics: display buffers, boot logos, Now Loading, FRAME.BIN and the unit-graphics queue. */

#include "fft/gfx.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* display */
extern DISPENV g_main_gfx_display_envs[2];
extern DRAWENV g_main_gfx_draw_envs[2];
extern s32 g_main_gfx_active_buffer_index;
extern s32 g_main_gfx_screen_polarity;
extern u32 g_main_gfx_battle_otags[2][0x180];

u32* main_gfx_get_otag(void);
void main_gfx_swap_and_clear_otag(void);
void main_gfx_reset_display(int width, int height, int projection, u8 red, u8 green, u8 blue);
void main_set_display_draw(int width, int height, int projection, u8 red, u8 green, u8 blue);

/* boot logos */
extern s32 g_main_boot_startup2_has_run;
extern u32 g_main_boot_squaresoft_logo_image_words[];

void main_boot_run_startup(void);
void main_boot_reset_game_state(void);
int main_boot_draw_sceap_logo(u32* otag, u32* image);
void main_boot_build_and_draw_sceap_logo(void);
void main_boot_build_and_draw_squaresoft_logo(void);
void main_boot_fade_out_squaresoft_logo(void);

/* Now Loading */
extern palette_t g_main_gfx_now_loading_palette;
extern u32 g_main_gfx_now_loading_frame_counter;
extern u32 g_main_gfx_now_loading_otags[2][2];
/* Seven textured quads for each display buffer. */
extern POLY_FT4 g_main_gfx_now_loading_primitives[2][7];
extern u32 g_main_gfx_now_loading_visible;
extern s32 g_main_gfx_loading_display_frame_counter;

void main_gfx_build_now_loading(u32 visible, s32 x, s32 y);
void main_gfx_build_now_loading_center(u32 visible);
void main_gfx_add_now_loading_to_otag(u32* otag);
void main_gfx_draw_now_loading_message(void);

/* FRAME.BIN */
/* EVENT/FRAME.BIN: 256x288 pixels at 4bpp and 22 palettes, sector rounded.
 * The loaders upload rows 32..287 and the two palette blocks. */
typedef struct main_gfx_frame_bin {
    u8 pixels[288][128];       /* 0x0000; two 4bpp pixels per byte */
    clut_t clut;               /* 0x9000; first 16 palettes */
    palette_t palette_tail[6]; /* 0x9200; remaining six palettes */
    u8 _padding_92c0[0x540];   /* 0x92c0; past the end of the file, to the sector boundary */
} main_gfx_frame_bin_t;
typedef char main_gfx_frame_bin_size_must_be_38912[(sizeof(main_gfx_frame_bin_t) == 38912) ? 1 : -1];

extern RECT g_main_gfx_frame_image_rect;
extern RECT g_main_gfx_frame_palette_rect;
extern RECT g_main_gfx_frame_palette_tail_rect;

void main_gfx_load_frame_bin_into_vram(void);

/* zodiac display */
extern upoint16_t g_main_gfx_zodiac_screen_position;
extern POLY_FT4 g_main_zodiac_primitives[2][30];

void main_gfx_load_zodiac_frame(void);
void main_gfx_call_build_zodiac_primitives(zodiac_draw_context_t* context, POLY_FT4* poly);

/* unit graphics */
/* One queued unit: where it stands, how it faces and which graphics it uses. */
typedef struct main_gfx_unit_graphics_load_descriptor {
    u8 map_x;
    u8 map_y;
    u8 map_level;
    u8 _padding_03; /* aligns facing */
    u16 facing;     /* a quarter turn per unit */
    s16 spritesheet_id;
    u16 palette_id;
    u16 runtime_id;
    unit_stats_t* stats;
    u32 flags; /* nonzero: the unit starts undrawn */
} main_gfx_unit_graphics_load_descriptor_t;
typedef char main_gfx_unit_graphics_load_descriptor_size_must_be_20
    [(sizeof(main_gfx_unit_graphics_load_descriptor_t) == 20) ? 1 : -1];

/* Resident unit-graphics queue, filled and consumed by the BATTLE loader. */
typedef struct main_gfx_unit_graphics_load_queue {
    s32 count;                                                                            /* 0x00 */
    main_gfx_unit_graphics_load_descriptor_t descriptors[BATTLE_UNIT_RUNTIME_SLOT_COUNT]; /* 0x04 */
} main_gfx_unit_graphics_load_queue_t;
typedef char
    main_gfx_unit_graphics_load_queue_size_must_be_324[(sizeof(main_gfx_unit_graphics_load_queue_t) == 324) ? 1 : -1];

extern main_gfx_unit_graphics_load_queue_t g_main_gfx_unit_graphics_load_queue;

/* primitive pool */
/* Two buffers of 40 POLY_GT4s that REQUIRE builds its gradient grid and reward
 * image quads in. _array_32 and _array_8 name the first 32 and the last 8
 * quads of the first buffer. */
extern POLY_GT4 g_main_poly_gt4_buffers[2][40];
extern POLY_GT4 g_main_poly_gt4_array_32[];
extern POLY_GT4 g_main_poly_gt4_array_8[];

/* other resources */
extern u8 g_main_common_menu_graphics_loaded;

void main_gfx_load_efc_fnt(void);

#endif
