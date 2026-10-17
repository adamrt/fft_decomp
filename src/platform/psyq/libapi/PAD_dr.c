/* 0x80021ff4: BIOS 0xb0:0x16 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void PAD_dr(void) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_PAD_DR);
}
