#include "fft/battle.h"
#include "fft/battle_camera.h"

/* Enters the status-execute state for the source unit and shows its message:
 * action type 0x500 shows message 0x183a with the kneeling animation; otherwise the
 * statuses to add select message 0x183d, 0x183c or none, followed by the
 * post-action display. */
void battle_state_enter_status_execute(void) {
    battle_unit_t* unit;
    u32 statuses;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_STATUS_EXECUTE;
    unit = battle_unit_get_source();
    action_clear_at_list_active();
    g_action_post_action = 0;
    if (g_action_turn_event_type == BATTLE_TURN_EVENT_MIME) {
        text_set_message_duration_frames(0x3C);
        menu_init_system_function(
            BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183A, unit->stats->battle_id, unit->stats->battle_id, 1);
        battle_unit_store_animation_facing_movement_data(BATTLE_UNIT_ANIMATION_KNEEL, unit->facing, unit);
        return;
    }
    text_set_message_duration_frames(0xB4);
    statuses = unit->statuses_to_add_5_6;
    if (statuses & BATTLE_UNIT_STATUS_CRYSTAL) {
        menu_init_system_function(
            BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183D, unit->stats->battle_id, unit->stats->battle_id, 1);
    } else if (statuses & BATTLE_UNIT_STATUS_TREASURE) {
        menu_init_system_function(
            BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183C, unit->stats->battle_id, unit->stats->battle_id, 1);
    } else {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0, unit->stats->battle_id, 0, 0);
    }
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
    battle_gfx_prepare_post_action_display_by_runtime_id(unit->runtime_id);
}
