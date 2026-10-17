#include "psx/abi_inline.h"
#include "psx/libapi.h"

void ExitCriticalSection(void) {
    PSYQ_BIOS_SYSCALL(PSYQ_BIOS_SYSCALL_EXIT_CRITICAL_SECTION);
}
