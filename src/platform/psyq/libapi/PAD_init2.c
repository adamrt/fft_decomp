/* 0x80021fe4: BIOS 0xb0:0x15 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

s32 PAD_init2(s32 type, u32* buttons) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_PAD_INIT2);
}
