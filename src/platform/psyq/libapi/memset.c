/* 0x800222fc: BIOS 0xa0:0x2b tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"
#include "psx/libc.h"

void* memset(void* destination, u8 value, int size) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_MEMSET);
}
