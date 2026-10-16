/* An uncalled setter of a variable nothing reads. */

#include "fft/event_equip.h"
#include "psx/types.h"

/* Store a value in g_equip_unknown_801d86ac. Nothing calls it and nothing reads that variable. */
void equip_set_s16_801d86ac(s32 value) {
    g_equip_unknown_801d86ac = value;
}
