#include "fft/menu.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/types.h"

/* Start the help text thread for the current help message, if one is set:
 * save the text section pointers, bind the help text table and lock the menu
 * input. The dialog type is `thread_data` + 0x38. */
void menu_text_start_help_thread(s32 thread_data) {
    if (g_menu_help_message_text_id > 0) {
        text_save_section_pointers();
        text_relocate_pointer_table(g_help_text_table);
        thread_start(THREAD_SCREEN_SLOT_TEXT, text_run_dialog_thread_variant);
        thread_set_parameters(THREAD_SCREEN_SLOT_TEXT, thread_data + 0x38, g_menu_help_message_text_id, 0);
        g_menu_input_lock_mode = 1;
    }
}
