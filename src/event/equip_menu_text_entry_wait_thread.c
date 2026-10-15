#include "fft/battle_script.h"
#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Menu thread body for an entry that shows a message: runs the text
 * binding's prepare hook, forces a Circle press so the menu action fires at
 * once, starts the text thread named by the entry's text binding with the entry's
 * header and text ids and the binding's speaker, then waits for the previous
 * thread slot and the text thread to end.
 */
void equip_menu_text_entry_wait_thread(void) {
    menu_entry_t* thread;
    menu_text_binding_t* task;
    u32* input;
    u16 text_id;
    u16 header_id;
    u16 child_id;
    u16 task_arg;

    thread = (menu_entry_t*)g_threads[g_thread_current_id].function_parameter_1;
    {
        menu_text_binding_t* binding = thread->context;

        binding->prepare();
    }

    input = script_get_controller_input_pointer(0);
    text_id = thread->text_id;
    task = thread->context;
    header_id = thread->header_id;
    g_menu_input_controller = input;
    child_id = task->text_thread_id;
    task_arg = task->speaker_id;
    *input = PSX_PAD_CIRCLE;
    menu_handle_action(thread, 0);

    thread_start(child_id, text_run_dialog_thread_variant);
    thread_set_four_parameters(child_id, header_id, text_id, task_arg, task_arg);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_wait_until_inactive(child_id);
    thread_exit_current();
}
