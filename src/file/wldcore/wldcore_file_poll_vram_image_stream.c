#include "fft/wldcore.h"
#include "psx/libcd.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Streams CD-resident image chunks to VRAM through a retrying state machine.
 *
 * The switch jump table occupies 0x80067000-0x80067020, at the start of
 * WLDCORE.BIN. */
void wldcore_file_poll_vram_image_stream(main_file_load_descriptor_t* stream) {
    RECT rect;
    u8 mode[8];
    s32 rect_count;
    s32 word;
    s32 n;
    s32 status;

    switch (stream->state) {
    case MAIN_FILE_LOAD_STATE_IDLE:
        break;
    case MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED:
        mode[0] = CdlModeSpeed;
        CdControl(CdlSetmode, mode, 0);
        g_main_system_session_frames = 0;
        stream->state++;
        break;
    case MAIN_FILE_LOAD_STATE_WAIT_AFTER_SET_MODE:
        if (g_main_system_session_frames < 4) {
            return;
        }
        stream->state++;
        break;
    case MAIN_FILE_LOAD_STATE_START_SEEK:
        CdControlF(CdlSetloc, (u8*)&stream->position);
        stream->state++;
        break;
    case MAIN_FILE_LOAD_STATE_POLL_SEEK:
        status = CdSync(1, 0);
        if (status == CdlComplete) {
            stream->state++;
            break;
        }
        if (status != CdlDiskError) {
            return;
        }
        stream->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
        stream->error_count++;
        return;
    case MAIN_FILE_LOAD_STATE_START_READ:
        if (CdRead2(CdlModeSpeed) == 0) {
            stream->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
            return;
        }
        stream->wait_frames = 0;
        stream->sector_index = 0;
        stream->state++;
        break;
    case MAIN_FILE_LOAD_STATE_POLL_READ:
        status = CdReady(1, 0);
        if (status == CdlDataReady) {
            CdGetSector(stream->destination + (stream->sector_index & 1) * 2048, 0x200);
            stream->wait_frames = 0;
            stream->state++;
            break;
        }
        if (status == CdlDiskError) {
            stream->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
        }
        if (stream->wait_frames > 0x40000 || (CdStatus() & 1) != 0) {
            main_file_reset_cd_subsystems();
            CdIntToPos(stream->lba + stream->sector_index, &stream->position);
            stream->state = MAIN_FILE_LOAD_STATE_SET_DOUBLE_SPEED;
            stream->error_count++;
            return;
        }
        stream->wait_frames++;
        return;
    case MAIN_FILE_LOAD_STATE_UPLOAD_VRAM:
        if (CdDataSync(1) != 0) {
            return;
        }
        /* Each sector has a count, then pairs of packed 32-bit XY/WH values and 32-bit image data. */
        word = (stream->sector_index & 1) * 512;
        rect_count = ((u32*)stream->destination)[(stream->sector_index & 1) * 512];
        word++;
        for (n = 0; n < rect_count; n++) {
            rect.x = ((u32*)stream->destination)[word];
            rect.y = (((u32*)stream->destination)[word] & 0xFFFF0000) >> 16;
            word++;
            rect.w = ((u32*)stream->destination)[word];
            rect.h = (((u32*)stream->destination)[word] & 0xFFFF0000) >> 16;
            word++;
            LoadImage(&rect, (u32*)stream->destination + word);
            word += rect.w * rect.h / 2;
        }
        stream->sector_index++;
        if (stream->sector_index < stream->sector_count) {
            stream->state = MAIN_FILE_LOAD_STATE_POLL_READ;
            return;
        }
        stream->state = MAIN_FILE_LOAD_STATE_IDLE;
        CdControlB(CdlPause, 0, 0);
        return;
    }
}
