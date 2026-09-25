#include "fft/battle.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Draws the active battle menu windows, last slot pair first.
 *
 * BATTLE twin of world_menu_draw_active_window_frames: it scales the frame
 * primitives by the window's own brightness, positions the scroll icons and
 * builds the portrait quad from portrait_code (or the unit portrait when -1).
 * As in the WORLD twin, the empty volatile barrier keeps the icon_sprites[1].y0
 * reload ahead of the packet-buffer-index address chain, where gcc otherwise
 * front-loads the longer chain. */
void battle_menu_draw_active_window_frames(void) {
    menu_window_buffer_t* window;
    menu_window_buffer_t* buffer;
    s32 height;
    s32 i;
    s32 offset;
    s32 top;
    s32 span;
    s32 flags;
    s32 bottom;
    s32 u;

    for (i = 4; i >= 0; i -= 2) {
        if (g_battle_menu_buffer_slots[i] == (void*)-1) {
            continue;
        }
        buffer = g_battle_menu_buffer_slots[i + g_battle_menu_packet_buffer_index];
        if (buffer->active != 1 || g_battle_thread_contexts[buffer->thread_id].is_running == 0) {
            continue;
        }
        window = buffer;
        battle_gfx_set_primitive_brightness((u8*)&window->sprites[0], window->r);
        battle_gfx_set_primitive_brightness((u8*)&window->sprites[1], window->r);
        battle_gfx_set_primitive_brightness((u8*)&window->sprites[2], window->r);
        battle_gfx_set_primitive_brightness((u8*)&window->sprites[3], window->r);
        battle_gfx_set_primitive_brightness((u8*)&window->quads[0], window->r);
        battle_gfx_set_primitive_brightness((u8*)&window->quads[1], window->r);
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
        __asm__ volatile("");
        top = window->icon_sprites[0].y0 + window->icon_sprites[0].h;
        span = (bottom - top - 4)
            * ((menu_window_buffer_t*)g_battle_menu_buffer_slots[i + g_battle_menu_packet_buffer_index])
                  ->scroll_position
            / ((menu_window_buffer_t*)g_battle_menu_buffer_slots[i + g_battle_menu_packet_buffer_index])->scroll_range;
        window->icon_sprites[2].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[2].y0 = top + span;
        flags = ((menu_window_buffer_t*)g_battle_menu_buffer_slots[i + g_battle_menu_packet_buffer_index])->icon_flags;
        if (flags & 1) {
            battle_gfx_draw_or_append_gpu_primitive(&window->icon_sprites[0]);
        }
        if (flags & 2) {
            battle_gfx_draw_or_append_gpu_primitive(&window->icon_sprites[1]);
        }
        if (flags & 4) {
            battle_gfx_draw_or_append_gpu_primitive(&window->icon_sprites[2]);
        }
        battle_gfx_draw_or_append_gpu_primitive(&window->sprites[0]);
        battle_gfx_draw_or_append_gpu_primitive(&window->sprites[1]);
        battle_gfx_draw_or_append_gpu_primitive(&window->sprites[2]);
        battle_gfx_draw_or_append_gpu_primitive(&window->sprites[3]);
        battle_gfx_draw_or_append_gpu_primitive(&window->draw_mode);
        if (window->portrait_code != -1) {
            u = window->portrait_code << 5;
            window->quads[1].u0 = u;
            window->quads[1].v0 = 0xCE;
            window->quads[1].u1 = u + 0x1F;
            window->quads[1].v1 = 0xCE;
            window->quads[1].u2 = u;
            window->quads[1].v2 = 0xFE;
            window->quads[1].u3 = u + 0x1F;
            window->quads[1].v3 = 0xFE;
            window->quads[1].tpage = GetTPage(0, 0, 0x1C0, 0);
            window->quads[1].clut = GetClut(window->portrait_code % 4 * 16 + 0x1C0, window->portrait_code / 4 + 0xFE);
        } else {
            battle_menu_build_unit_portrait_poly(&window->quads[1], window->portrait_battle_id);
        }
        battle_gfx_flip_poly_ft4_uv_if_negative(&window->quads[1], -window->portrait_flipped);
        battle_gfx_draw_or_append_gpu_primitive(&window->quads[1]);
        battle_gfx_draw_or_append_gpu_primitive(&window->quads[0]);
    }
}
