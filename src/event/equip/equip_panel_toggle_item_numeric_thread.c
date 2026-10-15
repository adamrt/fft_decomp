#include "fft/event_equip.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Start the item numeric panel thread when `enable` is set and the thread is
 * idle, ask it to stop otherwise, and record the state in
 * g_equip_item_numeric_thread_enabled and the numeric panel style. */
void equip_panel_toggle_item_numeric_thread(s32 enable) {
    if (enable != 0) {
        if (thread_is_running(EQUIP_THREAD_SLOT_ITEM_NUMERIC) == 0) {
            thread_start(EQUIP_THREAD_SLOT_ITEM_NUMERIC, menu_run_numeric_editor_frame_thread_variant);
            thread_set_parameters(EQUIP_THREAD_SLOT_ITEM_NUMERIC, (s32)&g_menu_preview_stats_thread_params, 0, 0);
        }
    } else {
        menu_request_thread_stop(EQUIP_THREAD_SLOT_ITEM_NUMERIC);
    }
    g_equip_item_numeric_thread_enabled = enable;
    g_equip_numeric_panel_style = enable;
}
