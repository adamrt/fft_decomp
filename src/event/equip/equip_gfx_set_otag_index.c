#include "fft/event_equip.h"
#include "fft/menu.h"
#include "psx/types.h"

/* Select the ordering-table bucket used by subsequent menu packets. */
void equip_gfx_set_otag_index(s32 value) {
    g_menu_gfx_otag_index = value;
}
