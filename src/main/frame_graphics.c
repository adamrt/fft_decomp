/* The resident zodiac drawing entrance and FRAME.BIN texture upload used by deployment
 * and menu screens. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/zodiac.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static RECT g_gfx_frame_image_rect = { 960, 256, 64, 256 };
static RECT g_gfx_frame_palette_rect = { 960, 496, 16, 16 };
static RECT g_gfx_frame_palette_tail_rect = { 976, 506, 16, 6 };

/* Call MAIN's own zodiac_build_primitives; ATTACK.OUT's deployment screen draws its
 * zodiac sprites through this wrapper rather than its own copy. */
void zodiac_resident_primitives_build(zodiac_draw_context_t* context, POLY_FT4* poly) {
    zodiac_build_primitives(context, poly);
}

/* Load EVENT/FRAME.BIN and upload its pixels and its two palette blocks. */
void gfx_frame_texture_load(void) {
    gfx_frame_bin_t* frame = file_heap_load_checked(DISC_SECTOR_EVENT_FRAME_BIN, sizeof(gfx_frame_bin_t));

    LoadImage(&g_gfx_frame_image_rect, (u32*)frame->pixels[32]);
    LoadImage(&g_gfx_frame_palette_rect, (u32*)frame->clut);
    LoadImage(&g_gfx_frame_palette_tail_rect, (u32*)frame->palette_tail);
    DrawSync(0);
    heap_block_release(frame);
}
