#include "fft/event_bunit.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Start (enable=1) or stop (enable=0) the equipment-and-ability panel in slot 9 and the numeric
 * editor in slot 12. Sister of bunit_panel_toggle_comparison_status_thread. */
void bunit_panel_toggle_equipment_and_numeric_editor_threads(s32 enable) {
    if (enable != 0) {
        if (g_bunit_status_display_flags & (MENU_STATUS_PANEL_SLIDE_DOWN | MENU_STATUS_PANEL_SLIDE_UP)) {
            return;
        }
        if (thread_is_running(THREAD_SCREEN_SLOT_CHARACTER_STATUS) != 0) {
            return;
        }
        thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_equipment_ability_panel_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, &g_bunit_character_status_frame_config, 0, 0);
        thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_frame_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, &g_bunit_numeric_editor_thread_params, 0, 0);
    } else {
        menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
    }
}
