/* 0x80021f14: BIOS 0xa0:0x39 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void InitHeap(u32* arena, u32 size) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_INIT_HEAP);
}
