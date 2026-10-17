/* 0x8002229c: BIOS 0xa0:0x13 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"
#include "psx/libc.h"

s32 setjmp(jmp_buf jump_buffer) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_SETJMP);
}
