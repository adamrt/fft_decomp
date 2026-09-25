#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Loads the 0x2000-byte portrait colour record (sector 0x164b + 4 * index) on
 * the main thread, yielding until the request is queued and again until the
 * read completes, then uploads the eight 8x0x30 palette columns and the
 * trailing image half to VRAM. */
void world_script_load_portrait_colors_event_instruction(s32 index) {
    RECT rect;
    u8* buffer;
    u8* source;
    u8* upper_half;
    s32 i;

    buffer = world_menu_alloc_ui_buffer(0x2000);
    do {
        world_thread_yield();
        g_world_thread_inner_subroutine_callback = (void (*)(void))main_file_call_build_header;
    } while (world_thread_call_on_main_stack(index * 4 + 0x164B, 0x2000, buffer) != 0);
    do {
        world_thread_yield();
        g_world_thread_inner_subroutine_callback = (void (*)(void))main_file_is_still_loading;
    } while (world_thread_call_on_main_stack() != 0);
    upper_half = buffer + 0x1800;
    world_script_copy_bytes(&rect, &g_world_menu_portrait_pixel_rect, 8);
    i = 0;
    source = buffer;
    rect.w = 8;
    rect.h = 0x30;
    for (i = 0; i < 8; i++) {
        LoadImage(&rect, source);
        rect.x += 8;
        source += 0x300;
    }
    LoadImage(&g_world_menu_portrait_palette_rect, upper_half);
    world_menu_free_memory(buffer);
    world_thread_yield();
}
