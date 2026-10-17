/* 0x80028740: BIOS 0xa0:0xab tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 _card_info(s32 port) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_CARD_INFO);
}
