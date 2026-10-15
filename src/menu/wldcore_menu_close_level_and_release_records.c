#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/open.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/wldcore.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Pop the current menu level and give back the display records it appended:
 * each of the four slots that is not -1 releases one window or render record
 * and its queue entry. With `restore` the level underneath is resumed. Also
 * unblocks the menu buttons and re-shows the location marker. */
void wldcore_menu_close_level_and_release_records(wldcore_menu_slot_set_t* slots, s32 restore) {
    g_menu_blocked_buttons = 0;
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
    g_main_system_flags &= ~MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
}
