#include "fft/battle.h"
#include "fft/main_runtime.h"

/* 24 signed bob offsets copied to the stack from BATTLE rodata. */
typedef struct battle_height_wave {
    s8 offsets[24];
} battle_height_wave_t;

/* The callee returns (s16), and this caller treats the result as a halfword
 * (sign-extending where it is widened). The shared prototype returns s32 for
 * callers that were matched against the full register. */
typedef s16 (*battle_screen_z_halfword_fn_t)(battle_unit_misc_data_t* unit);

extern battle_height_wave_t g_battle_unit_float_bob_offsets;

/* Raise a floating unit's height offset, or lower it back to the ground.
 *
 * A floating unit rises up to 12 and then bobs through
 * g_battle_unit_float_bob_offsets unless it is a mount's rider. Otherwise the offset decays to 0,
 * except during the crystal-learn state. The screen/real Z follow the lower
 * of the stored and recalculated heights. Jumping units whose height is
 * controlled by their jump animation are left untouched. */
void battle_unit_increment_or_decrement_height_mod(battle_unit_misc_data_t* unit) {
    battle_height_wave_t wave;
    s16 screen_z;
    u32 phase;

    wave = g_battle_unit_float_bob_offsets;
    if (unit->status_flags_5_6 & (BATTLE_MISC_STATUS_JUMP | BATTLE_MISC_STATUS_JUMP_HEIGHT_ACTIVE)) {
        return;
    }
    if (unit->movement.word & BATTLE_MOTION_FLAG_FLOAT) {
        if ((s16)unit->depth_height_offset < 12) {
            unit->depth_height_offset += g_animation_speed;
        } else if (unit->mount_state != BATTLE_MISC_MOUNT_STATE_RIDER) {
            phase = unit->float_bob_phase += g_animation_speed;
            unit->mounted_height_offset = wave.offsets[phase % 48 / 2];
        } else {
            unit->mounted_height_offset = 0;
        }
    } else if (g_battle_game_state != BATTLE_GAME_STATE_CRYSTAL_LEARN) {
        if ((s16)unit->depth_height_offset != 0) {
            unit->depth_height_offset -= g_animation_speed;
            if ((s16)unit->depth_height_offset < 0) {
                unit->depth_height_offset = 0;
            }
            screen_z = ((battle_screen_z_halfword_fn_t)battle_gfx_calculate_screen_z_from_misc_screen_data)(unit);
            unit->screen.vy = screen_z;
            unit->real.vy = screen_z << 12;
        } else {
            unit->mounted_height_offset = 0;
        }
    }
    screen_z = ((battle_screen_z_halfword_fn_t)battle_gfx_calculate_screen_z_from_misc_screen_data)(unit);
    if (unit->screen.vy > screen_z) {
        unit->screen.vy = screen_z;
        unit->real.vy = screen_z << 12;
    }
}
