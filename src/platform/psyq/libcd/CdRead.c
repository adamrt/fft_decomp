/* SCUS_942.21 0x80020c3c..0x80020d43. */
#include "psx/libcd.h"
#include "psx/libetc.h"
s32 CdRead(s32 sectors, u32* buffer, s32 mode) {
    g_psyq_cd_read_state.mode = mode;
    switch (g_psyq_cd_read_state.mode & (CdlModeSize0 | CdlModeSize1)) {
    case 0:
        g_psyq_cd_read_state.words = PSYQ_CD_DATA_WORDS;
        break;
    case CdlModeSize1:
        g_psyq_cd_read_state.words = PSYQ_CD_SECTOR_2340_WORDS;
        break;
    default:
        g_psyq_cd_read_state.words = PSYQ_CD_SECTOR_2328_WORDS;
        break;
    }
    g_psyq_cd_read_state.mode |= CdlModeSize1;
    g_psyq_cd_read_state.first_buffer = buffer;
    g_psyq_cd_read_state.sectors = sectors;
    g_psyq_cd_read_state.sync_callback = CdSyncCallback(0);
    g_psyq_cd_read_state.ready_callback = CdReadyCallback(0);
    g_psyq_cd_read_state.start_clock = VSync(-1);
    if (CdStatus() & (CdlStatPlay | CdlStatSeek | CdlStatRead))
        CdControlB(CdlPause, 0, 0);
    return cd_read_retry(0) > 0;
}
