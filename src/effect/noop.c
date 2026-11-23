/* The empty effect callback. */

#include "psx/types.h"

/* Native no-op callback at 0x801c44a0 in E000 (catalog ids 0, 24, 73, 92, 121); leaves the result unspecified. */
s32 effect_noop(s32 unused_record_index, s32 unused_callback_slot, s32 unused_emitter_index, s32 unused_frame) {
}
