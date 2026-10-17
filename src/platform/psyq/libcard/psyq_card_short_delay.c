#include "psx/cpu_abi_inline.h"
#include "psx/types.h"

#include "psx/libcard.h"

/* main 0x800289b8–0x800289d4: fixed 200-step BIOS timing delay. */
void psyq_card_short_delay(void) {
    register s32 psyq_card_delay_count __asm__("$8"); /* Unpinned allocation changes the timing loop bytes. */
    PSYQ_CPU_LOAD_WAIT();
    PSYQ_CPU_SIGNED_CONSTANT(psyq_card_delay_count, 200);
delay_next:
    --psyq_card_delay_count;
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (psyq_card_delay_count != 0)
        goto delay_next;
    PSYQ_CPU_LOAD_WAIT();
    PSYQ_CPU_SHARED_DELAY_END();
}
