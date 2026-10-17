#include "fft/suzuki.h"
#include "psx/types.h"

/* Store the retained 16-bit driver value. No caller or reader has been
 * identified in the retail code or SPU transfer paths. */
void suzuki_sound_set_unread_value(u16 value) {
    g_suzuki_sound_unread_value = value;
}
