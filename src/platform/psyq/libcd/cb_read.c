/* SCUS_942.21 0x80020840..0x80020a63. */
#include "psx/libcd.h"
#include "psx/libetc.h"
/* CdRead's data-ready handler: copy each sector into the next buffer slot (for 512-word reads
 * first checking the header position against the expected sector), restart the read from the
 * beginning on an error, and once all sectors are in or the timeout passes restore the caller's
 * callbacks, pause, and report CdlComplete or CdlDiskError to the CdRead callback. */
void cb_read(u8 event, u8* result) {
    u32 sector_header[3];
    if (event == CdlDataReady) {
        if (g_psyq_cd_read_state.remaining > 0) {
            if (g_psyq_cd_read_state.words == PSYQ_CD_DATA_WORDS) {
                CdGetSector(sector_header, 3);
                if (CdPosToInt((CdlLOC*)sector_header) != g_psyq_cd_read_state.expected_sector) {
                    puts(g_psyq_cd_sector_error_message);
                    g_psyq_cd_read_state.remaining = -1;
                }
            }
            CdGetSector(g_psyq_cd_read_state.next_buffer, g_psyq_cd_read_state.words);
            g_psyq_cd_read_state.next_buffer += g_psyq_cd_read_state.words;
            g_psyq_cd_read_state.remaining--;
            /* The volatile view preserves the native unused reload after the increment. */
            g_psyq_cd_read_state.expected_sector++;
        }
    } else {
        g_psyq_cd_read_state.remaining = -1;
    }
    g_psyq_cd_read_state.sector_clock = VSync(-1);
    if (g_psyq_cd_read_state.remaining < 0)
        cd_read_retry(1);
    if (VSync(-1) > g_psyq_cd_read_state.start_clock + PSYQ_CD_READ_TIMEOUT_VBLANKS)
        g_psyq_cd_read_state.remaining = -1;
    if (g_psyq_cd_read_state.remaining && VSync(-1) <= g_psyq_cd_read_state.start_clock + PSYQ_CD_READ_TIMEOUT_VBLANKS)
        return;
    CdSyncCallback(g_psyq_cd_read_state.sync_callback);
    CdReadyCallback(g_psyq_cd_read_state.ready_callback);
    CdControl(CdlPause, 0, 0);
    if (g_psyq_cd_read_callback)
        g_psyq_cd_read_callback(g_psyq_cd_read_state.remaining == 0 ? CdlComplete : CdlDiskError, result);
}
