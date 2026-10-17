#ifndef GFX_IMAGE_H
#define GFX_IMAGE_H

/* VRAM images: image loading and transfers, scene texture allocation, CLUT rectangles, item icons and the image
 * placements of the event library's icons. */

#include "fft/gfx.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* image loading */
/* Scene-local GPU texture window; also the default draw-area rectangle. */
extern RECT g_gfx_texture_window;

void gfx_set_image_primitive_layout(
    void* primitive, const point16_t* base_load, const RECT* base_screen, const image_load_parameters_t* parameters);
void gfx_load_image_and_wait(RECT* rect, void* data);
void gfx_move_image_and_wait(RECT* rect, s16 x, s16 y);
void gfx_store_image_and_wait(RECT* rect, void* data);
void gfx_load_evtchr_thread(void);
void gfx_set_clut_rect_from_id(RECT* rect, u16 clut_id);

/* scene texture allocation */
/* Independent 15-row, 16-column grids for BATTLE and WORLD. */
extern u8 g_gfx_texture_allocation_grid[15][16];
extern s32 g_gfx_free_rect_pixels[4];
extern s32 g_gfx_free_rect_cells[4];

void gfx_alloc_texture_rect(RECT* rect, void* primitive, u32* image);
void gfx_free_texture_rect(RECT* rect);
void gfx_clear_texture_allocation_grid(void);

/* Round x, y, width and height from texture pixels to 16-pixel cells. */
static inline void texture_rect_pixels_to_cells(const s32 pixels[4], s32 cells[4]) {
    s32 i;
    s32 value;

    for (i = 0; i < 4; i++) {
        value = pixels[i] >> 4;
        cells[i] = value;
        if (pixels[i] & 0xf) {
            cells[i] = value + 1;
        }
    }
}

/* item icons */
extern point16_t g_gfx_item_icon_image_location;
extern s16 g_gfx_item_icon_clut_base_x;
extern s16 g_gfx_item_icon_clut_base_y;

void gfx_set_item_icon_sprite(SPRT* sprite, s32 item_id);

/* icon placements */
/* Image placements passed to gfx_set_image_primitive_layout for the icon strips of the event library. The panel and
 * numeric-panel cells of the menu library, which each menu program defines for itself, are in unit_panel.h. */
extern image_load_parameters_t g_menu_icon_strip_image_params[20];
extern image_load_parameters_t g_menu_single_icon_image_params[5];

#endif
