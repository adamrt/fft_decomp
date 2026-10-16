/* File header building, the CD-ROM pause and the CD subsystem reset. */

#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* main_file_request_read with the Now Loading message turned off (NNL: no Now Loading); returns
 * its result. */
int main_file_request_read_quiet(main_file_load_descriptor_t* header, int sector, int sectors, void* destination) {
    return main_file_request_read(header, sector, sectors, destination, MAIN_FILE_LOADING_DISPLAY_OFF);
}

/* Initialize a descriptor and enter the asynchronous loader's setup phase. */
s32 main_file_request_read(
    main_file_load_descriptor_t* header, s32 lba, s32 sector_count, void* destination, s32 loading_display_mode) {
    RECT rect;

    if (header->state != MAIN_FILE_LOAD_STATE_IDLE) {
        return 1;
    }
    if (loading_display_mode != MAIN_FILE_LOADING_DISPLAY_PRESERVE) {
        g_main_gfx_loading_display_frame_counter = loading_display_mode;
    }
    if (loading_display_mode == MAIN_FILE_LOADING_DISPLAY_INIT) {
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
    header->_unknown_00 = 0;
    header->sector_index = 0;
    header->error_count = 0;
    header->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
    header->lba = lba;
    CdIntToPos(lba, &header->position);
    header->sector_count = sector_count;
    header->destination = destination;
    return 0;
}

/* Mark the load descriptor idle, flush pending CD commands, retry CdlPause until the drive accepts
 * it, then wait three vertical blanks. */
void main_file_reset_pause_cdrom(main_file_load_descriptor_t* state) {
    state->state = MAIN_FILE_LOAD_STATE_IDLE;
    CdFlush();
    while (CdControlB(CdlPause, 0, 0) == 0) { }
    VSync(3);
}

/* Reset the CD-ROM library (CdReset(0)); main_file_poll_load calls it before retrying a failed
 * seek or a stalled read. */
void main_file_reset_cd_subsystems(void) {
    CdReset(0);
}
