#include "fft/battle_text.h"
#include "fft/jobstts.h"

void jobstts_menu_load_images_and_clear_selection(void) {
    RECT rect;
    u16 buf[16];

    rect.x = 0x100;
    rect.y = 0;
    rect.w = 0x40;
    rect.h = 0x30;
    jobstts_gfx_load_image_and_wait(&rect, (u32*)g_battle_menu_glyph_image);

    rect.x = 0x100;
    rect.y = 0xfa;
    rect.w = 0x40;
    rect.h = 4;
    jobstts_gfx_load_image_and_wait(&rect, (u32*)g_jobstts_menu_palette_image);

    rect.x = 0x3c0;
    rect.y = 0x1f0;
    rect.w = 0x10;
    rect.h = 1;
    jobstts_gfx_store_image_and_wait(&rect, (u32*)&buf[0]);

    buf[4] = 0;
    rect.x = 0x120;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    jobstts_gfx_load_image_and_wait(&rect, (u32*)&buf[0]);

    rect.x = 0x100;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    jobstts_gfx_load_image_and_wait(&rect, (u32*)&buf[4]);

    rect.x = 0x3c0;
    rect.y = 0x1f4;
    rect.w = 0x10;
    rect.h = 1;
    jobstts_gfx_store_image_and_wait(&rect, (u32*)&buf[0]);

    buf[4] = 0;
    rect.x = 0x130;
    rect.y = 0xfe;
    rect.w = 0x10;
    rect.h = 1;
    jobstts_gfx_load_image_and_wait(&rect, (u32*)&buf[0]);

    rect.x = 0x110;
    rect.y = 0xfe;
    rect.w = 0xc;
    rect.h = 1;
    jobstts_gfx_load_image_and_wait(&rect, (u32*)&buf[4]);

    jobstts_menu_clear_selection_values();
    g_jobstts_text_selected_job_id = 0;
}
