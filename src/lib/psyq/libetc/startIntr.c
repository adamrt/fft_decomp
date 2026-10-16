#include "psx/libetc.h"
/* Take over interrupt handling: mask and acknowledge every IRQ, set the default DMA priorities,
 * hook trapIntr into the BIOS through a jump buffer with its own stack, start the VBlank and DMA
 * dispatchers and leave the critical section. Returns 0 when already started. */
psyq_interrupt_state_t* startIntr(void) {
    if (g_psyq_etc_interrupt_state.initialized)
        return 0;
    *g_psyq_etc_irq_status = *g_psyq_etc_irq_mask = 0;
    *g_psyq_etc_dma_dpcr = PSYQ_DMA_DEFAULT_PRIORITY;
    psyq_etc_clear_interrupt_state_words(
        (u32*)&g_psyq_etc_interrupt_state, sizeof(g_psyq_etc_interrupt_state) / sizeof(u32));
    if (setjmp(g_psyq_etc_interrupt_state.jump_buffer))
        trapIntr();
    g_psyq_etc_interrupt_state.jump_buffer[1]
        = (u32)&g_psyq_etc_interrupt_state
              .stack[sizeof(g_psyq_etc_interrupt_state.stack) / sizeof(u32) - PSYQ_ETC_INTERRUPT_STACK_RESERVED_WORDS];
    HookEntryInt(g_psyq_etc_interrupt_state.jump_buffer);
    g_psyq_etc_interrupt_state.initialized = 1;
    g_psyq_etc_dispatch->vblank_callback = startIntrVSync();
    {
        psyq_callback_setter_t setter = startIntrDMA();
        /* Retail loads the table pointer into a0, as if it were also passed to _96_remove; a plain store selects v1. */
        register psyq_interrupt_dispatch_t* dispatch __asm__("$4") = g_psyq_etc_dispatch;
        dispatch->dma_callback = setter;
    }
    _96_remove();
    ExitCriticalSection();
    return &g_psyq_etc_interrupt_state;
}
