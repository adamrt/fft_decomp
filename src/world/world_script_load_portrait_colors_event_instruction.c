#include "fft/memory.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Upload eight 32x48 portraits and their palettes from one EVTFACE.BIN block. */
void world_script_load_portrait_colors_event_instruction(s32 index) {
    RECT rect;
    menu_portrait_image_file_t* buffer;
    u32(*pixels)[48][4];
    palette_t* palettes;
    s32 i;

    buffer = menu_alloc_memory(sizeof(*buffer));
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (thread_call_on_main_stack(index * 4 + DISC_SECTOR_EVENT_EVTFACE_BIN, sizeof(*buffer), buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);
    palettes = buffer->palettes;
    mem_copy_bytes_nonempty(&rect, &g_menu_portrait_pixel_rect, sizeof(rect));
    i = 0;
    pixels = buffer->pixels;
    rect.w = 8;
    rect.h = 0x30;
    for (i = 0; i < 8; i++) {
        LoadImage(&rect, pixels[0][0]);
        rect.x += 8;
        pixels++;
    }
    LoadImage(&g_menu_portrait_palette_rect, (u32*)palettes);
    menu_free_memory(buffer);
    thread_yield();
}
