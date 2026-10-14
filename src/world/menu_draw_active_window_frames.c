#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "fft/world.h"

/*
 * Draws the active WORLD message and menu windows, last slot pair first.
 *
 * WORLD copy of battle_menu_draw_active_window_frames with one brightness for
 * every window. A window is drawn while its buffer is active and its owner
 * thread runs. A message box is dimmed while a menu task is active. The
 * scroll icons sit 15 pixels in from the right edge (13 when the width is not
 * 4 more than a multiple of 8), shifted 4 pixels up or down for a portrait
 * box, and the scroll marker lies between them in proportion to
 * scroll_position / scroll_range. The portrait quad takes its texture from
 * portrait_code unless that is -1.
 */
void menu_draw_active_window_frames(void) {
    menu_window_buffer_t* window;
    menu_window_buffer_t* buffer;
    s32 height;
    s32 i;
    s32 offset;
    s32 top;
    s32 span;
    s32 flags;
    s32 bottom;

    for (i = 4; i >= 0; i -= 2) {
        if (g_menu_window_buffer_slots[i] == (menu_window_buffer_t*)-1) {
            continue;
        }
        buffer = g_menu_window_buffer_slots[i + g_menu_packet_buffer_index];
        if (buffer->active != 1 || g_thread_contexts[buffer->thread_id].is_running == 0) {
            continue;
        }
        window = buffer;
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[0], g_world_menu_color_red);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[1], g_world_menu_color_red);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[2], g_world_menu_color_red);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[3], g_world_menu_color_red);
        gfx_set_primitive_brightness((P_TAG*)&window->quads[0], g_world_menu_color_red);
        gfx_set_primitive_brightness((P_TAG*)&window->quads[1], g_world_menu_color_red);
        if (window->is_message_box != 0) {
            if (g_menu_input_lock_mode != 0) {
                window->quads[0].clut = 0x7D3C;
                setRGB0(&window->quads[1], 0x40, 0x50, 0x70);
            } else {
                window->quads[0].clut = 0x7C3C;
            }
        }
        offset = window->quads[0].x1 - window->quads[0].x0;
        if (offset / 8 * 8 == offset - 4) {
            offset -= 15;
        } else {
            offset -= 13;
        }
        height = window->quads[0].y2 - window->quads[0].y0;
        window->icon_sprites[0].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[0].y0 = window->quads[0].y0 + 8;
        window->icon_sprites[1].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[1].y0 = window->quads[0].y0 + height - 0x1A;
        if (window->box_type == DIALOG_BOX_TYPE_PORTRAIT) {
            if (window->dialogue_selector == 1) {
                window->icon_sprites[0].y0 -= 4;
                window->icon_sprites[1].y0 -= 4;
            } else {
                window->icon_sprites[0].y0 += 4;
                window->icon_sprites[1].y0 += 4;
            }
        }
        bottom = window->icon_sprites[1].y0;
        top = window->icon_sprites[0].y0 + window->icon_sprites[0].h;
        bottom -= top;
        /* Reload the pointer cell: retaining buffer removes target loads. */
        span = (bottom - 4) * g_menu_window_buffer_slots[i + g_menu_packet_buffer_index]->scroll_position
            / g_menu_window_buffer_slots[i + g_menu_packet_buffer_index]->scroll_range;
        window->icon_sprites[2].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[2].y0 = top + span;
        flags = g_menu_window_buffer_slots[i + g_menu_packet_buffer_index]->icon_flags;
        if (flags & MENU_WINDOW_ICON_SCROLL_UP) {
            gfx_draw_or_append_gpu_primitive(&window->icon_sprites[0]);
        }
        if (flags & MENU_WINDOW_ICON_SCROLL_DOWN) {
            gfx_draw_or_append_gpu_primitive(&window->icon_sprites[1]);
        }
        if (flags & MENU_WINDOW_ICON_SCROLL_MARKER) {
            gfx_draw_or_append_gpu_primitive(&window->icon_sprites[2]);
        }
        gfx_draw_or_append_gpu_primitive(&window->sprites[0]);
        gfx_draw_or_append_gpu_primitive(&window->sprites[1]);
        gfx_draw_or_append_gpu_primitive(&window->sprites[2]);
        gfx_draw_or_append_gpu_primitive(&window->sprites[3]);
        gfx_draw_or_append_gpu_primitive(&window->draw_mode);
        if (window->portrait_code != -1) {
            world_gfx_set_portrait_poly_texture(&window->quads[1], window->portrait_code);
        }
        gfx_draw_or_append_gpu_primitive(&window->quads[1]);
        gfx_draw_or_append_gpu_primitive(&window->quads[0]);
    }
}
