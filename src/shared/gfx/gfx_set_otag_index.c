#include "fft/event_card.h"
#include "psx/types.h"

/* Select the ordering-table bucket used by subsequent menu packets. */
void gfx_set_otag_index(s32 value) {
    g_menu_gfx_otag_index = value;
}
