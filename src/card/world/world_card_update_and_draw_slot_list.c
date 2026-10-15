#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Memory-card slot list: in menu step 3 handle slot selection input, then
 * draw the visible slot rows, the row cursor and the scroll arrows.
 *
 * WORLD twin of CARD's card_save_update_and_draw_slot_list. The row sprites
 * and number commands are separate globals: the target keeps one address per
 * field and derives each call argument from it (reg - 2, reg - 8), which an
 * array of records would fold into a single base register.
 */
void world_card_update_and_draw_slot_list(s32 input) {
    s32 i;
    s32 row;
    s32 remaining;
    s32 busy;
    s16 source_v;
    u16 texture_page;
    s32 value;
    RECT rect;

    if (g_world_card_menu_step == WORLD_CARD_MENU_STEP_SELECT_SAVE) {
        if (g_world_card_slot_list_input_ready == 0) {
            g_world_card_overwrite_prompt_running = 0;
            g_world_card_load_prompt_running = 0;
            if ((input & 0xffff) == 0) {
                g_world_card_slot_list_input_ready = 1;
            } else {
                input = 0;
            }
        }
        if (card_poll_info_result_variant() > CARD_IO_RESULT_COMPLETE) {
            if (card_io_wait_operation_result_variant(3) != CARD_IO_RESULT_COMPLETE) {
                menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU);
                menu_stop_thread_and_wait(THREAD_SCREEN_SLOT_MENU_CHILD);
                g_world_card_menu_step = WORLD_CARD_MENU_STEP_SCANNING;
                g_main_card_save_last_used_slot = CARD_SAVE_LAST_USED_SLOT_NONE;
            }
        } else if (g_world_card_overwrite_prompt_running != 0) {
            g_world_card_overwrite_prompt_running
                = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_world_card_overwrite_confirm_prompt);
            if (g_world_card_overwrite_prompt_running == 0 && g_world_menu_selection_results[5] == 0) {
                g_world_card_menu_step = WORLD_CARD_MENU_STEP_SAVING;
            }
        } else if (g_world_card_load_prompt_running != 0) {
            g_world_card_load_prompt_running
                = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, g_world_card_load_confirm_prompt);
            if (g_world_card_load_prompt_running == 0 && g_world_menu_selection_results[5] == 0) {
                g_world_card_menu_step = WORLD_CARD_MENU_STEP_LOADING;
            }
        } else if ((input & PSX_PAD_CIRCLE) && g_world_card_list_scroll_velocity == 0) {
            main_sound_play_sfx(MAIN_SFX_CONFIRM);
            remaining = g_world_card_slot_cursor_row;
            if (g_world_card_save_mode == 0) {
                for (i = 0; i < 15; i++) {
                    if (g_main_card_save_slot_file_states[i] != CARD_SAVE_SLOT_UNAVAILABLE) {
                        remaining--;
                    }
                    if (remaining < 0) {
                        break;
                    }
                }
            } else {
                for (i = 0; i < 15; i++) {
                    if (g_main_card_save_slot_file_states[i] == CARD_SAVE_SLOT_OCCUPIED) {
                        remaining--;
                    }
                    if (remaining < 0) {
                        break;
                    }
                }
            }
            g_world_card_active_slot = i;
            i = g_world_card_slot_cursor_row - g_world_card_list_first_visible_row;
            switch (i) {
            case 0:
                i = 0x5a;
                break;
            case 1:
                i = 0x88;
                break;
            case 2:
                i = 0x1e;
                break;
            case 3:
                i = 0x3a;
                break;
            }
            if (g_world_card_save_mode == 0) {
                g_world_card_confirm_prompt = g_world_card_overwrite_confirm_prompt;
                g_world_card_overwrite_confirm_prompt[1].selected_index = 1;
            } else {
                g_world_card_confirm_prompt = g_world_card_load_confirm_prompt;
                g_world_card_load_confirm_prompt[1].selected_index = 0;
            }
            g_world_card_confirm_prompt->window.y = i;
            if (g_world_card_save_mode == 0) {
                if (g_main_card_save_slot_file_states[g_world_card_active_slot] == CARD_SAVE_SLOT_OCCUPIED) {
                    g_world_card_overwrite_prompt_running = 1;
                } else {
                    g_world_card_menu_step = WORLD_CARD_MENU_STEP_SAVING;
                }
            } else {
                g_world_card_load_prompt_running = 1;
            }
        } else if (input & PSX_PAD_CROSS) {
            if (g_world_card_list_scroll_velocity == 0) {
                g_sound_queued_effect_id = MAIN_SFX_CANCEL;
                g_world_card_menu_step = WORLD_CARD_MENU_STEP_SELECT_CARD;
            }
        } else if (g_world_card_list_scroll_velocity == 0) {
            i = g_world_card_slot_cursor_row;
            if (input & PSX_PAD_DOWN) {
                g_world_card_slot_cursor_row = i + 1;
                if (g_world_card_slot_cursor_row >= g_world_card_selectable_slot_count) {
                    g_world_card_slot_cursor_row = g_world_card_selectable_slot_count - 1;
                }
            } else if ((input & PSX_PAD_UP) && i != 0) {
                g_world_card_slot_cursor_row = i - 1;
            }
            if (i != g_world_card_slot_cursor_row) {
                g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
            }
        }
        if (g_world_card_menu_step != WORLD_CARD_MENU_STEP_SELECT_SAVE) {
            input = 0;
            g_world_card_slot_list_input_ready = 0;
        }
    }

    world_card_scroll_menu_list(g_world_card_slot_cursor_row);
    g_menu_window_rect_source = &g_world_card_slot_window;
    busy = g_world_card_menu_step != WORLD_CARD_MENU_STEP_SELECT_SAVE;
    for (i = 0, row = 0; i < 15; i++, row++) {
        if (g_world_card_save_mode != 0) {
            while (g_main_card_save_slot_file_states[i] != CARD_SAVE_SLOT_OCCUPIED) {
                i++;
                if (i >= 15) {
                    break;
                }
            }
        } else {
            while (g_main_card_save_slot_file_states[i] == CARD_SAVE_SLOT_UNAVAILABLE) {
                i++;
                if (i >= 15) {
                    break;
                }
            }
        }
        if (i >= 15) {
            break;
        }
        g_world_card_slot_window.rect.y
            = g_world_card_list_scroll_y + g_world_card_list_scroll_velocity + row * 48 + 12;
        if (g_world_card_slot_window.rect.y < -48 || g_world_card_slot_window.rect.y >= 241) {
            continue;
        }
        gfx_set_otag_index(10);
        value = row != g_world_card_slot_cursor_row || busy != 0;
        world_menu_set_palette_mode(value);
        world_menu_parse_draw_window_frame((menu_instruction_t*)&g_world_card_slot_window_command);
        g_world_card_slot_quad_3.position.y = g_world_card_slot_quad_4.position.y = g_world_card_slot_quad_0.position.y
            = g_world_card_slot_quad_1.position.y = g_world_card_slot_window.rect.y - 14;
        g_world_card_slot_quad_3.position.y = g_world_card_slot_quad_3.position.y + 1;
        g_world_card_slot_quad_4.position.y = g_world_card_slot_quad_4.position.y + 1;
        g_world_card_slot_quad_1.position.y++;
        if (g_world_card_slot_quad_0.position.y >= 0 && g_world_card_slot_quad_0.position.y < 241) {
            menu_gfx_enqueue_oriented_textured_quad(
                &g_world_card_slot_quad_0, menu_gfx_get_draw_color_variant(), TEXTURE_UV_FLIP_NONE, 0, 11);
            menu_gfx_enqueue_oriented_textured_quad(
                &g_world_card_slot_quad_1, menu_gfx_get_draw_color_variant(), TEXTURE_UV_FLIP_NONE, 0, 11);
            menu_gfx_enqueue_oriented_textured_quad(
                &g_world_card_slot_quad_3, menu_gfx_get_draw_color_variant(), TEXTURE_UV_FLIP_NONE, 0, 11);
            menu_gfx_enqueue_oriented_textured_quad(
                &g_world_card_slot_quad_4, menu_gfx_get_draw_color_variant(), TEXTURE_UV_FLIP_NONE, 0, 11);
            gfx_set_otag_index(11);
            g_world_card_slot_number_command.y = g_world_card_slot_quad_0.position.y;
            g_world_card_slot_number_command.value = i + 1;
            menu_script_draw_formatted_number_variant((menu_instruction_t*)&g_world_card_slot_number_command);
        }
        g_world_card_slot_quad_2.position.y = g_world_card_slot_quad_0.position.y;
        if (g_world_card_slot_quad_2.position.y >= 0 && g_world_card_slot_quad_2.position.y < 241) {
            menu_gfx_enqueue_oriented_textured_quad(
                &g_world_card_slot_quad_2, menu_gfx_get_draw_color_variant(), TEXTURE_UV_FLIP_NONE, 0, 11);
            g_world_card_playtime_seconds_command.y = g_world_card_playtime_minutes_command.y
                = g_world_card_playtime_hours_command.y = g_world_card_slot_quad_0.position.y;
            g_world_card_playtime_seconds_command.value = g_main_card_save_slot_playtimes[i].seconds;
            g_world_card_playtime_minutes_command.value = g_main_card_save_slot_playtimes[i].minutes;
            g_world_card_playtime_hours_command.value = g_main_card_save_slot_playtimes[i].hours;
            if (g_world_card_playtime_hours_command.value >= 100) {
                g_world_card_playtime_hours_command.value = 99;
            }
            menu_script_draw_formatted_number_variant((menu_instruction_t*)&g_world_card_playtime_hours_command);
            menu_script_draw_formatted_number_variant((menu_instruction_t*)&g_world_card_playtime_minutes_command);
            menu_script_draw_formatted_number_variant((menu_instruction_t*)&g_world_card_playtime_seconds_command);
        }
        if (g_main_card_save_slot_file_states[i] == CARD_SAVE_SLOT_AVAILABLE) {
            rect.x = 0x74;
            rect.y = g_world_card_slot_window.rect.y + 5;
            rect.w = 0x28;
            rect.h = 0x10;
        } else {
            rect.x = 0x1c;
            rect.y = g_world_card_slot_window.rect.y - 3;
            rect.w = 0xc8;
            rect.h = 0x1f;
        }
        if (i < 6) {
            texture_page = g_world_menu_window_tpage;
            source_v = i * 0x20 + 0x30;
        } else if (i < 11) {
            texture_page = GetTPage(0, 0, 0x1c0, 0);
            source_v = (i - 6) * 0x20 + 0x40;
        } else {
            texture_page = GetTPage(0, 0, 0x180, 0);
            source_v = (i - 11) * 0x20 + 0x60;
        }
        menu_gfx_enqueue_textured_quad(&rect, 0, source_v, 0, 0, texture_page, g_world_menu_window_clut, 11);
    }

    gfx_set_otag_index(15);
    value = g_world_card_slot_cursor_row - g_world_card_list_first_visible_row;
    if (value < 0) {
        value = 0;
    }
    if (value >= 4) {
        value = 3;
    }
    g_world_card_slot_cursor_point.y = value * 48 + 0x28;
    menu_update_and_draw_sliding_cursor_variant(&g_world_card_slot_cursor_point, &g_world_card_slot_cursor_anim, busy);
    if (g_world_card_list_first_visible_row != 0) {
        rect.x = 0xe3;
        rect.y = 0x20;
        rect.w = 8;
        rect.h = 0x10;
        menu_gfx_enqueue_textured_quad(
            &rect, 0xd8, 0, 0, 0, g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, 15);
    }
    if (g_world_card_selectable_slot_count - g_world_card_list_first_visible_row >= 5) {
        rect.x = 0xe3;
        rect.y = 0xba;
        rect.w = 8;
        rect.h = 0x10;
        menu_gfx_enqueue_textured_quad(
            &rect, 0xe0, 0, 0, 0, g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, 15);
    }
}
