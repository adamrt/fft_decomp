#include "fft/battle_menu.h"
#include "fft/menu.h"

/* Reload the unit and redraw its status panel; a negative list index draws dashes. */
void menu_refresh_right_unit_stats_display(void) {
    menu_load_selected_unit_variant();
    g_menu_right_unit_stats_setup[6].format = 2;
    if (g_menu_comparison_unit_status.list_index < 0) {
        g_menu_right_unit_stats_setup[6].format = 0x0c00;
    }
    menu_display_hovered_unit_stats(&g_menu_right_unit_stats_rect, UNIT_STATS_PANEL_RIGHT_UNIT,
        g_menu_right_unit_stats_setup, &g_menu_comparison_unit_status);
}
