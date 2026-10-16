/* The thread drawing the selected unit's equipment and ability status panel. */

#include "fft/data_item.h"
#include "fft/gfx.h"
#include "fft/main_gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Display the selected unit's equipment and ability status panel. */
void menu_run_status_panel_thread(void) {
    menu_status_thread_t* thread;
    menu_status_panel_buffer_t* screen;
    menu_status_panel_buffer_t* base;
    menu_status_panel_slot_state_t* state;
    s32 frame;
    s32 mode;
    s32 i;
    s32 scale;
    s32 mask;
    RECT* rect;
    RECT* offsets;

    thread_set_current_task_id(THREAD_TASK_STATUS_PANEL);
    thread = (menu_status_thread_t*)g_threads[g_thread_current_id].function_parameter_1;
    g_menu_input_controller = script_get_controller_input_pointer(0);

    screen = g_panel_frames_a;
    base = g_panel_frames_a;
    state = &g_menu_selected_unit_fields.state;
    rect = g_menu_status_panel_frame_rect;
    menu_init_numeric_display_frame_primitives(rect, &screen->numeric_frame);
    menu_init_status_panel_primitives(screen);
    menu_init_scaled_draw_area_packets(&screen->portrait);

    /* Move the tiles and lines from panel-relative to screen coordinates. */
    for (i = 0; i < 2; i++) {
        screen->tiles[i].x0 += rect->x;
        screen->tiles[i].y0 += rect->y;
    }
    i = 0;
    /* The line pass reloads the origin address instead of reusing `rect`. */
    offsets = g_menu_status_panel_frame_rect;
    for (; i < 8; i++) {
        screen->lines[i].x0 += offsets->x;
        screen->lines[i].y0 += offsets->y;
        screen->lines[i].x1 += offsets->x;
        screen->lines[i].y1 += offsets->y;
    }

    menu_init_sprite_array(&screen->sprites[19], 5, 0x7cfc);
    menu_init_sprite_array(&screen->sprites[0], 12, 0x7d7c);
    menu_init_sprite_array(&screen->sprites[12], 7, MENU_CLUT_NORMAL);
    screen->sprites[0].clut = 0x7cbc;
    screen->sprites[1].clut = 0x7cbc;
    mem_copy_bytes_nonempty(&screen[1], screen, sizeof(menu_status_panel_buffer_t));

    frame = 0;
    for (;;) {
        screen = &base[frame & 1];

        /* The label layout follows the unit: two-handed adds 2, a monster adds 1. */
        mode = (state->two_hands != 0) * 2;
        if (state->generic_monster != 0) {
            mode++;
        }
        for (i = 0; i < 19; i++) {
            if (mode == 0) {
                gfx_init_image_loading(&screen->sprites[i], g_editor_numeric_geometry, g_panel_origin_offsets,
                    &g_panel_label_layouts_mode0[i]);
            }
            if (mode == 1) {
                gfx_init_image_loading(&screen->sprites[i], g_editor_numeric_geometry, g_panel_origin_offsets,
                    &g_panel_label_layouts_mode1[i]);
            }
            if (mode == 2) {
                gfx_init_image_loading(&screen->sprites[i], g_editor_numeric_geometry, g_panel_origin_offsets,
                    &g_panel_label_layouts_mode2[i]);
            }
            if (mode == 3) {
                gfx_init_image_loading(&screen->sprites[i], g_editor_numeric_geometry, g_panel_origin_offsets,
                    &g_panel_label_layouts_mode3[i]);
            }
        }

        /* A monster's empty ability rows move their sprites off screen. */
        if (state->generic_monster != 0) {
            for (i = 0; i < 5; i++) {
                u16 ability = state->ability_ids[i];
                if (ability == MENU_STATUS_PANEL_LABEL_NONE || ability == 0) {
                    screen->sprites[7 + i].x0 -= 0x200;
                }
            }
        }

        /* Redraw and upload the equipment and ability names on the first frame or on request. */
        if (frame == 0 || thread->redraw_request == 1) {
            menu_clear_render_buffer(g_panel_text_image_a, 0xc80);
            g_menu_text_state.pen.stride = 0x50;
            menu_set_text_origin(0, 0);
            for (i = 0; i < 5; i++) {
                if ((state->equipment[i] & ITEM_ID_MASK) != ITEM_ID_NONE) {
                    menu_display_text_entry(
                        state->equipment[i] + TEXT_ID_ITEM_NAME_BASE, g_panel_text_image_a, &g_menu_text_state.pen);
                }
                g_menu_text_state.pen.rect.y += 0x10;
            }
            LoadImage(&g_panel_text_upload_rect_a, (u32*)g_panel_text_image_a);

            menu_clear_render_buffer(g_panel_text_image_b, 0xc80);
            g_menu_text_state.pen.stride = 0x50;
            menu_set_text_origin(0, 0);
            for (i = 0; i < 2; i++) {
                /* `mode` is reused as the text id: the first two rows name skillsets, or abilities
                 * for a monster. */
                mode = state->ability_ids[i] + TEXT_ID_SKILLSET_NAME_SOURCE_BASE;
                if (state->generic_monster != 0) {
                    mode = state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE;
                }
                if ((u16)state->ability_ids[i] == MENU_STATUS_PANEL_LABEL_NONE) {
                    g_menu_text_state.pen.rect.y += 0x10;
                } else {
                    menu_display_text_entry(mode, g_panel_text_image_b, &g_menu_text_state.pen);
                    g_menu_text_state.pen.rect.y += 0x10;
                }
            }
            for (; i < 5; i++) {
                if (state->ability_ids[i] != -1) {
                    menu_display_text_entry(state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE, g_panel_text_image_b,
                        &g_menu_text_state.pen);
                }
                g_menu_text_state.pen.rect.y += 0x10;
            }
            LoadImage(&g_panel_text_upload_rect_b, (u32*)g_panel_text_image_b);
            thread->redraw_request = 0;
        }

        for (i = 0; i < 5; i++) {
            gfx_init_image_loading(&screen->sprites[19 + i], g_panel_item_icon_texture, g_menu_status_panel_frame_rect,
                &g_panel_item_icon_layouts[i]);
            if ((state->equipment[i] & ITEM_ID_MASK) != ITEM_ID_NONE) {
                gfx_set_item_icon_sprite(&screen->sprites[19 + i], state->equipment[i]);
            } else {
                /* The target indexes the five icon sprites from their own base (i * 0x14 + 0x1a8), as a
                 * separate array after the 19 label sprites would. */
                (screen->sprites + i)[19].x0 = -0x200;
            }
        }

        /* A scale (field y) of 0 draws into the lower frame-buffer half (offset 0xF0), 0xF0 into
         * the upper half. */
        scale = g_main_gfx_screen_polarity * 0xf0;
        mask = scale ^ 0xf0;
        mask = -(mask != 0);
        g_gfx_draw_offset_y = mask & 0xf0;
        menu_build_scaled_draw_area_packets(
            &screen->portrait, &g_character_status_draw_area_rect, frame, scale, thread);
        screen->draw_offsets[0].position.x = thread->x - 0x80;
        screen->draw_offsets[0].position.y = thread->y + g_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[0].packet, (u16*)&screen->draw_offsets[0].position);
        screen->draw_offsets[1].position.x = -0x80;
        screen->draw_offsets[1].position.y = g_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[1].packet, (u16*)&screen->draw_offsets[1].position);

        menu_set_status_panel_primitive_colors(screen, thread);
        menu_select_status_primitive_color_palette(&screen->numeric_frame, thread);

        /* Primitives link in at the head of the ordering-table entry, so they draw in reverse:
         * each draw mode or offset added below applies to the sprites added above it. */
        if (frame < 13) {
            gfx_draw_or_append_gpu_primitive(&screen->portrait.areas[1]);
        }
        gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[1]);
        for (i = 0; i < 12; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->sprites[i]);
        }
        for (i = 14; i < 19; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->sprites[i]);
        }
        gfx_draw_or_append_gpu_primitive(&screen->frame_draw_mode);
        gfx_draw_or_append_gpu_primitive(&screen->sprites[12]);
        gfx_draw_or_append_gpu_primitive(&screen->sprites[13]);
        gfx_draw_or_append_gpu_primitive(&screen->text_draw_mode);
        for (i = 0; i < 5; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->sprites[19 + i]);
        }
        gfx_draw_or_append_gpu_primitive(&screen->image_draw_mode);
        gfx_draw_or_append_gpu_primitive(&screen->tiles[0]);
        gfx_draw_or_append_gpu_primitive(&screen->tiles[1]);
        for (i = 0; i < 8; i++) {
            gfx_draw_or_append_gpu_primitive(&screen->lines[i]);
        }
        menu_submit_numeric_display_frame_primitives(&screen->numeric_frame);
        gfx_draw_or_append_gpu_primitive(&screen->draw_offsets[0]);
        /* During the opening zoom the panel draws clipped to the scaled area; portrait.areas[1],
         * drawn last, restores the full area. */
        if (frame < 13) {
            gfx_draw_or_append_gpu_primitive(&screen->portrait);
        }

        thread_yield();
        if (thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }

    thread_yield();
    thread_exit_current();
}
