#include "fft/battle_effect.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/sound.h"
#include "psx/types.h"

/*
 * Advance a walking unit by one frame: the elaborate sibling of
 * move_update_knockback_step, with the full 0x3c-entry
 * step-phase table instead of the knockback subset.
 *
 * The jump-start phases dismount any rider once the jump animation has run
 * out; the two teleport phases play the teleport. When no step is active the
 * path count selects what follows: MOVE_PATH_TELEPORT starts the
 * teleport, MOVE_PATH_TELEPORT_FAILED also clears the pending mount and
 * restarts the AI action, 0 or an exhausted path mounts the pending mount,
 * and otherwise the next path byte starts a step (flag 0x80 and 0x40 of
 * movement_flags pick their own starters, and bit 2 of the step byte picks the
 * climb speed).
 */
void move_update_path_step(battle_unit_t* unit) {
    u32 offset;
    u8 count;
    u8 flags;

    move_get_current_and_destination_tiles(unit, &g_move_current_tile, &g_move_destination_tile);
    g_move_step_value = unit->current_path_step;
    switch (unit->step_phase) {
    case MOVE_STEP_PHASE_JUMP_START_SOUTH:
    case MOVE_STEP_PHASE_JUMP_START_EAST:
    case MOVE_STEP_PHASE_JUMP_START_NORTH:
    case MOVE_STEP_PHASE_JUMP_START_WEST:
        if (unit->animation.wait == 0) {
            unit->step_phase++;
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_FALL, unit->facing, unit);
            sound_play_movement_sfx(unit, MAIN_SFX_JUMP);
            battle_unit_dismount_rider(unit);
        }
        break;
    case MOVE_STEP_PHASE_JUMP_LANDING_SOUTH:
    case MOVE_STEP_PHASE_JUMP_LANDING_EAST:
    case MOVE_STEP_PHASE_JUMP_LANDING_NORTH:
    case MOVE_STEP_PHASE_JUMP_LANDING_WEST:
        move_update_knockback_after_animation(unit);
        break;
    case MOVE_STEP_PHASE_JUMP_ASCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_ASCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_ASCENT_WEST:
        move_update_airborne_ascent_phase(unit);
        /* fallthrough */
    case MOVE_STEP_PHASE_JUMP_DESCENT_SOUTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_EAST:
    case MOVE_STEP_PHASE_JUMP_DESCENT_NORTH:
    case MOVE_STEP_PHASE_JUMP_DESCENT_WEST:
        move_finish_unit_step_at_tile_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_JUMP_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_EAST:
    case MOVE_STEP_PHASE_FLOAT_JUMP_NORTH:
    case MOVE_STEP_PHASE_FLOAT_JUMP_WEST:
        move_update_unit_step_to_destination_tile_entry_edge(unit);
        break;
    case MOVE_STEP_PHASE_WALK_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_CENTER_WEST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_CENTER_WEST:
        move_update_unit_step_to_destination_tile_center(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_CENTER_WEST:
        move_update_float_step_to_destination_tile_center(unit);
        break;
    case MOVE_STEP_PHASE_FLY_TO_CENTER_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_EAST:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_CENTER_WEST:
        move_update_unit_vertical_step_to_destination_tile_center(unit);
        break;
    case MOVE_STEP_PHASE_WALK_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_WALK_TO_EDGE_WEST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_CLIMB_TO_EDGE_WEST:
        move_update_walking_step_at_tile_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLOAT_TO_EDGE_WEST:
        move_update_float_step_to_current_tile_exit_edge(unit);
        break;
    case MOVE_STEP_PHASE_FLY_TO_EDGE_SOUTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_EAST:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_NORTH:
    case MOVE_STEP_PHASE_FLY_TO_EDGE_WEST:
        move_update_unit_step_to_current_tile_exit_edge(unit);
        break;
    case MOVE_STEP_PHASE_TELEPORT_START:
        move_update_teleport_start(unit);
        break;
    case MOVE_STEP_PHASE_TELEPORT_ANIMATION:
        move_finalize_path_after_animation(unit);
        break;
    }
    if (unit->step_phase == MOVE_STEP_PHASE_NONE) {
        count = unit->path.count;
        if (count == MOVE_PATH_TELEPORT) {
            battle_unit_dismount_rider_and_update_display(unit);
            move_start_teleport_step(unit);
        } else if (count == MOVE_PATH_TELEPORT_FAILED) {
            battle_unit_dismount_rider_and_update_display(unit);
            move_start_teleport_step(unit);
            unit->path.pending_mount = 0;
            ai_init_selected_action();
        } else if (count == 0) {
            battle_unit_mount_onto_pending_mount(unit);
        } else if (unit->movement_path_offset < count) {
            if (unit->movement_path_offset == count - 1) {
                move_set_unit_path_flag(unit);
            }
            offset = unit->movement_path_offset;
            unit->movement_path_offset = offset + 1;
            unit->current_path_step = g_move_step_value = unit->path.data.steps[offset];
            move_set_unit_step_slope_scales(unit);
            flags = unit->path.flags;
            if (flags & BATTLE_EFFECTIVE_MOVEMENT_FLY) {
                move_start_fly_step(unit, g_move_current_tile, g_move_destination_tile);
            } else if (flags & BATTLE_EFFECTIVE_MOVEMENT_FLOAT) {
                move_start_float_step(unit, g_move_current_tile, g_move_destination_tile);
            } else {
                switch (g_move_step_value.slow) {
                case 0:
                    unit->step_speed = 0x2000;
                    move_start_unit_step(unit, g_move_current_tile, g_move_destination_tile);
                    break;
                case 1:
                    move_start_unit_step_at_climb_speed(unit, g_move_current_tile, g_move_destination_tile);
                    break;
                }
            }
        } else {
            battle_unit_mount_onto_pending_mount(unit);
        }
    }
}
