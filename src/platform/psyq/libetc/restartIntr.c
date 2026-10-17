#include "psx/libetc.h"
/* Undo stopIntr: rehook the jump buffer, restore the saved IRQ mask and DMA priorities and leave
 * the critical section. Returns 0 when already running. */
psyq_interrupt_state_t* restartIntr(void) {
    if (g_psyq_etc_interrupt_state.initialized)
        return 0;
    HookEntryInt(g_psyq_etc_interrupt_state.jump_buffer);
    g_psyq_etc_interrupt_state.initialized = 1;
    *g_psyq_etc_irq_mask = g_psyq_etc_interrupt_state.saved_mask;
    *g_psyq_etc_dma_dpcr = g_psyq_etc_interrupt_state.saved_dma_priority;
    ExitCriticalSection();
    return &g_psyq_etc_interrupt_state;
}
