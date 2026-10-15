#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Menu thread body for an entry that shows a message: runs the text
 * binding's prepare hook, confirms the entry, starts the text thread named by the
 * entry's text binding with the entry's header and text ids and the
 * binding's speaker, then waits for the previous thread slot and the text
 * thread to end.
 */
void world_menu_text_entry_wait_thread(void) {
    menu_entry_t* param;
    menu_text_binding_t* task;
    u16 thread_id;
    u16 first;
    u16 second;
    u16 third;
    menu_text_binding_t* binding;

    param = (menu_entry_t*)g_threads[g_thread_current_id].function_parameter_1;
    binding = param->context;
    binding->prepare();
    g_world_input_frame_controller_input = script_get_controller_input_pointer(0);
    second = param->text_id;
    task = param->context;
    first = param->header_id;
    thread_id = task->text_thread_id;
    third = task->speaker_id;
    *g_world_input_frame_controller_input = PSX_PAD_CIRCLE;
    menu_handle_action(param, 0);
    thread_start(thread_id, text_run_dialog_thread_variant);
    thread_set_four_parameters(thread_id, first, second, third, third);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_wait_until_inactive(thread_id);
    thread_exit_current();
}
