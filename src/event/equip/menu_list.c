/* Menu list rendering, formatted numbers and animated selection cursors. */

#include "fft/equipment.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Draw the getter's signed decimal value or dashes in the requested digit style: 0 the
 * 6x11 digits, 1 the 8x14 ones, 2 and up the 10x16 digits (eight to a row from u 0x20)
 * in the window CLUT. The getter is given the command's value, or the list row inside a
 * scrollable list; a negative result gets a minus sign before the first cell. */
menu_instruction_t* menu_opcode_0d_draw_formatted_number(menu_instruction_t* data) {
    s8 text[16];
    menu_digit_glyph_t glyph;
    RECT minus_rect;
    menu_row_callback_t callback;
    s32 arg;
    s32 y;
    s32 value;
    /* Clear this 16-bit value after drawing the sign to retain the store schedule. */
    s16 negative;
    s32 cell;
    s32 dash_x;
    s32 cell_x;
    s32 i;
    s16 pad;
    s32 pos;
    s32 digit;
    u16 clut;

    callback = g_menu_row_callbacks[data->number.getter].value;
    negative = 0;
    if (g_menu_list_row_mode == 0) {
        arg = data->number.value;
        y = data->number.y;
    } else {
        arg = g_menu_scroll_base_index + g_menu_list_row_index;
        y = ((g_menu_list_row_height * g_menu_list_row_index) + data->number.y) - g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            arg -= 1;
            y -= g_menu_list_row_height;
        }
    }
    value = callback(arg);
    if (value < 0) {
        value = -value;
        negative = 1;
    }
    if (data->number.alternate_palette != 0) {
        clut = g_menu_alternate_number_clut;
    } else {
        clut = g_menu_palette_clut;
    }
    if (value & MENU_NUMBER_VALUE_DISABLED) {
        clut = g_menu_disabled_number_clut;
        value &= ~MENU_NUMBER_VALUE_DISABLED;
    }
    if (data->number.style == 1) {
        glyph.u = 0x78;
        glyph.v = 1;
        glyph.w = 8;
        glyph.h = 0xE;
    } else if (data->number.style == 0) {
        glyph.u = 0x78;
        glyph.v = 0x10;
        glyph.w = 6;
        glyph.h = 0xB;
    } else {
        glyph.u = 0x20;
        glyph.v = 0;
        glyph.w = 0xA;
        glyph.h = 0x10;
    }
    if (value & MENU_NUMBER_VALUE_DASHES) {
        value = data->number.width;
        while (value > 0) {
            cell = value - 1;
            dash_x = data->number.x + (cell * data->number.spacing);
            glyph.rect.y = y + 5;
            glyph.rect.w = 6;
            glyph.rect.h = 3;
            glyph.rect.x = dash_x - 1;
            menu_gfx_enqueue_textured_quad(&glyph.rect, 0xBA, 0x13, 0, g_menu_gfx_semitransparency,
                g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
            value = cell;
        }
        return menu_next_instruction(data);
    }
    pad = data->number.width - sprintf(text, g_menu_text_decimal_format, value) - negative;
    pos = 0;
    for (i = 0; i < data->number.width; i++) {
        cell_x = data->number.x + (i * data->number.spacing);
        glyph.rect.w = glyph.w;
        glyph.rect.y = y;
        glyph.rect.h = glyph.h;
        glyph.rect.x = cell_x;
        if (negative != 0) {
            minus_rect.x = cell_x - 6;
            minus_rect.y = y + 2;
            minus_rect.w = 6;
            minus_rect.h = 0xB;
            menu_gfx_enqueue_textured_quad(&minus_rect, 0xBA, 0x10, 0, g_menu_gfx_semitransparency,
                g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
            negative = 0;
        } else {
            digit = 0;
            if (pad != 0) {
                pad -= 1;
            } else {
                digit = text[pos] - '0';
                pos += 1;
            }
            if (data->number.style < 2) {
                menu_gfx_enqueue_textured_quad(&glyph.rect, glyph.u + (glyph.w * digit), glyph.v, 0,
                    g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
            } else {
                if (digit < 8) {
                    glyph.v = 0;
                    glyph.u = glyph.w * digit + 0x20;
                } else {
                    glyph.v = 0x10;
                    glyph.u = glyph.w * (digit - 8) + 0x20;
                }
                menu_gfx_enqueue_textured_quad(&glyph.rect, glyph.u, glyph.v, 0, g_menu_gfx_semitransparency,
                    g_menu_atlas_texture_page, g_menu_window_clut, g_menu_gfx_otag_index);
            }
        }
    }
    return menu_next_instruction(data);
}

/* Draw the row callback's value right-aligned in a field of `width` 8x16 cells: blank
 * cells on the left, then the digit glyphs (u 0xa8 + 8 per digit, v 0x30 on the cursor
 * texture page). A value with more digits than `width` would start the blank-cell
 * countdown below -1 and run it through the whole 16-bit range. */
menu_instruction_t* menu_opcode_17_draw_right_aligned_number(menu_instruction_t* cmd) {
    s8 digits[16];
    RECT glyph_rect;
    s32 count;
    s16 blanks;
    s16 i;
    s32 j;
    s16 x;
    u8 y;

    count = g_menu_row_callbacks[cmd->number.getter].value(cmd->number.value);
    count = sprintf(digits, g_menu_text_decimal_format, count);
    blanks = cmd->number.width - count;
    x = cmd->number.x;
    y = cmd->number.y;
    count = cmd->number.width - blanks;
    g_gfx_blank_cell_rect.position.y = y + 4;
    for (i = blanks - 1; i != -1; i--) {
        g_gfx_blank_cell_rect.position.x = x;
        menu_gfx_enqueue_oriented_textured_quad(
            &g_gfx_blank_cell_rect, 0, TEXTURE_UV_FLIP_NONE, 0, g_menu_gfx_otag_index);
        x += 8;
    }
    glyph_rect.x = x;
    glyph_rect.y = y;
    glyph_rect.w = 8;
    glyph_rect.h = 0x10;
    for (j = 0; j < count; j++) {
        menu_gfx_enqueue_textured_quad(&glyph_rect, (digits[j] - '0') * 8 + 0xA8, 0x30, 0, 0,
            g_menu_cursor_texture_page, g_menu_text_clut_1_mode0, g_menu_gfx_otag_index);
        glyph_rect.x += 8;
    }
    return menu_next_instruction(cmd);
}

/* Draw the sliding 16x16 marker and its semi-transparent shadow at the current
 * ordering-table entry. State packs the active flag in bit 15 and the frame counter in
 * bits 0-6. Mode 0 cancels the active curve; an odd mode starts it. The x offset follows
 * the forward keyframes (looping every 0x2F frames) or, while active, the reverse keyframes
 * (holding at frame 0x24). The copy before each active test preserves the original register
 * allocation. */
void menu_update_and_draw_animated_cursor(const point16_t* anchor, u16* state, s32 mode) {
    RECT rect;
    s32 active = *state >> 15;
    s32 active_test = active;
    s32 frame;
    s32 i;
    s8 raw_offset;
    s32 x_offset;
    u16 clut;
    u16 shadow_clut;

    if (mode == 0) {
        if (active != 0) {
            *state = 0;
            active = 0;
        }
    }
    active_test = active;
    if (active_test == 0 && (mode & 1)) {
        *state = 0x8000;
        active = 1;
    }

    frame = *state;
    frame = frame & 0x7F;
    frame = frame * menu_get_event_speed();
    active_test = active;
    if (active_test != 0) {
        if (frame >= 0x25) {
            frame = 0x24;
        }
    } else if (frame >= 0x2F) {
        frame = 0;
    }

    active_test = active;
    if (active_test != 0) {
        for (i = 0; g_menu_cursor_reverse_offset_keyframes[i] < frame; i += 2) { }
        raw_offset = g_menu_cursor_reverse_offset_keyframes[i + 1];
    } else {
        for (i = 0; g_menu_cursor_forward_offset_keyframes[i] < frame; i += 2) { }
        raw_offset = g_menu_cursor_forward_offset_keyframes[i + 1];
    }
    x_offset = raw_offset;

    frame += menu_get_event_speed();
    active_test = active;
    if (active_test != 0) {
        if (frame < 0x25) {
            *state = *state + 1;
        }
    } else if (frame < 0x2F) {
        *state = *state + 1;
    } else {
        *state = 0;
    }

    if (mode != 0) {
        clut = g_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_menu_cursor_mode1_background_clut;
    } else {
        clut = g_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_menu_cursor_mode0_background_clut;
    }

    rect.x = x_offset + anchor->x - 2;
    rect.y = anchor->y;
    rect.w = 0x10;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, g_menu_cursor_texture_page, clut, g_menu_gfx_otag_index);
    rect.x = rect.x + 2;
    rect.y = rect.y + 2;
    menu_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, shadow_clut, g_menu_gfx_otag_index);
}

