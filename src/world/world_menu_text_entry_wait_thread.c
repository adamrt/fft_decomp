#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

void world_menu_text_entry_wait_thread(void) {
    world_menu_text_entry_wait_param_t* param;
    world_menu_text_entry_wait_task_t* task;
    u16 thread_id;
    u16 first;
    u16 second;
    u16 third;

    param = (world_menu_text_entry_wait_param_t*)g_world_threads[g_world_thread_current_id].function_parameter_1;
    param->task->setup();
    g_world_input_frame_controller_input = world_input_get_menu_controller(0);
    second = param->text_id;
    task = param->task;
    first = param->header_id;
    thread_id = task->text_thread_id;
    third = task->text_parameter;
    *g_world_input_frame_controller_input = PSX_PAD_CIRCLE;
    world_menu_handle_entry_confirm((world_menu_entry_t*)param, 0);
    world_thread_start(thread_id, world_text_character_handling_thread);
    world_thread_set_parameters_4(thread_id, first, second, third, third);
    world_thread_wait_until_inactive(g_world_thread_current_id - 1);
    world_thread_wait_until_inactive(thread_id);
    world_thread_exit_current();
}
