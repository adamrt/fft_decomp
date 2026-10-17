#include "fft/suzuki.h"

/* Commit pending key-ons; shared with the root-counter handler. */
void suzuki_sound_flush_key_on(void) {
    suzuki_sound_flush_key_on_inline();
}
