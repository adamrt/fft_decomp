#include "fft/battle.h"
#include "psx/types.h"

/*
 * Distortion animation 0x0c: launch a unit straight up at the start of a jump.
 *
 * Phase 0 picks an initial rise speed of sqrt(2 * gravity * (0x120 - screen
 * z)) and the matching frame count; phase 1 then applies the velocity to the
 * height each frame, decelerating by g_battle_move_jump_gravity, and ends the animation when
 * the count runs out. `unused_10` reproduces an unreferenced 16-byte frame
 * slot below the velocity copy.
 */
void battle_move_animate_jump_start(battle_unit_misc_data_t* unit) {
    VECTOR unused_10;
    VECTOR velocity;
    s32 timer;
    s32 speed;

    timer = unit->distortion_timer;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        speed = SquareRoot12(
            (-battle_gfx_calculate_screen_z_from_misc_screen_data(unit) + 0x120) * g_battle_move_jump_gravity * 2);
        timer = speed / g_battle_move_jump_gravity;
        velocity.vy = -speed;
        battle_gfx_init_position_vector_copies(unit);
        unit->status_flags_5_6 |= BATTLE_MISC_STATUS_JUMP_HEIGHT_ACTIVE;
        unit->distortion_phase++;
    case 1:
        if (timer > 0) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_battle_move_jump_gravity;
        } else {
            unit->position_copies_active = 0;
            unit->shadow_graphic_trigger = 0;
            unit->distortion_animation_id = 0;
            battle_unit_store_animation_facing_movement_data(2, (s16)unit->facing, unit);
        }
        timer--;
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_screen_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
}
