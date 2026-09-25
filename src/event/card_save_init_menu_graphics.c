#include "fft/event_card.h"
#include "psx/types.h"

void card_save_init_menu_graphics(void) {
    RECT rect;
    u16 image_data[16];

    g_battle_text_section_pointers[27] = g_card_text_data;
    g_battle_text_section_pointers[24] = g_card_text_command_help;

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    card_gfx_load_image_and_wait(&rect, (u32*)g_battle_menu_glyph_image);

    rect.x = 0x100;
    rect.y = 0xfa;
    rect.w = 0x40;
    rect.h = 4;
    card_gfx_load_image_and_wait(&rect, (u32*)g_card_save_menu_palette_image);

    rect.x = 0x3c0;
    rect.y = 0x1f0;
    rect.w = 0x10;
    rect.h = 1;
    card_gfx_store_image_and_wait(&rect, (u32*)image_data);
    image_data[4] = 0;

    rect.x = 0x120;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    card_gfx_load_image_and_wait(&rect, (u32*)image_data);

    rect.x = 0x100;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    card_gfx_load_image_and_wait(&rect, (u32*)&image_data[4]);

    rect.x = 0x3c0;
    rect.y = 0x1f4;
    rect.w = 0x10;
    rect.h = 1;
    card_gfx_store_image_and_wait(&rect, (u32*)image_data);
    image_data[4] = 0;

    rect.x = 0x130;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    card_gfx_load_image_and_wait(&rect, (u32*)image_data);

    rect.x = 0x110;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    card_gfx_load_image_and_wait(&rect, (u32*)&image_data[4]);

    card_gfx_start_decreasing_fade();
    g_card_save_menu_state = 0;
    battle_thread_suspend(15);
}
