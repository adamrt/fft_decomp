/* The resident zodiac drawing entrance and FRAME.BIN texture upload used by deployment
 * and menu screens. */

#include "fft/gfx/gfx.h"
#include "fft/gfx/zodiac.h"
#include "fft/sys/file.h"
#include "fft/sys/memory.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static RECT s_gfx_frame_image_rect = { 960, 256, 64, 256 };
static RECT s_gfx_frame_palette_rect = { 960, 496, 16, 16 };
static RECT s_gfx_frame_palette_tail_rect = { 976, 506, 16, 6 };

/* Call MAIN's own zodiac_build_primitives; ATTACK.OUT's deployment screen draws its
 * zodiac sprites through this wrapper rather than its own copy. */
void zodiac_build_resident_primitives(zodiac_draw_context_t* context, POLY_FT4* poly) {
    zodiac_build_primitives(context, poly);
}

/* Load EVENT/FRAME.BIN and upload its pixels and its two palette blocks. */
void gfx_load_frame_texture(void) {
    gfx_frame_bin_t* frame = file_load_into_heap_checked(DISC_SECTOR_EVENT_FRAME_BIN, sizeof(gfx_frame_bin_t));

    LoadImage(&s_gfx_frame_image_rect, (u32*)frame->pixels[32]);
    LoadImage(&s_gfx_frame_palette_rect, (u32*)frame->clut);
    LoadImage(&s_gfx_frame_palette_tail_rect, (u32*)frame->palette_tail);
    DrawSync(0);
    heap_release_block(frame);
}
