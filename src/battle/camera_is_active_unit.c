#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/battle_target.h"
#include "fft/battle_unit.h"
#include "fft/main.h"
#include "psx/types.h"

/* 1 while the camera is moving: a pan vector is left, or it is rotating,
 * tilting or zooming. */
s32 camera_is_active(void) {
    if (g_camera_step_vector.vx == 0 && g_camera_step_vector.vy == 0 && g_camera_step_vector.vz == 0
        && g_camera_rotation_action == CAMERA_ROTATION_NONE && g_camera_tilt_action == CAMERA_STEP_NONE
        && g_camera_zoom_action == CAMERA_STEP_NONE) {
        return 0;
    }
    return 1;
}

/* Move the cursor to the unit with this battle id. Returns 1 when the unit has
 * runtime data, otherwise 0 with the cursor unchanged. */
s32 target_move_cursor_to_battle_id(u32 battle_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_battle_id(battle_id & 0xFFFF);
    if (unit != 0) {
        target_move_cursor_to_unit(unit);
        return 1;
    }
    return 0;
}

/* Aim a projectile from one unit at another: set up the direct trajectory to the
 * target's tile and make the target the attacker's only target. Returns 1 when
 * both units exist. Nothing in the game calls it. */
s32 effect_init_single_target_trajectory(u32 attacker_id, u32 target_id) {
    battle_unit_t* attacker;
    battle_unit_t* target;
    SVECTOR target_position;

    attacker = battle_unit_get_by_battle_id(attacker_id);
    target = battle_unit_get_by_battle_id(target_id);
    if (target != 0) {
        main_util_set_svector(&target_position, target->map_x, target->map_y, target->map_z);
        effect_check_direct_trajectory_to_target(attacker_id, &target_position, target_id);
        if (attacker != 0) {
            attacker->strike.target_count = 1;
            attacker->strike.target_list[0] = target_id;
            return 1;
        }
    }
    return 0;
}

s32 effect_set_secondary_teleport_by_runtime_id(u32 runtime_id) {
    battle_unit_t* unit;

    unit = battle_unit_get_by_runtime_id(runtime_id);
    if (unit != 0) {
        effect_set_secondary_teleport(unit);
        return 1;
    }
    return 0;
}
