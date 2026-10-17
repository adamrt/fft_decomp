#include "fft/suzuki.h"
#include "psx/libspu.h"
#include "psx/types.h"

/* Sets the SPU reverb mode and parameters.
 *
 * Modes 0-9 are Psy-Q reverb types (0 off, 4 studio C, the mode of every
 * SMD on the disc): the reverb is switched off while a changed mode (or mode
 * 0) is programmed with the work-area clear flag 0x100; any mode from 10 up
 * only records depth, delay and feedback. A negative mode selects the mode
 * stored here. That copy keeps the 0x100 flag, which LIBSPU strips from its
 * own, so the two never compare equal and the stored mode is not below 10:
 * a negative mode also only records the three values. */
void suzuki_sound_set_reverb_mode(s32 mode, s16 depth, s32 delay, s32 feedback) {
    SpuReverbAttr current;

    SpuGetReverbModeParam(&current);
    if (mode < 0) {
        if (current.mode == g_suzuki_sound_reverb_attr.mode) {
            return;
        }
        mode = g_suzuki_sound_reverb_attr.mode;
    }
    g_suzuki_sound_spu_state.reverb_depth = depth;
    g_suzuki_sound_reverb_attr.delay = delay;
    g_suzuki_sound_reverb_attr.feedback = feedback;
    if (mode < PSYQ_SPU_REVERB_MODE_COUNT) {
        if (current.mode != mode || mode == SPU_REV_MODE_OFF) {
            SpuSetReverb(SPU_OFF);
            g_suzuki_sound_reverb_attr.mode = mode | SPU_REV_MODE_CLEAR_WA;
            g_suzuki_sound_reverb_attr.mask |= SPU_REV_MODE;
            SpuSetReverbModeParam(&g_suzuki_sound_reverb_attr);
            g_suzuki_sound_reverb_attr.mask = 0;
            SpuSetReverb(SPU_ON);
        }
        suzuki_sound_commit_volume_change();
        SpuSetReverbDepth(&g_suzuki_sound_reverb_attr);
        g_suzuki_sound_reverb_attr.mask = 0;
    }
}
