#include "fft/battle_map.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/*
 * Advance the 32 map texture animations once per frame.
 *
 * UV modes step the frame across the canvas row by row (1 loops, 2/0x80
 * ping-pong, 0x15-0x1C fast-forward to the last frame and then play 0x82 in
 * reverse, 5-0xC play once) and queue the DR_MOVE packet of the row;
 * palette modes (3 loops, 4/0x81 ping-pong, 0xD-0x14 play once) reload
 * CLUT canvas_x / 16 of the map row from palette frame frame_x + frame. Mode 0x1D
 * starts a rotation of the map row's colors canvas_x..canvas_x + width - 1.
 * Only the low byte of g_main_system_animation_speed is read (lbu), and the start call
 * passes the color range unmasked, as the target does.
 */
void map_update_texture_animations(void) {
    s32 i;

    for (i = 0; i < MAP_TEXTURE_ANIMATION_COUNT; i++) {
        if (g_map_texture_animations[i].active == 0) {
            continue;
        }
        switch (g_map_texture_animations[i].mode) {
        case MAP_TEXTURE_ANIMATION_MODE_UV_LOOP:
            g_map_texture_animations[i].timer++;
            if (g_map_texture_animations[i].timer < g_map_texture_animations[i].duration) {
                break;
            }
            g_map_texture_animations[i].timer = 0;
            g_map_texture_animations[i].frame++;
            g_map_texture_animations[i].column++;
            if (g_map_texture_animation_moves[i].sx + g_map_texture_animations[i].width * 2
                > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                g_map_texture_animations[i].column = 0;
                g_map_texture_animations[i].row++;
                g_map_texture_animation_moves[i].sx
                    = g_map_texture_animations[i].frame_x / 64 * 64 + 2 - g_map_texture_animations[i].width;
            }
            if (g_map_texture_animations[i].frame_count - 1 < g_map_texture_animations[i].frame) {
                g_map_texture_animations[i].frame = 0;
                g_map_texture_animations[i].column = 0;
                g_map_texture_animations[i].row = 0;
                g_map_texture_animation_moves[i].sx
                    = g_map_texture_animations[i].frame_x - g_map_texture_animations[i].width;
            }
            g_map_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
            g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
            AddPrim(g_battle_gfx_current_render_buffer->otag, &g_map_texture_animation_moves[i]);
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame++;
                g_map_texture_animations[i].column++;
                if (g_map_texture_animation_moves[i].sx + g_map_texture_animations[i].width * 2
                    > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row++;
                    g_map_texture_animation_moves[i].sx
                        = g_map_texture_animations[i].frame_x / 64 * 64 + 2 - g_map_texture_animations[i].width;
                }
                if (g_map_texture_animations[i].frame_count - 2 < g_map_texture_animations[i].frame) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK;
                }
                g_map_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
                g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_map_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG_BACK:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame--;
                g_map_texture_animations[i].column--;
                if (g_map_texture_animation_moves[i].sx - g_map_texture_animations[i].width
                    < g_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_map_texture_animations[i].column
                        = (0x40 - (g_map_texture_animations[i].frame_x % 64)) / g_map_texture_animations[i].width;
                    g_map_texture_animations[i].row--;
                    g_map_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].column * g_map_texture_animations[i].width;
                }
                if (g_map_texture_animations[i].frame == 0) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_PING_PONG;
                }
                g_map_texture_animation_moves[i].sx -= g_map_texture_animations[i].width;
                g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_map_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_LOOP:
            g_map_texture_animations[i].timer++;
            if (g_map_texture_animations[i].timer < g_map_texture_animations[i].duration) {
                break;
            }
            g_map_texture_animations[i].timer = 0;
            g_map_texture_animations[i].frame++;
            g_map_texture_animations[i].column++;
            if (g_map_texture_animations[i].frame_count - 1 < g_map_texture_animations[i].frame) {
                g_map_texture_animations[i].frame = 0;
                g_map_texture_animations[i].column = 0;
            }
            map_load_palette_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                    + g_map_texture_animations[i].column,
                MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame++;
                if (g_map_texture_animations[i].frame == g_map_texture_animations[i].frame_count - 1) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK;
                }
                map_load_palette_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG_BACK:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame--;
                if (g_map_texture_animations[i].frame == 0) {
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_PALETTE_PING_PONG;
                }
                map_load_palette_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_0:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_1:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_2:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_3:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_4:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_5:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_6:
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_7:
            g_map_texture_animations[i].frame = 0;
            while (g_map_texture_animations[i].frame < g_map_texture_animations[i].frame_count) {
                g_map_texture_animations[i].column++;
                if (g_map_texture_animation_moves[i].sx + g_map_texture_animations[i].width * 2
                    > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row++;
                    g_map_texture_animation_moves[i].sx
                        = g_map_texture_animations[i].frame_x / 64 * 64 + 2 - g_map_texture_animations[i].width;
                }
                g_map_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
                g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                g_map_texture_animations[i].frame++;
            }
            g_map_texture_animations[i].timer = 0x7E;
            g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING;
            /* fall through */
        case MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_PLAYING:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame--;
                g_map_texture_animations[i].column--;
                if (g_map_texture_animation_moves[i].sx - g_map_texture_animations[i].width
                    < g_map_texture_animations[i].frame_x / 64 * 64 + 2) {
                    g_map_texture_animations[i].column
                        = (0x40 - (g_map_texture_animations[i].frame_x % 64)) / g_map_texture_animations[i].width - 1;
                    g_map_texture_animations[i].row--;
                }
                if (g_map_texture_animations[i].frame == 0) {
                    g_map_texture_animations[i].active = 0;
                    g_map_texture_animations[i].mode = MAP_TEXTURE_ANIMATION_MODE_UV_REVERSE_ONCE_0;
                }
                g_map_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x
                    + g_map_texture_animations[i].width * g_map_texture_animations[i].column;
                g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y
                    + g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_map_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_0:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_1:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_2:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_3:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_4:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_5:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_6:
        case MAP_TEXTURE_ANIMATION_MODE_UV_ONCE_7:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                if (g_map_texture_animations[i].column != 0) {
                    g_map_texture_animation_moves[i].sx += g_map_texture_animations[i].width;
                }
                g_map_texture_animations[i].timer = 0;
                g_map_texture_animations[i].frame++;
                g_map_texture_animations[i].column++;
                if (g_map_texture_animation_moves[i].sx + g_map_texture_animations[i].width
                    > g_map_texture_animations[i].frame_x / 64 * 64 + 0x3F) {
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row++;
                    g_map_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x / 64 * 64 + 2;
                    g_map_texture_animation_moves[i].sy
                        += g_map_texture_animations[i].height * g_map_texture_animations[i].row;
                }
                if (g_map_texture_animations[i].frame > g_map_texture_animations[i].frame_count) {
                    g_map_texture_animations[i].frame = 0;
                    g_map_texture_animations[i].column = 0;
                    g_map_texture_animations[i].row = 0;
                    g_map_texture_animations[i].active = 0;
                    g_map_texture_animation_moves[i].sx = g_map_texture_animations[i].frame_x;
                    g_map_texture_animation_moves[i].sy = g_map_texture_animations[i].frame_y;
                    break;
                }
                AddPrim(g_battle_gfx_current_render_buffer->otag, &g_map_texture_animation_moves[i]);
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_0:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_1:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_2:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_3:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_4:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_5:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_6:
        case MAP_TEXTURE_ANIMATION_MODE_PALETTE_ONCE_7:
            g_map_texture_animations[i].timer += g_main_system_animation_speed;
            if (g_map_texture_animations[i].timer > g_map_texture_animations[i].duration) {
                g_map_texture_animations[i].timer = 0;
                map_load_palette_data(g_map_palette_animation_frames + g_map_texture_animations[i].frame_x
                        + g_map_texture_animations[i].frame,
                    MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x / 16, 0);
                g_map_texture_animations[i].frame++;
                if (g_map_texture_animations[i].frame == g_map_texture_animations[i].frame_count) {
                    g_map_texture_animations[i].active = 0;
                }
            }
            break;
        case MAP_TEXTURE_ANIMATION_MODE_COLOR_CYCLE:
            if (g_map_texture_animation_cycle_handles[i] == 0) {
                g_map_texture_animation_cycle_handles[i] = map_start_clut_cycle(g_map_texture_animations[i].duration,
                    MAP_CLUT_ROW_MAP, g_map_texture_animations[i].canvas_x,
                    g_map_texture_animations[i].canvas_x + g_map_texture_animations[i].width - 1);
                if (g_map_texture_animation_cycle_handles[i] == 0) {
                    main_system_handle_malloc_exception(2, 0x3CC);
                }
            }
            break;
        }
    }
}
