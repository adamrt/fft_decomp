/* 0x80022074: BIOS 0xb0:0x34 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 read(s32 fd, void* destination, s32 size) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_FILE_READ);
}
