#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Local view of world_menu_thread_data_t with the +0x0a field. */
typedef struct world_card_prompt_thread_data {
    u8 _unused_00[0x0a];
    s16 window_y; /* 0x0a: menu record window y; prompt y chosen from the selected row */
    u8 _unused_0c[0x74 - 0x0c];
    s16 selection; /* 0x74 */
} world_card_prompt_thread_data_t;

extern world_card_prompt_thread_data_t g_world_card_overwrite_confirm_prompt;
extern world_card_prompt_thread_data_t g_world_card_load_confirm_prompt;
extern world_card_prompt_thread_data_t* g_world_card_confirm_prompt;

extern world_draw_number_command_t* world_menu_script_draw_formatted_number();

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

    if (g_world_card_menu_step == 3) {
        if (g_world_card_slot_list_input_ready == 0) {
            g_world_card_overwrite_prompt_running = 0;
            g_world_card_load_prompt_running = 0;
            if ((input & 0xffff) == 0) {
                g_world_card_slot_list_input_ready = 1;
            } else {
                input = 0;
            }
        }
        if (world_card_check_selected() > CARD_IO_RESULT_COMPLETE) {
            if (world_card_wait_for_selected_status(3) != CARD_IO_RESULT_COMPLETE) {
                world_menu_stop_thread_and_wait(6);
                world_menu_stop_thread_and_wait(5);
                g_world_card_menu_step = 2;
                g_card_save_last_written_slot = 0xff;
            }
        } else if (g_world_card_overwrite_prompt_running != 0) {
            g_world_card_overwrite_prompt_running = world_menu_run_thread(6, &g_world_card_overwrite_confirm_prompt);
            if (g_world_card_overwrite_prompt_running == 0 && g_world_menu_selection_results[5] == 0) {
                g_world_card_menu_step = 5;
            }
        } else if (g_world_card_load_prompt_running != 0) {
            g_world_card_load_prompt_running = world_menu_run_thread(6, &g_world_card_load_confirm_prompt);
            if (g_world_card_load_prompt_running == 0 && g_world_menu_selection_results[5] == 0) {
                g_world_card_menu_step = 4;
            }
        } else if ((input & PSX_PAD_CIRCLE) && g_world_card_list_scroll_velocity == 0) {
            main_sound_play_sfx(MAIN_SFX_CONFIRM);
            remaining = g_world_card_slot_cursor_row;
            if (g_world_card_save_mode == 0) {
                for (i = 0; i < 15; i++) {
                    if (g_card_save_slot_file_states[i] != -1) {
                        remaining--;
                    }
                    if (remaining < 0) {
                        break;
                    }
                }
            } else {
                for (i = 0; i < 15; i++) {
                    if (g_card_save_slot_file_states[i] == 0) {
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
                g_world_card_confirm_prompt = &g_world_card_overwrite_confirm_prompt;
                g_world_card_overwrite_confirm_prompt.selection = 1;
            } else {
                g_world_card_confirm_prompt = &g_world_card_load_confirm_prompt;
                g_world_card_load_confirm_prompt.selection = 0;
            }
            g_world_card_confirm_prompt->window_y = i;
            if (g_world_card_save_mode == 0) {
                if (g_card_save_slot_file_states[g_world_card_active_slot] == 0) {
                    g_world_card_overwrite_prompt_running = 1;
                } else {
                    g_world_card_menu_step = 5;
                }
            } else {
                g_world_card_load_prompt_running = 1;
            }
        } else if (input & PSX_PAD_CROSS) {
            if (g_world_card_list_scroll_velocity == 0) {
                g_world_menu_sound_effect_id = MAIN_SFX_CANCEL;
                g_world_card_menu_step = 0;
            }
        } else if (g_world_card_list_scroll_velocity == 0) {
            i = g_world_card_slot_cursor_row;
            if (input & PSX_PAD_DOWN) {
                g_world_card_slot_cursor_row = i + 1;
                if (g_world_card_slot_cursor_row >= (s8)g_world_card_selectable_slot_count) {
                    g_world_card_slot_cursor_row = (s8)g_world_card_selectable_slot_count - 1;
                }
            } else if ((input & PSX_PAD_UP) && i != 0) {
                g_world_card_slot_cursor_row = i - 1;
            }
            if (i != g_world_card_slot_cursor_row) {
                g_world_menu_sound_effect_id = MAIN_SFX_CURSOR_MOVE;
            }
        }
        if (g_world_card_menu_step != 3) {
            input = 0;
            g_world_card_slot_list_input_ready = 0;
        }
    }

    world_card_scroll_menu_list(g_world_card_slot_cursor_row);
    g_world_menu_window_rect_source = &g_world_card_slot_window;
    busy = g_world_card_menu_step != 3;
    for (i = 0, row = 0; i < 15; i++, row++) {
        if (g_world_card_save_mode != 0) {
            while (g_card_save_slot_file_states[i] != 0) {
                i++;
                if (i >= 15) {
                    break;
                }
            }
        } else {
            while (g_card_save_slot_file_states[i] == -1) {
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
            = (u16)g_world_card_list_scroll_y + (u16)g_world_card_list_scroll_velocity + row * 48 + 12;
        if (g_world_card_slot_window.rect.y < -48 || g_world_card_slot_window.rect.y >= 241) {
            continue;
        }
        world_menu_set_draw_priority(10);
        value = row != g_world_card_slot_cursor_row || busy != 0;
        world_menu_set_palette_mode(value);
        world_menu_parse_draw_window_frame(&g_world_card_slot_window_command);
        g_world_card_slot_quad_3.y = g_world_card_slot_quad_4.y = g_world_card_slot_quad_0.y
            = g_world_card_slot_quad_1.y = g_world_card_slot_window.rect.y - 14;
        g_world_card_slot_quad_3.y = g_world_card_slot_quad_3.y + 1;
        g_world_card_slot_quad_4.y = g_world_card_slot_quad_4.y + 1;
        g_world_card_slot_quad_1.y++;
        if (g_world_card_slot_quad_0.y >= 0 && g_world_card_slot_quad_0.y < 241) {
            world_gfx_enqueue_oriented_textured_quad(
                (world_oriented_quad_t*)&g_world_card_slot_quad_0, (u8*)world_menu_get_sprite_color(), 0, 0, 11);
            world_gfx_enqueue_oriented_textured_quad(
                (world_oriented_quad_t*)&g_world_card_slot_quad_1, (u8*)world_menu_get_sprite_color(), 0, 0, 11);
            world_gfx_enqueue_oriented_textured_quad(
                (world_oriented_quad_t*)&g_world_card_slot_quad_3, (u8*)world_menu_get_sprite_color(), 0, 0, 11);
            world_gfx_enqueue_oriented_textured_quad(
                (world_oriented_quad_t*)&g_world_card_slot_quad_4, (u8*)world_menu_get_sprite_color(), 0, 0, 11);
            world_menu_set_draw_priority(11);
            g_world_card_slot_number_command.y = g_world_card_slot_quad_0.y;
            g_world_card_slot_number_command.value = (u8)i + 1;
            world_menu_script_draw_formatted_number(&g_world_card_slot_number_command);
        }
        g_world_card_slot_quad_2.y = g_world_card_slot_quad_0.y;
        if (g_world_card_slot_quad_2.y >= 0 && g_world_card_slot_quad_2.y < 241) {
            world_gfx_enqueue_oriented_textured_quad(
                (world_oriented_quad_t*)&g_world_card_slot_quad_2, (u8*)world_menu_get_sprite_color(), 0, 0, 11);
            g_world_card_playtime_seconds_command.y = g_world_card_playtime_minutes_command.y
                = g_world_card_playtime_hours_command.y = g_world_card_slot_quad_0.y;
            g_world_card_playtime_seconds_command.value = g_card_save_slot_playtimes[i][2];
            g_world_card_playtime_minutes_command.value = g_card_save_slot_playtimes[i][1];
            g_world_card_playtime_hours_command.value = g_card_save_slot_playtimes[i][0];
            if (g_world_card_playtime_hours_command.value >= 100) {
                g_world_card_playtime_hours_command.value = 99;
            }
            world_menu_script_draw_formatted_number(&g_world_card_playtime_hours_command);
            world_menu_script_draw_formatted_number(&g_world_card_playtime_minutes_command);
            world_menu_script_draw_formatted_number(&g_world_card_playtime_seconds_command);
        }
        if (g_card_save_slot_file_states[i] == 1) {
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
        world_gfx_enqueue_textured_quad(&rect, 0, source_v, 0, 0, texture_page, g_world_menu_window_clut, 11);
    }

    world_menu_set_draw_priority(15);
    value = g_world_card_slot_cursor_row - g_world_card_list_first_visible_row;
    if (value < 0) {
        value = 0;
    }
    if (value >= 4) {
        value = 3;
    }
    g_world_card_slot_cursor_point.y = value * 48 + 0x28;
    world_menu_draw_animated_cursor(&g_world_card_slot_cursor_point, &g_world_card_slot_cursor_anim, busy);
    if (g_world_card_list_first_visible_row != 0) {
        rect.x = 0xe3;
        rect.y = 0x20;
        rect.w = 8;
        rect.h = 0x10;
        world_gfx_enqueue_textured_quad(&rect, 0xd8, 0, 0, 0, g_world_menu_icon_tpage, g_world_menu_clut_front, 15);
    }
    if ((s8)g_world_card_selectable_slot_count - g_world_card_list_first_visible_row >= 5) {
        rect.x = 0xe3;
        rect.y = 0xba;
        rect.w = 8;
        rect.h = 0x10;
        world_gfx_enqueue_textured_quad(&rect, 0xe0, 0, 0, 0, g_world_menu_icon_tpage, g_world_menu_clut_front, 15);
    }
}
