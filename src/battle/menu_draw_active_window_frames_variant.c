#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Draws the active battle menu windows, last slot pair first.
 *
 * BATTLE twin of menu_draw_active_window_frames_variant: it scales the frame
 * primitives by the window's own brightness, positions the scroll icons and
 * builds the portrait quad from portrait_code (or the unit portrait when -1).
 * span starts as the lower icon's y and is narrowed in place to the scroll
 * thumb offset; a single-assignment temporary would load that y last. */
void menu_draw_active_window_frames_variant(void) {
    menu_window_buffer_t* window;
    menu_window_buffer_t* buffer;
    s32 height;
    s32 i;
    s32 offset;
    s32 top;
    s32 span;
    s32 flags;
    s32 u;

    for (i = 4; i >= 0; i -= 2) {
        if (g_menu_window_buffer_slots[i] == (menu_window_buffer_t*)-1) {
            continue;
        }
        buffer = g_menu_window_buffer_slots[i + g_menu_packet_buffer_index];
        if (buffer->active != 1 || g_thread_contexts[buffer->thread_id].is_running == 0) {
            continue;
        }
        window = buffer;
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[0], window->color.r);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[1], window->color.r);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[2], window->color.r);
        gfx_set_primitive_brightness((P_TAG*)&window->sprites[3], window->color.r);
        gfx_set_primitive_brightness((P_TAG*)&window->quads[0], window->color.r);
        gfx_set_primitive_brightness((P_TAG*)&window->quads[1], window->color.r);
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
        span = window->icon_sprites[1].y0;
        top = window->icon_sprites[0].y0 + window->icon_sprites[0].h;
        span -= top;
        span = (span - 4) * g_menu_window_buffer_slots[i + g_menu_packet_buffer_index]->scroll_position
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
            u = window->portrait_code << 5;
            setUV4(&window->quads[1], u, 0xCE, u + 0x1F, 0xCE, u, 0xFE, u + 0x1F, 0xFE);
            window->quads[1].tpage = GetTPage(0, 0, 0x1C0, 0);
            window->quads[1].clut = GetClut(window->portrait_code % 4 * 16 + 0x1C0, window->portrait_code / 4 + 0xFE);
        } else {
            menu_build_unit_portrait_poly_variant(&window->quads[1], window->portrait_battle_id);
        }
        gfx_flip_poly_ft4_uv_if_negative(&window->quads[1], -window->portrait_flipped);
        gfx_draw_or_append_gpu_primitive(&window->quads[1]);
        gfx_draw_or_append_gpu_primitive(&window->quads[0]);
    }
}
