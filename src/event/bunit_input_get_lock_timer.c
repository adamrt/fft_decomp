#include "fft/bunit.h"
#include "psx/types.h"

s32 bunit_input_get_lock_timer(void) {
    return g_bunit_input_lock_timer;
}
