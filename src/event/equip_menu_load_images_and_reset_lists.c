#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "psx/types.h"

/*
 * Load the EQUIP screen's VRAM images and reset its menu state.
 *
 * Uploads the menu glyph image to (0x100, 0) and the menu palettes to
 * (0x100, 0xfa), and derives four more palette rows at y 0xfe from the CLUT
 * rows at (0x3c0, 0x1f0) and (0x3c0, 0x1f4): each row is copied with entry 4
 * made transparent, and its entries 4..15 are copied again as a 12-color
 * row. The 16-entry row is read into buf1, which holds four entries; the
 * other twelve land in buf2, which follows it on the stack.
 *
 * Then clears the selection values, indices and records, totals the
 * inventory and rebuilds the five item category lists.
 */
void equip_menu_load_images_and_reset_lists(void) {
    RECT rect;
    u16 buf1[4];
    u16 buf2[12];

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    gfx_load_image_and_wait(&rect, g_menu_glyph_sheet->pixels);
    rect.x = 0x100;
    rect.y = 0xFA;
    rect.w = 0x40;
    rect.h = 4;
    gfx_load_image_and_wait(&rect, g_equip_menu_palette_image);
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
    equip_menu_clear_selection_values();
    g_equip_text_help_message_id = 0;
    equip_menu_reset_selection_indices();
    equip_item_build_inventory_totals();
    item_rebuild_category_list(0);
    item_rebuild_category_list(1);
    item_rebuild_category_list(2);
    item_rebuild_category_list(3);
    item_rebuild_category_list(4);
    menu_clear_selection_records();
}
