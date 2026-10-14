#include "fft/battle_script.h"
#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* The frame is menu_status_panel_buffer_t (0x3d8 bytes, double buffered); the
 * WORLD twin world_menu_equipment_panel_thread uses the same layout. */

/* Equipment panel thread (task 0x3b): lists the five equipped item names and
 * draws their icons; EQUIP twin of WORLD world_menu_equipment_panel_thread.
 *
 * The sprite sheet layout (0-3) comes from the two mode flags; the text image
 * is rebuilt on the first frame or on request. The frame offset goes through
 * the `area` local: the target reads it back through that register inside the
 * line loop but folds the tile y read to the absolute address. */
void equip_panel_run_equipment_list_thread(void) {
    u8* text;
    s32 u;
    menu_status_panel_buffer_t* base;
    menu_status_panel_slot_state_t* state;
    RECT* upload;
    RECT* area;
    menu_status_thread_t* thread;
    menu_status_panel_buffer_t* screen;
    s32 frame;
    s32 mode;
    s32 color_bits;
    s16 id;
    s32 i;

    thread_set_current_task_id(THREAD_TASK_STATUS_PANEL);
    thread = (menu_status_thread_t*)g_threads[g_thread_current_id].function_parameter_1;
    g_menu_input_controller = script_get_controller_input_pointer(0);
    if (g_thread_current_id == EQUIP_THREAD_SLOT_EQUIPMENT_LIST) {
        text = g_menu_status_panel_text_image_a;
        screen = g_menu_status_panel_frames;
        state = &g_menu_status_panel_fields.state;
        base = g_menu_status_panel_frames;
        upload = &g_menu_status_panel_text_upload_rect_a;
        u = 0;
    } else {
        text = g_menu_status_panel_text_image_b;
        screen = g_equip_panel_frames_b;
        state = &g_equip_item_preview_stat_detail.state;
        upload = &g_menu_status_panel_text_upload_rect_b;
        base = g_equip_panel_frames_b;
        u = 0x50;
    }
    area = g_menu_status_panel_origin_offsets;
    menu_init_numeric_display_frame_primitives(area, &screen->numeric_frame);
    menu_init_status_panel_primitives(screen);
    menu_init_scaled_draw_area_packets(&screen->portrait);
    screen->tiles[0].x0 += area->x;
    screen->tiles[0].y0 += area->y;
    for (i = 0; i < 4; i++) {
        screen->lines[i].x0 += area->x;
        screen->lines[i].y0 += area->y;
        screen->lines[i].x1 += area->x;
        screen->lines[i].y1 += area->y;
    }
    menu_init_sprite_array(&screen->sprites[0], 12, 0x7d7c);
    menu_init_sprite_array(&screen->sprites[12], 7, 0x7c3c);
    screen->sprites[0].clut = 0x7cbc;
    screen->sprites[1].clut = 0x7cbc;
    mem_copy_bytes_nonempty(&screen[1], screen, sizeof(menu_status_panel_buffer_t));

    frame = 0;
    for (;;) {
        screen = &base[frame & 1];
        mode = (state->two_hands != 0) * 2;
        if (state->generic_monster != 0) {
            mode++;
        }
        for (i = 0; i < 19; i++) {
            if (mode == 0) {
                gfx_init_image_loading(&screen->sprites[i], g_menu_status_panel_numeric_geometry,
                    g_menu_status_panel_origin_offsets, g_menu_status_panel_label_layouts_mode0 + i);
            }
            if (mode == 1) {
                gfx_init_image_loading(&screen->sprites[i], g_menu_status_panel_numeric_geometry,
                    g_menu_status_panel_origin_offsets, g_menu_status_panel_label_layouts_mode1 + i);
            }
            if (mode == 2) {
                gfx_init_image_loading(&screen->sprites[i], g_menu_status_panel_numeric_geometry,
                    g_menu_status_panel_origin_offsets, g_menu_status_panel_label_layouts_mode2 + i);
            }
            if (mode == 3) {
                gfx_init_image_loading(&screen->sprites[i], g_menu_status_panel_numeric_geometry,
                    g_menu_status_panel_origin_offsets, g_menu_status_panel_label_layouts_mode3 + i);
            }
        }
        if (frame == 0 || thread->redraw_request == 1) {
            menu_clear_render_buffer(text, 0xc80);
            g_menu_text_state.pen.stride = 0x50;
            menu_set_text_origin(0, 0);
            for (i = 0; i < 5; i++) {
                if ((state->equipment[i] & ITEM_ID_MASK) != ITEM_ID_NONE) {
                    color_bits = state->equipment[i] & 0xc000;
                    g_menu_text_palette_offset = color_bits >> 12;
                    menu_display_text_entry(
                        (state->equipment[i] & ITEM_ID_MASK) | TEXT_ID_ITEM_NAME_BASE, text, &g_menu_text_state.pen);
                }
                g_menu_text_state.pen.rect.y += 0x10;
            }
            LoadImage(upload, (u32*)text);
            thread->redraw_request = 0;
        }
        screen->sprites[12].u0 = u;
        screen->sprites[13].u0 = u;
        menu_init_sprite_array(&screen->sprites[19], 5, 0x7cfc);
        for (i = 0; i < 5; i++) {
            gfx_init_image_loading(&screen->sprites[19 + i], g_menu_status_panel_item_icon_texture,
                g_menu_status_panel_origin_offsets, g_menu_status_panel_item_icon_layouts + i);
            /* Read unsigned: the target extends the id only where it is passed on. */
            id = state->equipment[i];
            if ((id & ITEM_ID_MASK) != ITEM_ID_NONE) {
                gfx_set_item_icon_sprite(&screen->sprites[19 + i], id);
            } else {
                (&screen->sprites[i])[19].x0 = 0;
            }
        }
        screen->sprites[12].u0 = u;
        screen->sprites[13].u0 = u;
        screen->draw_offsets[0].position.x = thread->x - 0x80;
        g_menu_gfx_draw_offset_y = g_main_gfx_screen_polarity * 0xf0 != 0xf0 ? 0xf0 : 0;
        screen->draw_offsets[0].position.y = thread->y + g_menu_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[0].packet, (u16*)&screen->draw_offsets[0].position);
        screen->draw_offsets[1].position.x = -0x80;
        screen->draw_offsets[1].position.y = g_menu_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[1].packet, (u16*)&screen->draw_offsets[1].position);
        menu_build_scaled_draw_area_packets(
            &screen->portrait, &g_menu_status_panel_frame_rect[1], frame, g_main_gfx_screen_polarity * 0xf0, thread);
        menu_set_status_panel_primitive_colors(screen, thread);
        menu_select_status_primitive_color_palette(&screen->numeric_frame, thread);
        gfx_draw_or_append_gpu_primitive(&screen->portrait.areas[1]);
        gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[1]);
        gfx_draw_or_append_gpu_primitive(&screen->sprites[0]);
        for (i = 2; i < 7; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->sprites[i]);
        }
        for (i = 14; i < 19; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&screen->draw_modes[0]);
        gfx_draw_or_append_gpu_primitive(&screen->sprites[12]);
        gfx_draw_or_append_gpu_primitive(&screen->draw_modes[1]);
        for (i = 0; i < 5; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->sprites[19 + i]);
        }
        gfx_draw_or_append_gpu_primitive(&screen->draw_modes[2]);
        gfx_draw_or_append_gpu_primitive(&screen->tiles[0]);
        for (i = 0; i < 4; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->lines[i]);
        }
        menu_submit_numeric_display_frame_primitives(&screen->numeric_frame);
        gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[0]);
        gfx_draw_or_append_gpu_primitive(&screen->portrait);
        thread_yield();
        if (battle_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    thread_yield();
    thread_exit_current();
}
