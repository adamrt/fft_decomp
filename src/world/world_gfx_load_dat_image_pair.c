#include "fft/main_file.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Loads one 0x2000-byte image record (sector 0x164b + 4 * index) through the
 * main thread and uploads its two halves to VRAM. */
void world_gfx_load_dat_image_pair(s32 index) {
    void* buffer;

    buffer = world_menu_alloc_ui_buffer(0x2000);
    g_world_thread_inner_subroutine_callback = (void (*)(void))main_file_load_checked_to_address;
    world_thread_call_on_main_stack(index * 4 + 0x164B, 0x2000, buffer);
    LoadImage(&g_world_menu_portrait_pixel_rect, buffer);
    LoadImage(&g_world_menu_portrait_palette_rect, (u8*)buffer + 0x1800);
    world_menu_free_memory(buffer);
}
