/* 0x800220e4: BIOS 0xb0:0x55 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 _get_error(s32 fd) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_FILE_GET_ERROR);
}
