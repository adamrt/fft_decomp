/* SCUS_942.21 0x8001eb88..0x8001ecbf. */
#include "psx/libcd.h"

s32 CdControl(u8 command, u8* parameter, u8* result) {
    return psyq_cd_issue_with_retry(command, parameter, result, 0) == 0;
}
