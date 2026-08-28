#include "psx/types.h"

/* BUNIT.OUT 001c3150 - Return the pad-1 input, or 0 while the input lock timer is active. */
s32 bunit_input_read_pad1_unless_locked(void) {
    u32 input;

    input = PadRead(0);
    if (bunit_input_get_lock_timer() != 0) {
        input = 0;
    }
    return input;
}
