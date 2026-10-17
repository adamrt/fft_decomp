/* 0x800222cc: BIOS 0xa0:0x19 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"
#include "psx/libc.h"

char* strcpy(char* destination, const char* source) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_STRCPY);
}
