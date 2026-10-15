#include "fft/battle_gfx.h"
#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Deployment-screen render thread: builds the double-buffered primitive block,
 * then each frame draws the arrow, portraits, cursors and tile grid until the
 * thread's third parameter is set.
 *
 * Both arms of the arrow test end with the same tile call and screen-offset
 * computation; cross-jumping merges them, which keeps the polarity offset
 * chain at the top of the joined block as in the target. The offset has its
 * own variable rather than reusing `x`. */
void attack_deploy_run_render_thread(void) {
    s32 frame;
    s32 i;
    s32 x;
    s32 y;
    s32 count;
    SPRT* tile;
    s32 offset_y;

    attack_deploy_init_cursor_primitives();
    attack_deploy_build_screen_arrow();
    attack_gfx_init_frame_ft4_array();
    menu_init_sprite_array(g_attack_deploy_render_buffers[0].tiles[0], 0x19, 0x7ebc);
    LoadImage(&g_attack_deploy_tile_clut_rect, (u32*)g_attack_deploy_tile_clut_data);
    StoreImage(&g_attack_deploy_tile_clut_rect, (u32*)g_attack_deploy_tile_clut_buffer);
    StoreImage(&g_attack_deploy_rect_09c, (u32*)g_attack_deploy_semitrans_clut_buffer);
    thread_wait_frames(1);
    for (i = 1; i < 0x30; i++) {
        g_attack_deploy_tile_clut_buffer[i].semi_transparent = 1;
    }
    g_attack_deploy_tile_clut_buffer[0].red = g_attack_deploy_tile_clut_buffer[0].green
        = g_attack_deploy_tile_clut_buffer[0].blue = g_attack_deploy_tile_clut_buffer[0].semi_transparent = 0;
    LoadImage(&g_attack_deploy_rect_0cc, (u32*)g_attack_deploy_tile_clut_buffer);
    for (i = 1; i < 0x10; i++) {
        g_attack_deploy_semitrans_clut_buffer[i].semi_transparent = 1;
    }
    g_attack_deploy_semitrans_clut_buffer[0].red = g_attack_deploy_semitrans_clut_buffer[0].green
        = g_attack_deploy_semitrans_clut_buffer[0].blue = g_attack_deploy_semitrans_clut_buffer[0].semi_transparent = 0;
    LoadImage(&g_attack_deploy_rect_0ac, (u32*)g_attack_deploy_semitrans_clut_buffer);
    SetDrawMode(
        &g_attack_deploy_render_buffers[0].draw_mode_9c4, 0, 0, GetTPage(0, 1, 0x100, 0), &g_menu_draw_area_template);
    SetDrawMode(
        &g_attack_deploy_render_buffers[0].draw_mode_9d0, 0, 0, GetTPage(0, 1, 0x100, 0), &g_menu_draw_area_template);
    SetDrawMode(
        &g_attack_deploy_render_buffers[0].draw_mode_9dc, 0, 0, GetTPage(0, 1, 0x100, 0), &g_menu_draw_area_template);
    SetDrawMode(
        &g_attack_deploy_render_buffers[0].draw_mode_9e8, 0, 0, GetTPage(0, 1, 0x100, 0), &g_menu_draw_area_template);
    SetDrawMode(&g_attack_deploy_render_buffers[0].draw_mode_9f4, 0, 0, GetTPage(0, 1, 0x3c0, 0x100),
        &g_menu_draw_area_template);
    attack_deploy_prepare_valid_tiles(g_attack_deploy_render_buffers[0].tiles[0]);
    mem_copy_bytes_nonempty(
        &g_attack_deploy_render_buffers[1], &g_attack_deploy_render_buffers[0], sizeof(attack_deploy_render_buffer_t));
    frame = 0;
    while (1) {
        thread_yield();
        if (g_attack_deploy_tiles_only_mode == 0 && g_attack_deploy_menu_state != ATTACK_DEPLOY_MENU_HIDDEN) {
            attack_deploy_build_menu_cursor_primitives(
                frame, g_attack_deploy_menu_state, &g_attack_deploy_render_buffers[frame & 1]);
        }
        gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].draw_mode_9c4);
        if (g_attack_deploy_tiles_only_mode != 0) {
            if (g_attack_deploy_arrow_position_mode == 0) {
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x0 = 0xb9;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x1 = 0xe0;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x2 = 0xc2;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y0 = 0x7e;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y1 = 0x91;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y2 = 0x90;
            } else if (g_attack_deploy_arrow_position_mode == 1) {
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x0 = 0x140;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x1 = 0x119;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x2 = 0x137;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y0 = 0x7e;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y1 = 0x91;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y2 = 0x90;
            } else if (g_attack_deploy_arrow_position_mode == 2) {
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x0 = 0x140;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x1 = 0x119;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x2 = 0x137;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y0 = 0x5e;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y1 = 0x4b;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y2 = 0x4c;
            } else {
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x0 = 0xb9;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x1 = 0xe0;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].x2 = 0xc2;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y0 = 0x5c;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y1 = 0x49;
                g_attack_deploy_render_buffers[frame & 1].screen_arrow.tri[0].y2 = 0x4a;
            }
            attack_deploy_prepare_valid_tiles(g_attack_deploy_render_buffers[frame & 1].tiles[0]);
            offset_y = g_main_gfx_screen_polarity == 0 ? 0xf0 : 0;
        } else {
            attack_deploy_prepare_valid_tiles(g_attack_deploy_render_buffers[frame & 1].tiles[0]);
            offset_y = g_main_gfx_screen_polarity == 0 ? 0xf0 : 0;
        }
        g_attack_deploy_render_buffers[frame & 1].world_position.x = g_attack_deploy_grid_slide_x - 0x80;
        g_attack_deploy_render_buffers[frame & 1].world_position.y = g_attack_deploy_grid_slide_y + offset_y;
        SetDrawOffset(&g_attack_deploy_render_buffers[frame & 1].world_offset,
            (u16*)&g_attack_deploy_render_buffers[frame & 1].world_position);
        g_attack_deploy_render_buffers[frame & 1].screen_position.x = -0x80;
        g_attack_deploy_render_buffers[frame & 1].screen_position.y = offset_y;
        SetDrawOffset(&g_attack_deploy_render_buffers[frame & 1].screen_offset,
            (u16*)&g_attack_deploy_render_buffers[frame & 1].screen_position);
        if (thread_get_current_parameter_3_variant() != 0) {
            break;
        }
        if (g_attack_deploy_tiles_only_mode == 0) {
            attack_gfx_build_status_group_primitives(g_attack_deploy_render_buffers[frame & 1].status);
        }
        gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].screen_offset);
        attack_deploy_update_cursor_primitives(frame, &g_attack_deploy_render_buffers[frame & 1]);
        if (g_attack_deploy_tiles_only_mode == 0) {
            gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].screen_arrow.poly);
        }
        count = 0;
        tile = g_attack_deploy_render_buffers[frame & 1].tiles[0];
        g_attack_deploy_cursor_1_submitted = 0;
        g_attack_deploy_cursor_0_submitted = 0;
        if (g_attack_deploy_active_cursor == 1) {
            g_attack_deploy_cursor_1_poly = &g_attack_deploy_render_buffers[frame & 1].cursor[1][0];
            g_attack_deploy_cursor_1_submitted = 1;
            gfx_draw_or_append_gpu_primitive(g_attack_deploy_cursor_1_poly);
        }
        g_attack_deploy_cursor_0_poly = &g_attack_deploy_render_buffers[frame & 1].cursor[0][0];
        g_attack_deploy_cursor_0_submitted = 1;
        gfx_draw_or_append_gpu_primitive(g_attack_deploy_cursor_0_poly);
        for (y = 0; y < 5; y++) {
            for (x = 0; x < 5; x++) {
                if (g_attack_deploy_tiles_only_mode == 0
                    && g_attack_deploy_roster_id_by_tile[y][x] != ATTACK_DEPLOY_ROSTER_ID_NONE) {
                    attack_deploy_build_portrait_quads(x, y, g_attack_deploy_roster_id_by_tile[y][x],
                        &g_attack_deploy_render_buffers[frame & 1].portraits[count]);
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].portraits[count]);
                    count++;
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].portraits[count]);
                    count++;
                }
            }
        }
        gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].draw_mode_9f4);
        for (y = 0; y < 5; y++) {
            for (x = 0; x < 5; x++) {
                if (g_attack_deploy_cursor_column[1] == x && g_attack_deploy_cursor_row[1] == y
                    && g_attack_deploy_active_cursor == 1) {
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].draw_mode_9d0);
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].cursor[1][1]);
                }
                if (g_attack_deploy_cursor_column[0] == x && g_attack_deploy_cursor_row[0] == y) {
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].draw_mode_9dc);
                    gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].cursor[0][1]);
                }
                if (g_attack_deploy_tiles_only_mode == 0) {
                    if (g_attack_deploy_valid_tiles[y][x] != 0) {
                        tile->u0 = 0x28;
                        if (g_attack_deploy_roster_id_by_tile[y][x] == ATTACK_DEPLOY_ROSTER_ID_NONE) {
                            tile->clut
                                = GetClut(g_attack_deploy_tile_clut_rect.x + 0x10, g_attack_deploy_tile_clut_rect.y);
                        } else {
                            tile->clut = GetClut(g_attack_deploy_tile_clut_rect.x, g_attack_deploy_tile_clut_rect.y);
                        }
                    } else {
                        tile->u0 = 0;
                        tile->clut = GetClut(g_attack_deploy_tile_clut_rect.x + 0x20, g_attack_deploy_tile_clut_rect.y);
                    }
                } else if (g_attack_deploy_valid_tiles[y][x] != 0) {
                    tile->u0 = 0x28;
                    tile->clut = GetClut(g_attack_deploy_tile_clut_rect.x + 0x30, g_attack_deploy_tile_clut_rect.y);
                    setRGB0(tile, 0xe0, 0x60, 0x60);
                } else {
                    tile->u0 = 0;
                    tile->clut = GetClut(g_attack_deploy_tile_clut_rect.x + 0x50, g_attack_deploy_tile_clut_rect.y);
                    setRGB0(tile, 0x80, 0x80, 0xc0);
                }
                SetSemiTrans(tile, 1);
                SetShadeTex(tile, 0);
                gfx_draw_or_append_gpu_primitive(tile);
                tile++;
            }
        }
        gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].draw_mode_9e8);
        gfx_draw_or_append_gpu_primitive(&g_attack_deploy_render_buffers[frame & 1].world_offset);
        g_attack_deploy_zodiac_draw_context.ot = g_gfx_current_otag_entry;
        if (g_attack_deploy_zodiac_draw_context.scale_x > ONE) {
            g_attack_deploy_zodiac_draw_context.scale_x -= 0x100;
            g_attack_deploy_zodiac_draw_context.scale_y -= 0x100;
        }
        if (g_attack_deploy_tiles_only_mode == 0) {
            main_gfx_call_build_zodiac_bin(&g_attack_deploy_zodiac_draw_context, g_main_zodiac_primitives[frame & 1]);
        }
        frame++;
    }
    thread_exit_current();
}
