#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_effect.h"
#include "fft/effect.h"
#include "psx/types.h"

/* Advance the effect camera rotation, position and zoom tracks one frame.
 *
 * Each track's mode word (g_battle_effect_camera_rotation_mode/D0/D4, bits 0x1e00 of the timeline
 * command) selects an interpolation between the start and target values over
 * the track duration; the mode clears once the counter reaches the duration. */
void battle_camera_update_effect_rotation_and_zoom(void) {
    VECTOR position;
    VECTOR delta;
    SVECTOR rotation;
    SVECTOR rotation_delta;

    switch ((u32)g_battle_effect_camera_rotation_mode) {
    case 0:
        break;
    case 0x200:
        battle_camera_set_rotation((battle_camera_rotation_t*)&g_battle_effect_camera_rotation_target);
        g_battle_effect_camera_rotation_mode = 0;
        g_battle_effect_camera_rotation_start.vx = g_battle_effect_camera_rotation_current.vx
            = g_battle_effect_camera_rotation_target.vx & 0xfff;
        g_battle_effect_camera_rotation_start.vy = g_battle_effect_camera_rotation_current.vy
            = g_battle_effect_camera_rotation_target.vy & 0xfff;
        g_battle_effect_camera_rotation_start.vz = g_battle_effect_camera_rotation_current.vz
            = g_battle_effect_camera_rotation_target.vz & 0xfff;
        g_battle_effect_camera_rotation_frame = g_battle_effect_camera_rotation_duration;
        break;
    case 0x400:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            battle_effect_interpolate_svector_cos(&g_battle_effect_camera_rotation_start.vx,
                &g_battle_effect_camera_rotation_target.vx, g_battle_effect_camera_rotation_duration,
                g_battle_effect_camera_rotation_frame, &g_battle_effect_camera_rotation_current.vx);
            battle_camera_set_rotation((battle_camera_rotation_t*)&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            battle_effect_mask_vector_low_12_bits(
                &g_battle_effect_camera_rotation_target.vx, &g_battle_effect_camera_rotation_start.vx);
            g_battle_effect_camera_rotation_mode = 0;
            battle_camera_set_rotation((battle_camera_rotation_t*)&g_battle_effect_camera_rotation_start);
        }
        break;
    case 0x600:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            battle_effect_interpolate_svector_cos_q12(&g_battle_effect_camera_rotation_start.vx,
                &g_battle_effect_camera_rotation_target.vx, g_battle_effect_camera_rotation_duration,
                g_battle_effect_camera_rotation_frame, &g_battle_effect_camera_rotation_current.vx);
            battle_camera_set_rotation((battle_camera_rotation_t*)&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_mode = 0;
        }
        break;
    case 0x800:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            battle_effect_interpolate_svector_linear(&g_battle_effect_camera_rotation_start.vx,
                &g_battle_effect_camera_rotation_target.vx, g_battle_effect_camera_rotation_duration,
                g_battle_effect_camera_rotation_frame, &g_battle_effect_camera_rotation_current.vx);
            battle_camera_set_rotation((battle_camera_rotation_t*)&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            battle_effect_mask_vector_low_12_bits(
                &g_battle_effect_camera_rotation_target.vx, &g_battle_effect_camera_rotation_start.vx);
            g_battle_effect_camera_rotation_mode = 0;
            battle_camera_set_rotation((battle_camera_rotation_t*)&g_battle_effect_camera_rotation_start);
        }
        break;
    case 0xa00:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            battle_effect_interpolate_svector_linear(&rotation.vx, &g_battle_effect_camera_rotation_target.vx,
                g_battle_effect_camera_rotation_duration, g_battle_effect_camera_rotation_frame, &rotation_delta.vx);
            battle_effect_add_rotation_vectors_mod_q12(
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_current,
                (battle_effect_rotation_vector_t*)&rotation_delta,
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_current);
            battle_camera_set_rotation((battle_camera_rotation_t*)&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_target = g_battle_effect_camera_rotation_current;
            g_battle_effect_camera_rotation_start = g_battle_effect_camera_rotation_target;
            g_battle_effect_camera_rotation_mode = 0;
        }
        break;
    case 0xc00:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            battle_effect_add_rotation_vectors_mod_q12(
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_current,
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_target,
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_current);
            battle_camera_set_rotation((battle_camera_rotation_t*)&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_target = g_battle_effect_camera_rotation_current;
            g_battle_effect_camera_rotation_start = g_battle_effect_camera_rotation_target;
            g_battle_effect_camera_rotation_mode = 0;
        }
        break;
    case 0xe00:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            battle_effect_interpolate_svector_linear(&g_battle_effect_camera_rotation_target.vx, &rotation.vx,
                g_battle_effect_camera_rotation_duration, g_battle_effect_camera_rotation_frame, &rotation_delta.vx);
            battle_effect_add_rotation_vectors_mod_q12(
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_current,
                (battle_effect_rotation_vector_t*)&rotation_delta,
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_current);
            battle_camera_set_rotation((battle_camera_rotation_t*)&g_battle_effect_camera_rotation_current);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_target = g_battle_effect_camera_rotation_current;
            g_battle_effect_camera_rotation_start = g_battle_effect_camera_rotation_target;
            g_battle_effect_camera_rotation_mode = 0;
        }
        break;
    case 0x1000:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            battle_effect_interpolate_svector_linear(&rotation.vx, &g_battle_effect_camera_rotation_target.vx,
                g_battle_effect_camera_rotation_duration, g_battle_effect_camera_rotation_frame, &rotation_delta.vx);
            battle_effect_add_random_rotation_offsets(
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_start,
                (battle_effect_rotation_vector_t*)&rotation_delta, (battle_effect_rotation_vector_t*)&rotation);
            battle_camera_set_rotation((battle_camera_rotation_t*)&rotation);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_mode = 0;
        }
        break;
    case 0x1200:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            battle_effect_add_random_rotation_offsets(
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_start,
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_target,
                (battle_effect_rotation_vector_t*)&rotation);
            battle_camera_set_rotation((battle_camera_rotation_t*)&rotation);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_mode = 0;
        }
        break;
    case 0x1400:
        if (g_battle_effect_camera_rotation_duration > g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_frame++;
            rotation.vx = rotation.vy = rotation.vz = 0;
            battle_effect_interpolate_svector_linear(&g_battle_effect_camera_rotation_target.vx, &rotation.vx,
                g_battle_effect_camera_rotation_duration, g_battle_effect_camera_rotation_frame, &rotation_delta.vx);
            battle_effect_add_random_rotation_offsets(
                (battle_effect_rotation_vector_t*)&g_battle_effect_camera_rotation_start,
                (battle_effect_rotation_vector_t*)&rotation_delta, (battle_effect_rotation_vector_t*)&rotation);
            battle_camera_set_rotation((battle_camera_rotation_t*)&rotation);
        }
        if (g_battle_effect_camera_rotation_duration == g_battle_effect_camera_rotation_frame) {
            g_battle_effect_camera_rotation_mode = 0;
        }
        break;
    }

    switch ((u32)g_battle_effect_camera_position_mode) {
    case 0:
        break;
    case 0x200:
        battle_effect_shift_vector_left_12(&g_battle_effect_camera_position_target.vx, &position.vx);
        battle_camera_set_current_real_coords(&position);
        g_battle_effect_camera_position_current = g_battle_effect_camera_position_target;
        g_battle_effect_camera_position_start = g_battle_effect_camera_position_current;
        g_battle_effect_camera_position_mode = 0;
        g_battle_effect_camera_position_frame = g_battle_effect_camera_position_duration;
        break;
    case 0x400:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            battle_effect_interpolate_vector_q13_cos(&g_battle_effect_camera_position_start.vx,
                &g_battle_effect_camera_position_target.vx, g_battle_effect_camera_position_duration,
                g_battle_effect_camera_position_frame, &g_battle_effect_camera_position_current.vx);
            battle_effect_shift_vector_left_12(&g_battle_effect_camera_position_current.vx, &position.vx);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_start = g_battle_effect_camera_position_target;
            g_battle_effect_camera_position_mode = 0;
        }
        break;
    case 0x600:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            battle_effect_interpolate_vector_q13_cos_q12(&g_battle_effect_camera_position_start.vx,
                &g_battle_effect_camera_position_target.vx, g_battle_effect_camera_position_duration,
                g_battle_effect_camera_position_frame, &g_battle_effect_camera_position_current.vx);
            battle_effect_shift_vector_left_12(&g_battle_effect_camera_position_current.vx, &position.vx);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_mode = 0;
        }
        break;
    case 0x800:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_position_start.vx,
                &g_battle_effect_camera_position_target.vx, g_battle_effect_camera_position_duration,
                g_battle_effect_camera_position_frame, &g_battle_effect_camera_position_current.vx);
            battle_effect_shift_vector_left_12(&g_battle_effect_camera_position_current.vx, &position.vx);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_start = g_battle_effect_camera_position_target;
            g_battle_effect_camera_position_mode = 0;
        }
        break;
    case 0xa00:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&position.vx, &g_battle_effect_camera_position_target.vx,
                g_battle_effect_camera_position_duration, g_battle_effect_camera_position_frame, &delta.vx);
            battle_effect_add_vector_and_store_q12(
                &delta.vx, &g_battle_effect_camera_position_current.vx, &position.vx);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_target = g_battle_effect_camera_position_current;
            g_battle_effect_camera_position_start = g_battle_effect_camera_position_target;
            g_battle_effect_camera_position_mode = 0;
        }
        break;
    case 0xc00:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            battle_effect_add_vector_and_store_q12(
                &g_battle_effect_camera_position_target.vx, &g_battle_effect_camera_position_current.vx, &position.vx);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_target = g_battle_effect_camera_position_current;
            g_battle_effect_camera_position_start = g_battle_effect_camera_position_target;
            g_battle_effect_camera_position_mode = 0;
        }
        break;
    case 0xe00:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_position_target.vx, &position.vx,
                g_battle_effect_camera_position_duration, g_battle_effect_camera_position_frame, &delta.vx);
            battle_effect_add_vector_and_store_q12(
                &delta.vx, &g_battle_effect_camera_position_current.vx, &position.vx);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_target = g_battle_effect_camera_position_current;
            g_battle_effect_camera_position_start = g_battle_effect_camera_position_target;
            g_battle_effect_camera_position_mode = 0;
        }
        break;
    case 0x1000:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&position.vx, &g_battle_effect_camera_position_target.vx,
                g_battle_effect_camera_position_duration, g_battle_effect_camera_position_frame, &delta.vx);
            battle_effect_add_random_vector_offsets_and_store_q12(
                &g_battle_effect_camera_position_start, &delta, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_mode = 0;
        }
        break;
    case 0x1200:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            battle_effect_add_random_vector_offsets_and_store_q12(
                &g_battle_effect_camera_position_start, &g_battle_effect_camera_position_target, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_mode = 0;
        }
        break;
    case 0x1400:
        if (g_battle_effect_camera_position_duration > g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_position_target.vx, &position.vx,
                g_battle_effect_camera_position_duration, g_battle_effect_camera_position_frame, &delta.vx);
            battle_effect_add_random_vector_offsets_and_store_q12(
                &g_battle_effect_camera_position_start, &delta, &position);
            battle_camera_set_current_real_coords(&position);
        }
        if (g_battle_effect_camera_position_duration == g_battle_effect_camera_position_frame) {
            g_battle_effect_camera_position_mode = 0;
        }
        break;
    }

    switch ((u32)g_battle_effect_camera_zoom_mode) {
    case 0:
        break;
    case 0x200:
        battle_camera_set_zoom(&g_battle_effect_camera_zoom_target);
        g_battle_effect_camera_zoom_current = g_battle_effect_camera_zoom_target;
        g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_current;
        g_battle_effect_camera_zoom_mode = 0;
        g_battle_effect_camera_zoom_frame = g_battle_effect_camera_zoom_duration;
        break;
    case 0x400:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            battle_effect_interpolate_vector_q13_cos(&g_battle_effect_camera_zoom_start.vx,
                &g_battle_effect_camera_zoom_target.vx, g_battle_effect_camera_zoom_duration,
                g_battle_effect_camera_zoom_frame, &g_battle_effect_camera_zoom_current.vx);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target;
            g_battle_effect_camera_zoom_mode = 0;
        }
        break;
    case 0x600:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            battle_effect_interpolate_vector_q13_cos_q12(&g_battle_effect_camera_zoom_start.vx,
                &g_battle_effect_camera_zoom_target.vx, g_battle_effect_camera_zoom_duration,
                g_battle_effect_camera_zoom_frame, &g_battle_effect_camera_zoom_current.vx);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_mode = 0;
        }
        break;
    case 0x800:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_zoom_start.vx,
                &g_battle_effect_camera_zoom_target.vx, g_battle_effect_camera_zoom_duration,
                g_battle_effect_camera_zoom_frame, &g_battle_effect_camera_zoom_current.vx);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target;
            g_battle_effect_camera_zoom_mode = 0;
        }
        break;
    case 0xa00:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&position.vx, &g_battle_effect_camera_zoom_target.vx,
                g_battle_effect_camera_zoom_duration, g_battle_effect_camera_zoom_frame, &delta.vx);
            battle_effect_add_vectors(
                &delta.vx, &g_battle_effect_camera_zoom_current.vx, &g_battle_effect_camera_zoom_current.vx);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_target = g_battle_effect_camera_zoom_current;
            g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target;
            g_battle_effect_camera_zoom_mode = 0;
        }
        break;
    case 0xc00:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            battle_effect_add_vectors(&g_battle_effect_camera_zoom_target.vx, &g_battle_effect_camera_zoom_current.vx,
                &g_battle_effect_camera_zoom_current.vx);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_target = g_battle_effect_camera_zoom_current;
            g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target;
            g_battle_effect_camera_zoom_mode = 0;
        }
        break;
    case 0xe00:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_zoom_target.vx, &position.vx,
                g_battle_effect_camera_zoom_duration, g_battle_effect_camera_zoom_frame, &delta.vx);
            battle_effect_add_vectors(
                &delta.vx, &g_battle_effect_camera_zoom_current.vx, &g_battle_effect_camera_zoom_current.vx);
            battle_camera_set_zoom(&g_battle_effect_camera_zoom_current);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_target = g_battle_effect_camera_zoom_current;
            g_battle_effect_camera_zoom_start = g_battle_effect_camera_zoom_target;
            g_battle_effect_camera_zoom_mode = 0;
        }
        break;
    case 0x1000:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&position.vx, &g_battle_effect_camera_zoom_target.vx,
                g_battle_effect_camera_zoom_duration, g_battle_effect_camera_zoom_frame, &delta.vx);
            battle_effect_add_random_vector_offsets(&g_battle_effect_camera_zoom_start, &delta, &position);
            battle_camera_set_zoom(&position);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_mode = 0;
        }
        break;
    case 0x1200:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            battle_effect_add_random_vector_offsets(
                &g_battle_effect_camera_zoom_start, &g_battle_effect_camera_zoom_target, &position);
            battle_camera_set_zoom(&position);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_mode = 0;
        }
        break;
    case 0x1400:
        if (g_battle_effect_camera_zoom_duration > g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_frame++;
            position.vx = position.vy = position.vz = 0;
            battle_effect_interpolate_vector_q13_linear(&g_battle_effect_camera_zoom_target.vx, &position.vx,
                g_battle_effect_camera_zoom_duration, g_battle_effect_camera_zoom_frame, &delta.vx);
            battle_effect_add_random_vector_offsets(&g_battle_effect_camera_zoom_start, &delta, &position);
            battle_camera_set_zoom(&position);
        }
        if (g_battle_effect_camera_zoom_duration == g_battle_effect_camera_zoom_frame) {
            g_battle_effect_camera_zoom_mode = 0;
        }
        break;
    }
}
