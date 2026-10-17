/* 0x80021f44: BIOS 0xa0:0x72 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void _96_remove(void) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_96_REMOVE);
}
