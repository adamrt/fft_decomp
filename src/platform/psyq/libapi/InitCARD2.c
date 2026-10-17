/* 0x80028894: BIOS 0xb0:0x4a tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void InitCARD2(s32 pad_enable) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_INIT_CARD2);
}
