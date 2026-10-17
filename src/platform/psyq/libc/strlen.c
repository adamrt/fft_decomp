#include "psx/abi_inline.h"
#include "psx/libc.h"
int strlen(const char* source) {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_STRLEN);
}
