#include "fft/wldcore.h"

/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_formation_tutorials_clear_result(wldcore_menu_panel_level_t* level) {
    g_wldcore_menu_result = -1;
}
