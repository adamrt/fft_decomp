/* Asynchronous CD file access in the resident executable: starting a read on a load descriptor,
 * pausing the drive and resetting the CD library, advancing a read, and blinking the Now Loading
 * message while it is unfinished. Only the older ASPSX assembler reproduces it: one object between
 * the hand-written stack helpers and the Suzuki sound library. */

#include "fft/gfx/gfx.h"
#include "fft/sys/file.h"
#include "fft/sys/system.h"
#include "psx/libcd.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

static void draw_loading_message(void);

/* read requests */

/* MAIN keeps the shared read descriptor across overlay loads. */
file_load_descriptor_t g_file_cd_state = { 0 };

/* file_request_read with the Now Loading message turned off (NNL: no Now Loading); returns
 * its result. */
s32 file_request_read_quiet(file_load_descriptor_t* header, s32 sector, s32 sectors, void* destination) {
    return file_request_read(header, sector, sectors, destination, FILE_LOADING_DISPLAY_OFF);
}

/* Queue an asynchronous read of sector_count sectors from lba into destination
 * on an idle descriptor; file_poll_load then carries it out. Returns 1
 * without queuing when the descriptor is busy, else 0.
 *
 * loading_display_mode other than PRESERVE restarts the Now Loading counter
 * (OFF, -1, keeps the message hidden). INIT also blanks the display, builds
 * the message, clears both 256x240 buffers of VRAM and sets up a plain
 * double-buffered 256x240 display for it. */
s32 file_request_read(file_load_descriptor_t* header, s32 lba, s32 sector_count, void* destination,
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
        rect.w = 256;
        rect.h = 480;
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
    header->_unread_00 = 0;
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
void file_cd_pause(file_load_descriptor_t* state) {
    state->state = FILE_LOAD_STATE_IDLE;
    CdFlush();
    while (CdControlB(CdlPause, 0, 0) == 0) { }
    VSync(3);
}

/* Reset the CD-ROM library (CdReset(0)); file_poll_load calls it before retrying a failed
 * seek or a stalled read. */
void file_cd_reset(void) {
    CdReset(0);
}

/* read progress */

/* Advance one asynchronous CD read and keep the loading indicator current.
 *
 * The read sets double speed, waits four frames, seeks, then reads. A failed
 * seek, or a read that has delivered no sector (or reports an error) more than
 * 256 frames after it started, resets the CD library and starts again from the
 * speed change, counting an error; the retries never give up. A descriptor
 * without a destination ends after the seek.
 *
 * Sequential transitions use ++ because explicit enum assignments replace the
 * target's increment instructions and do not match. */
void file_poll_load(file_load_descriptor_t* header) {
    u8 mode;
    s32 result;

    switch (header->state) {
    case FILE_LOAD_STATE_IDLE:
        break;
    case FILE_LOAD_STATE_SET_DOUBLE_SPEED:
        mode = CdlModeSpeed;
        CdControl(CdlSetmode, &mode, 0);
        g_system_wait_frames = 0;
        header->state++;
        break;
    case FILE_LOAD_STATE_WAIT_AFTER_SET_MODE:
        if (g_system_wait_frames >= 4) {
            header->state++;
        }
        break;
    case FILE_LOAD_STATE_START_SEEK:
        CdControlF(CdlSeekL, (u8*)&header->position);
        header->state++;
        break;
    case FILE_LOAD_STATE_POLL_SEEK:
        result = CdSync(1, 0);
        if (result == CdlComplete) {
            header->state++;
            if (header->destination == 0) {
                header->state = FILE_LOAD_STATE_IDLE;
            }
        } else if (result == CdlDiskError) {
            file_cd_reset();
            header->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            header->error_count++;
        }
        break;
    case FILE_LOAD_STATE_START_READ:
        if (CdRead(header->sector_count, header->destination, CdlModeSpeed) == 0) {
            header->error_count++;
        } else {
            g_system_wait_frames = 0;
            header->wait_frames = 0;
            header->state++;
        }
        break;
    case FILE_LOAD_STATE_POLL_READ:
        result = CdReadSync(1, 0);
        if (result == 0) {
            header->state = FILE_LOAD_STATE_IDLE;
        } else if (result == header->sector_count || result == -1) {
            if (g_system_wait_frames > 0x100) {
                g_system_wait_frames = 0;
                header->wait_frames = 0;
                file_cd_reset();
                header->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
                header->error_count++;
            } else {
                header->wait_frames++;
            }
        } else {
            header->wait_frames = 0;
        }
        break;
    }
    if (header->state != FILE_LOAD_STATE_IDLE) {
        draw_loading_message();
    }
}

/* loading indicator */

extern u32 g_loading_otags[2][2];

/* Called by file_poll_load on every poll of an unfinished read. Unless the
 * counter is -1 (message off), the 64th call unmasks the display and from then on
 * every call draws the blinking message on its own, alternating the two display
 * buffers; the counter stays at 0x40 or 0x41. */
static void draw_loading_message(void) {
    s32 frame = g_loading_display_frame_counter;
    u32* otag;

    if (frame != FILE_LOADING_DISPLAY_OFF) {
        frame++;
        g_loading_display_frame_counter = frame;
        if (frame >= 0x40) {
            frame &= 1;
            PutDrawEnv(&g_gfx_draw_envs[frame]);
            PutDispEnv(&g_gfx_display_envs[frame]);
            otag = g_loading_otags[frame];
            ClearOTag(otag, 2);
            loading_message_append_to_otag(otag);
            DrawOTag(otag);
            SetDispMask(1);
            g_loading_display_frame_counter &= 0x41;
        }
    }
}
