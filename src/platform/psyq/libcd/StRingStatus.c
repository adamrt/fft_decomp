/* SCUS_942.21 0x80021e4c..0x80021f13. */
#include "psx/libcd.h"

void StRingStatus(s16* free_sectors, s16* read_sectors) {
    s32 index = g_psyq_cd_stream_ring_sectors;
    StHEADER* record;
    *free_sectors = 0;
    *read_sectors = g_psyq_cd_stream_write_index - g_psyq_cd_stream_read_index;
    if (*read_sectors < 0) {
        for (index--; index >= 0; index--) {
            record = &g_psyq_cd_stream_ring[index];
            if (record->id == StREWIND)
                break;
        }
        index++;
        *read_sectors += index;
    }
    for (index--; index >= 0; index--) {
        record = &g_psyq_cd_stream_ring[index];
        if (record->id == StFREE) {
            (*free_sectors)++;
        }
    }
}
