#include "fft/battle_text.h"
#include "fft/bunit.h"
#include "psx/types.h"

typedef struct {
    u8 data[0x200];
} bunit_block_200_t;

extern bunit_block_200_t bunit_D_801BF000;

void bunit_gfx_init_vram_and_start_fade(void) {
    RECT rect;
    u16 buf1[4];
    u16 buf2[12];
    bunit_block_200_t block = bunit_D_801BF000;

    bunit_menu_set_event_speed(0);
    g_battle_text_section_pointers[0] = g_bunit_text_menu_section;
    g_battle_text_section_pointers[25] = g_bunit_text_reorder_list_buffer;
    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    bunit_gfx_load_image_and_wait(&rect, (u32*)g_battle_menu_glyph_image);
    rect.x = 0x3C0;
    rect.y = 0x1F0;
    rect.w = 0x10;
    rect.h = 1;
    bunit_gfx_store_image_and_wait(&rect, (u32*)buf1);
    buf2[0] = 0;
    rect.x = 0x120;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    bunit_gfx_load_image_and_wait(&rect, (u32*)buf1);
    rect.x = 0x100;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    bunit_gfx_load_image_and_wait(&rect, (u32*)buf2);
    rect.x = 0x3C0;
    rect.y = 0x1F4;
    rect.w = 0x10;
    rect.h = 1;
    bunit_gfx_store_image_and_wait(&rect, (u32*)buf1);
    buf2[0] = 0;
    rect.x = 0x130;
    rect.y = 0xFE;
    rect.w = 0x10;
    rect.h = 1;
    bunit_gfx_load_image_and_wait(&rect, (u32*)buf1);
    rect.x = 0x110;
    rect.y = 0xFE;
    rect.w = 0xC;
    rect.h = 1;
    bunit_gfx_load_image_and_wait(&rect, (u32*)buf2);
    rect.x = 0x100;
    rect.y = 0xFA;
    rect.w = 0x40;
    rect.h = 4;
    bunit_gfx_load_image_and_wait(&rect, (u32*)&block);
    g_bunit_text_selection_id = 0;
    g_bunit_help_screen_id = 0;
    bunit_gfx_start_decreasing_fade();
}
