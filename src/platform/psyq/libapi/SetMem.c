/* 0x80021f54: BIOS 0xa0:0x9f tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void SetMem(s32 mode) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_SET_MEM);
}
