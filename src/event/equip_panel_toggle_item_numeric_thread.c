#include "fft/event_equip.h"
#include "psx/types.h"

void equip_panel_toggle_item_numeric_thread(s32 enable) {
    if (enable != 0) {
        if (battle_thread_is_running(0xB) == 0) {
            battle_thread_start(0xB, equip_editor_run_numeric_thread);
            battle_thread_set_parameters(0xB, (s32)&g_equip_item_numeric_panel_params, 0, 0);
        }
    } else {
        equip_thread_request_stop(0xB);
    }
    g_equip_item_numeric_thread_enabled = enable;
    g_equip_numeric_panel_style = enable;
}
