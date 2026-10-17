/* 0x80021f24: BIOS 0xa0:0x44 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void FlushCache(void) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_FLUSH_CACHE);
}
