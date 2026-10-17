/* 0x80022064: BIOS 0xb0:0x33 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 lseek(s32 fd, s32 offset, s32 origin) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_FILE_SEEK);
}
