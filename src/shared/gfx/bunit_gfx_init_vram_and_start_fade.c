#include "fft/event_bunit.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/types.h"

/*
 * Load the BUNIT screen's VRAM images and start the fade-in.
 *
 * Binds the overlay's text sections, uploads the menu glyph image to
 * (0x100, 0) and derives four palette rows at y 0xfe from the CLUT rows at
 * (0x3c0, 0x1f0) and (0x3c0, 0x1f4): each row is copied with entry 4 made
 * transparent, and its entries 4..15 are copied again as a 12-color row.
 * The 16-entry row is read into buf1, which holds four entries; the other
 * twelve land in buf2, which follows it on the stack. The 0x200-byte menu
 * palette image is copied to the stack before it is uploaded to
 * (0x100, 0xfa).
 */
void bunit_gfx_init_vram_and_start_fade(void) {
    RECT rect;
    u16 buf1[4];
    u16 buf2[12];
    clut_t block;

    memcpy(block, g_bunit_menu_clut, sizeof(block));

    menu_set_vsync_mode_and_event_speed(0);
    g_text_section_pointers[TEXT_SECTION_EVENT_MESSAGES] = g_bunit_text_menu_section;
    g_text_section_pointers[TEXT_SECTION_COMPOSED_TEXT] = g_bunit_text_reorder_list_buffer;
    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);
    rect.x = 0x3C0;
    rect.y = 0x1F0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, buf1);
    buf2[0] = 0;
    rect.x = 0x120;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf1);
    rect.x = 0x100;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf2);
    rect.x = 0x3C0;
    rect.y = 0x1F4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, buf1);
    buf2[0] = 0;
    rect.x = 0x130;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf1);
    rect.x = 0x110;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, buf2);
    rect.x = 0x100;
    rect.y = 0xFA;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, block);
    g_text_selection_id = 0;
    g_bunit_help_screen_id = MENU_GRID_HELP_NONE;
    gfx_start_decreasing_fade();
}
