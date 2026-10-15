#include "fft/battle_action.h"
#include "fft/battle_camera.h"

/* Moves the camera position by this frame's step vector (g_camera_step_vector)
 * toward the unit's position without passing it.
 *
 * Each component of g_camera_current_real_coords (20.12) advances only
 * while it is still short of the target in the step's direction, and is set to
 * the target exactly when the step carries it past. Called for the acting unit
 * while it moves, chooses its wait direction or starts an attack animation. */
void camera_step_real_coords_toward_target(const battle_unit_t* target) {
    if (g_camera_step_vector.vx < 0) {
        if (g_camera_current_real_coords.vx / ONE > target->world_coords.vx) {
            g_camera_current_real_coords.vx += g_camera_step_vector.vx;
            if (g_camera_current_real_coords.vx / ONE < target->world_coords.vx) {
                g_camera_current_real_coords.vx = target->world_coords.vx * ONE;
            }
        }
    } else {
        if (g_camera_current_real_coords.vx / ONE < target->world_coords.vx) {
            g_camera_current_real_coords.vx += g_camera_step_vector.vx;
            if (g_camera_current_real_coords.vx / ONE > target->world_coords.vx) {
                g_camera_current_real_coords.vx = target->world_coords.vx * ONE;
            }
        }
    }
    if (g_camera_step_vector.vy < 0) {
        if (g_camera_current_real_coords.vy / ONE > target->world_coords.vy) {
            g_camera_current_real_coords.vy += g_camera_step_vector.vy;
            if (g_camera_current_real_coords.vy / ONE < target->world_coords.vy) {
                g_camera_current_real_coords.vy = target->world_coords.vy * ONE;
            }
        }
    } else {
        if (g_camera_current_real_coords.vy / ONE < target->world_coords.vy) {
            g_camera_current_real_coords.vy += g_camera_step_vector.vy;
            if (g_camera_current_real_coords.vy / ONE > target->world_coords.vy) {
                g_camera_current_real_coords.vy = target->world_coords.vy * ONE;
            }
        }
    }
    if (g_camera_step_vector.vz < 0) {
        if (g_camera_current_real_coords.vz / ONE > target->world_coords.vz) {
            g_camera_current_real_coords.vz += g_camera_step_vector.vz;
            if (g_camera_current_real_coords.vz / ONE < target->world_coords.vz) {
                g_camera_current_real_coords.vz = target->world_coords.vz * ONE;
            }
        }
    } else {
        if (g_camera_current_real_coords.vz / ONE < target->world_coords.vz) {
            /* Retail quirk: only this positive map-depth step doubles the component. */
            g_camera_current_real_coords.vz += g_camera_step_vector.vz * 2;
            if (g_camera_current_real_coords.vz / ONE > target->world_coords.vz) {
                g_camera_current_real_coords.vz = target->world_coords.vz * ONE;
            }
        }
    }
}
