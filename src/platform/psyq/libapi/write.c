/* 0x80022084: BIOS 0xb0:0x35 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 write(s32 fd, const void* source, s32 size) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_FILE_WRITE);
}
