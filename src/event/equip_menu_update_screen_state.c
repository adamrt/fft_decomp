#include "fft/equip.h"
#include "fft/main_sound.h"
#include "psx/types.h"

void equip_menu_update_screen_state(void) {
    s32 result;
    s32 state;
    s32 mode;
    s16* value_ptr;

    if (g_equip_menu_thread_running == 0) {
        if (battle_thread_is_running(5) == 0) {
            g_equip_command_menu.selected_index = 0;
            g_battle_menu_thread_menu_data = &g_equip_command_menu;
            battle_thread_start(5, equip_menu_run_thread);
            battle_thread_set_parameters(5, (s32)g_battle_menu_thread_menu_data, 0, 0);
            g_equip_thread_idle_stop_pending = 0;
            g_equip_menu_thread_running = 1;
        }
    }
    if (g_equip_thread_idle_stop_pending != 0) {
        battle_thread_suspend(4);
        g_equip_thread_idle_stop_pending = 0;
    }
    state = g_equip_menu_screen_state;
    if (state == 1) {
        value_ptr = &g_equip_menu_result_rows[0];
        mode = *value_ptr;
        g_equip_text_help_message_id = g_equip_command_menu.selected_index + 0x1029;
        if (mode == 1) {
            g_equip_sound_queued_effect_id = MAIN_SFX_INVALID;
            equip_menu_reset_selection_indices();
        } else if (mode != -1) {
            battle_thread_start(4, equip_thread_wait_forever);
            g_equip_menu_screen_state = *(u8*)value_ptr + 2;
            equip_menu_set_selection_value(0, 0);
            equip_menu_set_selection_value(1, 0);
            equip_menu_set_selection_value(2, 0);
            equip_menu_reset_selection_indices();
        }
        g_equip_menu_thread_running = battle_thread_is_running(5);
        if (g_equip_menu_thread_running == 0) {
            g_equip_menu_screen_state = 0xFF;
        }
        return;
    }
    switch (state) {
    case 2:
        result = equip_menu_run_equip_mode();
        break;
    case 4:
        result = equip_menu_run_remove_mode();
        break;
    case 5:
        result = equip_menu_update_equipment();
        break;
    }
    if (result == 0) {
        g_equip_menu_screen_state = 1;
        g_equip_thread_idle_stop_pending = 1;
    }
}
