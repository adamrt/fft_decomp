/*
 * Sibling of battle_move_animate_jump_start (0x8008a35c) and
 * battle_move_animate_jump_arc_to_own_tile (0x8008a118): same
 * distortion-animation shape.
 */
#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/main_gfx.h"
#include "psx/types.h"

/* The target sets $a1 to the coordinate buffer before this call too, so the
 * original declared both screen-z helpers with the same two arguments. */
#define screen_z_from_misc_screen_data                                                                                 \
    ((s32 (*)(                                                                                                         \
        battle_unit_misc_data_t*, const battle_screen_coords_t*))battle_gfx_calculate_screen_z_from_misc_screen_data)

void battle_move_animate_fall_to_target_tile(battle_unit_misc_data_t* unit) {
    battle_screen_coords_t coords;
    /* Unreferenced 8-byte frame slot above `coords`; it puts `velocity` at
     * sp+0x20, matching the sibling distortion handlers. */
    battle_screen_coords_t unused_18;
    VECTOR velocity;
    battle_unit_misc_data_t* other;
    battle_stats_t* stats;
    s32 timer;
    s32 x;
    s32 y;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        velocity.vy = g_battle_move_jump_gravity;
        battle_gfx_init_position_vector_copies(unit);
        unit->shadow_graphic_trigger = 1;
        unit->distortion_phase++;
    case 1:
        if (g_battle_game_state == BATTLE_GAME_STATE_EVENT) {
            unit->target_count = 0;
            x = unit->map_x * 28 + 14;
            unit->screen.vx = x;
            unit->real.vx = x << 12;
            y = unit->map_y * 28 + 14;
            unit->screen.vz = y;
            /* Target really stores the map-Y fixed-point word into real.vy
             * (0x01c), not real.vz (0x020); the height branch below overwrites
             * it, so this looks like an original bug. */
            unit->real.vy = y << 12;
            main_util_set_svector((SVECTOR*)&coords, unit->map_x * 28 + 14, unit->map_z, unit->map_y * 28 + 14);
            coords.z = screen_z_from_misc_screen_data(unit, &coords);
        } else {
            if (unit->target_count != 0) {
                other = battle_unit_get_misc_data_by_battle_id(unit->target_list[0]);
                main_util_set_svector((SVECTOR*)&coords, other->map_x * 28 + 14, other->map_z, other->map_y * 28 + 14);
            } else {
                stats = unit->battle_data;
                main_util_set_svector((SVECTOR*)&coords, stats->action_target_x * 28 + 14,
                    stats->action_target_elevation, stats->action_target_y * 28 + 14);
            }
            coords.z = battle_gfx_calculate_screen_z_with_caller_data(unit, &coords);
        }
        if (unit->screen.vy < coords.z) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_battle_move_jump_gravity;
        } else {
            unit->real.vy = coords.z << 12;
            unit->position_copies_active = 0;
            unit->distortion_animation_id = 0;
        }
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_screen_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}
