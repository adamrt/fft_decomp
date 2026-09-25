#include "fft/event_equip.h"
#include "psx/types.h"

void equip_panel_toggle_equipment_threads(s32 enable) {
    if (enable != 0) {
        if (battle_thread_is_running(0xA) == 0) {
            battle_thread_start(0xA, equip_panel_run_equipment_list_thread);
            battle_thread_set_parameters(0xA, (s32)g_equip_equipment_panel_params, 0, 0);
        }
        equip_panel_toggle_numeric_thread(enable);
        return;
    }
    equip_thread_request_stop(0xA);
    equip_thread_request_stop(0xC);
}
