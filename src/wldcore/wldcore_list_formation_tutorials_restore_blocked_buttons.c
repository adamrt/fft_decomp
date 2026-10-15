#include "fft/wldcore.h"
#include "psx/pad.h"

void wldcore_list_formation_tutorials_restore_blocked_buttons(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
}
