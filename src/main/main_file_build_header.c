#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "psx/cd.h"

/* Initialize a descriptor and enter the asynchronous loader's setup phase. */
s32 main_file_build_header(
    main_file_load_descriptor_t* header, s32 lba, s32 sector_count, void* destination, s32 loading_display_mode) {
    RECT rect;

    if (header->state != MAIN_FILE_LOAD_STATE_IDLE) {
        return 1;
    }
    if (loading_display_mode != 1) {
        g_main_gfx_loading_display_frame_counter = loading_display_mode;
    }
    if (loading_display_mode == 0) {
        SetDispMask(0);
        main_gfx_build_now_loading(1, 0, 0);
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x100;
        rect.h = 0x1e0;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        SetDefDrawEnv(&g_main_gfx_draw_envs[0], 0, 0, 0x100, 0xf0);
        SetDefDispEnv(&g_main_gfx_display_envs[0], 0, 0xf0, 0x100, 0xf0);
        SetDefDrawEnv(&g_main_gfx_draw_envs[1], 0, 0xf0, 0x100, 0xf0);
        SetDefDispEnv(&g_main_gfx_display_envs[1], 0, 0, 0x100, 0xf0);
        g_main_gfx_draw_envs[1].ofs[1] = 0xf0;
        g_main_gfx_draw_envs[1].ofs[0] = 0;
        g_main_gfx_draw_envs[0].ofs[1] = 0;
        g_main_gfx_draw_envs[0].ofs[0] = 0;
        g_main_gfx_draw_envs[1].dfe = 1;
        g_main_gfx_draw_envs[1].isbg = 1;
        g_main_gfx_draw_envs[0].dfe = 1;
        g_main_gfx_draw_envs[0].isbg = 1;
        PutDrawEnv(&g_main_gfx_draw_envs[1]);
        PutDispEnv(&g_main_gfx_display_envs[1]);
    }
    header->unknown_00 = 0;
    header->sector_index = 0;
    header->error_count = 0;
    header->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
    header->lba = lba;
    CdIntToPos(lba, header->position);
    header->sector_count = sector_count;
    header->destination = destination;
    return 0;
}
