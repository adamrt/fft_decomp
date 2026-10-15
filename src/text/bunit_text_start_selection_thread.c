#include "fft/event_bunit.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Start the text thread for g_bunit_text_selection_id (ids above 0x1FFFF are
 * stashed in g_bunit_help_screen_id instead); thread_data + 0x38 is the text
 * thread's dialog type, as in jobstts_text_start_help_thread. */
void bunit_text_start_selection_thread(s32 thread_data) {
    if (g_text_selection_id > 0x1FFFF) {
        g_bunit_help_screen_id = g_text_selection_id;
        return;
    }
    if (g_text_selection_id <= 0) {
        return;
    }
    text_save_section_pointers();
    text_relocate_pointer_table(g_help_text_table);
    thread_start(THREAD_SCREEN_SLOT_TEXT, text_run_dialog_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_TEXT, thread_data + 0x38, g_text_selection_id, 0);
    g_menu_input_lock_mode = 1;
}
