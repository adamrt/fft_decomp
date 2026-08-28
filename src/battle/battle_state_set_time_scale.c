#include "fft/battle.h"
#include "fft/main_runtime.h"
#include "psx/types.h"

void battle_state_set_time_scale(s32 value) {
    if ((u32)(value - 1) < 9) {
        g_frame_pacing = value;
    }
}
