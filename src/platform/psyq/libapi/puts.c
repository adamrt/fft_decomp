/* 0x8002228c: BIOS 0xb0:0x3f tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"
#include "psx/libc.h"

void puts(const char* text) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_PUTS);
}
