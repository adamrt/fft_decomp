#include "fft/suzuki.h"
#include "psx/types.h"

/* Sets or clears SUZUKI_SOUND_DRIVER_SFX_ENABLED; disabling also silences
 * every SFX channel first. The Play Sound entry points test the flag. */
void suzuki_sound_set_sfx_enabled(s32 enabled) {
    if (enabled != 0) {
        g_suzuki_sound_driver_flags |= SUZUKI_SOUND_DRIVER_SFX_ENABLED;
    } else {
        suzuki_sound_stop_all_sfx();
        g_suzuki_sound_driver_flags &= ~SUZUKI_SOUND_DRIVER_SFX_ENABLED;
    }
}
