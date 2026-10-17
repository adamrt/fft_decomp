#include "psx/libetc.h"

s32 StopCallback(void) {
    /* Return the interrupt-state address as a 32-bit integer. */
    return (s32)g_psyq_etc_dispatch->stop();
}
