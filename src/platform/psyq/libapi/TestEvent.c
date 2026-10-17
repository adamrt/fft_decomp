/* 0x80021fa4: BIOS 0xb0:0x0b tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 TestEvent(s32 event) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_TEST_EVENT);
}
