#include "fft/event_card.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/*
 * Load the save screen's VRAM images and start the fade-in.
 *
 * Binds the overlay's text sections, uploads the menu glyph image to
 * (0x100, 0) and the save menu palettes to (0x100, 0xfa), and derives four
 * more palette rows at y 0xfe from the CLUT rows at (0x3c0, 0x1f0) and
 * (0x3c0, 0x1f4): each row is copied with entry 4 made transparent, and its
 * entries 4..15 are copied again as a 12-color row. Sets the menu state to
 * the initial prompt and suspends thread 15.
 */
void card_save_init_menu_graphics(void) {
    RECT rect;
    palette_t image_data;

    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_card_text_data;
    g_text_section_pointers[TEXT_SECTION_FORMATION_MESSAGES] = g_card_text_command_help;

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);

    rect.x = 0x100;
    rect.y = 0xfa;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, g_card_save_menu_clut);

    rect.x = 0x3c0;
    rect.y = 0x1f0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, image_data);
    *(u16*)&image_data[4] = 0;

    rect.x = 0x120;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, image_data);

    rect.x = 0x100;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &image_data[4]);

    rect.x = 0x3c0;
    rect.y = 0x1f4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, image_data);
    /* Fieldwise or aggregate clearing adds instructions under this routine's -O0 profile. */
    *(u16*)&image_data[4] = 0;

    rect.x = 0x130;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, image_data);

    rect.x = 0x110;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &image_data[4]);

    card_gfx_start_decreasing_fade();
    g_card_save_menu_state = CARD_SAVE_MENU_INITIAL_PROMPT;
    thread_suspend(15);
}
