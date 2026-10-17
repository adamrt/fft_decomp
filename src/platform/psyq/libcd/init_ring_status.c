/* SCUS_942.21 0x800211c8..0x80021207. */
#include "psx/libcd.h"

/* Retail declares an int result and never sets it. */
s32 init_ring_status(u32 first_sector, u32 sector_count) {
    u32 sector_index;
    for (sector_index = 0; sector_index < sector_count; sector_index++) {
        /* The 32-bit store clears both id and type. */
        *(u32*)&g_psyq_cd_stream_ring[sector_index + first_sector].id = 0;
    }
}
