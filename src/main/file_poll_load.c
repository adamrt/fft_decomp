/* Advancing an asynchronous CD read and keeping the loading indicator current. */

#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_gfx.h"
#include "psx/libcd.h"
#include "psx/types.h"

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
void main_file_poll_load(main_file_load_descriptor_t* header) {
    u8 mode;
    s32 result;

    switch (header->state) {
    case MAIN_FILE_LOAD_STATE_IDLE:
        break;
    case MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED:
        mode = CdlModeSpeed;
        CdControl(CdlSetmode, &mode, 0);
        g_main_system_session_frames = 0;
        header->state++;
        break;
    case MAIN_FILE_LOAD_STATE_WAIT_AFTER_SET_MODE:
        if (g_main_system_session_frames >= 4) {
            header->state++;
        }
        break;
    case MAIN_FILE_LOAD_STATE_START_SEEK:
        CdControlF(CdlSeekL, (u8*)&header->position);
        header->state++;
        break;
    case MAIN_FILE_LOAD_STATE_POLL_SEEK:
        result = CdSync(1, 0);
        if (result == CdlComplete) {
            header->state++;
            if (header->destination == 0) {
                header->state = MAIN_FILE_LOAD_STATE_IDLE;
            }
        } else if (result == CdlDiskError) {
            main_file_reset_cd_subsystems();
            header->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            header->error_count = header->error_count + 1;
        }
        break;
    case MAIN_FILE_LOAD_STATE_START_READ:
        if (CdRead(header->sector_count, (u32*)header->destination, CdlModeSpeed) == 0) {
            header->error_count = header->error_count + 1;
        } else {
            g_main_system_session_frames = 0;
            header->wait_frames = 0;
            header->state++;
        }
        break;
    case MAIN_FILE_LOAD_STATE_POLL_READ:
        result = CdReadSync(1, 0);
        if (result == 0) {
            header->state = MAIN_FILE_LOAD_STATE_IDLE;
        } else if (result == header->sector_count || result == -1) {
            if (g_main_system_session_frames > 0x100) {
                g_main_system_session_frames = 0;
                header->wait_frames = 0;
                main_file_reset_cd_subsystems();
                header->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
                header->error_count = header->error_count + 1;
            } else {
                header->wait_frames = header->wait_frames + 1;
            }
        } else {
            header->wait_frames = 0;
        }
        break;
    }
    if (header->state != MAIN_FILE_LOAD_STATE_IDLE) {
        main_gfx_draw_now_loading_message();
    }
}
