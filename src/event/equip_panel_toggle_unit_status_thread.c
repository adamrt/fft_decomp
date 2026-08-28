#include "fft/equip.h"
#include "psx/types.h"

void equip_panel_toggle_unit_status_thread(s32 enable) {
    if (enable != 0) {
        if (battle_thread_is_running(0xD) == 0) {
            battle_thread_start(0xD, equip_render_unit_status_panel_thread);
            battle_thread_set_parameters(0xD, (s32)g_equip_unit_status_panel_params, 0, 0);
        }
    } else {
        equip_thread_request_stop(0xD);
    }
}
