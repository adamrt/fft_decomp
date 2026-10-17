/* 0x80021fb4: BIOS 0xb0:0x0c tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 EnableEvent(s32 event) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_ENABLE_EVENT);
}