/* Skip a command plus, for group opcodes 1 and 2, the child commands it counts;
 * return the command that follows. */
menu_instruction_t* menu_script_skip_group(menu_instruction_t* command) {
    s32 count;

    if (command->header.opcode == MENU_SCRIPT_OP_IF_GROUP) {
        count = command->conditional.count + 1;
    } else if (command->header.opcode == MENU_SCRIPT_OP_IF_ELSE_GROUP) {
        count = command->if_else.count_true + command->if_else.count_false + 1;
    } else {
        count = 1;
    }

    while (--count != -1) {
        command = menu_next_instruction(command);
    }
    return command;
}

/* Draw the visible rows of a scrollable list.
 *
 * The stream's MENU_SCRIPT_OP_SCROLLABLE_LIST command supplies the list geometry; a stream that
 * reaches its end opcode first has no list. The selection and scroll position are clamped to
 * the list and, with a text table set, the visible rows' text is redrawn into VRAM at
 * (0x100, 0x30).
 */
void menu_draw_scrollable_list(const menu_instruction_t* script) {
    RECT rect;
    s16 row_ids[20];
    s32 i;
    s16 visible_rows;
    s16 entry_count;
    s16 selected;
    const menu_list_record_t* list;

    while (script->header.opcode != MENU_SCRIPT_OP_SCROLLABLE_LIST) {
        if (script->header.opcode == EQUIPMENT_SCRIPT_END) {
            return;
        }
        script = menu_next_const_instruction(script);
    }

    list = &script->list;
    g_menu_list_row_group_count = list->skip_count;
    g_menu_list_row_height = list->row_height;
    visible_rows = list->visible_rows;
    g_menu_list_visible_rows = visible_rows;
    g_menu_list_glyph_width = list->width_a;
    g_menu_list_text_columns = list->width_b;
    g_menu_list_vram_width = (g_menu_list_glyph_width * g_menu_list_text_columns) >> 2;

    entry_count = g_menu_list_entry_count;
    if (g_menu_list_selected_index >= entry_count) {
        g_menu_list_selected_index = entry_count - 1;
    }
    selected = g_menu_list_selected_index;
    if (selected - g_menu_scroll_base_index >= visible_rows) {
        g_menu_scroll_base_index = selected;
    }
    if (selected < g_menu_scroll_base_index) {
        g_menu_scroll_base_index = selected;
    }
    if (entry_count < visible_rows) {
        g_menu_scroll_base_index = 0;
    } else if (entry_count - g_menu_scroll_base_index < visible_rows) {
        g_menu_scroll_base_index = entry_count - visible_rows;
    }

    if (g_menu_list_selected_index >= g_menu_list_entry_count) {
        g_menu_list_selected_index = g_menu_list_entry_count - 1;
    }

    if (g_menu_list_text_table != 0) {
        rect.x = 0x100;
        rect.y = 0x30;
        rect.w = g_menu_list_vram_width;
        rect.h = g_menu_list_visible_rows * 16;
        ClearImage(&rect, 0, 0, 0);
        for (i = 0; i < g_menu_list_visible_rows; i++) {
            row_ids[i] = g_menu_list_entries[i + g_menu_scroll_base_index];
        }
        row_ids[i] = -1;
        rect.x = 0x100;
        rect.y = 0x30;
        rect.w = g_menu_list_vram_width;
        rect.h = g_menu_list_row_height;
        text_render_id_rows_to_vram(g_menu_list_text_table, row_ids, &rect, 0);
    }

    g_menu_list_scroll_direction = 0;
    g_menu_list_redraw_pending = 0;
}

