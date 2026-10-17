/* 0x80028770: BIOS 0xb0:0x5c tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 _card_status(s32 slot) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_CARD_STATUS);
}
