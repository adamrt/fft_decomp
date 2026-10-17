/* SCUS_942.21 0x8002110c..0x800211c7. */
#include "psx/libcd.h"
u32 StFreeRing(u32* frame_data) {
    s32 ring_sectors = g_psyq_cd_stream_ring_sectors;
    StHEADER* ring = g_psyq_cd_stream_ring;
    /* Integer address sums preserve operand order; typed pointer additions swap it. */
    s32 first_sector = (frame_data - (u32*)((ring_sectors * sizeof(*ring)) + (u32)ring)) / PSYQ_CD_STREAM_PAYLOAD_WORDS;
    StHEADER* record = (StHEADER*)((first_sector * sizeof(*ring)) + (u32)ring);
    s16 frame_sectors = record->nSectors;
    s32 sector_index;
    s16 status = record->id;
    if (status != StLOCK)
        return 1;
    /* Signed status stores keep the ring-base reload in each iteration. */
    for (sector_index = 0; sector_index < frame_sectors; sector_index++) {
        *(s16*)&g_psyq_cd_stream_ring[sector_index + first_sector].id = StFREE;
    }
    g_psyq_cd_stream_read_index = sector_index + first_sector;
    return 0;
}
