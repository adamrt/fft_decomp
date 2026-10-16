#ifndef MENU_GFX_H
#define MENU_GFX_H

/* Menu drawing: primitive setup, the per-frame primitive queues, texture pages, palettes and CLUTs, and the glyph sheet
 * the windows and numbers are cut from. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* primitives */
extern point16_t g_menu_texture_origin; /* VRAM origin for menu image placement */

/* An off-screen quad with neutral shading, the allocated-image texture page and the normal menu CLUT. */
void menu_init_default_poly_ft4(POLY_FT4* poly);

/* BATTLE's body also shades the texture. */
void menu_init_sprt(SPRT* sprite);

/* menu_init_sprt for count consecutive sprites, each then given clut. */
void menu_init_sprite_array(SPRT* sprites, s32 count, s32 clut);

/* primitive queues */
/* The menu_gfx_enqueue_* helpers take primitives from the pools of g_menu_gfx_context, counting each kind, and
 * link them into its ordering table at the given entry. */

/* Six-byte menu sprite cell: screen rectangle and texture origin. */
typedef struct menu_sprite_cell {
    u8 x;
    u8 y;
    u8 w;
    u8 h;
    u8 u;
    u8 v;
} menu_sprite_cell_t;
typedef char menu_sprite_cell_size_must_be_6[(sizeof(menu_sprite_cell_t) == 6) ? 1 : -1];

extern menu_gfx_context_t* g_menu_gfx_context;
extern s16 g_menu_gfx_otag_index;
extern u16 g_menu_atlas_texture_page;
extern s16 g_menu_gfx_semitransparency;
extern u16 g_menu_gfx_poly_ft4_count;
extern u16 g_menu_gfx_poly_gt4_count;
extern u16 g_menu_gfx_draw_mode_count;
extern u16 g_menu_gfx_draw_area_count;
extern u16 g_menu_gfx_draw_move_count;
extern u16 g_menu_gfx_drawenv_y;

/* Half of each double-buffered packet pair this frame fills; the menu frame update toggles it. */
extern s32 g_menu_packet_buffer_index;

/* Shade color of the menu's text and textured quads (menu_gfx_get_draw_color), set by the palette mode and
 * gfx_set_draw_color. */
extern rgb24_t g_menu_text_color;

rgb24_t* menu_gfx_get_draw_color(void);
void menu_gfx_enqueue_draw_area(RECT* rect, s32 idx);
void menu_gfx_enqueue_draw_mode(s32 dfe, s32 dtd, s32 tpage, RECT* tw, s32 idx);
void menu_gfx_enqueue_draw_move(const RECT* rect, s32 u, s32 v, s32 otag_index);
void menu_gfx_enqueue_oriented_textured_quad(
    const oriented_quad_t* descriptor, const rgb24_t* color, s32 orientation, s32 semitrans, s32 otag_index);
void menu_gfx_enqueue_textured_quad(
    const RECT* rect, s32 u, s32 v, const rgb24_t* color, s32 semitrans, u16 texture_page, u16 clut, s32 otag_index);

/* Draws count cells at one ordering-table entry; a null color leaves the texture unshaded. */
void menu_gfx_enqueue_textured_quad_list(const menu_sprite_cell_t* quads, const rgb24_t* color, s32 semitrans,
    u16 texture_page, u16 clut, s32 otag_index, s32 count);

/* texture pages */
/* Second argument of gfx_set_draw_mode_for_texture_page: which of the menu
 * code's five 4-bit texture pages a draw mode selects, named by what the game
 * keeps there. Any value above 3 selects the last one. */
typedef enum menu_texture_page {
    MENU_TEXTURE_PAGE_FRAME = 0,            /* VRAM (960, 256): FRAME.BIN, the window frames and UI sprites */
    MENU_TEXTURE_PAGE_ALLOCATED_IMAGES = 1, /* VRAM (448, 0): the grid gfx_alloc_texture_rect hands out */
    MENU_TEXTURE_PAGE_TEXT_IMAGES = 2,      /* VRAM (384, 0): rendered message and list text */
    MENU_TEXTURE_PAGE_UNIT_SPRITES = 3,     /* VRAM (832, 256): the first unit spritesheet column; no caller */
    MENU_TEXTURE_PAGE_ITEM_ICONS = 4,       /* VRAM (896, 256): ITEM.BIN; the code passes y = 288 */
} menu_texture_page_e;

