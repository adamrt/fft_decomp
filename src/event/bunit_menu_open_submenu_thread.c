#include "fft/battle_text.h"
#include "fft/bunit.h"
#include "fft/thread.h"
#include "psx/pad.h"
#include "psx/types.h"

void bunit_menu_open_submenu_thread(void) {
    world_menu_text_entry_wait_param_t* obj;
    world_menu_text_entry_wait_task_t* task;
    u32* ctl;
    u16 id;
    u16 arg;
    u16 unknown_2c;
    u16 unknown_1c;

    obj = (world_menu_text_entry_wait_param_t*)g_battle_threads[g_battle_current_thread_id].function_parameter_1;
    obj->task->setup();
    ctl = battle_script_get_controller_input_pointer(0);
    unknown_1c = obj->text_id;
    task = obj->task;
    unknown_2c = obj->header_id;
    g_bunit_input_controller = ctl;
    id = task->text_thread_id;
    arg = task->text_parameter;
    *ctl = PSX_PAD_CIRCLE;
    battle_menu_handle_action(obj, 0);
    battle_thread_start(id, battle_text_character_handling_thread);
    battle_thread_set_parameters_4(id, unknown_2c, unknown_1c, arg, arg);
    battle_thread_wait_until_inactive(g_battle_current_thread_id - 1);
    battle_thread_wait_until_inactive(id);
    battle_thread_exit_current();
}
