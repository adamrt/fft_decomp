#include "fft/world.h"
#include "psx/types.h"

/* Store the 0x1000-wide band of [-0x4000, 0x4000) that contains value. */
void world_camera_store_yaw_band(s32 value) {
    s32 band_start;

    for (band_start = (s16)0xC000; band_start < 0x3FF8; band_start += ONE) {
        if (value < band_start + ONE && value >= band_start) {
            g_world_camera_yaw_band = band_start;
        }
    }
}
