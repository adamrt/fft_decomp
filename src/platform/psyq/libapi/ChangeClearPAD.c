/* 0x800220f4: BIOS 0xb0:0x5b tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void ChangeClearPAD(s32 mode) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_CHANGE_CLEAR_PAD);
}
