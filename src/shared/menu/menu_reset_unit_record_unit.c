#include "fft/battle_menu.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"

/* Reset one unit's menu record while the unit is not under auto-battle control. */
void menu_reset_unit_record(s32 battle_id) {
    unit_stats_t* stats;
    menu_unit_selection_record_t* records;
    s32 i;

    stats = battle_unit_get_stats_from_battle_id(battle_id);
    records = g_menu_unit_selection_records;
    for (i = 0; i < 17; i++) {
        if (stats->auto_battle_setting == AI_AUTOBATTLE_MANUAL) {
            ((u8*)&records[battle_id])[i] = 0;
            g_menu_unit_selection_records[battle_id]._unknown_01 = 0xFF;
        }
    }
}

ai_command_t* menu_get_selected_command_address(void) {
    return &g_menu_selected_command;
}

/* Post the close command after two frames under the mini-menu task id. */
void menu_mini_menu_cancel_delay_thread(void) {
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    thread_wait_frames(2);
    g_menu_pending_selection[0] = 7;
    thread_exit_current();
}

/* Queue the confirm sound and hold the mini-menu task id for 16 frames. */
void menu_confirm_sound_delay_thread(void) {
    g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    thread_wait_frames(16);
    thread_exit_current();
}
