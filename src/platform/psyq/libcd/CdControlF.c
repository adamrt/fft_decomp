/* SCUS_942.21 0x8001ecc0..0x8001edeb. */
#include "psx/libcd.h"

s32 CdControlF(u8 command, u8* parameter) {
    return psyq_cd_issue_with_retry(command, parameter, 0, 1) == 0;
}
