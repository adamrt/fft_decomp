#include "psx/cpu_abi_inline.h"
#include "psx/types.h"

#include "psx/libcard.h"

#define PSYQ_CARD_LONG_DELAY_COUNT 0x00320000

/* main 0x80028a40–0x80028a58: fixed long BIOS timing delay. */
void psyq_card_long_delay(void) {
    register s32 psyq_card_delay_count __asm__("$14"); /* Preserve the handwritten branch/count register. */
    psyq_card_delay_count = PSYQ_CARD_LONG_DELAY_COUNT;
    __asm__("" : "=r"(psyq_card_delay_count) : "0"(psyq_card_delay_count));
    --psyq_card_delay_count;
delay_test:
    /* The C decrement below occupies the original branch delay and runs on both edges. */
    PSYQ_CPU_SHARED_DELAY_BEGIN();
    if (psyq_card_delay_count != 0)
        goto delay_test;
    __asm__("" : "=r"(psyq_card_delay_count) : "0"(psyq_card_delay_count)); /* Hide the C fall-through zero identity. */
    --psyq_card_delay_count;
    __asm__ volatile(""
        :
        : "r"(psyq_card_delay_count)); /* Preserve the branch-delay decrement of the architectural counter. */
    PSYQ_CPU_SHARED_DELAY_END();
}
