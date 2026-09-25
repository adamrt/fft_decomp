#include "fft/battle.h"

void battle_action_choose_facing_for_wait(void) {
    battle_unit_misc_data_t* mounted_unit;
    battle_unit_misc_data_t* unit;
    s32 facing;

    battle_state_stop_game_flow();
    unit = battle_unit_get_source_misc_data();
    facing = *(s16*)&unit->facing;
    g_battle_game_state = BATTLE_GAME_STATE_WAIT_DIRECTION;
    g_current_facing_direction = facing;
    unit->state_frame_counter = 0;
    if (unit->mount_state == BATTLE_MISC_MOUNT_STATE_RIDER) {
        mounted_unit = battle_unit_get_misc_data_by_misc_id(unit->mount_partner_misc_id);
        if (mounted_unit != 0) {
            battle_unit_decide_facing_direction(unit, *(s16*)&mounted_unit->facing);
            if (unit->battle_data != 0) {
                battle_unit_set_tile_position(unit->battle_data->misc_unit_id, unit->map_x, unit->map_y, unit->map_z,
                    (u8)(*(s16*)&mounted_unit->facing / 0x400));
            }
        }
    }
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) == 0) {
        unit->command_state.cursor.facing_hint = battle_ai_choose_wait_facing();
    }
}
