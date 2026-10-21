/* DarkScreen animates BONUS.BIN as a grid of tiles with delayed motion,
 * then ends its thread after the burst and flash finish. */

#include "fft/after_battle.h"
#include "fft/gfx_primitive.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script_variable.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/types.h"

/* One cell of the 12-row, 24-column burst grid: two flat quads and its motion state. */
typedef struct conditions_burst_tile {
    POLY_F4 poly[2];   /* 0x00 */
    s32 delay;         /* 0x30 */
    s32 radius;        /* 0x34 */
    s32 angle;         /* 0x38 */
    s32 x;             /* 0x3C */
    s32 y;             /* 0x40 */
    s32 initial_delay; /* 0x44 */
} conditions_burst_tile_t;
typedef char conditions_burst_tile_size_must_be_72[sizeof(conditions_burst_tile_t) == 72 ? 1 : -1];

/* The mode byte of the bonus-image burst parameters: the distance that sets
 * each tile's start delay. Any other value measures along a rotated x axis. */
typedef enum conditions_burst_mode {
    CONDITIONS_BURST_FROM_CENTER = 0,
    CONDITIONS_BURST_FROM_ORIGIN = 1, /* the top-left corner */
    CONDITIONS_BURST_HORIZONTAL = 2   /* x only */
} conditions_burst_mode_e;

extern conditions_burst_tile_t g_conditions_burst_tiles[12][24];
extern POLY_F4 g_conditions_burst_flash_polys[2];
extern DR_MODE g_conditions_burst_draw_modes[2];
static MATRIX g_conditions_burst_matrix;
static SVECTOR g_conditions_burst_rotation;
static VECTOR g_conditions_burst_offset;
static VECTOR g_conditions_burst_rotated_offset;

/* Call thread_current_exit. */
void thread_exit_current_wrapper(void) {
    thread_current_exit();
}

