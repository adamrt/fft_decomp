#include "fft/main.h"

/* Low immediate byte of FntLoad's CLUT-Y addiu instruction. */
extern u8 g_psyq_gpu_font_clut_y_offset_immediate;

void main_gfx_load_efc_fnt(void) {
    TIM_IMAGE image;
    u32* data;
    int loaded = 0;

    g_psyq_gpu_font_clut_y_offset_immediate = 0x7f;
    FntLoad(0x280, 0);

    while (!loaded) {
        data = main_file_load_checked_to_heap(DISC_SECTOR_BATTLE_EFC_FNT_TIM, 0x8800);
        if (data != 0) {
            if (OpenTIM(data) == 0 && ReadTIM(&image) != 0) {
                LoadTPage(image.paddr, 0, 0, 0x280, 0, image.prect->w * 4, image.prect->h);
                loaded = 1;
                LoadClut2(image.caddr, 0x280, 0x7f);
            }
            main_heap_call_free(data);
        } else {
            VSync(0);
            main_file_poll_load(&g_main_file_cd_state);
        }
    }

    SetDumpFnt(FntOpen(0x88, 0x10, 0x100, 0x100, 0, 0x200));
    DrawSync(0);
    DrawSync(0);
}
