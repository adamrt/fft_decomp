#include "fft/event_equip.h"

/* EQUIP.OUT 001bf590 - Prime the four "menu section dirty" flags before the
 * next redraw pass. */
void equip_menu_reset_state_flags(void) {
    g_equip_unit_status_panel_redraw = 1;
    g_equip_numeric_panel_redraw = 1;
    g_menu_preview_stats_thread_params.redraw_request = 1;
    g_equip_equipment_panel_redraw = 1;
}
