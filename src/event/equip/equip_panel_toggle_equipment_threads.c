#include "fft/event_equip.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Start the equipment list thread (when idle) and the numeric editor thread
 * when `enable` is set; ask both to stop otherwise. */
void equip_panel_toggle_equipment_threads(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(EQUIP_THREAD_SLOT_EQUIPMENT_LIST) == 0) {
            thread_start(EQUIP_THREAD_SLOT_EQUIPMENT_LIST, equip_panel_run_equipment_list_thread);
            thread_set_parameters(EQUIP_THREAD_SLOT_EQUIPMENT_LIST, (s32)g_equip_equipment_panel_params, 0, 0);
        }
        menu_toggle_numeric_editor_thread(enable);
        return;
    }
    menu_request_thread_stop(EQUIP_THREAD_SLOT_EQUIPMENT_LIST);
    menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
}
