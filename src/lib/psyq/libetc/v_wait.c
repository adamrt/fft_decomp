#include "psx/libetc.h"
/* Spin until Vcount reaches vblank. On timeout (frames << PSYQ_ETC_VSYNC_TIMEOUT_SHIFT polls)
 * print a message and make the BIOS stop acknowledging the VBlank pad and root-counter interrupts,
 * as setIntr does while a VBlank callback is installed. */
void v_wait(s32 vblank, s32 frames) {
    /* Volatile preserves the native per-iteration stack store/reload of the timeout. */
    volatile s32 timeout = frames << PSYQ_ETC_VSYNC_TIMEOUT_SHIFT;
    while (Vcount < vblank) {
        if (--timeout == -1) {
            puts(g_psyq_etc_vsync_timeout_message);
            ChangeClearPAD(0);
            ChangeClearRCnt(PSYQ_ETC_VBLANK_ROOT_COUNTER, 0);
            break;
        }
    }
}
