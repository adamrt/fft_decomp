#include "fft/wldcore.h"

/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_story_events_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}
