#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Menu-script handler for a scrollable list: the WORLD original of the
 * overlays' scrollable-list body commands.
 *
 * Moves the cursor on Up/Down (bits 0x10000000 and 0x40000000 of the script
 * input, the held-repeat flags of Up and Down, keep a scroll going without
 * moving the cursor) and pages through
 * world_menu_scroll_list_page. Moving past the first or last visible row
 * starts a one-row scroll: the newly exposed row is rendered into the list's
 * VRAM strip at x 0x240, the pixel offset advances by the slow step until the
 * repeat timer reaches g_main_menu_scroll_accel_delay and by the fast step
 * after it, and when a full row height is covered the strip is moved one row
 * with draw-move primitives (one per row when scrolling up). Then it draws the list image, the up and down
 * arrows, the scrollbar thumb and the cursor, and replays the per-row record
 * group once per visible row, plus one while a scroll is in progress.
 *
 * An empty list skips the list command and its following row group. Returns
 * the record that follows the list.
 */
menu_instruction_t* menu_cmd_draw_scrollable_list_body_handler_variant(menu_instruction_t* instruction) {
    RECT rect;
    s32 unused[4];
    point16_t point;
    u16 priority;
    u8 speed;
    s32 i;
    s32 j;
    s32 rows;
    s32 index;
    u16 old_cursor;
    menu_instruction_t* base;

    priority = g_menu_gfx_otag_index;
    if (g_menu_list_entry_count <= 0) {
        for (j = instruction->list.skip_count; j != -1; j--) {
            instruction = world_menu_skip_instruction_group(instruction);
        }
        return instruction;
    }
    speed = menu_get_event_speed();
    g_world_menu_use_scroll_position = 1;
    if (g_world_menu_list_layout_pending != 0) {
        world_menu_load_scrollable_list_layout(instruction);
    }
    old_cursor = g_menu_list_selected_index;
    if (g_world_menu_list_scroll_direction == 0) {
        i = menu_input_read_page_scroll_direction_variant();
        if (i != 0) {
            if (i != 2) {
                menu_scroll_list_by_page_variant(i, instruction);
            }
        } else if ((g_menu_cmd_stream_input & 0x10000000) || (g_menu_cmd_stream_input & PSX_PAD_UP)) {
            index = g_menu_scroll_base_index;
            if (index == g_menu_list_selected_index) {
                if (index > 0) {
                    g_world_menu_list_scroll_direction = -1;
                    if (g_world_menu_list_text_table != 0) {
                        g_world_menu_list_row_text_ids[0] = g_world_menu_list_entry_ids[index - 1];
                        rect.x = 0x240;
                        rect.y = 0x120;
                        rect.w = g_world_menu_list_image_width;
                        rect.h = g_world_menu_row_height;
                        world_text_render_id_list_to_image_rows(
                            g_world_menu_list_text_table, g_world_menu_list_row_text_ids, &rect, 0);
                    }
                }
            } else if (g_menu_cmd_stream_input & PSX_PAD_UP) {
                g_menu_list_selected_index--;
            }
        } else if ((g_menu_cmd_stream_input & 0x40000000) || (g_menu_cmd_stream_input & PSX_PAD_DOWN)) {
            if (g_menu_scroll_base_index + g_world_menu_list_visible_rows - 1 == g_menu_list_selected_index) {
                if (g_menu_scroll_base_index + g_world_menu_list_visible_rows - 1 < g_menu_list_entry_count - 1) {
                    g_world_menu_list_scroll_direction = 1;
                    if (g_world_menu_list_text_table != 0) {
                        index = g_menu_scroll_base_index + g_world_menu_list_visible_rows - 1;
                        g_world_menu_list_row_text_ids[0] = g_world_menu_list_entry_ids[index + 1];
                        rect.x = 0x240;
                        rect.y = g_world_menu_list_visible_rows * g_world_menu_row_height + 0x130;
                        rect.w = g_world_menu_list_image_width;
                        rect.h = g_world_menu_row_height;
                        world_text_render_id_list_to_image_rows(
                            g_world_menu_list_text_table, g_world_menu_list_row_text_ids, &rect, 0);
                    }
                }
            } else {
                if (g_menu_list_selected_index < g_menu_list_entry_count - 1
                    && (g_menu_cmd_stream_input & PSX_PAD_DOWN)) {
                    g_menu_list_selected_index++;
                }
            }
        } else {
            g_world_menu_scroll_repeat_timer = 0;
        }
    }
    if (g_world_menu_list_scroll_direction < 0) {
        if (g_world_menu_scroll_repeat_timer >= g_main_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset -= g_main_menu_scroll_fast_step * speed;
        } else {
            g_world_menu_scroll_repeat_timer += speed;
            g_menu_list_scroll_offset -= g_main_menu_scroll_slow_step * speed;
        }
        if (g_menu_list_scroll_offset <= -g_world_menu_row_height) {
            g_menu_list_scroll_offset = 0;
            g_world_menu_list_scroll_direction = 0;
            g_menu_scroll_base_index = --g_menu_list_selected_index;
            if (g_world_menu_list_text_table != 0) {
                for (i = 0; i < g_world_menu_list_visible_rows; i++) {
                    rect.x = 0x240;
                    rect.y = (i - 1) * g_world_menu_row_height + 0x130;
                    rect.w = g_world_menu_list_image_width;
                    rect.h = g_world_menu_row_height;
                    menu_gfx_enqueue_draw_move(&rect, 0x240, g_world_menu_row_height * i + 0x130, priority - 1);
                }
            }
        }
    } else if (g_world_menu_list_scroll_direction > 0) {
        if (g_world_menu_scroll_repeat_timer >= g_main_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset += g_main_menu_scroll_fast_step * speed;
        } else {
            g_world_menu_scroll_repeat_timer += speed;
            g_menu_list_scroll_offset += g_main_menu_scroll_slow_step * speed;
        }
        if (g_menu_list_scroll_offset >= g_world_menu_row_height) {
            g_menu_list_scroll_offset = 0;
            g_world_menu_list_scroll_direction = 0;
            g_menu_scroll_base_index++;
            g_menu_list_selected_index++;
            if (g_world_menu_list_text_table != 0) {
                rect.x = 0x240;
                rect.y = g_world_menu_row_height + 0x130;
                rect.w = g_world_menu_list_image_width;
                rect.h = g_world_menu_row_height * g_world_menu_list_visible_rows;
                menu_gfx_enqueue_draw_move(&rect, 0x240, 0x130, priority - 1);
            }
        }
    }
    if (g_world_menu_list_text_table != 0) {
        if (g_menu_list_scroll_offset != 0) {
            rect.x = instruction->list.x;
            rect.y = instruction->list.y - 5;
            rect.w = g_world_menu_list_image_width * 4;
            rect.h = g_world_menu_row_height * g_world_menu_list_visible_rows + 10;
            menu_gfx_enqueue_textured_quad(&rect, 0, g_menu_list_scroll_offset + 0x2B, 0, g_menu_gfx_semitransparency,
                g_world_menu_window_tpage, g_world_menu_window_clut, g_menu_gfx_otag_index);
        } else {
            rect.x = instruction->list.x;
            rect.y = instruction->list.y;
            rect.w = g_world_menu_list_image_width * 4;
            rect.h = g_world_menu_row_height * g_world_menu_list_visible_rows;
            menu_gfx_enqueue_textured_quad(&rect, 0, 0x30, 0, g_menu_gfx_semitransparency, g_world_menu_window_tpage,
                g_world_menu_window_clut, g_menu_gfx_otag_index);
        }
    }
    if (g_world_menu_use_alternate_palette == 0) {
        if (g_menu_scroll_base_index != 0) {
            rect.x = g_world_menu_window_right_x;
            rect.y = instruction->list.arrow_up_y;
            rect.w = 8;
            rect.h = 0x10;
            menu_gfx_enqueue_textured_quad(&rect, 0xD8, 0, 0, g_menu_gfx_semitransparency, g_menu_cursor_texture_page,
                g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
        if (g_menu_scroll_base_index < g_menu_list_entry_count - g_world_menu_list_visible_rows) {
            rect.x = g_world_menu_window_right_x;
            rect.y = instruction->list.arrow_down_y;
            rect.w = 8;
            rect.h = 0x10;
            menu_gfx_enqueue_textured_quad(&rect, 0xE0, 0, 0, g_menu_gfx_semitransparency, g_menu_cursor_texture_page,
                g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
        if (g_menu_list_entry_count > g_world_menu_list_visible_rows) {
            i = instruction->list.arrow_up_y + 0xF;
            j = instruction->list.arrow_down_y - (s16)(i + 5);
            i += j * g_menu_list_selected_index / (g_menu_list_entry_count - 1);
            rect.x = g_world_menu_window_right_x;
            rect.y = i;
            rect.w = 8;
            rect.h = 8;
            menu_gfx_enqueue_textured_quad(&rect, 0x10, 0x10, 0, g_menu_gfx_semitransparency,
                g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
    }
    if (instruction->list.show_cursor != 0) {
        point.x = instruction->list.x - instruction->list.cursor_x_offset;
        point.y
            = instruction->list.y + (g_menu_list_selected_index - g_menu_scroll_base_index) * g_world_menu_row_height;
        menu_update_and_draw_sliding_cursor_variant(
            &point, &g_world_menu_list_cursor_anim, g_world_menu_use_alternate_palette);
    }
    instruction = menu_next_instruction(instruction);
    g_world_menu_scroll_row_offset = 0;
    rows = g_world_menu_list_visible_rows + 1;
    if (g_menu_list_scroll_offset == 0) {
        rows--;
    }
    base = instruction;
    for (i = 0; i < rows; i++, g_world_menu_scroll_row_offset++) {
        if (g_menu_scroll_base_index + i > g_menu_list_entry_count - 1 && g_menu_list_scroll_offset == 0) {
            break;
        }
        instruction = base;
        for (j = 0; j < g_world_menu_list_row_record_count; j++) {
            instruction = g_menu_cmd_handlers[instruction->header.opcode](instruction);
        }
    }
    g_world_menu_use_scroll_position = 0;
    if (old_cursor != g_menu_list_selected_index) {
        g_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
    return instruction;
}
