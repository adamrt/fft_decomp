#include "fft/open.h"
#include "fft/thread.h"

/* Handler 8: once the message thread has ended, give back the blocked buttons,
 * pop the controller and, with run_followup, restore the text section pointers
 * open_controller_start_text_message replaced. */
void open_controller_wait_text_message(open_controller_record_t* record) {
    if (thread_is_running(THREAD_SCREEN_SLOT_MESSAGE) != 0) {
        return;
    }
    g_open_menu_blocked_buttons = record->text_message.saved_blocked_buttons;
    g_open_current_controller_index--;
    if (record->text_message.run_followup != 0) {
        text_restore_section_pointers();
    }
}
