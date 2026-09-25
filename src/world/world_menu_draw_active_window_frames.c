#include "fft/world.h"

void world_menu_draw_active_window_frames(void) {
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
        if (g_world_menu_window_buffer_pointers[i] == (menu_window_buffer_t*)-1) {
            continue;
        }
        buffer = g_world_menu_window_buffer_pointers[i + g_world_menu_packet_buffer_index];
        if (buffer->active != 1 || g_world_thread_contexts[buffer->thread_id].is_running == 0) {
            continue;
        }
        window = buffer;
        world_gfx_set_primitive_brightness((u8*)&window->sprites[0], g_world_menu_color_red);
        world_gfx_set_primitive_brightness((u8*)&window->sprites[1], g_world_menu_color_red);
        world_gfx_set_primitive_brightness((u8*)&window->sprites[2], g_world_menu_color_red);
        world_gfx_set_primitive_brightness((u8*)&window->sprites[3], g_world_menu_color_red);
        world_gfx_set_primitive_brightness((u8*)&window->quads[0], g_world_menu_color_red);
        world_gfx_set_primitive_brightness((u8*)&window->quads[1], g_world_menu_color_red);
        if (window->is_message_box != 0) {
            if (g_world_thread_task_active != 0) {
                window->quads[0].clut = 0x7D3C;
                window->quads[1].r0 = 0x40;
                window->quads[1].g0 = 0x50;
                window->quads[1].b0 = 0x70;
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
        span = (bottom - 4) * g_world_menu_window_buffer_pointers[i + g_world_menu_packet_buffer_index]->scroll_position
            / g_world_menu_window_buffer_pointers[i + g_world_menu_packet_buffer_index]->scroll_range;
        window->icon_sprites[2].x0 = window->quads[0].x0 + offset;
        window->icon_sprites[2].y0 = top + span;
        flags = g_world_menu_window_buffer_pointers[i + g_world_menu_packet_buffer_index]->icon_flags;
        if (flags & 1) {
            world_gfx_draw_or_append_gpu_primitive(&window->icon_sprites[0]);
        }
        if (flags & 2) {
            world_gfx_draw_or_append_gpu_primitive(&window->icon_sprites[1]);
        }
        if (flags & 4) {
            world_gfx_draw_or_append_gpu_primitive(&window->icon_sprites[2]);
        }
        world_gfx_draw_or_append_gpu_primitive(&window->sprites[0]);
        world_gfx_draw_or_append_gpu_primitive(&window->sprites[1]);
        world_gfx_draw_or_append_gpu_primitive(&window->sprites[2]);
        world_gfx_draw_or_append_gpu_primitive(&window->sprites[3]);
        world_gfx_draw_or_append_gpu_primitive(&window->draw_mode);
        if (window->portrait_code != -1) {
            world_gfx_set_portrait_poly_texture(&window->quads[1], window->portrait_code);
        }
        world_gfx_draw_or_append_gpu_primitive(&window->quads[1]);
        world_gfx_draw_or_append_gpu_primitive(&window->quads[0]);
    }
}
