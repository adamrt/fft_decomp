/* 0x800222bc: BIOS 0xa0:0x17 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"
#include "psx/libc.h"

s32 strcmp(const char* left, const char* right) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_STRCMP);
}
