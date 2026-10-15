#include "fft/wldcore.h"

/* Withholds every button from the WORLD menu frame while the next menu level runs. */
/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_propositions_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}
