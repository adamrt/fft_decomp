#include "psx/suzuki.h"
#include "psx/types.h"

/* Stores a 16-bit sound-driver value that the driver only ever writes. Nothing
 * calls this function. */
void suzuki_sound_set_unread_value(u16 value) {
    g_suzuki_sound_unread_value = value;
}
