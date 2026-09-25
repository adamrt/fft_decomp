#include "fft/event_equip.h"
#include "psx/pad.h"
#include "psx/types.h"

void equip_menu_open_submenu_thread(void) {
    world_menu_text_entry_wait_param_t* thread;
    world_menu_text_entry_wait_task_t* task;
    u32* input;
    s32 unknown_1c;
    s32 unknown_2c;
    u16 child_id;
    u16 task_arg;

    thread = (world_menu_text_entry_wait_param_t*)g_battle_threads[g_battle_current_thread_id].function_parameter_1;
    thread->task->setup();

    input = battle_script_get_controller_input_pointer(0);
    unknown_1c = thread->text_id;
    task = thread->task;
    unknown_2c = thread->header_id;
    g_equip_input_controller = input;
    child_id = task->text_thread_id;
    task_arg = task->text_parameter;
    *input = PSX_PAD_CIRCLE;
    battle_menu_handle_action(thread, 0);

    battle_thread_start(child_id, battle_text_character_handling_thread);
    battle_thread_set_parameters_4(child_id, unknown_2c, unknown_1c, task_arg, task_arg);
    battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
    battle_thread_wait_until_inactive(child_id);
    battle_thread_exit_current();
}
