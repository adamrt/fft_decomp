#include "fft/battle.h"
#include "psx/types.h"

void battle_move_update_float_step_to_destination_tile_center(battle_unit_misc_data_t* unit) {
    s32 direction;
    s16 screen_z;

    direction = battle_move_get_direction(unit);
    battle_move_apply_unit_step_velocity(unit);
    if (g_battle_game_state != BATTLE_GAME_STATE_EVENT) {
        battle_move_displace_overlapping_unit(unit, direction);
    }
    if (battle_move_has_reached_destination_tile_center(direction, unit) != 0) {
        battle_move_snap_axis_to_destination_tile_center(direction, unit);
        unit->velocity.vz = 0;
        unit->velocity.vx = 0;
        screen_z = battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
        g_battle_move_target_screen_z = screen_z;
        if (unit->screen.vy >= screen_z) {
            unit->map_x = unit->movement.bytes.destination_x;
            unit->map_y = unit->movement.bytes.destination_y;
            unit->velocity.vy = 0;
            unit->real.vy = g_battle_move_target_screen_z << 12;
            unit->screen.vy = unit->real.vy / ONE;
            unit->centre_tile_offset = 0;
            unit->shadow_dirty |= 1;
        }
    }
}
