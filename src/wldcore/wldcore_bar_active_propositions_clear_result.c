#include "fft/wldcore.h"
#include "psx/types.h"

/* Clear the shared world-menu result before the next menu state runs. */
/* The caller hands in its menu level; the target reads no argument. */
void wldcore_bar_active_propositions_clear_result(wldcore_menu_panel_level_t* level) {
    g_wldcore_menu_result = -1;
}
