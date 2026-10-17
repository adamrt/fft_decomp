#include "fft/suzuki.h"
#include "psx/libapi.h"

/* Disable the root-counter-2 tick event if SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT says it is
 * enabled, and clear that flag. Nothing in the tree calls it. */
void suzuki_sound_disable_root_counter_2_event(void) {
    if ((g_suzuki_sound_driver_flags & SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT) != 0) {
        DisableEvent(g_suzuki_sound_root_counter_2_event);
        g_suzuki_sound_driver_flags &= ~SUZUKI_SOUND_DRIVER_ROOT_COUNTER_EVENT;
    }
}
