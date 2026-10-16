#ifndef MAIN_GFX_H
#define MAIN_GFX_H

/* Resident graphics: display buffers, boot logos, Now Loading, FRAME.BIN and the unit-graphics queue. */

#include "fft/gfx.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* display */
/* BATTLE's draw areas begin 0x80 pixels left of the displayed area, so the left edge of the battle
 * screen is draw x MAIN_GFX_BATTLE_SCREEN_X. */
enum { MAIN_GFX_BATTLE_SCREEN_X = 0x80 };

extern DISPENV g_gfx_display_envs[2];
extern DRAWENV g_gfx_draw_envs[2];
extern s32 g_gfx_active_buffer_index;
extern s32 g_gfx_screen_polarity;
extern u32 g_gfx_battle_otags[2][0x180];

u32* gfx_get_battle_otag(void);
void gfx_swap_and_clear_battle_otag(void);
void gfx_reset_display(int width, int height, int projection, u8 red, u8 green, u8 blue);
void gfx_set_display_draw(int width, int height, int projection, u8 red, u8 green, u8 blue);

/* boot logos */
extern u32 g_boot_squaresoft_logo_image_words[];

void boot_run_startup(void);
void boot_reset_game_state(void);
int boot_draw_sceap_logo(u32* otag, u32* image);
void boot_show_sceap_logo(void);
void boot_fade_in_squaresoft_logo(void);
void boot_fade_out_squaresoft_logo(void);

/* Now Loading */
extern palette_t g_loading_palette;
extern u32 g_loading_frame_counter;
extern u32 g_loading_otags[2][2];
/* Seven textured quads for each display buffer. */
extern POLY_FT4 g_loading_primitives[2][7];
extern u32 g_loading_visible;
extern s32 g_loading_display_frame_counter;

void loading_build_message(u32 visible, s32 x, s32 y);
void loading_build_message_centered(u32 visible);
void loading_add_to_otag(u32* otag);
void loading_draw_message(void);

/* FRAME.BIN */
/* EVENT/FRAME.BIN: 256x288 pixels at 4bpp and 22 palettes, sector rounded.
 * The loaders upload rows 32..287 and the two palette blocks. */
typedef struct gfx_frame_bin {
    u8 pixels[288][128];       /* 0x0000; two 4bpp pixels per byte */
    clut_t clut;               /* 0x9000; first 16 palettes */
    palette_t palette_tail[6]; /* 0x9200; remaining six palettes */
    u8 _padding_92c0[0x540];   /* 0x92c0; past the end of the file, to the sector boundary */
} gfx_frame_bin_t;
typedef char main_gfx_frame_bin_size_must_be_38912[(sizeof(gfx_frame_bin_t) == 38912) ? 1 : -1];

void gfx_load_frame_bin(void);

/* zodiac display */
extern upoint16_t g_zodiac_screen_position;
extern POLY_FT4 g_zodiac_primitives[2][30];

void zodiac_load_frame(void);
void zodiac_call_build_primitives(zodiac_draw_context_t* context, POLY_FT4* poly);

/* unit graphics */
/* One queued unit: where it stands, how it faces and which graphics it uses. */
typedef struct unit_graphics_load_descriptor {
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
} unit_graphics_load_descriptor_t;
typedef char
    main_gfx_unit_graphics_load_descriptor_size_must_be_20[(sizeof(unit_graphics_load_descriptor_t) == 20) ? 1 : -1];

/* Resident unit-graphics queue, filled and consumed by the BATTLE loader. */
typedef struct unit_graphics_load_queue {
    s32 count;                                                            /* 0x00 */
    unit_graphics_load_descriptor_t descriptors[UNIT_RUNTIME_SLOT_COUNT]; /* 0x04 */
} unit_graphics_load_queue_t;
typedef char main_gfx_unit_graphics_load_queue_size_must_be_324[(sizeof(unit_graphics_load_queue_t) == 324) ? 1 : -1];

extern unit_graphics_load_queue_t g_unit_graphics_load_queue;

/* primitive pool */
/* Two buffers of 40 POLY_GT4s that REQUIRE builds its gradient grid and reward
 * image quads in. _array_32 and _array_8 name the first 32 and the last 8
 * quads of the first buffer. */
extern POLY_GT4 g_gfx_poly_gt4_buffers[2][40];
extern POLY_GT4 g_gfx_poly_gt4_array_32[];
extern POLY_GT4 g_gfx_poly_gt4_array_8[];

/* other resources */
extern u8 g_menu_common_graphics_loaded;

void debug_load_efc_font(void);

#endif
