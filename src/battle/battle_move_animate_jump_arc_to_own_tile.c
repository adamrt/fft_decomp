/*
 * Sibling of battle_move_animate_jump_arc_to_target (0x80089f24) and
 * battle_move_animate_jump_start (0x8008a35c): same distortion-animation
 * shape (timer/velocity copy, switch on distortion_phase, velocity write-back
 * then battle_unit_set_screen_coords_from_real_coords).
 */
#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_move.h"
#include "fft/main_gfx.h"
#include "psx/types.h"

void battle_move_animate_jump_arc_to_own_tile(battle_unit_misc_data_t* unit) {
    /* Unreferenced 8-byte frame slot below the target point, like the one in
     * battle_move_animate_jump_start; it puts `target` at sp+0x18. */
    battle_screen_coords_t unused_10;
    battle_screen_coords_t target;
    VECTOR velocity;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        target.x = unit->map_x * 28 + 14;
        target.y = unit->map_y * 28 + 14;
        target.z = battle_gfx_calculate_screen_z_from_misc_map_data(unit);
        timer
            = battle_move_calculate_jump_arc_velocity((const battle_screen_coords_t*)&unit->screen, &target, &velocity);
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_battle_move_jump_gravity;
        } else {
            velocity.vz = 0;
            velocity.vx = 0;
            unit->distortion_phase++;
        }
        timer--;
        break;
    case 2:
        main_util_set_svector((SVECTOR*)&target, unit->map_x * 28 + 14, 0, unit->map_y * 28 + 14);
        target.z = battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
        if (unit->screen.vy < target.z) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_battle_move_jump_gravity;
        } else {
            battle_unit_set_real_coords_from_map_coords(unit);
            unit->distortion_animation_id = 0;
        }
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_screen_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}
