/* An uncalled setter of a variable nothing reads. */

#include "fft/event_equip.h"
#include "psx/types.h"

/* Store a value in g_equip_input_unread_value. Nothing calls it and nothing reads that variable. */
void equip_input_set_unread_value(s32 value) {
    g_equip_input_unread_value = value;
}
