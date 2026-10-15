#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "psx/types.h"

/* Put the unit back on the tile, facing and mount state saved before its move,
 * then refresh its screen position and animation facing. */
void move_return_to_previous_map_square(battle_unit_t* unit) {
    unit->map_x = unit->previous_map_x;
    unit->map_y = unit->previous_map_y;
    unit->map_z = unit->previous_map_z;
    unit->facing = unit->previous_facing;
    unit->mount_state = unit->previous_mount_state;
    unit->mount_partner_runtime_id = unit->previous_mount_partner_runtime_id;
    unit->standing_on_battle_id_plus_one = unit->previous_standing_on_battle_id_plus_one;
    battle_unit_set_move_and_world_coords(unit);
    battle_unit_set_tile_position(
        unit->stats->battle_id, unit->map_x, unit->map_y, unit->map_z, (unit->facing / BATTLE_FACING_QUARTER_TURN));
    battle_unit_store_animation_facing_movement_data(unit->animation.sequence_id >> 1, unit->facing, unit);
}

/* Cancel a move at the confirm prompt. The unit returns to its previous tile;
 * a mount takes its rider back with it, a unit that was riding is put back on
 * its mount, and a mount it climbed onto during the move is freed. */
void move_undo_unit_move(battle_unit_t* unit) {
    battle_unit_t* other;

    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        other->mount_state = BATTLE_UNIT_MOUNT_STATE_NONE;
        battle_unit_update_display_by_runtime_id(other->runtime_id);
    }
    move_return_to_previous_map_square(unit);
    if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (other != 0) {
            move_return_to_previous_map_square(other);
            battle_unit_update_display_by_runtime_id(other->runtime_id);
        }
    } else if (unit->mount_state == BATTLE_UNIT_MOUNT_STATE_RIDER) {
        unit->status_flags_1_4 |= BATTLE_UNIT_STATUS_MOUNTED;
        other = battle_unit_get_by_runtime_id(unit->mount_partner_runtime_id);
        if (other != 0) {
            other->mount_state = BATTLE_UNIT_MOUNT_STATE_MOUNT;
            other->mount_partner_runtime_id = unit->runtime_id;
            battle_unit_update_display_by_runtime_id(other->runtime_id);
        }
    } else {
        unit->status_flags_1_4 &= ~BATTLE_UNIT_STATUS_MOUNTED;
    }
    battle_unit_update_display_by_runtime_id(unit->runtime_id);
}

/*
 * After `rider` moves, re-registers its map position and, when it is the
 * rider of a mount (mount state MOUNT), copies tile, destination, real and
 * screen position to the mount 10 units / 0xa000 lower, then finishes the
 * acting unit's move. The two position blocks are copied through local block
 * types because the target uses struct copies (four lw/sw, and lwl/lwr for
 * the 16-bit-aligned screen block) that per-field stores do not reproduce.
 */

void battle_unit_copy_rider_data_to_mount(battle_unit_t* unit, battle_unit_t* rider) {
    battle_unit_t* mount;

    if (rider->stats != 0) {
        battle_unit_set_tile_position(
            rider->stats->battle_id, rider->map_x, rider->map_y, rider->map_z, (rider->facing / 1024));
        if (rider->mount_state == BATTLE_UNIT_MOUNT_STATE_MOUNT) {
            mount = battle_unit_get_by_runtime_id(rider->mount_partner_runtime_id);
            if (mount != 0) {
                mount->map_x = mount->movement.destination_x = rider->map_x;
                mount->map_y = mount->movement.destination_y = rider->map_y;
                mount->map_z = mount->movement.destination_z = rider->map_z;
                mount->real = rider->real;
                mount->world_coords = rider->world_coords;
                mount->world_coords.vy -= 10;
                mount->real.vy -= 0xa000;
                battle_unit_set_tile_position(
                    mount->stats->battle_id, rider->map_x, rider->map_y, rider->map_z, (rider->facing / 1024));
            }
        }
    }
    unit->turn_action_flags |= BATTLE_UNIT_TURN_ACTION_MOVED;
    action_set_only_movement_taken(unit->stats->battle_id);
    if ((unit->team_flags & BATTLE_TEAM_FLAG_PLAYER_CONTROLLED)
        && !(unit->path.flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT)) {
        main_sound_play_sfx(MAIN_SFX_CONFIRM);
    }
    battle_gfx_update_unit_palettes();
    move_init_post_movement_display();
}

/* MoveConfirmMenu game-state handler. Command 7 keeps the move and finishes it.
 * Command 8 or 0xff undoes it and returns to choosing a destination, except after
 * a teleport, which is kept like command 7. */
void battle_state_handle_move_confirm_menu_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    battle_unit_t* source_unit;
    battle_unit_t* casting_unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    source_unit = battle_unit_get_source();
    casting_unit = battle_unit_get_casting();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case 8:
    case 0xff:
        if (casting_unit->path.flags & BATTLE_EFFECTIVE_MOVEMENT_TELEPORT) {
        case 7:
            battle_unit_copy_rider_data_to_mount(source_unit, casting_unit);
            return;
        }
        move_undo_unit_move(casting_unit);
        battle_gfx_update_unit_palettes();
        battle_menu_close_move_help();
        return;
    }
}

/* WaitMenu game-state handler. Command 7 goes on to choosing a facing. Command 8
 * or 0xff returns to the after-command state, unless the unit's turn is over
 * (action_check_change_of_turn returns 1): then it chooses a facing too. */
void battle_state_handle_wait_menu_state(void) {
    ai_command_t* selected_command_address;
    s32 selected_command;
    battle_unit_t* source_unit;

    battle_state_handle_free_cursor_input();
    menu_update_menus_and_event_speed(main_gfx_get_otag(), g_input_current_buttons);
    selected_command_address = menu_get_selected_command_address();
    source_unit = battle_unit_get_source();
    selected_command = selected_command_address->kind;
    switch (selected_command) {
    case 8:
    case 0xff:
        if (action_check_change_of_turn(source_unit->stats->battle_id) != 1) {
            battle_state_enter_after_command();
            return;
        }
    case 7:
        action_choose_facing_for_wait();
        return;
    }
}
