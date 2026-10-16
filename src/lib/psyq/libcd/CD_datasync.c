/* SCUS_942.21 0x800204e4..0x8002064f. */
#include "psx/libcd.h"

/* Wait (mode 0) or poll for the CD-ROM DMA channel to go idle: 0 when idle, 1 while busy when
 * polling, -1 on timeout. */
s32 CD_datasync(s32 mode) {
    s32 result;
    psyq_cd_set_timeout(g_psyq_cd_data_sync_timeout_name);
    while (1) {
        if (psyq_cd_check_timeout()) {
            result = -1;
            break;
        }
        if (!(*g_psyq_cd_dma_control_register & 0x01000000)) {
            result = 0;
            break;
        }
        if (mode) {
            result = 1;
            break;
        }
    }
    return result;
}
