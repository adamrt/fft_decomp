#include "fft/battle.h"

/* Enters the status-execute state for the source unit and shows its message:
 * action type 0x500 shows message 0x183a with animation 0x16; otherwise the
 * statuses to add select message 0x183d, 0x183c or none, followed by the
 * post-action display. */
void battle_state_enter_status_execute(void) {
    battle_unit_misc_data_t* unit;
    u32 statuses;

    battle_state_stop_game_flow();
    g_battle_game_state = BATTLE_GAME_STATE_STATUS_EXECUTE;
    unit = battle_unit_get_source_misc_data();
    battle_action_clear_at_list_active();
    g_battle_action_post_action = 0;
    if (g_action_type == BATTLE_TURN_EVENT_MIME) {
        battle_text_set_message_duration_frames(0x3C);
        battle_menu_init_system_function(
            0xA, 0x183A, unit->battle_data->misc_unit_id, unit->battle_data->misc_unit_id, 1);
        battle_unit_store_animation_facing_movement_data(0x16, (s16)unit->facing, unit);
        return;
    }
    battle_text_set_message_duration_frames(0xB4);
    statuses = unit->statuses_to_add_5_6;
    if (statuses & BATTLE_MISC_STATUS_CRYSTAL) {
        battle_menu_init_system_function(
            0xA, 0x183D, unit->battle_data->misc_unit_id, unit->battle_data->misc_unit_id, 1);
    } else if (statuses & BATTLE_MISC_STATUS_TREASURE) {
        battle_menu_init_system_function(
            0xA, 0x183C, unit->battle_data->misc_unit_id, unit->battle_data->misc_unit_id, 1);
    } else {
        battle_menu_init_system_function(0xA, 0, unit->battle_data->misc_unit_id, 0, 0);
    }
    battle_unit_update_display_by_misc_id(unit->unit_id);
    battle_gfx_prepare_post_action_display_by_misc_id(unit->unit_id);
}
