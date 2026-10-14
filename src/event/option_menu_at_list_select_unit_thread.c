#include "fft/event_option.h"
#include "fft/thread.h"

/* Close the AT list's worker set and restore the parent option threads. */
void option_menu_at_list_select_unit_thread(void) {
    g_menu_modal_open = 1;
    g_menu_overlay_state = 2;

    thread_suspend(g_thread_current_id + 1);
    thread_suspend(g_thread_current_id + 2);
    thread_suspend(g_thread_current_id + 3);
    thread_suspend(g_thread_current_id + 4);

    while ((g_option_menu_transition_timer = g_option_menu_transition_duration + 100),
        g_menu_at_list_selection_finished == 0) {
        thread_yield();
    }

    g_menu_at_list_selection_finished = 0;
    g_option_menu_transition_timer = 0;
    battle_unit_start_view_thread(UNIT_VIEW_MODE_NONE, 0xff, 0xff);

    thread_wait_until_inactive(THREAD_SLOT_HOVERED_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_RIGHT_UNIT_STATS);
    thread_wait_until_inactive(THREAD_SLOT_UNIT_SUMMARY_PANEL);
    thread_wait_until_inactive(THREAD_SLOT_TILE_INFO_PANEL);

    g_text_section_pointers[TEXT_SECTION_AT_LIST] = g_option_menu_at_list_text;
    thread_resume(g_thread_current_id + 1);
    thread_resume(g_thread_current_id + 2);
    thread_resume(g_thread_current_id + 3);
    thread_resume(g_thread_current_id + 4);

    g_menu_modal_open = 0;
    g_menu_overlay_state = 0;
    thread_exit_current();
}
