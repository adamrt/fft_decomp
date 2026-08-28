#include "fft/equip.h"
#include "psx/types.h"

void equip_panel_toggle_numeric_thread(s32 enable) {
    if (enable != 0) {
        if (battle_thread_is_running(0xC) == 0) {
            battle_thread_start(0xC, equip_editor_run_numeric_thread);
            battle_thread_set_parameters(0xC, (s32)g_equip_numeric_panel_params, 0, 0);
        }
    } else {
        equip_thread_request_stop(0xC);
    }
}
