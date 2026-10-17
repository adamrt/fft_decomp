/* SCUS_942.21 0x8001e894..0x8001e8c3. */
#include "psx/libcd.h"

void StSetRing(u32* buffer, u32 sectors) {
    g_psyq_cd_stream_ring = (StHEADER*)buffer;
    g_psyq_cd_stream_ring_sectors = sectors;
    StClearRing();
}
