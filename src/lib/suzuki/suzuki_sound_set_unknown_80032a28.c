#include "psx/suzuki.h"
#include "psx/types.h"

/* Stores a 16-bit sound-driver value that the driver only ever writes. Nothing
 * calls this function. */
void suzuki_sound_set_unknown_80032a28(u16 value) {
    g_suzuki_unknown_80032a28 = value;
}