/* Animate a BONUS.BIN banner image as a burst of tiles.
 *
 * Profile gcc-2.6.3_O2_aspsx-2.34_divcheck: the per-tile delay is an unsigned
 * division whose divisor carries ASPSX's zero check (`bnez`/`break 7`).
 *
 * The bonus image is cut into a 12x24 grid of flat quads. Each tile starts
 * after a delay derived from its distance (script mode 0: from the center,
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
void require_dark_screen_entrypoint(void) {
    s32 phase;
    s32 max_delay;
    u32 divisor;
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
    conditions_burst_tile_t* tile;
    POLY_F4* poly;

    g_after_battle_thread_suspended_id = g_thread_current_id;
    g_text_section_pointers[TEXT_SECTION_UNIT_MESSAGE] = g_text_section_pointers[TEXT_SECTION_SKILLSET_NAMES_COPY];
    if (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON) {
        conditions_load_banner_image(0);
    } else {
        conditions_load_banner_image(1);
    }

    /* Named operand loads fold the three native cursor advances. */
    params = thread_current_parameter_2_get();
    /* Script parameter bytes: 1 mode, 2 delay divisor, 3-4 angle step (little-endian), 5 radius step. */
    params++;
    mode = *params;
    params++;
    divisor = *params;
    params++;
    max_delay = 0;
    radius_step = params[2];
    angle_step = (params[1] << 8) + params[0];

    /* Lay out the tiles; odd columns sit half a row higher. */
    for (row = 0; row < 12; row++) {
        for (col = 0; col < 24; col++) {
            tile = &g_conditions_burst_tiles[row][col];
            tile->x = col * 12 - 12;
            tile->y = row * 24 - 12;
            if (col & 1) {
                tile->y = row * 24 - 24;
            }
            if (mode == CONDITIONS_BURST_FROM_CENTER) {
                dx = tile->x - 0x80;
                dy = tile->y - 0x80;
                dist = SquareRoot0(dx * dx + dy * dy);
            } else if (mode == CONDITIONS_BURST_FROM_ORIGIN) {
                dx = tile->x;
                dy = tile->y;
                dist = SquareRoot0(dx * dx + dy * dy);
            } else if (mode == CONDITIONS_BURST_HORIZONTAL) {
                dx = tile->x;
                dist = SquareRoot0(dx * dx);
            } else {
                dx = tile->x - 0x80;
                dy = tile->y - 0x80;
                g_conditions_burst_rotation.vx = 0;
                g_conditions_burst_rotation.vy = 0;
                g_conditions_burst_rotation.vz = 0x200;
                RotMatrix(&g_conditions_burst_rotation, &g_conditions_burst_matrix);
                g_conditions_burst_offset.vx = dx * ONE;
                g_conditions_burst_offset.vy = dy * ONE;
                g_conditions_burst_offset.vz = 0;
                ApplyMatrixLV(
                    &g_conditions_burst_matrix, &g_conditions_burst_offset, &g_conditions_burst_rotated_offset);
                dist = abs(g_conditions_burst_rotated_offset.vx / ONE);
            }
            /* The delay grows with the square of the distance. */
            dist = (dist * dist) / (divisor << 8);
            tile->delay = dist;
            tile->angle = 0;
            tile->radius = 0;
            tile->initial_delay = dist;
            if (max_delay < tile->delay) {
                max_delay = tile->delay;
            }
            SetPolyF4(&tile->poly[0]);
            SetSemiTrans(&tile->poly[0], 1);
            mem_copy_bytes_nonempty(&tile->poly[1], &tile->poly[0], sizeof(POLY_F4));
        }
    }

    SetDrawMode(&g_conditions_burst_draw_modes[0], 0, 0, GetTPage(0, 0, 0x180, 0), &g_menu_draw_area_template);
    SetDrawMode(&g_conditions_burst_draw_modes[1], 0, 0, GetTPage(0, 0, 0x180, 0), &g_menu_draw_area_template);
    frame = 0;
    for (phase = 0; phase < 2; phase++) {
        done = 0;
        for (;;) {
            any = 0;
            thread_yield();
            for (row = 0; row < 12; row++) {
                for (col = 0; col < 24; col++) {
                    tile = &g_conditions_burst_tiles[row][col];
                    poly = &tile->poly[frame & 1];
                    /* A started tile is a square turned by its angle: corners on its radius at
                     * angle + 45 and + 135 degrees (0x200, 0x600) and their mirror images. */
                    if (tile->delay == 0) {
                        g_conditions_burst_rotation.vx = 0;
                        g_conditions_burst_rotation.vy = 0;
                        g_conditions_burst_rotation.vz = tile->angle + 0x200;
                        RotMatrix(&g_conditions_burst_rotation, &g_conditions_burst_matrix);
                        g_conditions_burst_offset.vx = tile->radius * ONE;
                        g_conditions_burst_offset.vy = 0;
                        g_conditions_burst_offset.vz = 0;
                        ApplyMatrixLV(
                            &g_conditions_burst_matrix, &g_conditions_burst_offset, &g_conditions_burst_rotated_offset);
                        ox = g_conditions_burst_rotated_offset.vx / 0x4000;
                        oy = g_conditions_burst_rotated_offset.vy / 0x4000;
                        poly->x0 = ox + tile->x + 0x80;
                        poly->y0 = oy + tile->y;
                        poly->x3 = tile->x - ox + 0x80;
                        poly->y3 = tile->y - oy;
                        g_conditions_burst_rotation.vx = 0;
                        g_conditions_burst_rotation.vy = 0;
                        g_conditions_burst_rotation.vz = tile->angle + 0x600;
                        RotMatrix(&g_conditions_burst_rotation, &g_conditions_burst_matrix);
                        g_conditions_burst_offset.vx = tile->radius * ONE;
                        g_conditions_burst_offset.vy = 0;
                        g_conditions_burst_offset.vz = 0;
                        ApplyMatrixLV(
                            &g_conditions_burst_matrix, &g_conditions_burst_offset, &g_conditions_burst_rotated_offset);
                        ox = g_conditions_burst_rotated_offset.vx / 0x4000;
                        oy = g_conditions_burst_rotated_offset.vy / 0x4000;
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
                        poly->r0 = g_after_battle_gfx_fade_rgb[0];
                        poly->g0 = g_after_battle_gfx_fade_rgb[1];
                        poly->b0 = g_after_battle_gfx_fade_rgb[2];
                        gfx_append_gpu_primitive_to_secondary_otag(poly);
                    } else {
                        poly->r0 = g_after_battle_gfx_fade_rgb[0];
                        poly->g0 = g_after_battle_gfx_fade_rgb[1];
                        poly->b0 = g_after_battle_gfx_fade_rgb[2];
                        /* Waiting tiles stay hidden in phase 0 and keep their full shape in phase 1. */
                        if (phase == 1) {
                            gfx_append_gpu_primitive_to_secondary_otag(poly);
                        }
                        any = 1;
                        tile->delay--;
                    }
                }
            }

            gfx_append_gpu_primitive_to_secondary_otag(&g_conditions_burst_draw_modes[frame & 1]);
            /* One more frame is drawn after every tile has finished. */
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

        SetPolyF4(&g_conditions_burst_flash_polys[0]);
        setXY4(&g_conditions_burst_flash_polys[0], 0x80, 0, 0x180, 0, 0x80, 0x100, 0x180, 0x100);
        g_conditions_burst_flash_polys[0].r0 = 0x30;
        g_conditions_burst_flash_polys[0].g0 = 0x28;
        g_conditions_burst_flash_polys[0].b0 = 0x10;
        SetSemiTrans(&g_conditions_burst_flash_polys[0], 1);
        mem_copy_bytes_nonempty(
            &g_conditions_burst_flash_polys[1], &g_conditions_burst_flash_polys[0], sizeof(POLY_F4));
        frame++;
        /* The controlling thread ends the hold by switching this task to THREAD_TASK_DARK_SCREEN. */
        thread_current_task_set(THREAD_TASK_DARK_SCREEN_HOLD);
        for (;;) {
            thread_yield();
            g_conditions_burst_flash_polys[frame & 1].r0 = g_after_battle_gfx_fade_rgb[0];
            g_conditions_burst_flash_polys[frame & 1].g0 = g_after_battle_gfx_fade_rgb[1];
            g_conditions_burst_flash_polys[frame & 1].b0 = g_after_battle_gfx_fade_rgb[2];
            gfx_append_gpu_primitive_to_secondary_otag(&g_conditions_burst_flash_polys[frame & 1]);
            gfx_append_gpu_primitive_to_secondary_otag(&g_conditions_burst_draw_modes[frame & 1]);
            if (g_threads[g_thread_current_id].task_id == THREAD_TASK_DARK_SCREEN) {
                break;
            }
            frame++;
        }

        frame++;
        /* Restart the delays so phase 1 collapses the tiles in the same order. */
        for (row = 0; row < 12; row++) {
            for (col = 0; col < 24; col++) {
                tile = &g_conditions_burst_tiles[row][col];
                tile->delay = tile->initial_delay;
            }
        }
    }

    thread_frames_wait(1);
    thread_current_exit();
}
