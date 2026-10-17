#include "psx/libcd.h"
s32 CdPosToInt(CdlLOC* position) {
    u32 minute_bcd = position->minute;
    u32 second_bcd = position->second;
    u32 sector_bcd;
    u32 elapsed;
    u32 frames;
    elapsed = btoi(minute_bcd) * PSYQ_CD_SECONDS_PER_MINUTE;
    elapsed += btoi(second_bcd);
    frames = elapsed * PSYQ_CD_SECTORS_PER_SECOND;
    sector_bcd = position->sector;
    return frames + btoi(sector_bcd) - PSYQ_CD_LEAD_IN_SECTORS;
}
