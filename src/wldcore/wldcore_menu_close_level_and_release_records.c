#include "fft/main_runtime.h"
#include "fft/wldcore.h"
#include "psx/types.h"

void wldcore_menu_close_level_and_release_records(wldcore_menu_slot_set_t* slots, s32 restore) {
    g_wldcore_menu_result = 0;
    if (slots->window_a != -1) {
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_object_count -= 1;
    }
    if (slots->render_a != -1) {
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 1;
    }
    if (slots->window_b != -1) {
        g_wldcore_window_record_count -= 1;
        g_wldcore_window_render_object_count -= 1;
    }
    if (slots->render_b != -1) {
        g_wldcore_window_render_record_count -= 1;
        g_wldcore_window_render_object_count -= 1;
    }
    g_wldcore_menu_stack_depth -= 1;
    if (restore != 0) {
        wldcore_menu_dispatch_resume_handler();
    }
    g_main_system_flags &= ~0x2000;
}
