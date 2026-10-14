#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu.h"
#include "fft/battle_text.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"

/* Wait for menu/event threads, then start the selected system-function entry. */
void menu_run_system_function_thread(void) {
    s32 id;
    s32 menu_id;
    menu_system_function_t* entry;

    g_menu_system_function_thread_busy = 1;
    text_init_section_pointers();
    id = g_thread_contexts[g_thread_current_id].function_parameter_1;
    if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
        id = g_menu_system_function_table[id].alternate_id;
    }
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        while (battle_thread_is_running_asm(THREAD_SLOT_EVENT) != 0) {
            if (g_thread_contexts[THREAD_SLOT_EVENT].task_id == THREAD_TASK_EXECUTE_EVENT) {
                thread_exit_current();
            }
            thread_yield();
        }
    }
    camera_wait_until_idle();
    entry = &g_menu_system_function_table[id];
    menu_id = entry->menu_id;
    g_menu_thread_menu_data[entry->menu_entry_index].text_id = entry->text_id;
    g_menu_system_function_row_actions[0] = entry->row_action;
    g_menu_current_id = menu_id;
    g_menu_thread_menu_data[entry->menu_entry_index].thread_count = entry->thread_count;
    if (id == 0x1f) {
        thread_start(THREAD_SLOT_MENU, menu_open_option_dead_unit_panel);
        thread_set_parameters(THREAD_SLOT_MENU, 0, g_menu_dead_unit_panel_parameter, 0);
        thread_exit_current();
    }
    if (id != 8) {
        script_pulse_tutorial_wait_value(id);
    }
    thread_start(THREAD_SLOT_MENU, entry->thread_entry);
    thread_set_four_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[entry->menu_entry_index], 0, 0, 0);
    g_menu_system_function_thread_busy = 0;
    thread_exit_current();
}
