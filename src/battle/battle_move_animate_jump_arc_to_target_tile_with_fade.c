/*
 * Sibling of battle_move_animate_jump_arc_to_own_tile (0x8008a118): the same
 * distortion-animation shape (timer/velocity copy, switch on
 * distortion_phase, velocity write-back then
 * battle_unit_set_screen_coords_from_real_coords), with a palette fade around
 * the arc and a second saved field (distortion_target) used as the frame threshold.
 */
#include "fft/battle.h"
#include "psx/types.h"

void battle_move_animate_jump_arc_to_target_tile_with_fade(battle_unit_misc_data_t* unit) {
    /* Unreferenced 8-byte frame slot below the target point, like the one in
     * battle_move_animate_jump_arc_to_own_tile; it puts `target` at sp+0x20. */
    battle_screen_coords_t unused_18;
    battle_screen_coords_t target;
    VECTOR velocity;
    s32 timer;
    s32 threshold;

    timer = unit->distortion_timer;
    threshold = unit->distortion_target;
    velocity = unit->velocity;
    switch (unit->distortion_phase) {
    case 0:
        target.x = unit->map_x * 28 + 14;
        target.y = unit->map_y * 28 + 14;
        target.z = battle_gfx_calculate_screen_z_from_misc_map_data(unit);
        timer
            = battle_move_calculate_jump_arc_velocity((const battle_screen_coords_t*)&unit->screen, &target, &velocity);
        unit->shadow_graphic_trigger = 0;
        unit->sprite_display_flags.half |= 0x61;
        battle_gfx_start_misc_unit_palette_modulation(4, 1, unit->unit_id, -31, -31, -31);
        threshold = 4;
        unit->distortion_phase++;
    case 1:
        if (timer > threshold) {
            unit->real.vx += velocity.vx;
            unit->real.vy += velocity.vy;
            unit->real.vz += velocity.vz;
            velocity.vy += g_battle_move_jump_gravity;
        } else {
            battle_gfx_start_misc_unit_palette_modulation(8, 1, unit->unit_id, 0, 0, 0);
            unit->distortion_phase++;
        }
        timer--;
        break;
    case 2:
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
    case 3:
        main_util_set_svector((SVECTOR*)&target, unit->map_x * 28 + 14, 0, unit->map_y * 28 + 14);
        target.z = battle_gfx_calculate_screen_z_from_misc_screen_data(unit);
        if (unit->screen.vy < target.z) {
            unit->real.vy += velocity.vy;
            velocity.vy += g_battle_move_jump_gravity;
        } else {
            battle_unit_set_real_coords_from_map_coords(unit);
            unit->shadow_graphic_trigger = 1;
            unit->status_flags_5_6 &= ~BATTLE_MISC_STATUS_JUMP_HEIGHT_ACTIVE;
            unit->distortion_animation_id = 0;
            unit->sprite_display_flags.half &= 0xfffe;
        }
        break;
    }
    unit->velocity = velocity;
    battle_unit_set_screen_coords_from_real_coords(unit);
    unit->distortion_timer = timer;
    unit->distortion_target = threshold;
}
