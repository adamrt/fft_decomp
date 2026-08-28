#include "fft/main_sound.h"
#include "psx/types.h"

/* Sets or clears the SFX-enable bit 0x1000 of the driver status; disabling
 * also silences every SFX channel first. The Play Sound
 * entry points test this bit. */
void main_sound_set_sfx_enabled(s32 enabled) {
    if (enabled != 0) {
        g_main_sound_driver_flags |= 0x1000;
    } else {
        SuzukiTurnOffAllMusic();
        g_main_sound_driver_flags &= ~0x1000;
    }
}
