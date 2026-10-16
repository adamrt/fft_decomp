#include "psx/libetc.h"

/* Return 1 while trapIntr is running interrupt callbacks; libcd waits use it to service the CD
 * interrupt themselves. */
s32 CheckCallback(void) {
    return g_psyq_etc_interrupt_state.in_callback;
}
