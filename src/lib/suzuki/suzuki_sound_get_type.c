#include "fft/suzuki.h"

/* The sound output setting as SUZUKI_SOUND_TYPE_MONO, _STEREO or _WIDE, read from
 * the driver flags. The opposite-phase wide mode is reported as _WIDE. */
suzuki_sound_type_e suzuki_sound_get_type(void) {
    suzuki_sound_type_e result;

    if (g_suzuki_sound_driver_flags
        & (SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE | SUZUKI_SOUND_DRIVER_WIDE | SUZUKI_SOUND_DRIVER_STEREO)) {
        result = SUZUKI_SOUND_TYPE_STEREO;
        if (g_suzuki_sound_driver_flags & (SUZUKI_SOUND_DRIVER_WIDE_OPPOSITE | SUZUKI_SOUND_DRIVER_WIDE)) {
            result = SUZUKI_SOUND_TYPE_WIDE;
        }
    } else {
        result = SUZUKI_SOUND_TYPE_MONO;
    }
    return result;
}
