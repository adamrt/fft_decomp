#include "fft/main_runtime.h"
#include "fft/main_sound.h"

s32 main_sound_get_type(void) {
    s32 result;

    if (g_main_sound_driver_flags & 0x700) {
        result = 1;
        if (g_main_sound_driver_flags & 0x600) {
            result = 2;
        }
    } else {
        result = 0;
    }
    return result;
}
