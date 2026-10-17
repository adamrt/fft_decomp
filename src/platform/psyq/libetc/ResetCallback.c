#include "psx/libetc.h"

s32 ResetCallback(void) {
    /* Return the interrupt-state address as a 32-bit integer. */
    return (s32)g_psyq_etc_dispatch->reset();
}
