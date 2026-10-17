#include "psx/card_interrupt_abi.h"
#include "psx/cpu_abi_inline.h"

/* main 0x800288d8–0x8002891c: BIOS-private interrupt continuation, live v1 base. */
void psyq_card_interrupt_tail(void) {
    /* BIOS enters with its live base in v1 and returns through the saved continuation. */
    psyq_card_interrupt_result = psyq_card_interrupt_base->interrupt_mask;
    PSYQ_CPU_LOAD_WAIT();
    psyq_card_interrupt_result &= PSYQ_CARD_IRQ_PENDING_MASK;
    if (psyq_card_interrupt_result == 0)
        goto interrupt_not_pending;
wait_clear:
    psyq_card_interrupt_result = psyq_card_interrupt_base->joy_status;
    PSYQ_CPU_LOAD_WAIT();
    psyq_card_interrupt_result &= PSYQ_CARD_IRQ_PENDING_MASK;
    if (psyq_card_interrupt_result != 0)
        goto wait_clear;
    psyq_card_interrupt_result = (u32)g_psyq_card_patch_continuation;
    PSYQ_CPU_LOAD_WAIT();
    goto*(void*)psyq_card_interrupt_result;
interrupt_not_pending:
}
