#include "psx/libetc.h"

/* VBlank IRQ handler: count Vcount and run each installed VBlank callback in slot order. */
void trapIntrVSync(void) {
    s32 slot = 0;
    psyq_interrupt_callback_t callback;
    Vcount++;
    for (; slot < PSYQ_ETC_CALLBACK_SLOTS; slot++) {
        callback = g_psyq_etc_vblank_callbacks[slot];
        if (callback)
            callback();
    }
}
