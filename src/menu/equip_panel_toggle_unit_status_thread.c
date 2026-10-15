#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Starts the unit status thread on equip_panel_run_unit_status_thread
 * (parameter g_equip_unit_status_panel_params) when `enable` is set and the
 * thread is idle; stops it otherwise. */
void equip_panel_toggle_unit_status_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(EQUIP_THREAD_SLOT_UNIT_STATUS) == 0) {
            thread_start(EQUIP_THREAD_SLOT_UNIT_STATUS, menu_run_unit_status_thread_variant);
            thread_set_parameters(EQUIP_THREAD_SLOT_UNIT_STATUS, (s32)g_equip_unit_status_panel_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(EQUIP_THREAD_SLOT_UNIT_STATUS);
    }
}
