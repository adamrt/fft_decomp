#include "fft/main.h"
#include "psx/cd.h"

/* Advance one asynchronous CD read and keep the loading indicator current.
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
        CdControlF(CdlSeekL, header->position);
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
