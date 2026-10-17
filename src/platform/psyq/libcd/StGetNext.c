/* SCUS_942.21 0x80021208..0x800212cb. */
#include "psx/libcd.h"
u32 StGetNext(u32** frame_data, u32** header) {
    StHEADER* record = &g_psyq_cd_stream_ring[g_psyq_cd_stream_read_index];
    if (*(volatile u16*)&record->id == StREWIND) {
        g_psyq_cd_stream_read_index = 0;
        if (g_psyq_cd_stream_end_frame)
            record->id = StFREE;
        record = &g_psyq_cd_stream_ring[g_psyq_cd_stream_read_index];
    }
    /* CD callbacks can change the frame status between these two reads. */
    if (*(volatile u16*)&record->id != StCOMPLETE)
        return 1;
    /* The addressed 16-bit field keeps the lock store before the payload address calculation. */
    *&record->id = StLOCK;
    *frame_data = (u32*)(g_psyq_cd_stream_ring + g_psyq_cd_stream_ring_sectors)
        + g_psyq_cd_stream_read_index * PSYQ_CD_STREAM_PAYLOAD_WORDS;
    *header = (u32*)record;
    return 0;
}
