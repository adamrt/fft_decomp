#include "fft/battle_script.h"
#include "fft/event_equip.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Same double-buffered menu_status_panel_buffer_t frame as equip_panel_run_equipment_list_thread and the
 * WORLD twin world_menu_ability_panel_thread. */

/* Two-column ability panel thread (task 0x3b); EQUIP twin of WORLD
 * world_menu_ability_panel_thread. Thread 10 draws the unit editor stats into the first
 * frame pair, other threads draw g_item_preview_stat_detail into the second. */
void menu_run_ability_list_thread_variant(void) {
    menu_status_thread_t offset;
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
    g_menu_input_controller = script_get_controller_input_pointer(0);
    if (g_thread_current_id == EQUIP_THREAD_SLOT_EQUIPMENT_LIST) {
        text = g_panel_text_image_a;
        screen = g_panel_frames_a;
        state = &g_menu_selected_unit_fields.state;
        base = g_panel_frames_a;
        upload = &g_panel_text_upload_rect_a;
        u = 0;
    } else {
        text = g_panel_text_image_b;
        screen = g_equip_panel_frames_b;
        state = &g_item_preview_stat_detail.state;
        upload = &g_panel_text_upload_rect_b;
        base = g_equip_panel_frames_b;
        u = 0x50;
    }
    menu_init_numeric_display_frame_primitives(&g_equip_right_panel_frame_rect, &screen->numeric_frame);
    menu_init_status_panel_primitives(screen);
    menu_init_scaled_draw_area_packets(&screen->portrait);
    /* The tile/line fixups must read g_panel_origin_offsets as an array
     * element: a cast of a u8[] view folds the loop's y reads to absolute
     * addresses instead of the target's pointer-relative loads. */
    screen->tiles[1].x0 += g_panel_origin_offsets[0].x;
    screen->tiles[1].y0 += g_panel_origin_offsets[0].y;
    for (i = 4; i < 8; i++) {
        screen->lines[i].x0 += g_panel_origin_offsets[0].x;
        screen->lines[i].y0 += g_panel_origin_offsets[0].y;
        screen->lines[i].x1 += g_panel_origin_offsets[0].x;
        screen->lines[i].y1 += g_panel_origin_offsets[0].y;
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
                gfx_init_image_loading(&screen->sprites[i], g_editor_numeric_geometry, g_panel_origin_offsets,
                    g_panel_label_layouts_mode0 + i);
            }
        } else {
            for (i = 0; i < 19; i++) {
                gfx_init_image_loading(&screen->sprites[i], g_editor_numeric_geometry, g_panel_origin_offsets,
                    g_panel_label_layouts_mode1 + i);
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
        /* The explicit reset lengthens i's live range, which keeps it in $s2. */
        i = 0;
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
        g_gfx_draw_offset_y = g_main_gfx_screen_polarity * 0xf0 != 0xf0 ? 0xf0 : 0;
        screen->draw_offsets[0].position.y = thread->y + g_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[0].packet, (u16*)&screen->draw_offsets[0].position);
        screen->draw_offsets[1].position.x = -0x80;
        screen->draw_offsets[1].position.y = g_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[1].packet, (u16*)&screen->draw_offsets[1].position);
        offset.x = thread->x - 0x73;
        offset.y = thread->y;
        menu_build_scaled_draw_area_packets(
            &screen->portrait, &g_menu_status_panel_frame_rect[2], frame, g_main_gfx_screen_polarity * 0xf0, &offset);
        menu_set_status_panel_primitive_colors(screen, thread);
        menu_select_status_primitive_color_palette(&screen->numeric_frame, thread);
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
        if (thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    thread_yield();
    thread_exit_current();
}
