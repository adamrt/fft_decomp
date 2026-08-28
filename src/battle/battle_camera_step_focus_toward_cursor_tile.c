/*
 * Exact match: cursor bounds are evaluated in a0, and clamped 12.4 values in
 * v0. The local fixed-register bindings emit no assembly; they reproduce the
 * target compiler allocation for the three otherwise identical clamp copies.
 */
#include "fft/battle.h"
#include "fft/battle_camera.h"

/* Move one 12.4 fixed-point camera component toward its bound by its per-frame
 * step, without overshooting. The bound is compared against the component's
 * whole-unit part, so the clamp lands exactly on bound << 12. The test is
 * written once per sign of the step because the overshoot comparison reverses
 * with it, and the bound is re-evaluated inside each arm because the target
 * recomputes it there. */
#define BATTLE_CAMERA_STEP_TOWARD(component, step, limit, whole_reg, bound_reg)                                        \
    do {                                                                                                               \
        if ((step) < 0) {                                                                                              \
            s32* position = (component);                                                                               \
            register s32 whole __asm__(whole_reg) = *position / 4096;                                                  \
            register s32 bound __asm__(bound_reg) = (limit);                                                           \
            if (whole > bound) {                                                                                       \
                *position += (step);                                                                                   \
                if (*position / 4096 < bound) {                                                                        \
                    register s32 clamped __asm__("$2") = bound << 12;                                                  \
                    *position = clamped;                                                                               \
                }                                                                                                      \
            }                                                                                                          \
        } else {                                                                                                       \
            s32* position = (component);                                                                               \
            register s32 whole __asm__(whole_reg) = *position / 4096;                                                  \
            register s32 bound __asm__(bound_reg) = (limit);                                                           \
            if (whole < bound) {                                                                                       \
                *position += (step);                                                                                   \
                if (bound < *position / 4096) {                                                                        \
                    register s32 clamped __asm__("$2") = bound << 12;                                                  \
                    *position = clamped;                                                                               \
                }                                                                                                      \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)

/*
 * Advance the camera focus toward the cursor tile by one frame's step.
 *
 * The horizontal bounds are the centre of the cursor tile (28 units per tile,
 * half a tile of bias); the caller supplies the height bound directly.
 */
void battle_camera_step_focus_toward_cursor_tile(s16 height) {
    BATTLE_CAMERA_STEP_TOWARD(&g_battle_camera_current_real_coords.vx, g_battle_current_vector.vx, ({
        register s32 cursor __asm__("$4") = g_battle_cursor_x;
        cursor * 28 + 14;
    }),
        "$3", "$4");
    BATTLE_CAMERA_STEP_TOWARD(&g_battle_camera_current_real_coords.vy, g_battle_current_vector.vy, height, "$2", "$3");
    BATTLE_CAMERA_STEP_TOWARD(&g_battle_camera_current_real_coords.vz, g_battle_current_vector.vz, ({
        register s32 cursor __asm__("$4") = g_battle_cursor_y;
        cursor * 28 + 14;
    }),
        "$3", "$4");
}
