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

/* Steps a message level: once the text window has popped in and
 * wldcore_text_is_window_finished reports 0, pops the level, restores the
 * blocked-button mask it saved and (when asked) the text section pointers,
 * and resumes the level underneath. */
void wldcore_menu_wait_text_then_pop_level(wldcore_menu_message_level_t* level) {
    if (g_wldcore_window_render_records[g_wldcore_scrollable_text_window.render_record_index].flags
        & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
        return;
    }
    g_input_current_buttons = 0;
    g_menu_blocked_buttons = level->saved_blocked_buttons;
    g_wldcore_window_record_count -= 1;
    g_wldcore_window_render_object_count -= 1;
    g_wldcore_menu_stack_depth -= 1;
    if (level->reload_text != 0) {
        text_restore_section_pointers();
    }
    wldcore_menu_dispatch_resume_handler();
}
