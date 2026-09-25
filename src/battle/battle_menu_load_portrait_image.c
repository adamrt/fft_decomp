#include "fft/battle.h"
#include "psx/types.h"

/* Load portrait sector 0x164b + 4 * portrait_id straight into VRAM. */
void battle_menu_load_portrait_image(s32 portrait_id) {
    u8* buffer;

    buffer = battle_menu_alloc_memory(0x2000);
    g_battle_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
    battle_thread_call_on_main_stack(portrait_id * 4 + 0x164B, 0x2000, buffer);
    LoadImage(&g_battle_menu_portrait_pixel_rect, (u32*)buffer);
    LoadImage(&g_battle_menu_portrait_palette_rect, (u32*)(buffer + 0x1800));
    battle_menu_free_memory(buffer);
}
