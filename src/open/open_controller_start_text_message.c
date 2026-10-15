#include "fft/open.h"
#include "fft/thread.h"

/* Start the text message thread and push the controller that waits for it. */
void open_controller_start_text_message(s32 parameter, s32 run_followup) {
    {
        s32 controller = g_open_current_controller_index;

        g_open_controller_records_next[controller].text_message.run_followup = run_followup;
    }
    if (run_followup != 0) {
        text_save_section_pointers();
        text_relocate_pointer_table(g_open_text_section_offsets);
    }

    thread_start(THREAD_SCREEN_SLOT_MESSAGE, text_run_dialog_thread_variant);
    thread_set_parameters(THREAD_SCREEN_SLOT_MESSAGE, 0x33, parameter, 0);

    {
        s32 controller = g_open_current_controller_index;
        s32 saved_blocked_buttons = g_open_menu_blocked_buttons;

        g_open_menu_blocked_buttons = -1;
        g_open_controller_records_next[controller].text_message.saved_blocked_buttons = saved_blocked_buttons;
    }
    main_sound_play_sfx(MAIN_SFX_WINDOW_OPEN);

    {
        s32 controller = g_open_current_controller_index;
        s32 next_controller = controller + 1;

        g_open_controller_handler_indices[controller] = OPEN_CONTROLLER_TEXT_MESSAGE;
        g_open_current_controller_index = next_controller;
    }
}
