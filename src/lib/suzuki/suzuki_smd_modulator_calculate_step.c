#include "fft/suzuki.h"
#include "psx/types.h"

/* Returns the amplitude a modulator steps by so that its waveform spans
 * depth over one timer period of speed steps: depth / speed for the triangle
 * waveforms (2-3), depth / (speed - 1) for the sawtooth (4, which resets on
 * the last step) and depth itself for the other waveforms or when depth or
 * speed is zero. */
s32 suzuki_smd_modulator_calculate_step(s32 depth, s16 speed, s16 waveform) {
    if (depth != 0 && speed != 0) {
        switch (waveform) {
        case SUZUKI_MODULATOR_WAVEFORM_TRIANGLE:
        case SUZUKI_MODULATOR_WAVEFORM_TRIANGLE_CENTERED:
            depth /= speed;
            break;
        case SUZUKI_MODULATOR_WAVEFORM_SAWTOOTH:
            if (speed != 1) {
                depth /= speed - 1;
            }
            break;
        }
    }
    return depth;
}
