/* Animate the reward bonus image as a burst of tiles.
 *
 * Profile gcc-2.6.3_O2_aspsx-2.34_divcheck: the per-tile delay is an unsigned
 * division whose divisor carries ASPSX's zero check (`bnez`/`break 7`).
 *
 * The bonus image is cut into a 12x24 grid of flat quads. Each tile starts
 * after a delay derived from its distance (script mode 0: from the centre,
 * 1: from the origin, 2: horizontal only, other: rotated x), then two
 * rotations of its radius place the quad corners while angle and radius grow
 * (phase 0) or shrink (phase 1). Between the phases a full-screen flash quad is
 * drawn until the controlling task switches to 0x36.
 *
 * Tile coordinates are written as expressions of the loop counters rather
 * than running variables: loop.c then derives the same induction values the
 * target keeps, and the extra insns keep it from hoisting the row offset and
 * the RotMatrix/vector addresses out of the column loop.
 */
#include "fft/battle_gfx.h"
#include "fft/battle_text.h"
#include "fft/main_heap.h"
#include "fft/require.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "psx/gpu.h"
#include "psx/gte.h"
#include "psx/libc.h"
#include "psx/types.h"

/* One 12x24 burst tile: a double-buffered flat quad plus its motion state. */
typedef struct {
    POLY_F4 poly[2];   /* 0x00 */
    s32 delay;         /* 0x30 */
    s32 radius;        /* 0x34 */
    s32 angle;         /* 0x38 */
    s32 x;             /* 0x3C */
    s32 y;             /* 0x40 */
    s32 initial_delay; /* 0x44 */
} require_reward_burst_tile_t;

extern require_reward_burst_tile_t g_require_reward_burst_tiles[12][24];

