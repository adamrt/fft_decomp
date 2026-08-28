#include "fft/main_file.h"
#include "fft/main_runtime.h"
#include "psx/cd.h"
#include "psx/gpu.h"
#include "psx/types.h"

enum {
    CdlSetloc = 0x02,
    CdlPause = 0x09,
    CdlDataReady = 0x01,
};

/* Streams CD-resident image chunks to VRAM through a retrying state machine.
 *
 * The named sector pointer, negative-offset subtraction, and in-place width
 * multiplication preserve the target's commutative operand ordering. The
 * volatile word reads prevent GCC from narrowing loads that feed s16 RECT
 * fields. The switch jump table occupies 0x80067000-0x80067020, at the start
 * of WLDCORE.BIN. */
void wldcore_file_poll_vram_image_stream(main_file_load_descriptor_t* stream) {
    RECT rect;
    u8 mode[8];
    s32 rect_count;
    s32 word;
    s32 n;
    s32 status;
    u8* sector_buffer;

    switch (stream->state) {
    case 0:
        break;
    case 1:
        mode[0] = CdlModeSpeed;
        CdControl(CdlSetmode, mode, 0);
        g_main_system_session_frames = 0;
        stream->state++;
        break;
    case 2:
        if (g_main_system_session_frames < 4) {
            return;
        }
        stream->state++;
        break;
    case 3:
        CdControlF(CdlSetloc, stream->position);
        stream->state++;
        break;
    case 4:
        status = CdSync(1, 0);
        if (status == CdlComplete) {
            stream->state++;
            break;
        }
        if (status != CdlDiskError) {
            return;
        }
        stream->state = 1;
        stream->error_count++;
        return;
    case 5:
        if (CdRead2(CdlModeSpeed) == 0) {
            stream->state = 1;
            stream->error_count++;
            return;
        }
        stream->wait_frames = 0;
        stream->sector_index = 0;
        stream->state++;
        break;
    case 6:
        status = CdReady(1, 0);
        if (status == CdlDataReady) {
            sector_buffer = stream->destination;
            sector_buffer += (stream->sector_index & 1) * 2048;
            CdGetSector(sector_buffer, 0x200);
            stream->wait_frames = 0;
            stream->state++;
            break;
        }
        if (status == CdlDiskError) {
            stream->state = 1;
            stream->error_count++;
        }
        if (stream->wait_frames > 0x40000 || (CdStatus() & 1) != 0) {
            main_file_reset_cd_subsystems();
            CdIntToPos(stream->lba + stream->sector_index, stream->position);
            stream->state = 1;
            stream->error_count++;
            return;
        }
        stream->wait_frames++;
        return;
    case 7:
        if (CdDataSync(1) != 0) {
            return;
        }
        word = (stream->sector_index & 1) * 512;
        rect_count = *(s32*)(stream->destination + (stream->sector_index & 1) * 2048);
        word++;
        for (n = 0; n < rect_count; n++) {
            rect.x = *(volatile s32*)(stream->destination + word * 4);
            rect.y = (*(s32*)(stream->destination + word * 4) & 0xFFFF0000) >> 16;
            word++;
            rect.w = *(volatile s32*)(stream->destination + word * 4);
            rect.h = (*(s32*)(stream->destination + word * 4) & 0xFFFF0000) >> 16;
            word++;
            LoadImage(&rect, (u32*)(stream->destination - -(word * 4)));
            {
                s32 width;
                s32 height;

                width = rect.w;
                height = rect.h;
                width *= height;
                word += width / 2;
            }
        }
        stream->sector_index++;
        if (stream->sector_index < stream->sector_count) {
            stream->state = 6;
            return;
        }
        stream->state = 0;
        CdControlb(CdlPause, 0, 0);
        return;
    }
}
