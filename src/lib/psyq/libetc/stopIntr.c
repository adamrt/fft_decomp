#include "psx/libetc.h"
/* Suspend libetc interrupt handling: inside a critical section save the IRQ mask and DMA
 * priorities, mask every IRQ, clear every DMA channel's enable bit and unhook from the BIOS.
 * Returns the state, or 0 when not started; restartIntr undoes it. */
psyq_interrupt_state_t* stopIntr(void) {
    if (!g_psyq_etc_interrupt_state.initialized)
        return 0;
    EnterCriticalSection();
    g_psyq_etc_interrupt_state.saved_mask = *g_psyq_etc_irq_mask;
    g_psyq_etc_interrupt_state.saved_dma_priority = *g_psyq_etc_dma_dpcr;
    *g_psyq_etc_irq_status = *g_psyq_etc_irq_mask = 0;
    *g_psyq_etc_dma_dpcr &= 0x77777777;
    ResetEntryInt();
    g_psyq_etc_interrupt_state.initialized = 0;
    return &g_psyq_etc_interrupt_state;
}
