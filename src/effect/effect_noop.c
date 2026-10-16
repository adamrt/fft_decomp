#include "fft/effect.h"
#include "psx/types.h"

/* Native no-op callback; leaves the result unspecified. */
s32 effect_noop(s32 unused_record_index, s32 unused_callback_slot, s32 unused_emitter_index, s32 unused_frame) {
}
