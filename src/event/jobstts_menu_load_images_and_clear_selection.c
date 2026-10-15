#include "fft/event_jobstts.h"
#include "fft/gfx.h"
#include "fft/menu.h"

/*
 * Load the JOBSTTS screen's VRAM images and clear its selection.
 *
 * Uploads the menu glyph image to (0x100, 0) and the menu palettes to
 * (0x100, 0xfa), and derives four more palette rows at y 0xfe from the CLUT
 * rows at (0x3c0, 0x1f0) and (0x3c0, 0x1f4): each row is copied with entry 4
 * made transparent, and its entries 4..15 are copied again as a 12-color
 * row.
 */
void jobstts_menu_load_images_and_clear_selection(void) {
    RECT rect;
    palette_t palette;

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);

    rect.x = 0x100;
    rect.y = 0xfa;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, g_jobstts_menu_palette_image);

    rect.x = 0x3c0;
    rect.y = 0x1f0;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);

    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    rect.x = 0x120;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);

    rect.x = 0x100;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);

    rect.x = 0x3c0;
    rect.y = 0x1f4;
    rect.w = 0x10;
    rect.h = 1;
    gfx_store_image_and_wait(&rect, palette);

    palette[4].red = palette[4].green = palette[4].blue = palette[4].semi_transparent = 0;
    rect.x = 0x130;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, palette);

    rect.x = 0x110;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    gfx_load_image_and_wait(&rect, &palette[4]);

    menu_clear_selection_values();
    g_jobstts_text_selection_id = 0;
}
