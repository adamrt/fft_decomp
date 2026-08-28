#include "fft/battle.h"
#include "fft/option.h"
#include "psx/types.h"

void battle_menu_disable_move_act_based_on_menu_id(void) {
    s16* stored_actions;
    s32 id;
    s32 menu;

    id = battle_thread_get_current_parameter_1();
    stored_actions = &g_battle_menu_confirm_stored_unit_row_actions;
    *stored_actions = -2;
    if (battle_unit_get_attacker_data_pointer()->status_sets.current[4] & 8) {
        *stored_actions = -1;
        battle_sound_set_effect_to_invalid();
        battle_menu_icon_linked_entry_thread();
        battle_thread_exit_current();
    }
    menu = g_battle_menu_current_id;
    if (menu == 0xE || menu == 0x14 || menu == 0x21 || menu == 0x2F || menu == 0x31 || menu == 0x33) {
        battle_sound_set_effect_to_invalid();
        battle_thread_exit_current();
    }
    battle_sound_set_effect_to_confirm_checked();
    battle_menu_confirm_action_silently((void*)id);
    battle_thread_exit_current();
}
