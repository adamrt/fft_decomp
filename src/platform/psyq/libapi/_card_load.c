/* 0x80028750: BIOS 0xa0:0xac tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 _card_load(s32 port) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_CARD_LOAD);
}
