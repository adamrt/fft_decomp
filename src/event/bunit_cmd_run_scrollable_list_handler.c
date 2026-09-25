#include "fft/event_bunit.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

#define ENTRY_COUNT ((s16)g_bunit_menu_list_entry_count)
#define ROW_HEIGHT  ((s16)g_bunit_menu_list_row_height)

/* Scrollable-list body command; BUNIT copy of EQUIP equip_cmd_draw_scrollable_list_body_handler whose
 * scroll speed is the event speed rather than a constant 2 (JOBSTTS
 * jobstts_cmd_draw_scrollable_list_body_handler is the same command).
 *
 * The padding arrays reproduce the target's frame (only rect and marker are
 * referenced). The scroll-down path follows WORLD's world_menu_script_draw_scrollable_list
 * spelling; its `(s16)` row-height cast adds the other 8 bytes of frame and
 * keeps the row count in a register of its own.
 */
u8* bunit_cmd_run_scrollable_list_handler(u8* command) {
    RECT rect;
    u8 pad0[0x10];
    RECT marker;
    u8 pad1[0x8];
    u16 ot_index;
    u16 previous_selected;
    s32 row_count;
    u16 speed;
    u16* row_ids;
    u16 text_id;
    s32 input;
    s32 top;
    s32 offset;
    s32 scroll_base;
    s32 height;
    s32 i;
    s32 j;
    u8* row_commands;

    ot_index = g_bunit_gfx_otag_index;
    if (ENTRY_COUNT <= 0) {
        j = command[4];
        while (j != -1) {
            command = bunit_cmd_skip_group(command);
            j -= 1;
        }
        return command;
    }
    speed = bunit_menu_get_event_speed();
    g_bunit_menu_scroll_list_active = 1;
    if (g_bunit_menu_list_redraw_pending != 0) {
        bunit_menu_draw_scrollable_list(command);
    }
    previous_selected = g_bunit_menu_list_selected_index;
    if (g_bunit_menu_list_scroll_direction == 0) {
        j = bunit_input_read_page_scroll_direction();
        if (j != 0) {
            if (j != 2) {
                bunit_menu_scroll_list_by_page(j, command);
            }
        } else {
            input = g_bunit_cmd_stream_input;
            if ((input & 0x10000000) || (input & PSX_PAD_UP)) {
                scroll_base = g_bunit_menu_scroll_base_index;
                if (scroll_base == g_bunit_menu_list_selected_index) {
                    if (scroll_base > 0) {
                        g_bunit_menu_list_scroll_direction = -1;
                        if (g_bunit_menu_list_text_table != 0) {
                            row_ids = &g_bunit_menu_list_scroll_entry_ids[0];
                            text_id = g_bunit_menu_list_entries[scroll_base - 1];
                            rect.x = 0x100;
                            rect.y = 0x20;
                            rect.w = g_bunit_menu_list_vram_width;
                            rect.h = ROW_HEIGHT;
                            *row_ids = text_id;
                            bunit_text_render_id_rows_to_vram((s32)g_bunit_menu_list_text_table, row_ids, &rect, 0);
                        }
                    }
                } else if (input & PSX_PAD_UP) {
                    g_bunit_menu_list_selected_index--;
                }
            } else if ((input & 0x40000000) || (input & PSX_PAD_DOWN)) {
                if (g_bunit_menu_scroll_base_index + g_bunit_menu_list_visible_rows - 1
                    == g_bunit_menu_list_selected_index) {
                    if (g_bunit_menu_scroll_base_index + g_bunit_menu_list_visible_rows - 1 < ENTRY_COUNT - 1) {
                        g_bunit_menu_list_scroll_direction = 1;
                        if (g_bunit_menu_list_text_table != 0) {
                            u16* entry_ids;

                            j = g_bunit_menu_scroll_base_index + g_bunit_menu_list_visible_rows - 1;
                            entry_ids = &g_bunit_menu_list_scroll_entry_ids[0];
                            *entry_ids = g_bunit_menu_list_entries[j + 1];
                            rect.x = 0x100;
                            rect.y = g_bunit_menu_list_visible_rows * (s16)ROW_HEIGHT + 0x30;
                            rect.w = g_bunit_menu_list_vram_width;
                            rect.h = ROW_HEIGHT;
                            bunit_text_render_id_rows_to_vram((s32)g_bunit_menu_list_text_table, entry_ids, &rect, 0);
                        }
                    }
                } else if (g_bunit_menu_list_selected_index < ENTRY_COUNT - 1 && (input & PSX_PAD_DOWN)) {
                    g_bunit_menu_list_selected_index++;
                }
            } else {
                g_bunit_menu_list_scroll_progress = 0;
            }
        }
    }
    if (g_bunit_menu_list_scroll_direction < 0) {
        if (g_bunit_menu_list_scroll_progress >= g_main_menu_scroll_accel_delay) {
            g_bunit_menu_scroll_pixel_offset -= g_main_menu_scroll_fast_step * (speed & 0xff);
        } else {
            g_bunit_menu_scroll_pixel_offset -= g_main_menu_scroll_slow_step * (speed & 0xff);
            g_bunit_menu_list_scroll_progress = (speed & 0xff) + g_bunit_menu_list_scroll_progress;
        }
        if (g_bunit_menu_scroll_pixel_offset <= -ROW_HEIGHT) {
            g_bunit_menu_scroll_pixel_offset = 0;
            g_bunit_menu_list_scroll_direction = 0;
            g_bunit_menu_list_selected_index -= 1;
            g_bunit_menu_scroll_base_index = g_bunit_menu_list_selected_index;
            if (g_bunit_menu_list_text_table != 0) {
                for (j = 0; j < g_bunit_menu_list_visible_rows; j++) {
                    height = ROW_HEIGHT;
                    rect.x = 0x100;
                    rect.w = g_bunit_menu_list_vram_width;
                    rect.h = height;
                    rect.y = (j - 1) * height + 0x30;
                    bunit_gfx_enqueue_draw_move(&rect, 0x100, height * j + 0x30, ot_index - 1);
                }
            }
        }
    } else if (g_bunit_menu_list_scroll_direction > 0) {
        if (g_bunit_menu_list_scroll_progress >= g_main_menu_scroll_accel_delay) {
            g_bunit_menu_scroll_pixel_offset += g_main_menu_scroll_fast_step * (speed & 0xff);
        } else {
            g_bunit_menu_scroll_pixel_offset += g_main_menu_scroll_slow_step * (speed & 0xff);
            g_bunit_menu_list_scroll_progress = (speed & 0xff) + g_bunit_menu_list_scroll_progress;
        }
        if (g_bunit_menu_scroll_pixel_offset >= ROW_HEIGHT) {
            g_bunit_menu_scroll_pixel_offset = 0;
            g_bunit_menu_list_scroll_direction = 0;
            g_bunit_menu_scroll_base_index++;
            g_bunit_menu_list_selected_index++;
            if (g_bunit_menu_list_text_table != 0) {
                rect.x = 0x100;
                rect.y = ROW_HEIGHT + 0x30;
                rect.w = g_bunit_menu_list_vram_width;
                rect.h = ROW_HEIGHT * g_bunit_menu_list_visible_rows;
                bunit_gfx_enqueue_draw_move(&rect, 0x100, 0x30, ot_index - 1);
            }
        }
    }
    if (g_bunit_menu_list_text_table != 0) {
        if (g_bunit_menu_scroll_pixel_offset != 0) {
            rect.x = command[7];
            rect.y = command[8] - 5;
            rect.w = g_bunit_menu_list_vram_width * 4;
            rect.h = ROW_HEIGHT * g_bunit_menu_list_visible_rows + 10;
            bunit_gfx_enqueue_textured_quad(&rect, 0, g_bunit_menu_scroll_pixel_offset + 0x2b, 0,
                g_bunit_gfx_semitrans_enabled, g_bunit_text_digit_texture_page, g_bunit_text_metric_0,
                g_bunit_gfx_otag_index);
        } else {
            rect.x = command[7];
            rect.y = command[8];
            rect.w = g_bunit_menu_list_vram_width * 4;
            rect.h = ROW_HEIGHT * g_bunit_menu_list_visible_rows;
            bunit_gfx_enqueue_textured_quad(&rect, 0, 0x30, 0, g_bunit_gfx_semitrans_enabled,
                g_bunit_text_digit_texture_page, g_bunit_text_metric_0, g_bunit_gfx_otag_index);
        }
    }
    if (g_bunit_text_layout_mode == 0) {
        if (g_bunit_menu_scroll_base_index != 0) {
            rect.x = g_bunit_menu_window_right_x;
            rect.y = command[12];
            rect.w = 8;
            rect.h = 0x10;
            bunit_gfx_enqueue_textured_quad(&rect, 0xd8, 0, 0, g_bunit_gfx_semitrans_enabled,
                g_bunit_menu_icon_texture_page, g_bunit_menu_cursor_mode0_foreground_clut, g_bunit_gfx_otag_index);
        }
        if (g_bunit_menu_scroll_base_index < ENTRY_COUNT - g_bunit_menu_list_visible_rows) {
            rect.x = g_bunit_menu_window_right_x;
            rect.y = command[13];
            rect.w = 8;
            rect.h = 0x10;
            bunit_gfx_enqueue_textured_quad(&rect, 0xe0, 0, 0, g_bunit_gfx_semitrans_enabled,
                g_bunit_menu_icon_texture_page, g_bunit_menu_cursor_mode0_foreground_clut, g_bunit_gfx_otag_index);
        }
        if (ENTRY_COUNT > g_bunit_menu_list_visible_rows) {
            top = command[12];
            offset = top + 0x14;
            j = command[13] - offset;
            offset = j * g_bunit_menu_list_selected_index / (ENTRY_COUNT - 1);
            j = top + 0xf;
            j += offset;
            rect.w = 8;
            rect.h = 8;
            rect.x = g_bunit_menu_window_right_x;
            rect.y = j;
            bunit_gfx_enqueue_textured_quad(&rect, 0x10, 0x10, 0, g_bunit_gfx_semitrans_enabled,
                g_bunit_menu_icon_texture_page, g_bunit_menu_cursor_mode0_foreground_clut, g_bunit_gfx_otag_index);
        }
    }
    if (command[2] != 0) {
        marker.x = command[7] - command[11];
        marker.y = command[8] + (g_bunit_menu_list_selected_index - g_bunit_menu_scroll_base_index) * ROW_HEIGHT;
        bunit_menu_update_and_draw_animated_cursor(
            (u16*)&marker, g_bunit_menu_list_cursor_anim_state, g_bunit_text_layout_mode);
    }
    command += command[1];
    g_bunit_menu_cursor_row = 0;
    row_count = g_bunit_menu_list_visible_rows + 1;
    if (g_bunit_menu_scroll_pixel_offset == 0) {
        row_count = g_bunit_menu_list_visible_rows;
    }
    row_commands = command;
    for (i = 0; i < row_count; i++) {
        if (g_bunit_menu_scroll_base_index + i > ENTRY_COUNT - 1 && g_bunit_menu_scroll_pixel_offset == 0) {
            break;
        }
        command = row_commands;
        for (j = 0; j < g_bunit_menu_list_row_group_count; j++) {
            command = g_bunit_cmd_handlers[*command](command);
        }
        g_bunit_menu_cursor_row += 1;
    }
    g_bunit_menu_scroll_list_active = 0;
    if (previous_selected != (u16)g_bunit_menu_list_selected_index) {
        g_bunit_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
    return command;
}
