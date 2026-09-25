#include "fft/event_bunit.h"
#include "psx/types.h"

/* Select the ordering-table bucket used by subsequent menu packets. */
void bunit_gfx_set_otag_index(s16 value) {
    g_bunit_gfx_otag_index = value;
}
