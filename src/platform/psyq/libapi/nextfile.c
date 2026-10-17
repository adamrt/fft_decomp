/* 0x800220c4: BIOS 0xb0:0x43 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

DIRENTRY* nextfile(DIRENTRY* entry) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_NEXTFILE);
}
