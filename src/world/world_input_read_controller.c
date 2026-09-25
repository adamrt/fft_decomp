#include "fft/world.h"

/* Read controller input; the wrapper ignores its argument. */
s32 world_input_read_controller(s32 unused) {
    return PadRead(0);
}
