#include "fft/world.h"
#include "psx/types.h"

void world_menu_refresh_hovered_unit_stats_display(void) {
    world_unit_load_selected_into_editor();
    g_world_menu_hovered_unit_stats_setup[6].param = 2;
    if (g_world_unit_selected_status_billboard.layout < 0)
        g_world_menu_hovered_unit_stats_setup[6].param = 0xc00;
    world_menu_display_hovered_unit_stats(&g_world_menu_hovered_unit_stats_display, 0,
        g_world_menu_hovered_unit_stats_setup, &g_world_unit_selected_status_billboard);
}
