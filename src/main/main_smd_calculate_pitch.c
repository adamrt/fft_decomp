#include "fft/main.h"
#include "psx/types.h"

/* Converts a key in 1/256 semitones (key number << 8 | fine step) to an SPU
 * pitch: the octave-6 table value for the semitone and fine step, shifted by
 * the key's octave distance from octave 6. */
s16 main_smd_calculate_pitch(s16 key) {
    s32 index;
    s32 shift;
    s16 pitch;

    index = (key & 0x7fff) >> 8;
    pitch = g_main_smd_pitch_table[(key & 0xff) + (g_main_smd_key_semitones[index] << 8)];
    shift = 6 - g_main_smd_key_octaves[index];
    if (shift >= 0)
        pitch = pitch >> shift;
    else
        pitch = pitch << -shift;
    return pitch;
}
