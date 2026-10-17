/* SCUS_942.21 0x8001edec..0x8001ef2f. */
#include "psx/libcd.h"

s32 CdControlB(u8 command, u8* parameter, u8* result) {
    if (psyq_cd_issue_with_retry(command, parameter, result, 0) != 0) {
        return 0;
    }
    return CD_sync(0, result) == CdlComplete;
}
