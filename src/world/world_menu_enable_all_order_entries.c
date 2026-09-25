#include "fft/world.h"
#include "psx/types.h"

void world_menu_enable_all_order_entries(void) {
    g_world_formation_panel_windows[0].enabled = 1;
    g_world_formation_panel_windows[1].enabled = 1;
    g_world_formation_panel_windows[5].enabled = 1;
    g_world_formation_panel_windows[6].enabled = 1;
    g_world_formation_panel_windows[7].enabled = 1;
    g_world_formation_panel_windows[2].enabled = 1;
    g_world_formation_panel_windows[3].enabled = 1;
    g_world_formation_panel_windows[4].enabled = 1;
}
