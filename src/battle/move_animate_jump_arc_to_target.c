#include "fft/battle_gfx.h"
#include "fft/battle_map.h"
#include "fft/battle_move.h"
#include "fft/battle_unit.h"
#include "fft/map.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Distortion animation 0x06: jump along an arc toward the first target's sprite
 * center (its screen position raised by half its spritesheet height), or to the
 * action target tile when the unit has no targets.
 *
 * Phase 0 builds the target point and asks
 * move_calculate_jump_arc_velocity for the velocity and frame count; a
 * unit without battle data ends the animation instead. Phase 1 applies the
 * velocity to all three real coordinates for that many frames, the height speed
 * gaining g_move_jump_gravity each frame, and ends the animation where the
 * arc leaves the unit. */
void move_animate_jump_arc_to_target(battle_unit_t* unit) {
    SVECTOR target;
    /* The retail frame reserves eight bytes beyond the target vector. */
    u8 unused_target_tail[8];
    VECTOR velocity;
    battle_unit_t* other;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->stats != 0) {
            if (unit->strike.target_count != 0) {
                other = battle_unit_get_by_battle_id(unit->strike.target_list[0]);
                setVector(&target, other->world_coords.vx,
                    other->world_coords.vy
                        - battle_gfx_get_unit_spritesheet_height_by_runtime_id(other->runtime_id) / 2,
                    other->world_coords.vz);
            } else {
                target.vx = unit->stats->action_command.target_position.vx * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                target.vz = unit->stats->action_command.target_position.vz * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2);
                target.vy = map_calculate_slope_height(&target, unit->stats->action_command.target_position.vy);
            }
            timer = move_calculate_jump_arc_velocity(&unit->world_coords, &target, &velocity);
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_move_jump_gravity;
        } else {
            unit->distortion_animation_id = BATTLE_DISTORTION_ANIMATION_NONE;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_world_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}
