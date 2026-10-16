#include "psx/libetc.h"

/* Set VBlank callback slot; returns the previous callback. */
psyq_interrupt_callback_t setIntrVSync(s32 slot, psyq_interrupt_callback_t callback) {
    psyq_interrupt_callback_t previous = g_psyq_etc_vblank_callbacks[slot];
    if (callback != previous)
        g_psyq_etc_vblank_callbacks[slot] = callback;
    return previous;
}
