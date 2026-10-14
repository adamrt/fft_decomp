#include "fft/battle.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "psx/types.h"

/*
 * Finishes a unit's step once it reaches the destination tile's entry edge:
 * drops the horizontal velocity, and either starts the next step (walk or
 * climb) or lands the unit, playing the landing sound and terrain effect.
 */
void move_finish_unit_step_at_tile_edge(battle_unit_t* unit) {
    s32 direction;
    s16 target_z;
    map_tile_t* tile;

    battle_gfx_load_trap_and_unit_frame_parts(
        unit, &unit->animation, unit->animation.frame, unit->animation.sequence_id);
    direction = move_get_direction(unit);
    move_apply_unit_step_velocity(unit);
    if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        move_displace_unit_at_destination_tile(unit, direction);
    }
    if (move_has_reached_destination_tile_entry_edge(direction, unit) == 0) {
        return;
    }
    unit->velocity.vz = 0;
    unit->velocity.vx = 0;
    target_z = battle_gfx_calculate_world_height_from_world_coords(unit);
    g_move_target_screen_z = target_z;
    if (unit->world_coords.vy < target_z) {
        return;
    }
    if (((unit->animation.sequence_id >> 1) - BATTLE_UNIT_ANIMATION_HOP) < 2U) {
        if (unit->velocity.vy < 0x6000) {
            unit->velocity.vy = 0;
            switch (g_move_step_value.climb) {
            case 0:
                move_start_unit_step(unit, g_move_current_tile, g_move_destination_tile);
                unit->step_phase = g_move_walk_to_center_phases[direction];
                break;
            case 1:
                move_start_unit_step_at_climb_speed(unit, g_move_current_tile, g_move_destination_tile);
                unit->step_phase = g_move_climb_to_center_phases[direction];
                break;
            }
            tile = map_get_tile_data_pointer(
                unit->movement.destination_x, unit->movement.destination_y, unit->movement.destination_z);
            if (tile->depth_half_height.depth == 0) {
                return;
            }
            if (-unit->world_coords.vy >= (tile->height + tile->depth_half_height.depth) * 12) {
                return;
            }
            sound_play_landing_sfx_by_surface_type(unit);
            effect_set_landing_secondary_by_surface_type(unit);
            return;
        }
        unit->velocity.vy = 0;
        unit->state_frame_counter = 2;
        unit->step_phase = g_move_jump_landing_phases[direction];
        if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
            battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_LAND, unit->facing, unit);
        }
        if (g_move_step_value.mount) {
            sound_play_movement_sfx(unit, MAIN_SFX_LAND_SOFT);
            return;
        }
        sound_play_landing_sfx_by_surface_type(unit);
        effect_set_landing_secondary_by_surface_type(unit);
        return;
    }
    unit->velocity.vy = 0;
    unit->state_frame_counter = 2;
    unit->step_phase = g_move_jump_landing_phases[direction];
    if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
        battle_unit_store_animation_facing(BATTLE_UNIT_ANIMATION_LAND, unit->facing, unit);
    }
    if (g_move_step_value.mount) {
        sound_play_movement_sfx(unit, MAIN_SFX_LAND_SOFT);
        return;
    }
    sound_play_landing_sfx_by_surface_type(unit);
    effect_set_landing_secondary_by_surface_type(unit);
}
