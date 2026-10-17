#include "psx/libetc.h"

s32 VSyncCallbacks(s32 slot, psyq_interrupt_callback_t callback) {
    /* Return the previous callback address as a 32-bit integer. */
    return (s32)g_psyq_etc_dispatch->vblank_callback(slot, callback);
}
