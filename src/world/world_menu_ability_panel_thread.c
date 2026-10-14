#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
/*
 * Thread task 0x3b: a two-column status panel that lists five ability labels
 * of an item stat-detail record. Thread 10 draws g_world_selected_unit_stat_detail into the first
 * frame pair, other threads draw g_world_item_preview_stat_detail into the
 * second. A trimmed relative of attack_panel_run_character_status_thread
 * (same 0x3d8-byte frame layout and callee sequence).
 */
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/types.h"

void world_menu_ability_panel_thread(void) {
    s32 offset[2];
    /* Never referenced: the target frame has these 16 bytes between the
       offset pair and the spill slots of u/base/upload. */
    s32 unused[4];
    s32 u;
    menu_status_panel_buffer_t* base;
    RECT* upload;
    menu_status_thread_t* thread;
    u8* text;
    menu_status_panel_buffer_t* screen;
    menu_status_panel_slot_state_t* state;
    s32 frame;
    s32 i;
    s32 id;

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
    menu_init_numeric_display_frame_primitives(&g_world_ability_panel_origin, &screen->numeric_frame);
    world_gfx_init_menu_tile_and_line_primitives(screen);
    world_panel_gfx_init_scaled_draw_area_packets(&screen->portrait);
    screen->tiles[1].x0 += g_world_equipment_panel_origin.x;
    screen->tiles[1].y0 += g_world_equipment_panel_origin.y;
    for (i = 4; i < 8; i++) {
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
        if (state->generic_monster == 0) {
            for (i = 0; i < 19; i++) {
                gfx_init_image_loading(&screen->sprites[i], g_world_editor_numeric_geometry,
                    &g_world_equipment_panel_origin, &g_world_equipment_panel_sprite_params_0[i]);
            }
        } else {
            for (i = 0; i < 19; i++) {
                gfx_init_image_loading(&screen->sprites[i], g_world_editor_numeric_geometry,
                    &g_world_equipment_panel_origin, &g_world_equipment_panel_sprite_params_1[i]);
            }
        }
        if (state->generic_monster != 0) {
            for (i = 0; i < 5; i++) {
                u16 ability = state->ability_ids[i];
                if (ability == MENU_STATUS_PANEL_LABEL_NONE || ability == 0) {
                    screen->sprites[7 + i].x0 -= 0x200;
                }
            }
        }
        /* No i = 0 here: the target's reset before this test is reorg
           hoisting the loop's own initialization into a delay slot, and an
           explicit one lengthens i's live range enough to lose it $s1. */
        if (frame == 0 || thread->redraw_request == 1) {
            menu_clear_render_buffer(text, 0xc80);
            g_menu_text_state.pen.stride = 0x50;
            menu_set_text_origin(0, 0);
            for (i = 0; i < 2; i++) {
                id = state->ability_ids[i] + TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
                if (state->generic_monster != 0) {
                    id = state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
                }
                if ((u16)state->ability_ids[i] == MENU_STATUS_PANEL_LABEL_NONE) {
                    g_menu_text_state.pen.rect.y += 0x10;
                } else {
                    menu_display_text_entry(id, text, &g_menu_text_state.pen);
                    g_menu_text_state.pen.rect.y += 0x10;
                }
            }
            for (; i < 5; i++) {
                if ((u16)state->ability_ids[i] != MENU_STATUS_PANEL_LABEL_NONE) {
                    menu_display_text_entry(
                        state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE, text, &g_menu_text_state.pen);
                }
                g_menu_text_state.pen.rect.y += 0x10;
            }
            LoadImage(upload, (u32*)text);
            thread->redraw_request = 0;
        }
        screen->sprites[12].u0 = u;
        screen->sprites[13].u0 = u;
        screen->draw_offsets[0].position.x = thread->x - 0xf3;
        g_world_gfx_draw_area_y = g_world_frame_arg != 0xf0 ? 0xf0 : 0;
        screen->draw_offsets[0].position.y = thread->y + g_world_gfx_draw_area_y;
        SetDrawOffset(&screen->draw_offsets[0].packet, (u16*)&screen->draw_offsets[0].position);
        screen->draw_offsets[1].position.x = -0x80;
        screen->draw_offsets[1].position.y = g_world_gfx_draw_area_y;
        SetDrawOffset(&screen->draw_offsets[1].packet, (u16*)&screen->draw_offsets[1].position);
        offset[0] = thread->x - 0x73;
        offset[1] = thread->y;
        menu_build_scaled_draw_area_packets(
            &screen->portrait, &g_world_ability_panel_draw_area, frame, g_world_frame_arg, offset);
        world_menu_configure_status_panel_primitive_colors(screen, thread);
        world_menu_select_primitive_color_palette(&screen->numeric_frame, thread);
        gfx_draw_or_append_gpu_primitive(&screen->portrait.areas[1]);
        gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[1]);
        gfx_draw_or_append_gpu_primitive(&screen->sprites[1]);
        for (i = 7; i < 12; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&screen->draw_modes[0]);
        gfx_draw_or_append_gpu_primitive(&screen->sprites[13]);
        gfx_draw_or_append_gpu_primitive(&screen->draw_modes[1]);
        gfx_draw_or_append_gpu_primitive(&screen->tiles[1]);
        for (i = 4; i < 8; i++) {
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
