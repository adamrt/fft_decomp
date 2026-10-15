#include "fft/battle_text.h"
#include "fft/event_equip.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Scrollable-list body command; EQUIP copy of JOBSTTS jobstts_cmd_draw_scrollable_list_body_handler and BUNIT
 * bunit_cmd_run_scrollable_list_handler without the event-speed multiplier.
 *
 * The padding arrays reproduce the target's frame (only rect and marker are
 * referenced). The scroll-down path follows WORLD's world_menu_script_draw_scrollable_list
 * spelling; its `(s16)` row-height cast adds the other 8 bytes of frame and
 * keeps the row count in a register of its own.
 */
menu_instruction_t* equip_cmd_draw_scrollable_list_body_handler(menu_instruction_t* command) {
    RECT rect;
    u8 pad0[0x10];
    RECT marker;
    u8 pad1[0x8];
    u16 ot_index;
    u16 previous_selected;
    s32 row_count;
    u16 step;
    s16* row_ids;
    s16 text_id;
    s32 input;
    s32 top;
    s32 offset;
    s32 scroll_base;
    s32 height;
    s32 i;
    s32 j;
    menu_instruction_t* row_commands;

    ot_index = g_menu_gfx_otag_index;
    if (g_menu_list_entry_count <= 0) {
        j = command->list.skip_count;
        while (j != -1) {
            command = menu_cmd_skip_group(command);
            j -= 1;
        }
        return command;
    }
    g_menu_list_row_mode = 1;
    previous_selected = g_menu_list_selected_index;
    step = 2;
    if (g_menu_list_redraw_pending != 0) {
        menu_draw_scrollable_list(command);
    }
    if (g_menu_list_scroll_direction == 0) {
        j = equip_input_read_page_scroll_direction();
        if (j != 0) {
            if (j != 2) {
                menu_scroll_list_by_page(j, command);
            }
        } else {
            input = g_menu_cmd_stream_input;
            if ((input & (PSX_PAD_UP << 16)) || (input & PSX_PAD_UP)) {
                scroll_base = g_menu_scroll_base_index;
                if (scroll_base == g_menu_list_selected_index) {
                    if (scroll_base > 0) {
                        g_menu_list_scroll_direction = -1;
                        if (g_menu_list_text_table != 0) {
                            row_ids = &g_equip_menu_list_scroll_entry_ids;
                            text_id = g_menu_list_entries[scroll_base - 1];
                            rect.x = 0x100;
                            rect.y = 0x20;
                            rect.w = g_menu_list_vram_width;
                            rect.h = g_menu_list_row_height;
                            *row_ids = text_id;
                            text_render_id_rows_to_vram(g_menu_list_text_table, row_ids, &rect, 0);
                        }
                    }
                } else if (input & PSX_PAD_UP) {
                    g_menu_list_selected_index--;
                }
            } else if ((input & (PSX_PAD_DOWN << 16)) || (input & PSX_PAD_DOWN)) {
                if (g_menu_scroll_base_index + g_menu_list_visible_rows - 1 == g_menu_list_selected_index) {
                    if (g_menu_scroll_base_index + g_menu_list_visible_rows - 1 < g_menu_list_entry_count - 1) {
                        g_menu_list_scroll_direction = 1;
                        if (g_menu_list_text_table != 0) {
                            s16* entry_ids;

                            j = g_menu_scroll_base_index + g_menu_list_visible_rows - 1;
                            entry_ids = &g_equip_menu_list_scroll_entry_ids;
                            *entry_ids = g_menu_list_entries[j + 1];
                            rect.x = 0x100;
                            rect.y = g_menu_list_visible_rows * g_menu_list_row_height + 0x30;
                            rect.w = g_menu_list_vram_width;
                            rect.h = g_menu_list_row_height;
                            text_render_id_rows_to_vram(g_menu_list_text_table, entry_ids, &rect, 0);
                        }
                    }
                } else if (g_menu_list_selected_index < g_menu_list_entry_count - 1 && (input & PSX_PAD_DOWN)) {
                    g_menu_list_selected_index++;
                }
            } else {
                g_equip_menu_list_scroll_progress = 0;
            }
        }
    }
    if (g_menu_list_scroll_direction < 0) {
        if (g_equip_menu_list_scroll_progress >= g_main_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset -= step * g_main_menu_scroll_fast_step;
        } else {
            g_menu_list_scroll_offset -= g_main_menu_scroll_slow_step * (step & 0xff);
            g_equip_menu_list_scroll_progress = (step & 0xff) + g_equip_menu_list_scroll_progress;
        }
        if (g_menu_list_scroll_offset <= -g_menu_list_row_height) {
            g_menu_list_scroll_offset = 0;
            g_menu_list_scroll_direction = 0;
            g_menu_list_selected_index -= 1;
            g_menu_scroll_base_index = g_menu_list_selected_index;
            if (g_menu_list_text_table != 0) {
                for (j = 0; j < g_menu_list_visible_rows; j++) {
                    height = g_menu_list_row_height;
                    rect.x = 0x100;
                    rect.w = g_menu_list_vram_width;
                    rect.h = height;
                    rect.y = (j - 1) * height + 0x30;
                    menu_gfx_enqueue_draw_move(&rect, 0x100, height * j + 0x30, ot_index - 1);
                }
            }
        }
    } else if (g_menu_list_scroll_direction > 0) {
        if (g_equip_menu_list_scroll_progress >= g_main_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset += step * g_main_menu_scroll_fast_step;
        } else {
            g_menu_list_scroll_offset += g_main_menu_scroll_slow_step * (step & 0xff);
            g_equip_menu_list_scroll_progress = (step & 0xff) + g_equip_menu_list_scroll_progress;
        }
        if (g_menu_list_scroll_offset >= g_menu_list_row_height) {
            g_menu_list_scroll_offset = 0;
            g_menu_list_scroll_direction = 0;
            g_menu_scroll_base_index++;
            g_menu_list_selected_index++;
            if (g_menu_list_text_table != 0) {
                rect.x = 0x100;
                rect.y = g_menu_list_row_height + 0x30;
                rect.w = g_menu_list_vram_width;
                rect.h = g_menu_list_row_height * g_menu_list_visible_rows;
                menu_gfx_enqueue_draw_move(&rect, 0x100, 0x30, ot_index - 1);
            }
        }
    }
    if (g_menu_list_text_table != 0) {
        if (g_menu_list_scroll_offset != 0) {
            rect.x = command->list.x;
            rect.y = command->list.y - 5;
            rect.w = g_menu_list_vram_width * 4;
            rect.h = g_menu_list_row_height * g_menu_list_visible_rows + 10;
            menu_gfx_enqueue_textured_quad(&rect, 0, g_menu_list_scroll_offset + 0x2b, 0, g_menu_gfx_semitransparency,
                g_menu_atlas_texture_page, g_menu_window_clut, g_menu_gfx_otag_index);
        } else {
            rect.x = command->list.x;
            rect.y = command->list.y;
            rect.w = g_menu_list_vram_width * 4;
            rect.h = g_menu_list_row_height * g_menu_list_visible_rows;
            menu_gfx_enqueue_textured_quad(&rect, 0, 0x30, 0, g_menu_gfx_semitransparency, g_menu_atlas_texture_page,
                g_menu_window_clut, g_menu_gfx_otag_index);
        }
    }
    if (g_menu_use_alternate_palette == 0) {
        if (g_menu_scroll_base_index != 0) {
            rect.x = g_menu_window_right_x;
            rect.y = command->list.arrow_up_y;
            rect.w = 8;
            rect.h = 0x10;
            menu_gfx_enqueue_textured_quad(&rect, 0xd8, 0, 0, g_menu_gfx_semitransparency, g_menu_cursor_texture_page,
                g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
        if (g_menu_scroll_base_index < g_menu_list_entry_count - g_menu_list_visible_rows) {
            rect.x = g_menu_window_right_x;
            rect.y = command->list.arrow_down_y;
            rect.w = 8;
            rect.h = 0x10;
            menu_gfx_enqueue_textured_quad(&rect, 0xe0, 0, 0, g_menu_gfx_semitransparency, g_menu_cursor_texture_page,
                g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
        if (g_menu_list_entry_count > g_menu_list_visible_rows) {
            top = command->list.arrow_up_y;
            offset = top + 0x14;
            j = command->list.arrow_down_y - offset;
            offset = j * g_menu_list_selected_index / (g_menu_list_entry_count - 1);
            j = top + 0xf;
            j += offset;
            rect.w = 8;
            rect.h = 8;
            rect.x = g_menu_window_right_x;
            rect.y = j;
            menu_gfx_enqueue_textured_quad(&rect, 0x10, 0x10, 0, g_menu_gfx_semitransparency,
                g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
    }
    if (command->list.show_cursor != 0) {
        marker.x = command->list.x - command->list.cursor_x_offset;
        marker.y = command->list.y + (g_menu_list_selected_index - g_menu_scroll_base_index) * g_menu_list_row_height;
        menu_update_and_draw_animated_cursor(
            &marker, g_equip_menu_list_cursor_anim_state, g_menu_use_alternate_palette);
    }
    command = menu_next_instruction(command);
    g_menu_list_row_index = 0;
    row_count = g_menu_list_visible_rows + 1;
    if (g_menu_list_scroll_offset == 0) {
        row_count = g_menu_list_visible_rows;
    }
    row_commands = command;
    for (i = 0; i < row_count; i++) {
        if (g_menu_scroll_base_index + i > g_menu_list_entry_count - 1 && g_menu_list_scroll_offset == 0) {
            break;
        }
        command = row_commands;
        for (j = 0; j < g_menu_list_row_group_count; j++) {
            command = g_menu_cmd_handlers[command->header.opcode](command);
        }
        g_menu_list_row_index += 1;
    }
    g_menu_list_row_mode = 0;
    if (previous_selected != g_menu_list_selected_index) {
        g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
    return command;
}
