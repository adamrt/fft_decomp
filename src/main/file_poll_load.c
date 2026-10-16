/* Advancing an asynchronous CD read and keeping the loading indicator current. */

#include "fft/file.h"
#include "fft/system.h"
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
void file_poll_load(file_load_descriptor_t* header) {
    u8 mode;
    s32 result;

    switch (header->state) {
    case FILE_LOAD_STATE_IDLE:
        break;
    case FILE_LOAD_STATE_SET_DOUBLE_SPEED:
        mode = CdlModeSpeed;
        CdControl(CdlSetmode, &mode, 0);
        g_system_session_frames = 0;
        header->state++;
        break;
    case FILE_LOAD_STATE_WAIT_AFTER_SET_MODE:
        if (g_system_session_frames >= 4) {
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
            file_reset_cd_subsystems();
            header->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            header->error_count = header->error_count + 1;
        }
        break;
    case FILE_LOAD_STATE_START_READ:
        if (CdRead(header->sector_count, (u32*)header->destination, CdlModeSpeed) == 0) {
            header->error_count = header->error_count + 1;
        } else {
            g_system_session_frames = 0;
            header->wait_frames = 0;
            header->state++;
        }
        break;
    case FILE_LOAD_STATE_POLL_READ:
        result = CdReadSync(1, 0);
        if (result == 0) {
            header->state = FILE_LOAD_STATE_IDLE;
        } else if (result == header->sector_count || result == -1) {
            if (g_system_session_frames > 0x100) {
                g_system_session_frames = 0;
                header->wait_frames = 0;
                file_reset_cd_subsystems();
                header->state = FILE_LOAD_STATE_SET_DOUBLE_SPEED;
                header->error_count = header->error_count + 1;
            } else {
                header->wait_frames = header->wait_frames + 1;
            }
        } else {
            header->wait_frames = 0;
        }
        break;
    }
    if (header->state != FILE_LOAD_STATE_IDLE) {
        loading_draw_message();
    }
}
