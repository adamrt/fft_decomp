/* Unit-moving setup, previous-state save and move confirm. */

#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_move.h"
#include "fft/battle_target.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

void battle_state_enter_unit_moving_setup(void) {
    battle_unit_t* src;
    battle_unit_t* cast;
    battle_unit_t* at_tile;
    u8 path_count;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_UNIT_MOVING_SETUP;
    src = battle_unit_get_source();
    cast = battle_unit_get_casting();

    if (cast->path.pending_mount & BATTLE_PENDING_MOUNT_SET) {
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, 1, src->stats->battle_id, 0,
            src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        return;
    }
    path_count = cast->path.count + 2;
    if (path_count < 2) {
        if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, 0, src->stats->battle_id, 0,
                src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
            return;
        }
        menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM, 0, src->stats->battle_id, 0,
            src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        return;
    }
    at_tile
        = battle_unit_get_crystal_or_treasure_at_map_coords(g_battle_cursor_x, g_battle_cursor_y, g_battle_cursor_z);
    if (at_tile != 0) {
        if (at_tile->status_flags_5_6 & BATTLE_UNIT_STATUS_CRYSTAL) {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, 4, src->stats->battle_id, 0,
                src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
            return;
        }
    }
    menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_FEEDBACK, 0, src->stats->battle_id, 0,
        src->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
}

/* Saves the unit's tile, facing, mount link and stood-on unit before it moves;
 * move_return_to_previous_map_square restores them. */
void battle_unit_save_previous_state(battle_unit_t* unit) {
    u8 x = unit->map_x;
    u8 y = unit->map_y;
    u8 z = unit->map_z;
    u16 facing = unit->facing;
    u8 mount_value = unit->mount_state;
    u8 mount_id = unit->mount_partner_runtime_id;
    u8 standing_on_battle_id_plus_one = unit->standing_on_battle_id_plus_one;

    unit->previous_map_x = x;
    unit->previous_map_y = y;
    unit->previous_map_z = z;
    unit->previous_facing = facing;
    unit->previous_mount_state = mount_value;
    unit->previous_mount_partner_runtime_id = mount_id;
    unit->previous_standing_on_battle_id_plus_one = standing_on_battle_id_plus_one;
}

/* Enter the unit-moving state: save the casting unit's tile, facing and mount
 * state for an undo (and its partner's when it is a mount) and start its path
 * from the first step at speed 0x2000. */
void battle_state_enter_unit_moving(void) {
    battle_unit_t* unit;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_UNIT_MOVING;
    target_store_cursor_unit_name_and_data();
    unit = battle_unit_get_casting();
    battle_unit_save_previous_state(unit);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
        battle_unit_t* mount = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (mount != 0) {
            battle_unit_save_previous_state(mount);
        }
    }
    unit->step_speed = 0x2000;
    unit->movement_path_offset = 0;
    g_battle_state_animation_continue_check = 0;
    action_clear_at_list_active();
}

/* Enter the move-confirm state once the unit has stopped moving and show the
 * matching prompt, with a one-second message time.
 *
 * A failed Teleport shows effect message 0x183b; a successful one the move
 * confirmation. Other movement tints the unit by team and asks for confirmation,
 * silently when navigation messages are off. The AT-list-active flag follows
 * whether the player controls the unit. */
void battle_menu_open_move_confirm(void) {
    battle_unit_t* source;
    battle_unit_t* caster;

    camera_disable_map_pan();
    g_battle_game_state = BATTLE_GAME_STATE_MOVE_CONFIRM_MENU;
    source = battle_unit_get_source();
    caster = battle_unit_get_casting();
    text_set_message_duration_frames(0x3C);
    if (caster->path.flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT) {
        if (caster->last_path_count == MOVE_PATH_TELEPORT_FAILED) {
            menu_init_system_function(
                BATTLE_MENU_SYSTEM_COMMAND_EFFECT_MESSAGE, 0x183B, source->stats->battle_id, 0, 1);
        } else {
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM, 0, source->stats->battle_id, 0, 0);
        }
        caster->last_path_count = 0;
    } else {
        if (g_main_game_options.fields.navigation_messages != GAME_OPTION_ON) {
            battle_gfx_modify_palette_based_on_team(source, 2);
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM_SILENT, 0, source->stats->battle_id, 0,
                source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        } else {
            battle_gfx_modify_palette_based_on_team(source, 2);
            menu_init_system_function(BATTLE_MENU_SYSTEM_COMMAND_MOVE_CONFIRM, 0, source->stats->battle_id, 0,
                source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED);
        }
    }
    if (source->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED) {
        action_set_at_list_active();
    } else {
        action_clear_at_list_active();
    }
}
