#include "fft/event.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Provisional: menu-descriptor view used by this thread. The record at +0x30
 * begins with the setup callback and carries the worker thread id at +0x06. */
typedef struct world_menu_list_task {
    void (*setup)(void); /* 0x00 */
    u16 param_3;         /* 0x04 */
    u16 thread_id;       /* 0x06 */
} world_menu_list_task_t;

typedef struct world_menu_list_param {
    u8 unknown_00[0x1c];
    u16 param_2; /* 0x1c */
    u8 unknown_1e[0x2c - 0x1e];
    u16 param_1; /* 0x2c */
    u8 unknown_2e[0x30 - 0x2e];
    world_menu_list_task_t* task; /* 0x30 */
} world_menu_list_param_t;

void world_menu_text_entry_wait_thread(void) {
    world_menu_list_param_t* param;
    world_menu_list_task_t* task;
    u16 thread_id;
    u16 first;
    u16 second;
    u16 third;

    param = (world_menu_list_param_t*)g_world_threads[g_world_thread_current_id].function_parameter_1;
    param->task->setup();
    g_world_input_frame_controller_input = world_input_get_menu_controller(0);
    second = param->param_2;
    task = param->task;
    first = param->param_1;
    thread_id = task->thread_id;
    third = task->param_3;
    *g_world_input_frame_controller_input = PSX_PAD_CIRCLE;
    world_menu_handle_entry_confirm((world_menu_entry_t*)param, 0);
    world_thread_start(thread_id, world_text_character_handling_thread);
    world_thread_set_parameters_4(thread_id, first, second, third, third);
    world_thread_wait_until_inactive(g_world_thread_current_id - 1);
    world_thread_wait_until_inactive(thread_id);
    world_thread_exit_current();
}
