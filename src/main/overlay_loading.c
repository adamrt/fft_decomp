/* The zodiac graphic, FRAME.BIN VRAM loading, the WORLD.BIN and WLDCORE.BIN opens and data loading from disc. */

#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "fft/menu_panel.h"
#include "fft/wldcore.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static void main_file_load_data_from_disc(int sector, int sectors, void* destination, int loading_display_mode);

void main_gfx_call_build_zodiac_bin(zodiac_draw_context_t* context, POLY_FT4* poly) {
    menu_build_zodiac_primitives(context, poly);
}

/* Load EVENT/FRAME.BIN and upload its pixels and its two palette blocks. */
void main_gfx_load_frame_bin_into_vram(void) {
    main_gfx_frame_bin_t* frame
        = main_file_load_checked_to_heap(DISC_SECTOR_EVENT_FRAME_BIN, sizeof(main_gfx_frame_bin_t));

    LoadImage(&g_main_gfx_frame_image_rect, (u32*)frame->pixels[32]);
    LoadImage(&g_main_gfx_frame_palette_rect, (u32*)frame->clut);
    LoadImage(&g_main_gfx_frame_palette_tail_rect, (u32*)frame->palette_tail);
    DrawSync(0);
    main_heap_call_free(frame);
}

s32 main_overlay_open_world_and_wldcore(s32 load_world) {
    main_file_load_data_from_disc(
        DISC_SECTOR_WORLD_WLDCORE_BIN, 0xDC, g_main_heap_low_overlay_load_address, MAIN_FILE_LOADING_DISPLAY_INIT);
    if (load_world != 0) {
        main_file_load_data_from_disc(DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_main_heap_world_overlay_load_address,
            MAIN_FILE_LOADING_DISPLAY_PRESERVE);
    }
    return wldcore_entrypoint();
}

/* Load WORLD.BIN alone. mode - 1 is the loading display mode of
 * main_file_build_header, so mode 1 shows the "Now Loading" screen. */
void main_overlay_open_world_bin(s32 mode) {
    main_file_load_data_from_disc(DISC_SECTOR_WORLD_WORLD_BIN, 0x1E0, g_main_heap_world_overlay_load_address, mode - 1);
}

/* Blocking read of whole sectors to destination through the shared loader. The
 * last argument is main_file_build_header's loading display mode: 0 blanks the
 * screen and sets up the "Now Loading" message, 1 leaves the display state alone,
 * and -1 turns the message off. */
static void main_file_load_data_from_disc(int sector, int sectors, void* destination, int loading_display_mode) {
    main_file_load_descriptor_t* descriptor = &g_main_file_cd_state;

    main_file_build_header(descriptor, sector, sectors, destination, loading_display_mode);
    while (g_main_file_still_loading != 0) {
        main_file_poll_load(descriptor);
        VSync(0);
    }
}
