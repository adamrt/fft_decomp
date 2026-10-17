#include "psx/abi_inline.h"

/* BIOS consumes the original argument registers and caller stack unchanged.
 * Its public variadic prototype in psx/libc.h is intentionally omitted here:
 * a variadic C definition adds four stores absent from this ABI tail veneer. */
int printf() {
    PSYQ_BIOS_TAIL_CALL(PSYQ_BIOS_TABLE_A, PSYQ_BIOS_A_PRINTF);
}
