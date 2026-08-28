#include "fft/battle_camera.h"
#include "psx/types.h"

void battle_camera_calculate_relative_offset_5(const u16* input, s16* output) {
    battle_camera_calculate_relative_offset(input, output, 5);
}
