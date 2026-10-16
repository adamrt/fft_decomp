/* The thread drawing the selected unit's combined equipment and ability status panel. */

#include "fft/data_item.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_panel.h"
#include "fft/menu_status.h"
#include "fft/script.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Thread body (task THREAD_TASK_STATUS_PANEL) of the selected unit's combined equipment and
 * ability panel: item icons, the five equipment names and the five ability names, laid out by
 * whether the unit is two-handed or a monster. Parameter 1 is the panel's unit_panel_thread_t
 * record, whose position places the panel and whose redraw request uploads the names again.
 * The panel zooms open over its first 13 frames and runs until a stop request. */
void unit_panel_equipment_ability_thread(void) {
    unit_panel_thread_t* thread;
    unit_panel_buffer_t* screen;
    unit_panel_buffer_t* base;
    menu_status_panel_slot_state_t* state;
    s32 frame;
    s32 mode;
    s32 i;

    thread_set_current_task_id(THREAD_TASK_STATUS_PANEL);
    thread = (unit_panel_thread_t*)g_threads[g_thread_current_id].function_parameter_1;
    g_menu_input_controller = script_get_controller_input_pointer(0);

    screen = g_unit_panel_equipment_frames;
    base = g_unit_panel_equipment_frames;
    state = &g_unit_panel_selected_unit_fields.state;
    status_screen_init_numbers_frame(&g_unit_panel_equipment_frame_rect, &screen->numeric_frame);
    unit_panel_init_primitives(screen);
    menu_init_scaled_draw_area_packets(&screen->portrait);

    /* Move the tiles and lines from panel-relative to screen coordinates. */
    for (i = 0; i < 2; i++) {
        screen->tiles[i].x0 += g_unit_panel_equipment_frame_rect.x;
        screen->tiles[i].y0 += g_unit_panel_equipment_frame_rect.y;
    }
    for (i = 0; i < 8; i++) {
        screen->lines[i].x0 += g_unit_panel_equipment_frame_rect.x;
        screen->lines[i].y0 += g_unit_panel_equipment_frame_rect.y;
        screen->lines[i].x1 += g_unit_panel_equipment_frame_rect.x;
        screen->lines[i].y1 += g_unit_panel_equipment_frame_rect.y;
    }

    menu_init_sprite_array(&screen->sprites[19], 5, 0x7cfc);
    menu_init_sprite_array(&screen->sprites[0], 12, 0x7d7c);
    menu_init_sprite_array(&screen->sprites[12], 7, MENU_CLUT_NORMAL);
    screen->sprites[0].clut = 0x7cbc;
    screen->sprites[1].clut = 0x7cbc;
    mem_copy_bytes_nonempty(&screen[1], screen, sizeof(unit_panel_buffer_t));

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
                gfx_init_image_loading(&screen->sprites[i], g_unit_panel_editor_numeric_geometry,
                    &g_unit_panel_equipment_origin, &g_unit_panel_equipment_sprite_params_0[i]);
            }
            if (mode == 1) {
                gfx_init_image_loading(&screen->sprites[i], g_unit_panel_editor_numeric_geometry,
                    &g_unit_panel_equipment_origin, &g_unit_panel_equipment_sprite_params_1[i]);
            }
            if (mode == 2) {
                gfx_init_image_loading(&screen->sprites[i], g_unit_panel_editor_numeric_geometry,
                    &g_unit_panel_equipment_origin, &g_unit_panel_equipment_sprite_params_2[i]);
            }
            if (mode == 3) {
                gfx_init_image_loading(&screen->sprites[i], g_unit_panel_editor_numeric_geometry,
                    &g_unit_panel_equipment_origin, &g_unit_panel_equipment_sprite_params_3[i]);
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
            menu_clear_render_buffer(g_unit_panel_equipment_item_text_image, 0xc80);
            g_menu_text_state.pen.stride = 0x50;
            menu_set_text_origin(0, 0);
            for (i = 0; i < 5; i++) {
                if ((state->equipment[i] & ITEM_ID_MASK) != ITEM_ID_NONE) {
                    menu_display_text_entry(state->equipment[i] + TEXT_ID_ITEM_NAME_BASE,
                        g_unit_panel_equipment_item_text_image, &g_menu_text_state.pen);
                }
                g_menu_text_state.pen.rect.y += 0x10;
            }
            LoadImage(&g_unit_panel_equipment_item_text_rect, (u32*)g_unit_panel_equipment_item_text_image);

            menu_clear_render_buffer(g_unit_panel_equipment_ability_text_image, 0xc80);
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
                    menu_display_text_entry(mode, g_unit_panel_equipment_ability_text_image, &g_menu_text_state.pen);
                    g_menu_text_state.pen.rect.y += 0x10;
                }
            }
            for (; i < 5; i++) {
                if (state->ability_ids[i] != -1) {
                    menu_display_text_entry(state->ability_ids[i] + TEXT_ID_ABILITY_NAME_BASE,
                        g_unit_panel_equipment_ability_text_image, &g_menu_text_state.pen);
                }
                g_menu_text_state.pen.rect.y += 0x10;
            }
            LoadImage(&g_unit_panel_equipment_ability_text_rect, (u32*)g_unit_panel_equipment_ability_text_image);
            thread->redraw_request = 0;
        }

        for (i = 0; i < 5; i++) {
            gfx_init_image_loading(&screen->sprites[19 + i], g_unit_panel_item_icon_texture,
                &g_unit_panel_equipment_frame_rect, &g_unit_panel_equipment_icon_params[i]);
            if ((state->equipment[i] & ITEM_ID_MASK) != ITEM_ID_NONE) {
                gfx_set_item_icon_sprite(&screen->sprites[19 + i], state->equipment[i]);
            } else {
                /* The target indexes the five icon sprites from their own base (i * 0x14 + 0x1a8), as a
                 * separate array after the 19 label sprites would. */
                (screen->sprites + i)[19].x0 = -0x200;
            }
        }

        /* A g_frame_arg (field y) of 0 draws into the lower frame-buffer half (offset 0xF0), 0xF0
         * into the upper half. */
        g_gfx_draw_offset_y = g_frame_arg != 0xf0 ? 0xf0 : 0;
        menu_build_scaled_draw_area_packets(
            &screen->portrait, &g_unit_panel_character_status_draw_area_rect, frame, g_frame_arg, thread);
        screen->draw_offsets[0].position.x = thread->x - 0x80;
        screen->draw_offsets[0].position.y = thread->y + g_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[0].packet, (u16*)&screen->draw_offsets[0].position);
        screen->draw_offsets[1].position.x = -0x80;
        screen->draw_offsets[1].position.y = g_gfx_draw_offset_y;
        SetDrawOffset(&screen->draw_offsets[1].packet, (u16*)&screen->draw_offsets[1].position);

        unit_panel_set_colors(screen, thread);
        unit_panel_select_palette(&screen->numeric_frame, thread);

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
        status_screen_submit_numbers_frame(&screen->numeric_frame);
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
