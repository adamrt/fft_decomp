#include "fft/main.h"
#include "psx/spu.h"
#include "psx/types.h"

/* Sets the SPU reverb mode and parameters.
 *
 * A negative mode keeps the current mode and returns early if the SPU
 * already uses it. Modes 0-9 are Psy-Q reverb types: the reverb is switched
 * off while a changed mode (or mode 0) is programmed with the work-area
 * clear flag 0x100; any mode from 10 up only records depth, delay and
 * feedback. */
void main_sound_set_reverb_mode(s32 mode, s16 depth, s32 delay, s32 feedback) {
    SpuReverbAttr current;

    SpuGetReverbModeParam(&current);
    if (mode < 0) {
        if (current.mode == g_main_sound_reverb_attr.mode) {
            return;
        }
        mode = g_main_sound_reverb_attr.mode;
    }
    g_main_sound_spu_state.reverb_depth = depth;
    g_main_sound_reverb_attr.delay = delay;
    g_main_sound_reverb_attr.feedback = feedback;
    if (mode < 10) {
        if (current.mode != mode || mode == 0) {
            SpuSetReverb(0);
            g_main_sound_reverb_attr.mode = mode | 0x100;
            g_main_sound_reverb_attr.mask |= 1;
            SpuSetReverbModeParam(&g_main_sound_reverb_attr);
            g_main_sound_reverb_attr.mask = 0;
            SpuSetReverb(1);
        }
        main_sound_commit_volume_change();
        SpuSetReverbDepth(&g_main_sound_reverb_attr);
        g_main_sound_reverb_attr.mask = 0;
    }
}
