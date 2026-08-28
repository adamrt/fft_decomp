#include "fft/main_sound.h"
#include "psx/etc.h"
#include "psx/types.h"

void main_system_enable_root_counter_2_evcb(void) {
    u16 flags;

    flags = g_main_sound_driver_flags;
    if ((flags & 1) == 0) {
        g_main_sound_driver_flags = flags | 1;
        EnableEvent(g_main_root_counter_2_event);
    }
}
