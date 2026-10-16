#include "psx/libetc.h"

/* Start the VBlank dispatcher: set root counter 1 to count horizontal blanks for VSync, zero
 * Vcount, clear the VBlank callback slots and install trapIntrVSync. Returns setIntrVSync for the
 * slot setter table. */
psyq_callback_setter_t startIntrVSync(void) {
    *g_psyq_etc_timer1_mode = PSYQ_ETC_VBLANK_COUNTER_MODE;
    Vcount = 0;
    psyq_etc_clear_vblank_callbacks(g_psyq_etc_vblank_callbacks, PSYQ_ETC_CALLBACK_SLOTS);
    InterruptCallback(PSYQ_IRQ_VBLANK, trapIntrVSync);
    return setIntrVSync;
}
