/* 0x80022054: BIOS 0xb0:0x32 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 open(const char* path, u32 mode) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_FILE_OPEN);
}
