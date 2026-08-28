#include "fft/event.h"
#include "fft/main_runtime.h"
#include "fft/thread.h"
#include "fft/world.h"

/*
 * Thread started by world_menu_start_system_function_thread: waits for the menu
 * threads to settle, then configures the menu entry named by the selected
 * system-function record and starts that record's thread as thread 8.
 */
void world_menu_run_system_function_thread(void) {
    s32 id;
    s32 value;
    world_system_function_t* entry;

    g_world_system_function_thread_busy = 1;
    world_text_init_section_pointers();
    id = g_world_thread_contexts[g_world_thread_current_id].function_parameter_1;
    if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
        id = g_world_system_function_table[id].alternate_id;
    }
    world_thread_wait_until_inactive(8);
    if (g_world_menu_input_disabled == 0) {
        while (world_thread_is_running_80100164(1) != 0) {
            if (g_world_thread_contexts[1].task_id == NATIVE_THREAD_TASK_EXECUTE_EVENT) {
                world_thread_exit_current();
            }
            world_thread_yield();
        }
    }
    world_camera_run_move_thread();
    entry = &g_world_system_function_table[id];
    value = entry->value_00;
    g_world_menu_thread_menu_data[entry->menu_entry_index].text_id = entry->text_id;
    g_world_menu_system_function_row_actions[0] = entry->value_06;
    g_world_menu_current_id = value;
    g_world_menu_thread_menu_data[entry->menu_entry_index].field_0x20 = entry->value_08;
    if (id == 0x1f) {
        world_thread_start(8, world_menu_start_main_mode_2);
        world_thread_set_parameters(8, 0, D_801A668C, 0);
        world_thread_exit_current();
    }
    if (id != 8) {
        world_script_pulse_tutorial_wait_value(id);
    }
    world_thread_start(8, entry->thread_entry);
    world_thread_set_parameters_4(8, (s32)&g_world_menu_thread_menu_data[entry->menu_entry_index], 0, 0, 0);
    g_world_system_function_thread_busy = 0;
    world_thread_exit_current();
}
