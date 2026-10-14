#include "fft/battle_gfx.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "fft/map.h"
#include "psx/types.h"

/* Distortion animation 0x07: jump along an arc to the center of the unit's own
 * map tile.
 *
 * Phase 0 asks move_calculate_jump_arc_velocity for the velocity and frame
 * count from the current screen position to the tile center at ground height.
 * Phase 1 applies the velocity for that many frames, the height speed gaining
 * g_move_jump_gravity each frame, then stops the horizontal motion. Phase
 * 2 keeps falling until the unit reaches the ground under it, snaps it to the
 * tile and ends the animation. */
void move_animate_jump_arc_to_own_tile(battle_unit_t* unit) {
    /* Unreferenced 8-byte frame slot below the target point, like the one in
     * move_animate_jump_start; it puts `target` at sp+0x18. */
    SVECTOR unused_10;
    SVECTOR target;
    VECTOR velocity;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        target.vx = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        target.vz = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
        target.vy = battle_gfx_calculate_world_height_from_unit_tile(unit);
        timer = move_calculate_jump_arc_velocity(&unit->world_coords, &target, &velocity);
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_move_jump_gravity;
        } else {
            velocity.vz = 0;
            velocity.vx = 0;
            unit->distortion_phase++;
        }
        timer--;
        break;
    case 2:
        main_util_set_svector(&target, unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), 0,
            unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
        target.vy = battle_gfx_calculate_world_height_from_world_coords(unit);
        if (unit->world_coords.vy < target.vy) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            battle_unit_set_real_coords_from_map_coords(unit);
            unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
        }
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}
