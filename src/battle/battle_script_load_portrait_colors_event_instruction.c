#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* VRAM destination of the 8 portrait pixel columns (x advances by 8) and of
 * the 0x1800-offset palette block. */

/* Loads portrait sector 0x164b + 4 * portrait_id into VRAM (0x2000 bytes). */
void battle_script_load_portrait_colors_event_instruction(s32 portrait_id) {
    RECT rect;
    u8* buffer;
    u8* pixels;
    s32 i;
    u8* palette;

    buffer = battle_menu_alloc_memory(0x2000);
    do {
        battle_thread_yield();
        g_battle_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (battle_thread_call_on_main_stack(portrait_id * 4 + 0x164B, 0x2000, buffer) != 0);
    do {
        battle_thread_yield();
        g_battle_thread_call_target = main_file_is_still_loading;
    } while (battle_thread_call_on_main_stack() != 0);

    palette = buffer + 0x1800;
    battle_copy_bytes(&rect, &g_battle_menu_portrait_pixel_rect, 8);
    i = 0;
    pixels = buffer;
    rect.w = 8;
    rect.h = 0x30;
    do {
        LoadImage(&rect, (u32*)pixels);
        rect.x += 8;
        pixels += 0x300;
        i += 1;
    } while (i < 8);
    LoadImage(&g_battle_menu_portrait_palette_rect, (u32*)palette);
    battle_menu_free_memory(buffer);
    battle_thread_yield();
}
