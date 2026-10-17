/* 0x80026f84: BIOS 0xa0:0x49 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void GPU_cw(u32 command) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_GPU_CW);
}
