/* The zodiac graphic, FRAME.BIN VRAM loading, the WORLD.BIN and WLDCORE.BIN opens and data loading from disc. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/overlay.h"
#include "fft/world_map.h"
#include "fft/zodiac.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static RECT g_gfx_frame_image_rect = { 960, 256, 64, 256 };
static RECT g_gfx_frame_palette_rect = { 960, 496, 16, 16 };
static RECT g_gfx_frame_palette_tail_rect = { 976, 506, 16, 6 };

static void file_load_data_from_disc(int sector, int sectors, void* destination, int loading_display_mode);

/* Call MAIN's own zodiac_build_primitives; ATTACK.OUT's deployment screen draws its
 * zodiac sprites through this wrapper rather than its own copy. */
void zodiac_call_build_primitives(zodiac_draw_context_t* context, POLY_FT4* poly) {
    zodiac_build_primitives(context, poly);
}

/* Load EVENT/FRAME.BIN and upload its pixels and its two palette blocks. */
void gfx_load_frame_bin(void) {
    gfx_frame_bin_t* frame = file_load_checked_to_heap(DISC_SECTOR_EVENT_FRAME_BIN, sizeof(gfx_frame_bin_t));

    LoadImage(&g_gfx_frame_image_rect, (u32*)frame->pixels[32]);
    LoadImage(&g_gfx_frame_palette_rect, (u32*)frame->clut);
    LoadImage(&g_gfx_frame_palette_tail_rect, (u32*)frame->palette_tail);
    DrawSync(0);
    heap_call_free(frame);
}

/* Load WLDCORE.BIN into the low overlay area with the Now Loading screen and, when load_world is
 * set, WORLD.BIN at the world overlay address, then run WLDCORE's entry point and return its
 * result. */
s32 overlay_open_world_and_wldcore(s32 load_world) {
    file_load_data_from_disc(
        DISC_SECTOR_WORLD_WLDCORE_BIN, 0xDC, g_heap_low_overlay_load_address, FILE_LOADING_DISPLAY_INIT);
    if (load_world != 0) {
        file_load_data_from_disc(
            DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_heap_world_overlay_load_address, FILE_LOADING_DISPLAY_PRESERVE);
    }
    return world_map_entrypoint();
}

/* Load WORLD.BIN alone. mode - 1 is the loading display mode of
 * file_request_read, so mode 1 shows the "Now Loading" screen. */
void overlay_open_world(s32 mode) {
    file_load_data_from_disc(DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_heap_world_overlay_load_address, mode - 1);
}

/* Blocking read of whole sectors to destination through the shared loader. The
 * last argument is file_request_read's loading display mode: 0 blanks the
 * screen and sets up the "Now Loading" message, 1 leaves the display state alone,
 * and -1 turns the message off. */
static void file_load_data_from_disc(int sector, int sectors, void* destination, int loading_display_mode) {
    file_load_descriptor_t* descriptor = &g_file_cd_state;

    file_request_read(descriptor, sector, sectors, destination, loading_display_mode);
    while (g_file_still_loading != 0) {
        file_poll_load(descriptor);
        VSync(0);
    }
}
