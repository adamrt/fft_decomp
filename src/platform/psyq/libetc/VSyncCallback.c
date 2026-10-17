#include "psx/libetc.h"

s32 VSyncCallback(psyq_interrupt_callback_t callback) {
    /* Return the previous callback address as a 32-bit integer. */
    return (s32)g_psyq_etc_dispatch->vblank_callback(0, callback);
}
