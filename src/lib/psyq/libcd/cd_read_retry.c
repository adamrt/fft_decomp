/* SCUS_942.21 0x80020a64..0x80020c3b. */
#include "psx/libcd.h"
#include "psx/libetc.h"
/* (Re)start CdRead's ReadN: with the lid open it only sends a Nop, restarts the timeout and
 * returns -1; a retry first pauses and seeks back to CdLastPos. Sets the read mode when it differs
 * (or on a retry), installs cb_read and starts reading into the first buffer. Returns the sector
 * count, or -1. */
s32 cd_read_retry(s32 retry) {
    u8 mode;
    u8 saved_mode;
    CdSyncCallback(0);
    CdReadyCallback(0);
    if (CdStatus() & CdlStatShellOpen) {
        if ((VSync(-1) & 0x3f) == 0)
            puts(g_psyq_cd_shell_open_message);
        CdControlF(CdlNop, 0);
        g_psyq_cd_read_state.start_clock = VSync(-1);
        g_psyq_cd_read_state.remaining = -1;
        return g_psyq_cd_read_state.remaining;
    }
    if (retry) {
        puts(g_psyq_cd_read_retry_message);
        CdControl(CdlPause, 0, 0);
        if (!CdControl(CdlSetloc, (u8*)CdLastPos(), 0)) {
            return g_psyq_cd_read_state.remaining = -1;
        }
    }
    CdFlush();
    saved_mode = g_psyq_cd_read_state.mode;
    mode = saved_mode;
    if (saved_mode != CdMode() || retry) {
        if (!CdControl(CdlSetmode, &mode, 0)) {
            g_psyq_cd_read_state.remaining = -1;
            return g_psyq_cd_read_state.remaining;
        }
    }
    g_psyq_cd_read_state.expected_sector = CdPosToInt(CdLastPos());
    CdReadyCallback(cb_read);
    g_psyq_cd_read_state.next_buffer = g_psyq_cd_read_state.first_buffer;
    CdControlF(CdlReadN, 0);
    g_psyq_cd_read_state.remaining = g_psyq_cd_read_state.sectors;
    g_psyq_cd_read_state.sector_clock = VSync(-1);
    return g_psyq_cd_read_state.remaining;
}
