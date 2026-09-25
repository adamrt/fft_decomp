/*
 * Distortion animation 0x06 (dispatcher 0x8008b234): jump along an arc toward
 * the first target's sprite centre, or the action target tile when the unit
 * has no targets. Phase 0 builds the target point and asks
 * battle_move_calculate_jump_arc_velocity for the velocity and frame count; phase 1
 * applies the velocity to all three real coordinates, accelerating the height
 * by g_battle_move_jump_gravity.
 */
#include "fft/battle.h"
#include "psx/types.h"

/* The target coordinates occupy a 16-byte frame slot, like the unreferenced
 * slot below the velocity copy in battle_move_animate_jump_start. */
typedef struct battle_move_jump_target {
    battle_screen_coords_t coords;
    u8 _unused_06[10];
} battle_move_jump_target_t;

void battle_move_animate_jump_arc_to_target(battle_unit_misc_data_t* unit) {
    battle_move_jump_target_t target;
    VECTOR velocity;
    battle_unit_misc_data_t* other;
    s32 timer;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        if (unit->battle_data != 0) {
            if (unit->target_count != 0) {
                other = battle_unit_get_misc_data_by_battle_id(unit->target_list[0]);
                target.coords.x = other->screen.vx;
                target.coords.z
                    = other->screen.vy - (s32)battle_gfx_get_unit_spritesheet_height_by_misc_id(other->unit_id) / 2;
                target.coords.y = other->screen.vz;
            } else {
                target.coords.x = unit->battle_data->action_target_x * 28 + 14;
                target.coords.y = unit->battle_data->action_target_y * 28 + 14;
                /* The target loads the elevation as an s16 halfword; the u8 prototype would load a byte. */
                target.coords.z = ((s16 (*)(battle_screen_coords_t*, s16))battle_map_calculate_slope_height)(
                    &target.coords, unit->battle_data->action_target_elevation);
            }
            timer = battle_move_calculate_jump_arc_velocity(
                (const battle_screen_coords_t*)&unit->screen, &target.coords, &velocity);
            unit->distortion_phase++;
        } else {
            unit->distortion_animation_id = 0;
            break;
        }
    case 1:
        if (timer > 0) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_battle_move_jump_gravity;
        } else {
            unit->distortion_animation_id = 0;
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_screen_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}
