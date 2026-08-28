#include "fft/main_sound.h"
#include "psx/types.h"

void main_suzuki_spu_callback_func(void) {
    g_main_sound_driver_flags &= 0xff8f;
}
