#include "psx/abi_inline.h"
#include "psx/libapi.h"

void EnterCriticalSection(void) {
    PSYQ_BIOS_SYSCALL(PSYQ_BIOS_SYSCALL_ENTER_CRITICAL_SECTION);
}
