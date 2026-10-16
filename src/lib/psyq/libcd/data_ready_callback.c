/* SCUS_942.21 0x80020f94..0x80021023. */
#include "psx/libcd.h"
/* Stream frame completion: mark the frame's ring record complete, remember its position and frame
 * number for StGetBackloc, start the next frame at the write index and call the frame callback. */
void data_ready_callback(void) {
    StHEADER* record = &g_psyq_cd_stream_ring[g_psyq_cd_stream_frame_start_index];
    /* This status view keeps the frame load ahead of the write-index load. */
    *(s16*)&record->id = StCOMPLETE;
    g_psyq_cd_backloc.position = record->loc;
    g_psyq_cd_backloc.frame = record->frameCount;
    g_psyq_cd_stream_frame_start_index = g_psyq_cd_stream_write_index;
    if (g_psyq_cd_stream_frame_callback)
        g_psyq_cd_stream_frame_callback();
    g_psyq_cd_stream_frame_dma_pending = 0;
}
