/* 0x80021f34: BIOS 0xa0:0x70 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void _bu_init(void) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_BU_INIT);
}
