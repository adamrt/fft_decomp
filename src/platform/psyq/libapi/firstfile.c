/* 0x800220b4: BIOS 0xb0:0x42 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

DIRENTRY* firstfile(char* pattern, DIRENTRY* entry) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_FIRSTFILE);
}
