/* 0x800287b8: BIOS 0xb0:0x4e tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 _card_write(s32 port, s32 sector, const u8* data) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_CARD_WRITE);
}
