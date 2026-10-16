/* LIBETC 8001e1e8-8001e33c; callback replacement and BIOS clear policy. */
#include "psx/libetc.h"

/* Install or remove the callback of IRQ slot and update the IRQ mask; for VBlank and the three
 * timers it also tells the BIOS to acknowledge those interrupts itself only while no callback is
 * set. Returns the previous callback. */
psyq_interrupt_callback_t setIntr(s32 slot, psyq_interrupt_callback_t callback) {
    psyq_interrupt_callback_t old = g_psyq_etc_interrupt_state.callbacks[slot];
    s32 saved_mask;
    if (callback != old && g_psyq_etc_interrupt_state.initialized) {
        saved_mask = *g_psyq_etc_irq_mask;
        *g_psyq_etc_irq_mask = 0;
        if (callback) {
            g_psyq_etc_interrupt_state.callbacks[slot] = callback;
            saved_mask |= 1 << slot;
            g_psyq_etc_interrupt_state.callback_mask |= 1 << slot;
        } else {
            g_psyq_etc_interrupt_state.callbacks[slot] = 0;
            saved_mask &= ~(1 << slot);
            g_psyq_etc_interrupt_state.callback_mask &= ~(1 << slot);
        }
        if (slot == PSYQ_IRQ_VBLANK) {
            ChangeClearPAD(callback == 0);
            ChangeClearRCnt(PSYQ_ETC_VBLANK_ROOT_COUNTER, callback == 0);
        }
        if (slot == PSYQ_IRQ_TIMER0)
            ChangeClearRCnt(0, callback == 0);
        if (slot == PSYQ_IRQ_TIMER1)
            ChangeClearRCnt(1, callback == 0);
        if (slot == PSYQ_IRQ_TIMER2)
            ChangeClearRCnt(2, callback == 0);
        *g_psyq_etc_irq_mask = saved_mask;
    }
    return old;
}
