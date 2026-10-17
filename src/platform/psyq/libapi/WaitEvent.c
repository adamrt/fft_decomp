/* 0x80021f94: BIOS 0xb0:0x0a tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 WaitEvent(s32 event) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_WAIT_EVENT);
}
