/* 0x800287c8: BIOS 0xb0:0x50 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void _new_card(void) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_NEW_CARD);
}
