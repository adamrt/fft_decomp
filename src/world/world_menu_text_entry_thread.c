#include "fft/script.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Menu thread body: runs the entry's text binding hook, starts the text
 * thread for it, then idles until input arrives or the previous thread's
 * third parameter is raised. */
void world_menu_text_entry_thread(void) {
    thread_t* thread = &g_threads[g_thread_current_id];
    menu_entry_t* entry = (menu_entry_t*)thread->function_parameter_1;
    menu_text_binding_t* binding;
    s32 text_thread;
    u16 second;
    s32 speaker_id;
    u16 text_id;
    menu_text_binding_t* prepare_binding;

    prepare_binding = entry->context;
    prepare_binding->prepare();
    g_world_menu_list_controller_input = script_get_controller_input_pointer(0);
    text_id = entry->text_id;
    second = entry->header_id;
    binding = entry->context;
    text_thread = binding->text_thread_id;
    speaker_id = binding->speaker_id;
    thread_start(text_thread, world_text_character_handling_thread);
    thread_set_parameters(text_thread, second, text_id, speaker_id);
    thread_wait_until_inactive(text_thread);
    *g_world_menu_list_controller_input = PSX_PAD_CIRCLE;
    while (1) {
        menu_handle_action(entry, 0);
        if (menu_should_close_thread(g_world_menu_list_controller_input) != 0) {
            break;
        }
        if ((thread - 1)->function_parameter_3 != 0) {
            break;
        }
        thread_wait_frames(1);
    }
    thread_exit_current();
}
