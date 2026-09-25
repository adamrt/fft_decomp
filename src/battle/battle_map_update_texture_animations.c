#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/*
 * Advance the 32 map texture animations once per frame.
 *
 * UV modes step the frame across the canvas row by row (1 loops, 2/0x80
 * ping-pong, 0x15-0x1C fast-forward to the last frame and then play 0x82 in
 * reverse, 5-0xC play once) and queue the DR_MOVE packet of the row;
 * palette modes (3 loops, 4/0x81 ping-pong, 0xD-0x14 play once) reload
 * CLUT row canvas_x / 16 from palette frame frame_x + frame. Mode 0x1D
 * starts a mesh texture animation over polygons canvas_x..canvas_x + width - 1.
 * Only the low byte of g_animation_speed is read (lbu), and the start call
 * passes the polygon range unmasked, as the target does.
 */
void battle_map_update_texture_animations(void) {
    s32 i;

    for (i = 0; i < 32; i++) {
        if (g_battle_map_texture_animations[i].active == 0) {
            continue;
        }
        switch (g_battle_map_texture_animations[i].mode) {
        case 1:
            g_battle_map_texture_animations[i].timer++;
            if (g_battle_map_texture_animations[i].timer < g_battle_map_texture_animations[i].duration) {
                break;
            }
            g_battle_map_texture_animations[i].timer = 0;
            g_battle_map_texture_animations[i].frame++;
            g_battle_map_texture_animations[i].column++;
            if (g_battle_map_texture_animation_moves[i].x0 + g_battle_map_texture_animations[i].width * 2
                > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                g_battle_map_texture_animations[i].column = 0;
                g_battle_map_texture_animations[i].row++;
                g_battle_map_texture_animation_moves[i].x0 = g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2
                    - g_battle_map_texture_animations[i].width;
            }
            if (g_battle_map_texture_animations[i].frame_count - 1 < g_battle_map_texture_animations[i].frame) {
                g_battle_map_texture_animations[i].frame = 0;
                g_battle_map_texture_animations[i].column = 0;
                g_battle_map_texture_animations[i].row = 0;
                g_battle_map_texture_animation_moves[i].x0
                    = g_battle_map_texture_animations[i].frame_x - g_battle_map_texture_animations[i].width;
            }
            g_battle_map_texture_animation_moves[i].x0 += g_battle_map_texture_animations[i].width;
            g_battle_map_texture_animation_moves[i].y0 = g_battle_map_texture_animations[i].frame_y
                + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
            AddPrim(g_battle_data->otag, &g_battle_map_texture_animation_moves[i]);
            break;
        case 2:
            g_battle_map_texture_animations[i].timer += (u8)g_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame++;
                g_battle_map_texture_animations[i].column++;
                if (g_battle_map_texture_animation_moves[i].x0 + g_battle_map_texture_animations[i].width * 2
                    > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row++;
                    g_battle_map_texture_animation_moves[i].x0 = g_battle_map_texture_animations[i].frame_x / 64 * 64
                        + 2 - g_battle_map_texture_animations[i].width;
                }
                if (g_battle_map_texture_animations[i].frame_count - 2 < g_battle_map_texture_animations[i].frame) {
                    g_battle_map_texture_animations[i].mode = 0x80;
                }
                g_battle_map_texture_animation_moves[i].x0 += g_battle_map_texture_animations[i].width;
                g_battle_map_texture_animation_moves[i].y0 = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                AddPrim(g_battle_data->otag, &g_battle_map_texture_animation_moves[i]);
            }
            break;
        case 0x80:
            g_battle_map_texture_animations[i].timer += (u8)g_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame--;
                g_battle_map_texture_animations[i].column--;
                if (g_battle_map_texture_animation_moves[i].x0 - g_battle_map_texture_animations[i].width
                    < g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_battle_map_texture_animations[i].column
                        = (0x40 - (s16)(g_battle_map_texture_animations[i].frame_x % 64))
                        / g_battle_map_texture_animations[i].width;
                    g_battle_map_texture_animations[i].row--;
                    g_battle_map_texture_animation_moves[i].x0 = g_battle_map_texture_animations[i].frame_x
                        + g_battle_map_texture_animations[i].column * g_battle_map_texture_animations[i].width;
                }
                if (g_battle_map_texture_animations[i].frame == 0) {
                    g_battle_map_texture_animations[i].mode = 2;
                }
                g_battle_map_texture_animation_moves[i].x0 -= g_battle_map_texture_animations[i].width;
                g_battle_map_texture_animation_moves[i].y0 = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                AddPrim(g_battle_data->otag, &g_battle_map_texture_animation_moves[i]);
            }
            break;
        case 3:
            g_battle_map_texture_animations[i].timer++;
            if (g_battle_map_texture_animations[i].timer < g_battle_map_texture_animations[i].duration) {
                break;
            }
            g_battle_map_texture_animations[i].timer = 0;
            g_battle_map_texture_animations[i].frame++;
            g_battle_map_texture_animations[i].column++;
            if (g_battle_map_texture_animations[i].frame_count - 1 < g_battle_map_texture_animations[i].frame) {
                g_battle_map_texture_animations[i].frame = 0;
                g_battle_map_texture_animations[i].column = 0;
            }
            battle_map_load_palette_data(
                g_battle_map_palette_animation_frames[g_battle_map_texture_animations[i].frame_x]
                    + g_battle_map_texture_animations[i].column * 16,
                0, g_battle_map_texture_animations[i].canvas_x / 16, 0);
            break;
        case 4:
            g_battle_map_texture_animations[i].timer += (u8)g_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame++;
                if (g_battle_map_texture_animations[i].frame == g_battle_map_texture_animations[i].frame_count - 1) {
                    g_battle_map_texture_animations[i].mode = 0x81;
                }
                battle_map_load_palette_data(
                    g_battle_map_palette_animation_frames[g_battle_map_texture_animations[i].frame_x]
                        + g_battle_map_texture_animations[i].frame * 16,
                    0, g_battle_map_texture_animations[i].canvas_x / 16, 0);
            }
            break;
        case 0x81:
            g_battle_map_texture_animations[i].timer += (u8)g_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame--;
                if (g_battle_map_texture_animations[i].frame == 0) {
                    g_battle_map_texture_animations[i].mode = 4;
                }
                battle_map_load_palette_data(
                    g_battle_map_palette_animation_frames[g_battle_map_texture_animations[i].frame_x]
                        + g_battle_map_texture_animations[i].frame * 16,
                    0, g_battle_map_texture_animations[i].canvas_x / 16, 0);
            }
            break;
        case 0x15:
        case 0x16:
        case 0x17:
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        case 0x1C:
            g_battle_map_texture_animations[i].frame = 0;
            while (g_battle_map_texture_animations[i].frame < g_battle_map_texture_animations[i].frame_count) {
                g_battle_map_texture_animations[i].column++;
                if (g_battle_map_texture_animation_moves[i].x0 + g_battle_map_texture_animations[i].width * 2
                    > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row++;
                    g_battle_map_texture_animation_moves[i].x0 = g_battle_map_texture_animations[i].frame_x / 64 * 64
                        + 2 - g_battle_map_texture_animations[i].width;
                }
                g_battle_map_texture_animation_moves[i].x0 += g_battle_map_texture_animations[i].width;
                g_battle_map_texture_animation_moves[i].y0 = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                g_battle_map_texture_animations[i].frame++;
            }
            g_battle_map_texture_animations[i].timer = 0x7E;
            g_battle_map_texture_animations[i].mode = 0x82;
            /* fall through */
        case 0x82:
            g_battle_map_texture_animations[i].timer += (u8)g_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame--;
                g_battle_map_texture_animations[i].column--;
                if (g_battle_map_texture_animation_moves[i].x0 - g_battle_map_texture_animations[i].width
                    < g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_battle_map_texture_animations[i].column
                        = (0x40 - (s16)(g_battle_map_texture_animations[i].frame_x % 64))
                            / g_battle_map_texture_animations[i].width
                        - 1;
                    g_battle_map_texture_animations[i].row--;
                }
                if (g_battle_map_texture_animations[i].frame == 0) {
                    g_battle_map_texture_animations[i].active = 0;
                    g_battle_map_texture_animations[i].mode = 0x15;
                }
                g_battle_map_texture_animation_moves[i].x0 = g_battle_map_texture_animations[i].frame_x
                    + g_battle_map_texture_animations[i].width * g_battle_map_texture_animations[i].column;
                g_battle_map_texture_animation_moves[i].y0 = g_battle_map_texture_animations[i].frame_y
                    + g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                AddPrim(g_battle_data->otag, &g_battle_map_texture_animation_moves[i]);
            }
            break;
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 0xA:
        case 0xB:
        case 0xC:
            g_battle_map_texture_animations[i].timer += (u8)g_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                if (g_battle_map_texture_animations[i].column != 0) {
                    g_battle_map_texture_animation_moves[i].x0 += g_battle_map_texture_animations[i].width;
                }
                g_battle_map_texture_animations[i].timer = 0;
                g_battle_map_texture_animations[i].frame++;
                g_battle_map_texture_animations[i].column++;
                if (g_battle_map_texture_animation_moves[i].x0 + g_battle_map_texture_animations[i].width
                    > g_battle_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row++;
                    g_battle_map_texture_animation_moves[i].x0
                        = g_battle_map_texture_animations[i].frame_x / 64 * 64 + 2;
                    g_battle_map_texture_animation_moves[i].y0
                        += g_battle_map_texture_animations[i].height * g_battle_map_texture_animations[i].row;
                }
                if (g_battle_map_texture_animations[i].frame > g_battle_map_texture_animations[i].frame_count) {
                    g_battle_map_texture_animations[i].frame = 0;
                    g_battle_map_texture_animations[i].column = 0;
                    g_battle_map_texture_animations[i].row = 0;
                    g_battle_map_texture_animations[i].active = 0;
                    g_battle_map_texture_animation_moves[i].x0 = g_battle_map_texture_animations[i].frame_x;
                    g_battle_map_texture_animation_moves[i].y0 = g_battle_map_texture_animations[i].frame_y;
                    break;
                }
                AddPrim(g_battle_data->otag, &g_battle_map_texture_animation_moves[i]);
            }
            break;
        case 0xD:
        case 0xE:
        case 0xF:
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
        case 0x14:
            g_battle_map_texture_animations[i].timer += (u8)g_animation_speed;
            if (g_battle_map_texture_animations[i].timer > g_battle_map_texture_animations[i].duration) {
                g_battle_map_texture_animations[i].timer = 0;
                battle_map_load_palette_data(
                    g_battle_map_palette_animation_frames[g_battle_map_texture_animations[i].frame_x]
                        + g_battle_map_texture_animations[i].frame * 16,
                    0, g_battle_map_texture_animations[i].canvas_x / 16, 0);
                g_battle_map_texture_animations[i].frame++;
                if (g_battle_map_texture_animations[i].frame == g_battle_map_texture_animations[i].frame_count) {
                    g_battle_map_texture_animations[i].active = 0;
                }
            }
            break;
        case 0x1D:
            if (g_battle_map_texture_animation_handles[i] == 0) {
                g_battle_map_texture_animation_handles[i] = battle_map_start_texture_animation(
                    g_battle_map_texture_animations[i].duration, 0, g_battle_map_texture_animations[i].canvas_x,
                    g_battle_map_texture_animations[i].canvas_x + g_battle_map_texture_animations[i].width - 1);
                if (g_battle_map_texture_animation_handles[i] == 0) {
                    main_system_handle_malloc_exception(2, 0x3CC);
                }
            }
            break;
        }
    }
}
