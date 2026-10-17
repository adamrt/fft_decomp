/* SCUS_942.21 0x80020e28..0x80020ebb. */
#include "psx/libcd.h"

s32 CdRead2(s32 mode) {
    u8 device_mode = mode;
    CdControl(CdlSetmode, &device_mode, 0);
    if (mode & CdlModeStream) {
        if (mode & CdlModeSize1)
            g_psyq_cd_stream_skip_sector_position = 0;
        else
            g_psyq_cd_stream_skip_sector_position = 1;
        CdDataCallback(data_ready_callback);
        CdReadyCallback(psyq_cd_stream_ready_callback);
    }
    return CdControl(CdlReadS, 0, 0);
}
