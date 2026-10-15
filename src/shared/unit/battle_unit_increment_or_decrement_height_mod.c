#include "fft/battle.h"
#include "fft/battle_gfx.h"
#include "fft/battle_unit.h"

/* The callee returns (s16), and this caller treats the result as a 16-bit value
 * (sign-extending where it is widened). The shared prototype returns s32 for
 * callers that were matched against the full register. */
typedef s16 (*battle_screen_z_halfword_fn_t)(battle_unit_t* unit);

/* Raise a floating unit's height offset, or lower it back to the ground.
 *
 * A floating unit rises up to 12 and then bobs through
 * g_battle_unit_float_bob_offsets unless it is a mount's rider. Otherwise the offset decays to 0,
 * except during the crystal-learn state. The screen/real Z follow the lower
 * of the stored and recalculated heights. Jumping units whose height is
 * controlled by their jump animation are left untouched. */
void battle_unit_increment_or_decrement_height_mod(battle_unit_t* unit) {
    s8 wave[24];
    s16 screen_z;
    u32 phase;

    memcpy(wave, g_battle_unit_float_bob_offsets, sizeof(wave));
    if (unit->status_flags_5_6 & (BATTLE_UNIT_STATUS_JUMP | BATTLE_UNIT_STATUS_JUMP_HEIGHT_ACTIVE)) {
        return;
    }
    if (unit->movement.floating) {
        if (unit->depth_height_offset < 12) {
            unit->depth_height_offset += g_main_system_animation_speed;
        } else if (unit->mount_state != BATTLE_UNIT_MOUNT_STATE_RIDER) {
            phase = unit->float_bob_phase += g_main_system_animation_speed;
            unit->float_bob_offset = wave[phase % 48 / 2];
        } else {
            unit->float_bob_offset = 0;
        }
    } else if (g_battle_game_state != BATTLE_GAME_STATE_CRYSTAL_LEARN) {
        if (unit->depth_height_offset != 0) {
            unit->depth_height_offset -= g_main_system_animation_speed;
            if (unit->depth_height_offset < 0) {
                unit->depth_height_offset = 0;
            }
            screen_z = ((battle_screen_z_halfword_fn_t)battle_gfx_calculate_world_height_from_world_coords)(unit);
            unit->world_coords.vy = screen_z;
            unit->real.vy = screen_z * ONE;
        } else {
            unit->float_bob_offset = 0;
        }
    }
    screen_z = ((battle_screen_z_halfword_fn_t)battle_gfx_calculate_world_height_from_world_coords)(unit);
    if (unit->world_coords.vy > screen_z) {
        unit->world_coords.vy = screen_z;
        unit->real.vy = screen_z * ONE;
    }
}
