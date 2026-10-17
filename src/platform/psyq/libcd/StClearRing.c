/* SCUS_942.21 0x80020edc..0x80020f3b. */
#include "psx/libcd.h"

void StClearRing(void) {
    g_psyq_cd_stream_read_index = 0;
    g_psyq_cd_stream_frame_start_index = 0;
    g_psyq_cd_stream_write_index = 0;
    g_psyq_cd_stream_frame_dma_pending = 0;
    init_ring_status(0, g_psyq_cd_stream_ring_sectors);
    g_psyq_cd_stream_interrupt_pending = 0;
    g_psyq_cd_stream_expected_sector_index = 0;
    g_psyq_cd_stream_frame_count = 0;
}
