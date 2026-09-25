#include "fft/main.h"
#include "psx/api.h"
#include "psx/types.h"

void main_system_disable_root_counter_2_evcb(void) {
    if ((g_main_sound_driver_flags & 1) != 0) {
        DisableEvent(g_main_root_counter_2_event);
        g_main_sound_driver_flags &= ~1;
    }
}
