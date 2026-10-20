/* Starting an asynchronous CD read on a load descriptor, pausing the drive and resetting the CD library. */

#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/system.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* MAIN keeps the shared read descriptor across overlay loads. */
file_load_descriptor_t g_file_cd_state = { 0 };

/* file_read_request with the Now Loading message turned off (NNL: no Now Loading); returns
 * its result. */
s32 file_read_request_quiet(file_load_descriptor_t* header, s32 sector, s32 sectors, void* destination) {
    return file_read_request(header, sector, sectors, destination, FILE_LOADING_DISPLAY_OFF);
}

/* Queue an asynchronous read of sector_count sectors from lba into destination
 * on an idle descriptor; file_load_poll then carries it out. Returns 1
 * without queuing when the descriptor is busy, else 0.
 *
 * loading_display_mode other than PRESERVE restarts the Now Loading counter
 * (OFF, -1, keeps the message hidden). INIT also blanks the display, builds
 * the message, clears both 256x240 buffers of VRAM and sets up a plain
 * double-buffered 256x240 display for it. */
s32 file_read_request(file_load_descriptor_t* header, s32 lba, s32 sector_count, void* destination,
    file_loading_display_mode_e loading_display_mode) {
    RECT rect;

    if (header->state != FILE_LOAD_STATE_IDLE) {
        return 1;
    }
    if (loading_display_mode != FILE_LOADING_DISPLAY_PRESERVE) {
        g_loading_display_frame_counter = loading_display_mode;
    }
    if (loading_display_mode == FILE_LOADING_DISPLAY_INIT) {
        SetDispMask(0);
        loading_message_build(1, 0, 0);
        rect.x = 0;
        rect.y = 0;
        rect.w = 0x100;
        rect.h = 0x1e0;
        ClearImage(&rect, 0, 0, 0);
        DrawSync(0);
        SetDefDrawEnv(&g_gfx_draw_envs[0], 0, 0, 0x100, 0xf0);
        SetDefDispEnv(&g_gfx_display_envs[0], 0, 0xf0, 0x100, 0xf0);
        SetDefDrawEnv(&g_gfx_draw_envs[1], 0, 0xf0, 0x100, 0xf0);
        SetDefDispEnv(&g_gfx_display_envs[1], 0, 0, 0x100, 0xf0);
        g_gfx_draw_envs[1].ofs[1] = 0xf0;
        g_gfx_draw_envs[1].ofs[0] = 0;
        g_gfx_draw_envs[0].ofs[1] = 0;
        g_gfx_draw_envs[0].ofs[0] = 0;
        g_gfx_draw_envs[1].dfe = 1;
        g_gfx_draw_envs[1].isbg = 1;
        g_gfx_draw_envs[0].dfe = 1;
        g_gfx_draw_envs[0].isbg = 1;
        PutDrawEnv(&g_gfx_draw_envs[1]);
        PutDispEnv(&g_gfx_display_envs[1]);
    }
    header->_unknown_00 = 0;
    header->sector_index = 0;
    header->error_count = 0;
    header->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
    header->lba = lba;
    CdIntToPos(lba, &header->position);
    header->sector_count = sector_count;
    header->destination = destination;
    return 0;
}

/* Mark the load descriptor idle, flush pending CD commands, retry CdlPause until the drive accepts
 * it, then wait three vertical blanks. */
void file_cd_pause_reset(file_load_descriptor_t* state) {
    state->state = FILE_LOAD_STATE_IDLE;
    CdFlush();
    while (CdControlB(CdlPause, 0, 0) == 0) { }
    VSync(3);
}

/* Reset the CD-ROM library (CdReset(0)); file_load_poll calls it before retrying a failed
 * seek or a stalled read. */
void file_cd_subsystems_reset(void) {
    CdReset(0);
}
