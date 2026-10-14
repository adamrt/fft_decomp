#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
/*
 * Thread task 0x3b: the equipment twin of world_menu_ability_panel_thread (same 0x3d8-byte
 * frame pair, thread record and callee sequence). It lists the five
 * equipment names of an item stat-detail record and draws their item icons.
 */
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Equipment panel thread. The sprite sheet layout (0-3) comes from the two
 * mode flags; the text image is rebuilt on the first frame or on request.
 *
 * The first text loop re-reads equipment[] instead of caching it: the target
 * reloads the slot as a byte after the color store, and the color field goes
 * through a signed temporary because the target shifts it with sra. */
void world_menu_equipment_panel_thread(void) {
    u8* text;
    s32 u;
    menu_status_panel_buffer_t* base;
    menu_status_panel_slot_state_t* state;
    RECT* upload;
    menu_status_thread_t* thread;
    menu_status_panel_buffer_t* screen;
    s32 frame;
    s32 mode;
    s32 color_bits;
    s16 id;
    s32 i;

    thread_set_current_task_id(THREAD_TASK_STATUS_PANEL);
    thread = (menu_status_thread_t*)g_threads[g_thread_current_id].function_parameter_1;
    g_world_input_frame_controller_input = script_get_controller_input_pointer(0);
    if (g_thread_current_id == WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL) {
        text = g_world_change_banner_text_image;
        screen = g_world_change_banner_panel_frames;
        state = &g_world_selected_unit_stat_detail.state;
        base = g_world_change_banner_panel_frames;
        upload = &g_world_equipment_panel_item_text_rect;
        u = 0;
    } else {
        text = g_world_stat_preview_text_image;
        screen = g_world_stat_preview_panel_frames;
        state = &g_world_item_preview_stat_detail.state;
        upload = &g_world_equipment_panel_ability_text_rect;
        base = g_world_stat_preview_panel_frames;
        u = 0x50;
    }
    menu_init_numeric_display_frame_primitives(&g_world_equipment_panel_origin, &screen->numeric_frame);
    world_gfx_init_menu_tile_and_line_primitives(screen);
    world_panel_gfx_init_scaled_draw_area_packets(&screen->portrait);
    screen->tiles[0].x0 += g_world_equipment_panel_origin.x;
    screen->tiles[0].y0 += g_world_equipment_panel_origin.y;
    for (i = 0; i < 4; i++) {
        screen->lines[i].x0 += g_world_equipment_panel_origin.x;
        screen->lines[i].y0 += g_world_equipment_panel_origin.y;
        screen->lines[i].x1 += g_world_equipment_panel_origin.x;
        screen->lines[i].y1 += g_world_equipment_panel_origin.y;
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
                gfx_init_image_loading(&screen->sprites[i], g_world_editor_numeric_geometry,
                    &g_world_equipment_panel_origin, &g_world_equipment_panel_sprite_params_0[i]);
            }
            if (mode == 1) {
                gfx_init_image_loading(&screen->sprites[i], g_world_editor_numeric_geometry,
                    &g_world_equipment_panel_origin, &g_world_equipment_panel_sprite_params_1[i]);
            }
            if (mode == 2) {
                gfx_init_image_loading(&screen->sprites[i], g_world_editor_numeric_geometry,
                    &g_world_equipment_panel_origin, &g_world_equipment_panel_sprite_params_2[i]);
            }
            if (mode == 3) {
                gfx_init_image_loading(&screen->sprites[i], g_world_editor_numeric_geometry,
                    &g_world_equipment_panel_origin, &g_world_equipment_panel_sprite_params_3[i]);
            }
        }
        if (frame == 0 || thread->redraw_request == 1) {
            world_clear_menu_render_buffer(text, 0xc80);
            g_menu_text_state.pen.stride = 0x50;
            menu_set_text_origin(0, 0);
            for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
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
        menu_init_sprite_array(&screen->sprites[19], WORLD_EQUIPMENT_SLOT_COUNT, 0x7cfc);
        for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
            gfx_init_image_loading(&screen->sprites[19 + i], g_world_panel_item_icon_texture,
                &g_world_equipment_panel_origin, &g_world_equipment_panel_icon_params[i]);
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
        g_world_gfx_draw_area_y = g_world_frame_arg != 0xf0 ? 0xf0 : 0;
        screen->draw_offsets[0].position.y = thread->y + g_world_gfx_draw_area_y;
        SetDrawOffset(&screen->draw_offsets[0].packet, (u16*)&screen->draw_offsets[0].position);
        screen->draw_offsets[1].position.x = -0x80;
        screen->draw_offsets[1].position.y = g_world_gfx_draw_area_y;
        SetDrawOffset(&screen->draw_offsets[1].packet, (u16*)&screen->draw_offsets[1].position);
        menu_build_scaled_draw_area_packets(
            &screen->portrait, &g_world_equipment_panel_draw_area, frame, g_world_frame_arg, thread);
        world_menu_configure_status_panel_primitive_colors(screen, thread);
        world_menu_select_primitive_color_palette(&screen->numeric_frame, thread);
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
        for (i = 0; i < WORLD_EQUIPMENT_SLOT_COUNT; i++) {
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
        if (world_thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    thread_yield();
    thread_exit_current();
}
