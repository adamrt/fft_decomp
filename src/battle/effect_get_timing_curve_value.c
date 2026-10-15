#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/data.h"
#include "fft/main.h"
#include "psx/libgpu.h"
#include "psx/suzuki.h"
#include "psx/types.h"

/* Time scale of the parent timeline's frame record->timeline.frame: one of the
 * 4-bit values the effect file's timing-curve section packs two to a byte, low
 * nibble first. */
s32 effect_get_timing_curve_value(effect_record_t* record) {
    s16 packed_index;
    s32 packed_value;
    s32 offset;

    packed_index = record->timeline.frame;
    offset = packed_index / 2;
    packed_value = g_effect_timing_curves->parent_timeline[offset];
    if (packed_index & 1) {
        packed_value >>= 4;
    }
    return packed_value & 0xf;
}
