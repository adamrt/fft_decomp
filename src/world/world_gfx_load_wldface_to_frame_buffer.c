#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Open WLDFACE.BIN and move it to the frame buffer.
 *
 * The 0x20000-byte file at disc sector 0x18ba is fetched on the main thread and
 * moved to VRAM as four 0x8000-byte pages; the last page holds the palettes.
 */
void world_gfx_load_wldface_to_frame_buffer(void) {
    u8* image;

    g_world_thread_inner_subroutine_callback = main_file_get_bin_as_tim;
    image = (u8*)world_thread_call_on_main_stack(0x18ba, 0x20000);
    LoadImage(&g_world_wldface_vram_rects, (u32*)image);
    LoadImage(&g_world_wldface_vram_rect_1, (u32*)(image + 0x8000));
    LoadImage(&g_world_wldface_vram_rect_2, (u32*)(image + 0x10000));
    LoadImage(&g_world_wldface_vram_rect_3, (u32*)(image + 0x18000));
    DrawSync(0);
    main_heap_call_free(image);
}
