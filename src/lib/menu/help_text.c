/* Starting the help text thread for the current help message. */

#include "fft/menu/help.h"
#include "fft/menu/menu_input.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/text/text.h"
#include "psx/types.h"

/* Start the help text thread for the current help message, if one is set:
 * save the text section pointers, bind the help text table and lock the menu
 * input. The dialog type is HELP_DIALOG_TYPE at the position `thread_data` gives. */
void help_start_text(s32 thread_data) {
    if (g_help_message_text_id > 0) {
        text_save_section_pointers();
        text_bind_section_pointers(&g_help_text_bank);
        thread_start(THREAD_SCREEN_SLOT_TEXT, message_dialog_thread);
        thread_set_parameters(THREAD_SCREEN_SLOT_TEXT, thread_data + HELP_DIALOG_TYPE, g_help_message_text_id, 0);
        g_menu_input_lock_mode = 1;
    }
}
