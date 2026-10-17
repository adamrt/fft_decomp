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
    void* primitive, const point16_t* base_load, const RECT* base_screen, const image_layout_t* parameters);
void gfx_load_image_and_wait(RECT* rect, void* data);
void gfx_move_image_and_wait(RECT* rect, s16 x, s16 y);
void gfx_store_image_and_wait(RECT* rect, void* data);
void gfx_load_evtchr_thread(void);
void gfx_set_clut_rect_from_id(RECT* rect, u16 clut_id);

/* scene texture allocation */
void gfx_alloc_texture_rect(RECT* rect, void* primitive, u32* image);
void gfx_free_texture_rect(RECT* rect);
void gfx_clear_texture_allocation_grid(void);

/* item icons */
void gfx_set_item_icon_sprite(SPRT* sprite, s32 item_id);

#endif
