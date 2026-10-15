#include "fft/event_jobstts.h"
#include "fft/menu.h"
#include "fft/menu_script.h"
#include "psx/libgpu.h"
#include "psx/pad.h"

/* Page-scroll direction latch: -1 up, 1 down, 0 idle. */

/* Scrollable-list body command.
 *
 * Handles line and page scrolling (animating the VRAM text strip and
 * rendering the newly exposed row), draws the list image, the arrows, the
 * scrollbar thumb and the selection marker, then replays the per-row command
 * group once per visible row, plus one while a scroll offset is in effect.
 * An empty list skips its row command groups. JOBSTTS copy of
 * EQUIP's equip_cmd_draw_scrollable_list_body_handler and BUNIT's bunit_cmd_run_scrollable_list_handler with an
 * event-speed multiplier. The unused arrays reproduce the target's frame: it references only rect (0x20) and marker
 * (0x38). The scroll-down path follows WORLD's world_menu_script_draw_scrollable_list spelling.
 */
menu_instruction_t* jobstts_cmd_draw_scrollable_list_body_handler(menu_instruction_t* command) {
    RECT rect;
    u8 unused_28[16];
    point16_t marker;
    u8 unused_40[8];
    menu_instruction_t* start;
    s16* ids;
    u16 previous;
    s32 input;
    u8 speed;
    s32 i;
    s32 j;
    s32 total;
    s32 y;
    u16 ot_index;
    s32 count;

    ot_index = g_menu_gfx_otag_index;
    speed = 1;
    if (g_menu_event_speed != 0) {
        speed = g_menu_event_speed;
    }
    if (g_menu_list_entry_count <= 0) {
        count = command->list.skip_count + 1;
        while (--count != -1) {
            command = menu_cmd_skip_group(command);
        }
        return command;
    }
    g_menu_list_row_mode = 1;
    previous = g_menu_list_selected_index;
    if (g_menu_list_redraw_pending != 0) {
        menu_draw_scrollable_list(command);
    }
    if (g_menu_list_scroll_direction == 0) {
        j = jobstts_input_read_page_scroll_direction();
        if (j != 0) {
            if (j != 2) {
                menu_scroll_list_by_page(j, command);
            }
        } else {
            input = g_menu_cmd_stream_input;
            if ((input & (PSX_PAD_UP << 16)) || (input & PSX_PAD_UP)) {
                if (g_menu_scroll_base_index == g_menu_list_selected_index) {
                    if (g_menu_scroll_base_index > 0) {
                        g_menu_list_scroll_direction = -1;
                        if (g_menu_list_text_table != 0) {
                            s16 id;
                            ids = &g_jobstts_menu_list_scroll_entry_ids;
                            id = g_menu_list_entries[g_menu_scroll_base_index - 1];
                            rect.x = 0x100;
                            rect.y = 0x20;
                            rect.w = g_menu_list_vram_width;
                            rect.h = g_menu_list_row_height;
                            *ids = id;
                            text_render_id_rows_to_vram(g_menu_list_text_table, ids, &rect, 0);
                        }
                    }
                } else if (input & PSX_PAD_UP) {
                    g_menu_list_selected_index -= 1;
                }
            } else if ((input & (PSX_PAD_DOWN << 16)) || (input & PSX_PAD_DOWN)) {
                if (g_menu_scroll_base_index + g_menu_list_visible_rows - 1 == g_menu_list_selected_index) {
                    if (g_menu_scroll_base_index + g_menu_list_visible_rows - 1 < g_menu_list_entry_count - 1) {
                        g_menu_list_scroll_direction = 1;
                        if (g_menu_list_text_table != 0) {
                            s16* entry_ids;

                            j = g_menu_scroll_base_index + g_menu_list_visible_rows - 1;
                            entry_ids = &g_jobstts_menu_list_scroll_entry_ids;
                            *entry_ids = g_menu_list_entries[j + 1];
                            rect.x = 0x100;
                            rect.y = g_menu_list_visible_rows * g_menu_list_row_height + 0x30;
                            rect.w = g_menu_list_vram_width;
                            rect.h = g_menu_list_row_height;
                            text_render_id_rows_to_vram(g_menu_list_text_table, entry_ids, &rect, 0);
                        }
                    }
                } else if ((g_menu_list_selected_index < (g_menu_list_entry_count - 1)) && (input & PSX_PAD_DOWN)) {
                    g_menu_list_selected_index += 1;
                }
            } else {
                g_jobstts_menu_list_scroll_progress = 0;
            }
        }
    }
    if (g_menu_list_scroll_direction < 0) {
        if (g_jobstts_menu_list_scroll_progress >= g_main_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset -= g_main_menu_scroll_fast_step * speed;
        } else {
            g_jobstts_menu_list_scroll_progress += speed;
            g_menu_list_scroll_offset -= g_main_menu_scroll_slow_step * speed;
        }
        if ((-g_menu_list_row_height) >= g_menu_list_scroll_offset) {
            g_menu_list_scroll_offset = 0;
            g_menu_list_scroll_direction = 0;
            g_menu_list_selected_index -= 1;
            g_menu_scroll_base_index = g_menu_list_selected_index;
            if (g_menu_list_text_table != 0) {
                for (j = 0; j < g_menu_list_visible_rows; j++) {
                    s32 height;

                    height = g_menu_list_row_height;
                    rect.x = 0x100;
                    rect.w = g_menu_list_vram_width;
                    rect.h = height;
                    rect.y = ((j - 1) * height) + 0x30;
                    menu_gfx_enqueue_draw_move(&rect, 0x100, (height * j) + 0x30, ot_index - 1);
                }
            }
        }
    } else if (g_menu_list_scroll_direction > 0) {
        if (g_jobstts_menu_list_scroll_progress >= g_main_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset += g_main_menu_scroll_fast_step * speed;
        } else {
            g_jobstts_menu_list_scroll_progress += speed;
            g_menu_list_scroll_offset += g_main_menu_scroll_slow_step * speed;
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
            rect.h = (g_menu_list_row_height * g_menu_list_visible_rows) + 0xa;
            menu_gfx_enqueue_textured_quad(&rect, 0, g_menu_list_scroll_offset + 0x2b, 0,
                g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, g_menu_window_clut,
                g_menu_gfx_otag_index);
        } else {
            rect.x = command->list.x;
            rect.y = command->list.y;
            rect.w = g_menu_list_vram_width * 4;
            rect.h = g_menu_list_row_height * g_menu_list_visible_rows;
            menu_gfx_enqueue_textured_quad(&rect, 0, 0x30, 0, g_jobstts_gfx_semitransparency,
                g_jobstts_gfx_background_texture_page, g_menu_window_clut, g_menu_gfx_otag_index);
        }
    }
    if (g_menu_use_alternate_palette == 0) {
        if (g_menu_scroll_base_index != 0) {
            rect.x = g_jobstts_menu_window_right_x;
            rect.y = command->list.arrow_up_y;
            rect.w = 8;
            rect.h = 0x10;
            menu_gfx_enqueue_textured_quad(&rect, 0xd8, 0, 0, g_jobstts_gfx_semitransparency,
                g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, 0x1e);
        }
        if (g_menu_scroll_base_index < (g_menu_list_entry_count - g_menu_list_visible_rows)) {
            rect.x = g_jobstts_menu_window_right_x;
            rect.y = command->list.arrow_down_y;
            rect.w = 8;
            rect.h = 0x10;
            menu_gfx_enqueue_textured_quad(&rect, 0xe0, 0, 0, g_jobstts_gfx_semitransparency,
                g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, 0x1e);
        }
        if (g_menu_list_entry_count > g_menu_list_visible_rows) {
            y = command->list.arrow_up_y + 0x14;
            j = command->list.arrow_down_y - y;
            y = (j * g_menu_list_selected_index) / (g_menu_list_entry_count - 1);
            j = command->list.arrow_up_y + 0xf;
            j += y;
            rect.w = 8;
            rect.h = 8;
            rect.x = g_jobstts_menu_window_right_x;
            rect.y = j;
            menu_gfx_enqueue_textured_quad(&rect, 0x10, 0x10, 0, g_jobstts_gfx_semitransparency,
                g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
        if (command->list.show_cursor != 0) {
            marker.x = command->list.x - command->list.cursor_x_offset;
            marker.y
                = command->list.y + ((g_menu_list_selected_index - g_menu_scroll_base_index) * g_menu_list_row_height);
            menu_update_and_draw_animated_cursor(
                &marker, &g_jobstts_menu_list_cursor_anim_state, g_menu_use_alternate_palette);
        }
    }
    command = menu_next_instruction(command);
    g_menu_list_row_index = 0;
    total = g_menu_list_visible_rows + 1;
    if (g_menu_list_scroll_offset == 0) {
        total = g_menu_list_visible_rows;
    }
    start = command;
    for (i = 0; i < total; i++) {
        if (((g_menu_scroll_base_index + i) > (g_menu_list_entry_count - 1)) && (g_menu_list_scroll_offset == 0)) {
            break;
        }
        command = start;
        for (j = 0; j < g_menu_list_row_group_count; j++) {
            command = g_menu_cmd_handlers[command->header.opcode](command);
        }
        g_menu_list_row_index += 1;
    }
    g_menu_list_row_mode = 0;
    if (previous != g_menu_list_selected_index) {
        g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
    return command;
}
