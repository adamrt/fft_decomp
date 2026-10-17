/* 0x80021fc4: BIOS 0xb0:0x0d tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 DisableEvent(s32 event) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_DISABLE_EVENT);
}
