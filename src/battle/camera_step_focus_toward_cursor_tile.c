#include "fft/battle.h"
#include "fft/battle_action.h"
#include "fft/battle_camera.h"
#include "psx/libgte.h"

/* Move one 20.12 fixed-point camera component toward its bound by its per-frame
 * step, without overshooting. The bound is compared against the component's
 * whole-unit part, so the clamp lands exactly on bound * ONE. The test is
 * written once per sign of the step because the overshoot comparison reverses
 * with it. The body is a bare if/else: wrapped in do/while (0) it becomes a
 * loop to the compiler, which doubles its reference counts and changes the
 * register allocation. */
#define CAMERA_STEP_TOWARD(component, step, limit)                                                                     \
    if (step < 0) {                                                                                                    \
        s32* position = component;                                                                                     \
        s32 whole = *position / ONE;                                                                                   \
        s32 bound = limit;                                                                                             \
        if (whole > bound) {                                                                                           \
            *position += step;                                                                                         \
            if (*position / ONE < bound) {                                                                             \
                *position = bound * ONE;                                                                               \
            }                                                                                                          \
        }                                                                                                              \
    } else {                                                                                                           \
        s32* position = component;                                                                                     \
        s32 whole = *position / ONE;                                                                                   \
        s32 bound = limit;                                                                                             \
        if (whole < bound) {                                                                                           \
            *position += step;                                                                                         \
            if (bound < *position / ONE) {                                                                             \
                *position = bound * ONE;                                                                               \
            }                                                                                                          \
        }                                                                                                              \
    }

/*
 * Advance the camera focus toward the cursor tile by one frame's step.
 *
 * The horizontal bounds are the center of the cursor tile (28 units per tile,
 * half a tile of bias); the caller supplies the height bound directly.
 */
void camera_step_focus_toward_cursor_tile(s16 height) {
    CAMERA_STEP_TOWARD(&g_camera_current_real_coords.vx, g_camera_step_vector.vx,
        g_battle_cursor_x * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
    CAMERA_STEP_TOWARD(&g_camera_current_real_coords.vy, g_camera_step_vector.vy, height);
    CAMERA_STEP_TOWARD(&g_camera_current_real_coords.vz, g_camera_step_vector.vz,
        g_battle_cursor_y * MAP_TILE_SIZE + (MAP_TILE_SIZE / 2));
}
