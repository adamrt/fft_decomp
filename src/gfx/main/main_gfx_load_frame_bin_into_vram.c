#include "fft/main.h"
#include "psx/types.h"

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
