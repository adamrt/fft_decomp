#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_text.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/pad.h"

/*
 * Menu thread body for an entry that shows a message: runs the text
 * binding's prepare hook, forces a Circle press so the menu action fires at
 * once, starts the text thread named by the entry's text binding with the entry's
 * header and text ids and the binding's speaker, then waits for the previous
 * thread slot and the text thread to end.
 */
void menu_text_entry_wait_thread(void) {
    menu_entry_t* obj;
    menu_text_binding_t* task;
    u32* ctl;
    u16 id;
    u16 arg;
    u16 header_id;
    u16 text_id;

    obj = (menu_entry_t*)g_threads[g_thread_current_id].function_parameter_1;
    {
        menu_text_binding_t* binding = obj->context;

        binding->prepare();
    }
    ctl = script_get_controller_input_pointer(0);
    text_id = obj->text_id;
    task = obj->context;
    header_id = obj->header_id;
    g_menu_input_controller = ctl;
    id = task->text_thread_id;
    arg = task->speaker_id;
    *ctl = PSX_PAD_CIRCLE;
    menu_handle_action(obj, 0);
    thread_start(id, text_run_dialog_thread_variant);
    thread_set_four_parameters(id, header_id, text_id, arg, arg);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_wait_until_inactive(id);
    thread_exit_current();
}
