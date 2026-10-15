#include "fft/event_equip.h"

/* Ask the unit status, equipment list, numeric editor and item numeric
 * panel threads to stop and clear the two panel styles. */
void equip_menu_stop_threads(void) {
    menu_request_thread_stop(EQUIP_THREAD_SLOT_UNIT_STATUS);
    menu_request_thread_stop(EQUIP_THREAD_SLOT_EQUIPMENT_LIST);
    menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
    equip_panel_toggle_item_numeric_thread(0);
    g_menu_preview_stats_thread_params.style = 0;
    g_equip_equipment_panel_style = 0;
}
