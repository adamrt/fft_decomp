/* 0x80021f74: BIOS 0xb0:0x08 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 OpenEvent(u32 desc, s32 spec, s32 mode, psyq_api_event_callback_t callback) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_OPEN_EVENT);
}