/* Texture page and CLUT of the menu script's textured quads, set by its CLUT and texture-page commands and by
 * icon row callbacks. */
extern u16 g_menu_gfx_texture_page;
extern u16 g_menu_gfx_clut_id;

/* palettes */
/* menu_set_palette_mode picks the normal (0) or alternate (1) palette: it copies the matching _mode0 or _mode1
 * text and cursor CLUTs into the active CLUT selections below and sets g_menu_text_color. While the input is
 * locked or the previous thread still runs, the sprite CLUT selectors pick the dimmed colors. */

/* Two sixteen-color banks: the normal menu colors followed by their dimmed forms. */
typedef struct menu_palette_pair {
    rgba5551_t colors[2 * PALETTE_COLOR_COUNT];
} menu_palette_pair_t;
typedef char menu_palette_pair_size_must_be_64[(sizeof(menu_palette_pair_t) == 64) ? 1 : -1];

/* CLUTs of the normal and dimmed menu colors: GetClut(0x3c0, 0x1f0) and
 * GetClut(0x3c0, 0x1f4). */
enum {
    MENU_CLUT_NORMAL = 0x7c3c,
    MENU_CLUT_DIMMED = 0x7d3c,
};

extern menu_palette_pair_t g_menu_palette_colors;
extern menu_palette_pair_t g_menu_default_palette_colors;
extern u8 g_menu_use_alternate_palette;
extern u16 g_menu_palette_clut;
extern u16 g_menu_active_clut_back;
extern u16 g_menu_active_clut_front;
extern u16 g_menu_window_clut;
extern u16 g_menu_text_clut_0_mode0;
extern u16 g_menu_text_clut_0_mode1;
extern u16 g_menu_text_clut_1_mode0;
extern u16 g_menu_text_clut_1_mode1;
extern u16 g_menu_text_clut_2_mode0;
extern u16 g_menu_text_clut_2_mode1;
extern u16 g_menu_text_clut_3_mode0;
extern u16 g_menu_text_clut_3_mode1;
extern u16 g_menu_alternate_number_clut;
extern u16 g_menu_disabled_number_clut;

void menu_set_palette_colors(const menu_palette_pair_t* source);
void menu_set_palette_mode(s32 mode);
void menu_select_sprite_clut(void* packet);
void menu_select_icon_sprite_clut(SPRT* slot);

/* glyph sheet */
/* Shared 4bpp menu glyph sheet: 32 rows of 128 packed pixel bytes, then
 * its palette. Window-frame builders copy aligned pairs of bytes. */
typedef struct menu_glyph_sheet {
    u8 pixels[4096];             /* 0x0000 */
    menu_palette_pair_t palette; /* 0x1000 */
} menu_glyph_sheet_t;
typedef char menu_glyph_sheet_size_must_be_4160[(sizeof(menu_glyph_sheet_t) == 4160) ? 1 : -1];

/* Points at g_menu_glyph_image_data once the scene's menu resources are set up. */
extern menu_glyph_sheet_t* g_menu_glyph_sheet;
extern menu_glyph_sheet_t g_menu_glyph_image_data;

/* sprite pages */
/* One double-buffered sprite page (0xa0 bytes): five DR_MODE texture-window packets followed by the five SPRTs
 * they apply to. menu_build_sprite_page builds two of them in g_menu_scratch_buffer for the unit stats panels. */
typedef struct menu_sprite_page {
    DR_MODE modes[5]; /* 0x00 */
    SPRT sprites[5];  /* 0x3c */
} menu_sprite_page_t;
typedef char menu_sprite_page_size_must_be_160[(sizeof(menu_sprite_page_t) == 160) ? 1 : -1];

#endif
