#include "fft/battle.h"
#include "psx/types.h"

void battle_camera_store_yaw_band(s32 value) {
    s32 lower_bound;

    for (lower_bound = -0x4000; lower_bound < 0x3FF8; lower_bound += ONE) {
        if (value < lower_bound + ONE && value >= lower_bound) {
            g_battle_camera_yaw_band = lower_bound;
        }
    }
}
