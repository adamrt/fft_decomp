/* 0x800222dc: BIOS 0xa0:0x27 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"
#include "psx/libc.h"

void* bcopy(const void* source, void* destination, int size) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_BCOPY);
}
