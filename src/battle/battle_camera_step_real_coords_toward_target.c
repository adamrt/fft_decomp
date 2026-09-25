#include "fft/battle.h"
#include "psx/types.h"

/* The caller's record carries the camera's destination as three signed
 * halfwords in X, Z, Y order at +0x40, which is battle_screen_coords_t: this
 * function reads 0x40, 0x42 and 0x44 off its argument and compares each against
 * the matching component of g_battle_camera_current_real_coords divided by
 * ONE. Provisional view of the record. */
typedef struct battle_camera_step_target {
    u8 _unused_00[0x40];
    battle_screen_coords_t coords; /* 0x40 */
} battle_camera_step_target_t;

void battle_camera_step_real_coords_toward_target(battle_camera_step_target_t* target) {
    if (g_battle_current_vector.vx < 0) {
        if (g_battle_camera_current_real_coords.vx / ONE > target->coords.x) {
            g_battle_camera_current_real_coords.vx += g_battle_current_vector.vx;
            if (g_battle_camera_current_real_coords.vx / ONE < target->coords.x) {
                g_battle_camera_current_real_coords.vx = target->coords.x << 12;
            }
        }
    } else {
        if (g_battle_camera_current_real_coords.vx / ONE < target->coords.x) {
            g_battle_camera_current_real_coords.vx += g_battle_current_vector.vx;
            if (g_battle_camera_current_real_coords.vx / ONE > target->coords.x) {
                g_battle_camera_current_real_coords.vx = target->coords.x << 12;
            }
        }
    }
    if (g_battle_current_vector.vy < 0) {
        if (g_battle_camera_current_real_coords.vy / ONE > target->coords.z) {
            g_battle_camera_current_real_coords.vy += g_battle_current_vector.vy;
            if (g_battle_camera_current_real_coords.vy / ONE < target->coords.z) {
                g_battle_camera_current_real_coords.vy = target->coords.z << 12;
            }
        }
    } else {
        if (g_battle_camera_current_real_coords.vy / ONE < target->coords.z) {
            g_battle_camera_current_real_coords.vy += g_battle_current_vector.vy;
            if (g_battle_camera_current_real_coords.vy / ONE > target->coords.z) {
                g_battle_camera_current_real_coords.vy = target->coords.z << 12;
            }
        }
    }
    if (g_battle_current_vector.vz < 0) {
        if (g_battle_camera_current_real_coords.vz / ONE > target->coords.y) {
            g_battle_camera_current_real_coords.vz += g_battle_current_vector.vz;
            if (g_battle_camera_current_real_coords.vz / ONE < target->coords.y) {
                g_battle_camera_current_real_coords.vz = target->coords.y << 12;
            }
        }
    } else {
        if (g_battle_camera_current_real_coords.vz / ONE < target->coords.y) {
            /* Retail quirk: only this positive map-depth step doubles the component. */
            g_battle_camera_current_real_coords.vz += g_battle_current_vector.vz * 2;
            if (g_battle_camera_current_real_coords.vz / ONE > target->coords.y) {
                g_battle_camera_current_real_coords.vz = target->coords.y << 12;
            }
        }
    }
}
