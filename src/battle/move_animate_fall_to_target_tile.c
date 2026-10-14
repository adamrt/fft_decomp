#include "fft/battle.h"
#include "psx/types.h"

/* The target sets $a1 to the coordinate buffer before this call too, so the
 * original declared both screen-z helpers with the same two arguments. */
#define screen_z_from_unit_screen_coords                                                                               \
    ((s32 (*)(battle_unit_t*, const SVECTOR*))battle_gfx_calculate_world_height_from_world_coords)

/* Distortion animation 0x0d: drop the unit straight down until it reaches the
 * ground.
 *
 * Phase 0 sets the fall speed to g_move_jump_gravity, starts the six
 * position copies that draw the sprite's trail and shows the shadow. Phase 1
 * takes the landing height each frame: during an event the ground under the
 * unit's own tile, otherwise the ground under the first target's tile, or under
 * the action target tile when the unit has no target. Above that height the unit
 * falls and gains g_move_jump_gravity per frame; on reaching it the unit
 * snaps to it, the position copies stop and the animation ends.
 *
 * During an event the unit is also centerd on its tile, but retail writes the
 * depth coordinate into real.vy instead of real.vz. The height then sits below
 * the ground on the next frame, so an event fall ends on its second frame. */
void move_animate_fall_to_target_tile(battle_unit_t* unit) {
    SVECTOR coords;
    /* Unreferenced 8-byte frame slot above `coords`; it puts `velocity` at
     * sp+0x20, matching the sibling distortion handlers. */
    SVECTOR unused_18;
    VECTOR velocity;
    battle_unit_t* other;
    unit_stats_t* stats;
    s32 timer;
    s32 x;
    s32 y;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        velocity.vy = g_move_jump_gravity;
        battle_gfx_init_position_vector_copies(unit);
        unit->shadow_graphic_trigger = 1;
        unit->distortion_phase++;
    case 1:
        if (g_battle_game_state == BATTLE_GAME_STATE_EVENT) {
            unit->strike.target_count = 0;
            x = unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            unit->world_coords.vx = x;
            unit->real.vx = x * ONE;
            y = unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
            unit->world_coords.vz = y;
            /* Retail stores the tile-center depth into real.vy (the height),
             * not real.vz. The first frame adds the fall speed to it and the
             * next one snaps the height to the ground. */
            unit->real.vy = y * ONE;
            main_util_set_svector(&coords, unit->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), unit->map_z,
                unit->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
            coords.vy = screen_z_from_unit_screen_coords(unit, &coords);
        } else {
            if (unit->strike.target_count != 0) {
                other = battle_unit_get_by_battle_id(unit->strike.target_list[0]);
                main_util_set_svector(&coords, other->map_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2), other->map_z,
                    other->map_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
            } else {
                stats = unit->stats;
                main_util_set_svector(&coords,
                    stats->action_command.target_position.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2),
                    stats->action_command.target_position.vy,
                    stats->action_command.target_position.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
            }
            coords.vy = battle_gfx_calculate_world_height_with_caller_coords(unit, &coords);
        }
        if (unit->world_coords.vy < coords.vy) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit->real.vy = coords.vy * ONE;
            unit->position_copies_active = 0;
            unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
        }
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}