/* Set up a scrollable list without a scroll base and draw it with the given menu script. */
void menu_init_and_draw_scrollable_list_simple(
    s16* entries, s32 selected_index, u8* text_table, const menu_instruction_t* script) {
    menu_init_scrollable_list_core(entries, selected_index, text_table);
    menu_draw_scrollable_list(script);
}

/* Scrollable-list body command.
 *
 * Handles line and page scrolling (animating the VRAM text strip and
 * rendering the newly exposed row), draws the list image, the arrows, the
 * scrollbar thumb and the selection marker, then replays the per-row command
 * group once per visible row, plus one while a scroll offset is in effect.
 * An empty list skips its row command groups.
 *
 * Uses a fixed speed of 2 and an animated cursor. The padding arrays retain
 * the original stack frame.
 */
menu_instruction_t* menu_opcode_10_draw_scrollable_list(menu_instruction_t* command) {
    RECT rect;
    u8 pad0[0x10];
    point16_t marker;
    u8 pad1[0x8];
    u16 ot_index;
    u16 previous_selected;
    s32 row_count;
    u16 speed;
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
            command = menu_script_skip_group(command);
            j -= 1;
        }
        return command;
    }
    g_menu_list_row_mode = 1;
    previous_selected = g_menu_list_selected_index;
    speed = 2;
    if (g_menu_list_redraw_pending != 0) {
        menu_draw_scrollable_list(command);
    }
    if (g_menu_list_scroll_direction == 0) {
        j = equipment_input_read_page_scroll_direction();
        if (j != 0) {
            if (j != 2) {
                menu_scroll_list_by_page(j, command);
            }
        } else {
            input = g_menu_script_input;
            if ((input & (PSX_PAD_UP << 16)) || (input & PSX_PAD_UP)) {
                scroll_base = g_menu_scroll_base_index;
                if (scroll_base == g_menu_list_selected_index) {
                    if (scroll_base > 0) {
                        g_menu_list_scroll_direction = -1;
                        if (g_menu_list_text_table != 0) {
                            row_ids = &g_menu_list_row_text_ids[0];
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
                            entry_ids = &g_menu_list_row_text_ids[0];
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
                g_menu_list_scroll_progress = 0;
            }
        }
    }
    if (g_menu_list_scroll_direction < 0) {
        if (g_menu_list_scroll_progress >= g_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset -= g_menu_scroll_fast_step * (speed & 0xff);
        } else {
            g_menu_list_scroll_offset -= g_menu_scroll_slow_step * (speed & 0xff);
            g_menu_list_scroll_progress = (speed & 0xff) + g_menu_list_scroll_progress;
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
        if (g_menu_list_scroll_progress >= g_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset += g_menu_scroll_fast_step * (speed & 0xff);
        } else {
            g_menu_list_scroll_offset += g_menu_scroll_slow_step * (speed & 0xff);
            g_menu_list_scroll_progress = (speed & 0xff) + g_menu_list_scroll_progress;
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
        menu_update_and_draw_animated_cursor(&marker, &g_menu_list_cursor_anim_state, g_menu_use_alternate_palette);
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
            command = g_menu_opcode_handlers[command->header.opcode](command);
        }
        g_menu_list_row_index += 1;
    }
    g_menu_list_row_mode = 0;
    if (previous_selected != g_menu_list_selected_index) {
        g_sound_queued_effect_id = SOUND_SFX_CURSOR_MOVE;
    }
    return command;
}
