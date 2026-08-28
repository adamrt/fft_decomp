#include "fft/card.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Input is accepted in selection state; rendering also runs
 * during the adjacent confirmation/write states. */
void card_save_update_and_draw_slot_list(u16 input) {
    s32 index;
    s32 value;
    s32 remaining;
    s32 row;
    s32 scrolling;
    s16 source_v;
    u16 texture_page;
    u16 cursor_clut;
    RECT rect;

    if (g_card_save_menu_state == CARD_SAVE_MENU_SELECT_SAVE) {
        if (g_card_save_list_input_armed == 0) {
            g_card_save_overwrite_prompt_active = 0;
            if (input == 0) {
                g_card_save_list_input_armed = 1;
            } else {
                input = 0;
            }
        }
        row = card_io_poll_operation_result();
        if (row > CARD_IO_RESULT_COMPLETE) {
            if (card_io_wait_operation_result(3) != CARD_IO_RESULT_COMPLETE) {
                card_thread_wait_and_clear_state(6);
                card_thread_wait_and_clear_state(5);
                g_card_save_overwrite_prompt_active = 0;
                g_card_save_menu_state = CARD_SAVE_MENU_SCANNING;
                g_card_save_last_written_slot = 0xff;
            }
        } else if (g_card_save_overwrite_prompt_active != 0) {
            g_card_save_overwrite_prompt_active = card_thread_start_managed(6, g_card_save_overwrite_prompt_descriptor);
            if (g_card_save_overwrite_prompt_active == 0 && g_card_save_slot_index == 0) {
                g_card_save_menu_state = CARD_SAVE_MENU_WRITING;
            }
        } else {
            if ((input & PSX_PAD_CIRCLE) && g_card_save_list_scroll_anim_offset == 0) {
                main_sound_play_sfx(MAIN_SFX_CONFIRM);
                remaining = g_card_save_list_cursor_index;
                index = 0;
                for (index = 0; index < CARD_SAVE_SLOT_COUNT; index++) {
                    if (g_card_save_slot_file_states[index] != CARD_SAVE_SLOT_UNAVAILABLE) {
                        remaining--;
                    }
                    if (remaining < 0) {
                        break;
                    }
                }
                g_card_save_list_selected_slot = index;
                if (g_card_save_slot_file_states[g_card_save_list_selected_slot] == CARD_SAVE_SLOT_OCCUPIED) {
                    index = g_card_save_list_cursor_index - g_card_save_list_top_row;
                    switch (index) {
                    case 0:
                        index = 0x5a;
                        break;
                    case 1:
                        index = 0x88;
                        break;
                    case 2:
                        index = 0x1e;
                        break;
                    case 3:
                        index = 0x3a;
                        break;
                    }
                    g_card_save_overwrite_prompt_descriptor[0].window_y = index;
                    g_card_save_overwrite_prompt_descriptor[1].selected_index = 1;
                    g_card_save_slot_index = -1;
                    g_card_save_overwrite_prompt_active = 1;
                } else {
                    g_card_save_menu_state = CARD_SAVE_MENU_WRITING;
                }
            }
            if ((input & PSX_PAD_CROSS) && g_card_save_list_scroll_anim_offset == 0) {
                g_card_sound_queued_effect_id = MAIN_SFX_CANCEL;
                g_card_save_menu_state = CARD_SAVE_MENU_SELECT_CARD;
            } else if (g_card_save_list_scroll_anim_offset == 0) {
                index = g_card_save_list_cursor_index;
                if (input & PSX_PAD_DOWN) {
                    g_card_save_list_cursor_index++;
                    if (g_card_save_list_cursor_index >= g_card_save_occupied_slot_count) {
                        g_card_save_list_cursor_index = g_card_save_occupied_slot_count - 1;
                    }
                } else if ((input & PSX_PAD_UP) && g_card_save_list_cursor_index != 0) {
                    g_card_save_list_cursor_index--;
                }
                if (index != g_card_save_list_cursor_index) {
                    g_card_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
                }
            }
        }
        if (g_card_save_menu_state != CARD_SAVE_MENU_SELECT_SAVE) {
            input = 0;
            g_card_save_list_input_armed = 0;
        }
    }

    card_save_update_slot_scroll(g_card_save_list_cursor_index);
    g_card_menu_window_rect_source = (card_window_rect_source_t*)g_card_save_slot_window_rect_source;
    scrolling = g_card_save_menu_state != CARD_SAVE_MENU_SELECT_SAVE;
    for (index = 0, row = 0; index < CARD_SAVE_SLOT_COUNT; index++, row++) {
        while (g_card_save_slot_file_states[index] == CARD_SAVE_SLOT_UNAVAILABLE) {
            index++;
            if (index >= CARD_SAVE_SLOT_COUNT) {
                break;
            }
        }
        if (index >= CARD_SAVE_SLOT_COUNT) {
            break;
        }
        g_card_save_slot_row_y
            = (u16)g_card_save_list_scroll_base_y + (u16)g_card_save_list_scroll_anim_offset + row * 48 + 12;
        if (g_card_save_slot_row_y < -48 || g_card_save_slot_row_y >= 241) {
            continue;
        }
        card_gfx_set_otag_index(10);
        value = row != g_card_save_list_cursor_index || scrolling != 0;
        card_text_set_palette_and_metrics(value);
        card_cmd_draw_window_frame_handler(&g_card_save_slot_window_command);
        g_card_save_slot_quads[3].y = g_card_save_slot_quads[4].y = g_card_save_slot_quads[0].y
            = g_card_save_slot_quads[1].y = g_card_save_slot_row_y - 14;
        g_card_save_slot_quads[3].y = g_card_save_slot_quads[3].y + 1;
        g_card_save_slot_quads[4].y = g_card_save_slot_quads[4].y + 1;
        g_card_save_slot_quads[1].y++;
        if (g_card_save_slot_quads[0].y >= 0 && g_card_save_slot_quads[0].y < 241) {
            card_gfx_enqueue_oriented_textured_quad(&g_card_save_slot_quads[0], card_gfx_get_draw_color(), 0, 0, 11);
            card_gfx_enqueue_oriented_textured_quad(&g_card_save_slot_quads[1], card_gfx_get_draw_color(), 0, 0, 11);
            card_gfx_enqueue_oriented_textured_quad(&g_card_save_slot_quads[3], card_gfx_get_draw_color(), 0, 0, 11);
            card_gfx_enqueue_oriented_textured_quad(&g_card_save_slot_quads[4], card_gfx_get_draw_color(), 0, 0, 11);
            card_gfx_set_otag_index(11);
            g_card_save_slot_number_commands[0][8] = g_card_save_slot_quads[0].y;
            g_card_save_slot_number_commands[0][9] = (u8)index + 1;
            card_cmd_draw_decimal_number_handler(g_card_save_slot_number_commands[0]);
        }
        g_card_save_slot_quads[2].y = g_card_save_slot_quads[0].y;
        if (g_card_save_slot_quads[2].y >= 0 && g_card_save_slot_quads[2].y < 241) {
            card_gfx_enqueue_oriented_textured_quad(&g_card_save_slot_quads[2], card_gfx_get_draw_color(), 0, 0, 11);
            g_card_save_slot_number_commands[3][8] = g_card_save_slot_number_commands[2][8]
                = g_card_save_slot_number_commands[1][8] = g_card_save_slot_quads[0].y;
            g_card_save_slot_number_commands[3][9] = g_card_save_slot_playtimes[index][2];
            g_card_save_slot_number_commands[2][9] = g_card_save_slot_playtimes[index][1];
            g_card_save_slot_number_commands[1][9] = g_card_save_slot_playtimes[index][0];
            if (g_card_save_slot_number_commands[1][9] >= 100) {
                g_card_save_slot_number_commands[1][9] = 99;
            }
            card_cmd_draw_decimal_number_handler(g_card_save_slot_number_commands[1]);
            card_cmd_draw_decimal_number_handler(g_card_save_slot_number_commands[2]);
            card_cmd_draw_decimal_number_handler(g_card_save_slot_number_commands[3]);
        }
        if (g_card_save_slot_file_states[index] == CARD_SAVE_SLOT_AVAILABLE) {
            rect.x = 0x74;
            rect.y = g_card_save_slot_row_y + 4;
            rect.w = 0x28;
            rect.h = 0x10;
        } else {
            rect.x = 0x1c;
            rect.y = g_card_save_slot_row_y - 4;
            rect.w = 0xc8;
            rect.h = 0x1f;
        }
        if (index < 6) {
            texture_page = g_card_text_digit_texture_page;
            source_v = index * 0x20 + 0x30;
        } else if (index < 11) {
            texture_page = GetTPage(0, 0, 0x1c0, 0);
            source_v = (index - 6) * 0x20 + 0x40;
        } else {
            texture_page = GetTPage(0, 0, 0x180, 0);
            source_v = (index - 11) * 0x20 + 0x60;
        }
        card_gfx_enqueue_textured_quad(&rect, 0, source_v, 0, 0, texture_page, g_card_text_metric_0, 11);
    }

    card_gfx_set_otag_index(15);
    value = g_card_save_list_cursor_index - g_card_save_list_top_row;
    if (value < 0) {
        value = 0;
    }
    if (value >= 4) {
        value = 3;
    }
    g_card_save_slot_cursor_position[1] = value * 48 + 0x28;
    card_menu_update_and_draw_animated_cursor(
        g_card_save_slot_cursor_position, g_card_save_slot_cursor_animation, scrolling);
    if (g_event_mode != 0) {
        cursor_clut = g_card_menu_cursor_mode1_foreground_clut;
    } else {
        cursor_clut = g_card_menu_cursor_mode0_foreground_clut;
    }
    if (g_card_save_list_top_row != 0) {
        rect.x = 0xe3;
        rect.y = 0x20;
        rect.w = 8;
        rect.h = 0x10;
        card_gfx_enqueue_textured_quad(
            &rect, 0xd8, 0, 0, 0, g_card_menu_cursor_texture_page, g_card_menu_cursor_mode0_foreground_clut, 15);
    }
    if (g_card_save_occupied_slot_count - g_card_save_list_top_row >= 5) {
        rect.x = 0xe3;
        rect.y = 0xba;
        rect.w = 8;
        rect.h = 0x10;
        card_gfx_enqueue_textured_quad(
            &rect, 0xe0, 0, 0, 0, g_card_menu_cursor_texture_page, g_card_menu_cursor_mode0_foreground_clut, 15);
    }
}
