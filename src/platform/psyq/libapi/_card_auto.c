/* 0x80028760: BIOS 0xa0:0xad tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 _card_auto(s32 mode) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_CARD_AUTO);
}
