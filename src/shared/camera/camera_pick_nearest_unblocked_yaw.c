#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/battle_script.h"
#include "fft/script.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* Returns the yaw a Focus instruction gives the camera for a pair of units.
 *
 * The flat-pitch camera-block masks of the two units' tiles are combined; each of
 * the four low bits rules out one entry of g_camera_diagonal_yaws. Of the
 * remaining yaws the one with the smallest angular distance from the current yaw
 * (taken modulo a full turn of 0x1000) wins, and the result is the yaw script
 * variable moved by that distance in the shorter direction, so the variable is
 * not wrapped.
 *
 * With all four directions blocked no candidate replaces the initial distance
 * 0x8000, and the result is the yaw variable plus 0x8000 truncated to s16. */
s16 camera_pick_nearest_unblocked_yaw(s32 runtime_id_a, s32 runtime_id_b) {
    s32 yaw;
    s32 mask;
    s32 raw;
    /* Removing the binding changes four loop-register assignments. */
    register s32 bit __asm__("$7");
    s32 best;
    s32 sign;
    s32 i;
    s32 delta;
    s32 wrapped;
    s32 dist;

    yaw = camera_wrap_yaw_angle() & 0xFFF;
    unit_copy_map_coords_by_runtime_id(runtime_id_a, &g_unit_scratch_coords);
    /* The raw result is assigned before it is masked: combine then knows
     * nothing about the mask's high bits and the loop test keeps its sll 16. */
    mask = map_get_tile_data_value(MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, g_unit_scratch_coords.vx,
        g_unit_scratch_coords.vz, g_unit_scratch_coords.vy);
    mask &= 0xFF;
    unit_copy_map_coords_by_runtime_id(runtime_id_b, &g_unit_scratch_coords);
    mask = (mask
               | map_get_tile_data_value(MAP_TILE_DATA_FLAT_CAMERA_BLOCK_MASK, g_unit_scratch_coords.vx,
                   g_unit_scratch_coords.vz, g_unit_scratch_coords.vy))
        & 0xF;
    bit = 1;
    best = 0x8000;
    sign = 1;
    for (i = 0; i < 4; i++) {
        if ((s16)(bit & mask) == 0) {
            /* Distance to this diagonal directly (delta) and the other way round (wrapped). */
            delta = g_camera_diagonal_yaws[i] - yaw;
            raw = delta;
            if (delta > 0) {
                wrapped = delta - ONE;
            } else {
                wrapped = delta + ONE;
            }
            delta = abs(delta);
            wrapped = abs(wrapped);

            if (wrapped < delta) {
                dist = wrapped;
            } else {
                dist = delta;
            }
            if (dist < best) {
                best = dist;
                if (raw > 0) {
                    sign = -1;
                    if (delta < wrapped) {
                        sign = 1;
                    }
                } else {
                    sign = 1;
                    if (delta < wrapped) {
                        sign = -1;
                    }
                }
            }
        }
        bit <<= 1;
    }
    return g_script_variables[EVENT_SCRIPT_VAR_CAMERA_YAW] + sign * best;
}
