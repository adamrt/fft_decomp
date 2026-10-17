/* 0x80022024: BIOS 0xb0:0x19 tail veneer. */
#include "psx/abi_inline.h"
#include "psx/libapi.h"

void HookEntryInt(jmp_buf jump_buffer) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_B, PSYQ_BIOS_B_HOOK_ENTRY_INT);
}
