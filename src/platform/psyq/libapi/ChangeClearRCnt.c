/* 0x80022104: BIOS 0xc0:0x0a tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 ChangeClearRCnt(s32 counter, s32 mode) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_C, PSYQ_BIOS_C_CHANGE_CLEAR_RCNT);
}
