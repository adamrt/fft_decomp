/* 0x800220a4: BIOS 0xb0:0x41 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 format(const char* path) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_FILE_FORMAT);
}
