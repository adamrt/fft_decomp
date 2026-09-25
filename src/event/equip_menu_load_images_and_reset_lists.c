#include "fft/event_equip.h"
#include "psx/types.h"

void equip_menu_load_images_and_reset_lists(void) {
    RECT rect;
    u16 buf1[4];
    u16 buf2[12];

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    equip_gfx_load_image_and_wait(&rect, (u32*)g_battle_menu_glyph_image);
    rect.x = 0x100;
    rect.y = 0xFA;
    rect.w = 0x40;
    rect.h = 4;
    equip_gfx_load_image_and_wait(&rect, (u32*)g_equip_menu_palette_image);
    rect.x = 0x3C0;
    rect.y = 0x1F0;
    rect.w = 0x10;
    rect.h = 1;
    equip_gfx_store_image_and_wait(&rect, (u32*)buf1);
    buf2[0] = 0;
    rect.x = 0x120;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    equip_gfx_load_image_and_wait(&rect, (u32*)buf1);
    rect.x = 0x100;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    equip_gfx_load_image_and_wait(&rect, (u32*)buf2);
    rect.x = 0x3C0;
    rect.y = 0x1F4;
    rect.w = 0x10;
    rect.h = 1;
    equip_gfx_store_image_and_wait(&rect, (u32*)buf1);
    buf2[0] = 0;
    rect.x = 0x130;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    equip_gfx_load_image_and_wait(&rect, (u32*)buf1);
    rect.x = 0x110;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    equip_gfx_load_image_and_wait(&rect, (u32*)buf2);
    equip_menu_clear_selection_values();
    g_equip_text_help_message_id = 0;
    equip_menu_reset_selection_indices();
    equip_item_build_inventory_totals();
    equip_item_rebuild_category_list(0);
    equip_item_rebuild_category_list(1);
    equip_item_rebuild_category_list(2);
    equip_item_rebuild_category_list(3);
    equip_item_rebuild_category_list(4);
    equip_menu_clear_selection_records();
}
