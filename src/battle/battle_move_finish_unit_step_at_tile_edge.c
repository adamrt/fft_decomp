#include "fft/battle.h"
#include "fft/battle_move.h"
#include "fft/map.h"
#include "psx/types.h"

/*
 * Finishes a unit's step once it reaches the destination tile's entry edge:
 * drops the horizontal velocity, and either starts the next step (walk or
 * climb) or lands the unit, playing the landing sound and terrain effect.
 */
void battle_move_finish_unit_step_at_tile_edge(battle_unit_misc_data_t* unit) {
    s32 direction;
    s16 target_z;
    map_tile_t* tile;

    battle_gfx_load_trap_and_unit_frame_parts(
        unit, (battle_unit_anim_state_t*)&unit->sprite_graphic_trigger, unit->animation_frame, unit->encoded_animation);
    direction = battle_move_get_direction(unit);
    battle_move_apply_unit_step_velocity(unit);
    if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        battle_move_displace_unit_at_destination_tile(unit, direction);
    }
    if (battle_move_has_reached_destination_tile_entry_edge(direction, unit) == 0) {
        return;
    }
    unit->velocity.vz = 0;
    unit->velocity.vx = 0;
    target_z = battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
    g_battle_move_target_screen_z = target_z;
    if (unit->screen.vy < target_z) {
        return;
    }
    if ((u32)((unit->encoded_animation >> 1) - 0x30) < 2U) {
        if (unit->velocity.vy < 0x6000) {
            unit->velocity.vy = 0;
            /* Keeps the g_battle_move_step_value load below this store. */
            __asm__ volatile("");
            switch ((g_battle_move_step_value >> 3) & 1) {
            case 0:
                /* The target passes the destination tile where the callee declares facing. */
                ((void (*)(battle_unit_misc_data_t*, const map_tile_t*, const map_tile_t*))battle_move_start_unit_step)(
                    unit, g_battle_move_current_tile, g_battle_move_destination_tile);
                unit->centre_tile_offset = g_battle_move_step_centre_offsets[direction];
                break;
            case 1:
                /* The target also passes the destination tile to this two-parameter callee. */
                ((void (*)(battle_unit_misc_data_t*, const map_tile_t*,
                    const map_tile_t*))battle_move_start_unit_step_at_climb_speed)(
                    unit, g_battle_move_current_tile, g_battle_move_destination_tile);
                unit->centre_tile_offset = g_battle_move_climb_step_centre_offsets[direction];
                break;
            }
            tile = battle_map_get_tile_data_pointer(unit->movement.bytes.destination_x,
                unit->movement.bytes.destination_y, unit->movement.bytes.destination_z);
            if ((tile->depth_half_height & MAP_TILE_DEPTH_MASK) == 0) {
                return;
            }
            if (-unit->screen.vy >= (tile->height + (tile->depth_half_height >> MAP_TILE_DEPTH_SHIFT)) * 12) {
                return;
            }
            battle_play_landing_sfx_by_tile_type(unit);
            battle_set_landing_secondary_effect_by_tile_type(unit);
            return;
        }
        unit->velocity.vy = 0;
        unit->state_frame_counter = 2;
        unit->centre_tile_offset = g_battle_move_landing_centre_offsets[direction];
        if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
            battle_unit_store_animation_facing(0x20, (s16)unit->facing, unit);
        }
        if (g_battle_move_step_value & 0x10) {
            battle_sound_play_movement_sfx(unit, 0x28);
            return;
        }
        battle_play_landing_sfx_by_tile_type(unit);
        battle_set_landing_secondary_effect_by_tile_type(unit);
        return;
    }
    unit->velocity.vy = 0;
    unit->state_frame_counter = 2;
    unit->centre_tile_offset = g_battle_move_landing_centre_offsets[direction];
    if (g_battle_game_state != BATTLE_GAME_STATE_ACTION_EXECUTE) {
        battle_unit_store_animation_facing(0x20, (s16)unit->facing, unit);
    }
    if (g_battle_move_step_value & 0x10) {
        battle_sound_play_movement_sfx(unit, 0x28);
        return;
    }
    battle_play_landing_sfx_by_tile_type(unit);
    battle_set_landing_secondary_effect_by_tile_type(unit);
}