void require_reward_animate_bonus_image_burst(void) {
    s32 phase;
    s32 max_delay;
    s32 divisor;
    s32 angle_step;
    s32 radius_step;
    s32 done;
    s32 mode;
    s32 frame;
    s32 any;
    s32 row;
    s32 col;
    s32 dx;
    s32 dy;
    s32 dist;
    s32 ox;
    s32 oy;
    u8* params;
    require_reward_burst_tile_t* tile;
    POLY_F4* poly;

    g_require_thread_suspended_id = g_battle_current_thread_id;
    g_battle_text_section_pointers[21] = g_battle_text_section_pointers[2];
    if (battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != 0x145) {
        require_reward_load_bonus_image(0);
    } else {
        require_reward_load_bonus_image(1);
    }
    params = (u8*)battle_thread_get_current_parameter_2();
    params++;
    mode = *params;
    params++;
    divisor = *params;
    params++;
    max_delay = 0;
    radius_step = params[2];
    angle_step = (params[1] << 8) + params[0];
    for (row = 0; row < 12; row++) {
        for (col = 0; col < 24; col++) {
            tile = &g_require_reward_burst_tiles[row][col];
            tile->x = col * 12 - 12;
            tile->y = row * 24 - 12;
            if (col & 1) {
                tile->y = row * 24 - 24;
            }
            if (mode == 0) {
                dx = tile->x - 0x80;
                dy = tile->y - 0x80;
                dist = SquareRoot0(dx * dx + dy * dy);
            } else if (mode == 1) {
                dx = tile->x;
                dy = tile->y;
                dist = SquareRoot0(dx * dx + dy * dy);
            } else if (mode == 2) {
                dx = tile->x;
                dist = SquareRoot0(dx * dx);
            } else {
                dx = tile->x - 0x80;
                dy = tile->y - 0x80;
                g_require_reward_burst_rotation.vx = 0;
                g_require_reward_burst_rotation.vy = 0;
                g_require_reward_burst_rotation.vz = 0x200;
                RotMatrix(&g_require_reward_burst_rotation, &g_require_reward_burst_matrix);
                g_require_reward_burst_offset.vx = dx << 12;
                g_require_reward_burst_offset.vy = dy << 12;
                g_require_reward_burst_offset.vz = 0;
                ApplyMatrixLV(&g_require_reward_burst_matrix, &g_require_reward_burst_offset,
                    &g_require_reward_burst_rotated_offset);
                dist = abs(g_require_reward_burst_rotated_offset.vx / ONE);
            }
            dist = (u32)(dist * dist) / (u32)(divisor << 8);
            tile->delay = dist;
            tile->angle = 0;
            tile->radius = 0;
            tile->initial_delay = dist;
            if (max_delay < tile->delay) {
                max_delay = tile->delay;
            }
            SetPolyF4(&tile->poly[0]);
            SetSemiTrans(&tile->poly[0], 1);
            battle_copy_bytes(&tile->poly[1], &tile->poly[0], sizeof(POLY_F4));
        }
    }
    SetDrawMode(
        &g_require_reward_burst_draw_modes[0], 0, 0, GetTPage(0, 0, 0x180, 0), &g_require_gfx_draw_area_template);
    SetDrawMode(
        &g_require_reward_burst_draw_modes[1], 0, 0, GetTPage(0, 0, 0x180, 0), &g_require_gfx_draw_area_template);
    frame = 0;
    for (phase = 0; phase < 2; phase++) {
        done = 0;
        for (;;) {
            any = 0;
            battle_thread_yield();
            for (row = 0; row < 12; row++) {
                for (col = 0; col < 24; col++) {
                    tile = &g_require_reward_burst_tiles[row][col];
                    poly = &tile->poly[frame & 1];
                    if (tile->delay == 0) {
                        g_require_reward_burst_rotation.vx = 0;
                        g_require_reward_burst_rotation.vy = 0;
                        g_require_reward_burst_rotation.vz = tile->angle + 0x200;
                        RotMatrix(&g_require_reward_burst_rotation, &g_require_reward_burst_matrix);
                        g_require_reward_burst_offset.vx = tile->radius << 12;
                        g_require_reward_burst_offset.vy = 0;
                        g_require_reward_burst_offset.vz = 0;
                        ApplyMatrixLV(&g_require_reward_burst_matrix, &g_require_reward_burst_offset,
                            &g_require_reward_burst_rotated_offset);
                        ox = g_require_reward_burst_rotated_offset.vx / 0x4000;
                        oy = g_require_reward_burst_rotated_offset.vy / 0x4000;
                        poly->x0 = ox + tile->x + 0x80;
                        poly->y0 = oy + tile->y;
                        poly->x3 = tile->x - ox + 0x80;
                        poly->y3 = tile->y - oy;
                        g_require_reward_burst_rotation.vx = 0;
                        g_require_reward_burst_rotation.vy = 0;
                        g_require_reward_burst_rotation.vz = tile->angle + 0x600;
                        RotMatrix(&g_require_reward_burst_rotation, &g_require_reward_burst_matrix);
                        g_require_reward_burst_offset.vx = tile->radius << 12;
                        g_require_reward_burst_offset.vy = 0;
                        g_require_reward_burst_offset.vz = 0;
                        ApplyMatrixLV(&g_require_reward_burst_matrix, &g_require_reward_burst_offset,
                            &g_require_reward_burst_rotated_offset);
                        ox = g_require_reward_burst_rotated_offset.vx / 0x4000;
                        oy = g_require_reward_burst_rotated_offset.vy / 0x4000;
                        poly->x1 = ox + tile->x + 0x80;
                        poly->y1 = oy + tile->y;
                        poly->x2 = tile->x - ox + 0x80;
                        poly->y2 = tile->y - oy;
                        if (phase == 0) {
                            tile->angle += angle_step;
                            tile->radius += radius_step;
                            if (tile->angle < 0x201 || tile->radius < 0x31) {
                                any = 1;
                            }
                            if (tile->angle > 0x200) {
                                tile->angle = 0x200;
                            }
                            if (tile->radius > 0x30) {
                                tile->radius = 0x30;
                            }
                        } else {
                            tile->angle -= angle_step;
                            tile->radius -= radius_step;
                            if (tile->angle >= 0 || tile->radius >= 0) {
                                any = 1;
                            }
                            if (tile->angle < 0) {
                                tile->angle = 0;
                            }
                            if (tile->radius < 0) {
                                tile->radius = 0;
                            }
                        }
                        poly->r0 = g_require_gfx_fade_rgb[0];
                        poly->g0 = g_require_gfx_fade_rgb[1];
                        poly->b0 = g_require_gfx_fade_rgb[2];
                        battle_gfx_append_gpu_primitive_to_secondary_otag((u32*)poly);
                    } else {
                        poly->r0 = g_require_gfx_fade_rgb[0];
                        poly->g0 = g_require_gfx_fade_rgb[1];
                        poly->b0 = g_require_gfx_fade_rgb[2];
                        if (phase == 1) {
                            battle_gfx_append_gpu_primitive_to_secondary_otag((u32*)poly);
                        }
                        any = 1;
                        tile->delay--;
                    }
                }
            }
            battle_gfx_append_gpu_primitive_to_secondary_otag((u32*)&g_require_reward_burst_draw_modes[frame & 1]);
            if (done == 1) {
                break;
            }
            frame++;
            if (any == 0) {
                done = 1;
            }
        }
        if (phase == 1) {
            break;
        }
        SetPolyF4(&g_require_reward_burst_flash_polys[0]);
        g_require_reward_burst_flash_polys[0].x0 = 0x80;
        g_require_reward_burst_flash_polys[0].y0 = 0;
        g_require_reward_burst_flash_polys[0].x1 = 0x180;
        g_require_reward_burst_flash_polys[0].y1 = 0;
        g_require_reward_burst_flash_polys[0].x2 = 0x80;
        g_require_reward_burst_flash_polys[0].y2 = 0x100;
        g_require_reward_burst_flash_polys[0].x3 = 0x180;
        g_require_reward_burst_flash_polys[0].y3 = 0x100;
        g_require_reward_burst_flash_polys[0].r0 = 0x30;
        g_require_reward_burst_flash_polys[0].g0 = 0x28;
        g_require_reward_burst_flash_polys[0].b0 = 0x10;
        SetSemiTrans(&g_require_reward_burst_flash_polys[0], 1);
        battle_copy_bytes(
            &g_require_reward_burst_flash_polys[1], &g_require_reward_burst_flash_polys[0], sizeof(POLY_F4));
        frame++;
        battle_thread_set_current_task_id(NATIVE_THREAD_TASK_DARK_SCREEN_HOLD);
        for (;;) {
            battle_thread_yield();
            g_require_reward_burst_flash_polys[frame & 1].r0 = g_require_gfx_fade_rgb[0];
            g_require_reward_burst_flash_polys[frame & 1].g0 = g_require_gfx_fade_rgb[1];
            g_require_reward_burst_flash_polys[frame & 1].b0 = g_require_gfx_fade_rgb[2];
            battle_gfx_append_gpu_primitive_to_secondary_otag((u32*)&g_require_reward_burst_flash_polys[frame & 1]);
            battle_gfx_append_gpu_primitive_to_secondary_otag((u32*)&g_require_reward_burst_draw_modes[frame & 1]);
            if (g_battle_threads[g_battle_current_thread_id].task_id == NATIVE_THREAD_TASK_DARK_SCREEN) {
                break;
            }
            frame++;
        }
        frame++;
        for (row = 0; row < 12; row++) {
            for (col = 0; col < 24; col++) {
                tile = &g_require_reward_burst_tiles[row][col];
                tile->delay = tile->initial_delay;
            }
        }
    }
    battle_thread_wait_frames(1);
    battle_thread_exit_current();
}
