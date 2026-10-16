/* An uncalled setter of a variable nothing reads. */

#include "fft/event_equip.h"
#include "psx/types.h"

/* Store a value in g_equipment_input_unread_value. Nothing calls it and nothing reads that variable. */
void equipment_set_unread_input_value(s32 value) {
    g_equipment_input_unread_value = value;
}
