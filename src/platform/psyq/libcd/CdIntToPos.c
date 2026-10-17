#include "psx/libcd.h"

CdlLOC* CdIntToPos(s32 sector, CdlLOC* position) {
    s32 seconds;
    s32 minutes;
    s32 frames;
    s32 frame_bcd;
    s32 second_bcd;
    sector += PSYQ_CD_LEAD_IN_SECTORS;
    seconds = sector / PSYQ_CD_SECTORS_PER_SECOND;
    frames = sector - seconds * PSYQ_CD_SECTORS_PER_SECOND;
    frame_bcd = itob(frames);
    minutes = seconds / PSYQ_CD_SECONDS_PER_MINUTE;

    seconds -= minutes * PSYQ_CD_SECONDS_PER_MINUTE;
    second_bcd = itob(seconds);
    position->second = second_bcd;
    position->sector = frame_bcd;
    position->minute = itob(minutes);
    return position;
}
