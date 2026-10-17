/* 0x800222ac: BIOS 0xa0:0x15 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"
#include "psx/libc.h"

char* strcat(char* destination, const char* source) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_STRCAT);
}
