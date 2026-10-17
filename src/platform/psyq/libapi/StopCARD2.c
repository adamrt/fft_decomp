/* 0x800288b4: BIOS 0xb0:0x4c tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 StopCARD2(void) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_STOP_CARD2);
}
