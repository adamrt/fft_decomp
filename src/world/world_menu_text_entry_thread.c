#include "fft/event.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef struct world_menu_confirm_entry world_menu_confirm_entry_t;
extern void world_menu_handle_entry_confirm(world_menu_confirm_entry_t* param, s32 value);

/* Menu thread body: runs the entry's text binding hook, starts the text
 * thread for it, then idles until input arrives or the previous thread's
 * third parameter is raised. */
void world_menu_text_entry_thread(void) {
    world_thread_t* thread = &g_world_threads[g_world_thread_current_id];
    world_menu_entry_t* entry = (world_menu_entry_t*)thread->function_parameter_1;
    s32 text_thread;
    s32 second;
    s32 speaker_id;
    s32 text_id;

    entry->text_binding->prepare();
    g_world_menu_list_controller_input = world_input_get_menu_controller(0);
    text_id = (u16)entry->text_id;
    second = entry->header_id;
    text_thread = entry->text_binding->text_thread_id;
    speaker_id = entry->text_binding->speaker_id;
    world_thread_start(text_thread, world_text_character_handling_thread);
    world_thread_set_parameters(text_thread, second, text_id, speaker_id);
    world_thread_wait_until_inactive(text_thread);
    *g_world_menu_list_controller_input = PSX_PAD_CIRCLE;
    while (1) {
        world_menu_handle_entry_confirm((world_menu_confirm_entry_t*)entry, 0);
        if (world_menu_check_thread_completion(g_world_menu_list_controller_input) != 0) {
            break;
        }
        if ((thread - 1)->function_parameter_3 != 0) {
            break;
        }
        world_thread_wait_frames(1);
    }
    world_thread_exit_current();
}
