/* Menu list rendering, formatted numbers and animated selection cursors. */

#include "fft/gfx.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_script.h"
#include "fft/sound.h"
#include "fft/status_screen.h"
#include "fft/text.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Draw the getter's decimal value, or dashes, using the current row and palette. */
menu_instruction_t* menu_opcode_0d_draw_formatted_number(menu_instruction_t* data) {
    s8 text[16];
    menu_digit_glyph_t glyph;
    u16 unused[8]; /* Preserves the original frame size. */
    menu_row_callback_t callback;
    s32 arg;
    s32 y;
    s32 value;
    /* Separate positions preserve the load/store order in the two loops. */
    s32 dash_x;
    s32 i;
    s32 pad;
    s32 pos;
    s32 digit;
    s32 cell_x;
    s32 cell;
    u16 clut;

    callback = g_menu_row_callbacks[data->number.getter].value;
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
    if (data->number.alternate_palette != 0) {
        clut = g_menu_alternate_number_clut;
    } else {
        clut = g_menu_palette_clut;
    }
    if (value & MENU_NUMBER_VALUE_DISABLED) {
        clut = g_menu_disabled_number_clut;
        value &= ~MENU_NUMBER_VALUE_DISABLED;
    }

    glyph.u = 0x78;
    glyph.v = 0x10;
    glyph.w = 6;
    glyph.h = 0xB;

    if (value & MENU_NUMBER_VALUE_DASHES) {
        value = data->number.width;
        while (value > 0) {
            cell = value - 1;
            dash_x = data->number.x + (cell * data->number.spacing);
            glyph.rect.y = y + 5;
            glyph.rect.w = 6;
            glyph.rect.h = 3;
            glyph.rect.x = dash_x;
            menu_gfx_enqueue_textured_quad(&glyph.rect, 0xBA, 0x13, 0, g_menu_gfx_semitransparency,
                g_status_screen_gfx_background_texture_page, clut, g_menu_gfx_otag_index);
            value = cell;
        }
        return menu_next_instruction(data);
    }

    pad = data->number.width - sprintf(text, g_menu_text_decimal_format, value);
    pos = 0;
    i = 0;
    if (data->number.width != 0) {
        do {
            if (pad != 0) {
                digit = 0;
                pad -= 1;
            } else {
                digit = text[pos] - 0x30;
                pos += 1;
            }
            cell_x = data->number.x + (i * data->number.spacing);
            glyph.rect.w = glyph.w;
            glyph.rect.y = y;
            glyph.rect.h = glyph.h;
            glyph.rect.x = cell_x;
            menu_gfx_enqueue_textured_quad(&glyph.rect, glyph.u + (glyph.w * digit), glyph.v, 0,
                g_menu_gfx_semitransparency, g_status_screen_gfx_background_texture_page, clut, g_menu_gfx_otag_index);
            i += 1;
        } while (i < data->number.width);
    }
    return menu_next_instruction(data);
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
            (*state)++;
        }
    } else if (frame < 0x2F) {
        (*state)++;
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
        if (script->header.opcode == STATUS_SCREEN_SCRIPT_END) {
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
        text_id_rows_to_vram_render(g_menu_list_text_table, row_ids, &rect, 0);
    }

    g_menu_list_scroll_direction = 0;
    g_menu_list_redraw_pending = 0;
}

/* Set up a scrollable list without a scroll base and draw it with the given menu script. */
void menu_init_and_draw_scrollable_list_simple(
    s16* entries, s32 selected_index, u8* text_table, const menu_instruction_t* script) {
    menu_scrollable_list_core_init(entries, selected_index, text_table);
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
 * Uses the event-speed multiplier and an animated cursor. The unused arrays
 * retain the original stack frame: rect is at 0x20 and marker at 0x38.
 */
menu_instruction_t* menu_opcode_10_draw_scrollable_list(menu_instruction_t* command) {
    RECT rect;
    u8 unused_28[16];
    point16_t marker;
    u8 unused_40[8];
    menu_instruction_t* start;
    s16* ids;
    u16 previous_selected;
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
            command = menu_script_skip_group(command);
        }
        return command;
    }
    g_menu_list_row_mode = 1;
    previous_selected = g_menu_list_selected_index;
    if (g_menu_list_redraw_pending != 0) {
        menu_draw_scrollable_list(command);
    }
    if (g_menu_list_scroll_direction == 0) {
        j = menu_input_page_scroll_direction_read();
        if (j != 0) {
            if (j != 2) {
                menu_scroll_list_by_page(j, command);
            }
        } else {
            input = g_menu_script_input;
            if ((input & (PSX_PAD_UP << 16)) || (input & PSX_PAD_UP)) {
                if (g_menu_scroll_base_index == g_menu_list_selected_index) {
                    if (g_menu_scroll_base_index > 0) {
                        g_menu_list_scroll_direction = -1;
                        if (g_menu_list_text_table != 0) {
                            s16 id;
                            ids = &g_menu_list_row_text_ids[0];
                            id = g_menu_list_entries[g_menu_scroll_base_index - 1];
                            rect.x = 0x100;
                            rect.y = 0x20;
                            rect.w = g_menu_list_vram_width;
                            rect.h = g_menu_list_row_height;
                            *ids = id;
                            text_id_rows_to_vram_render(g_menu_list_text_table, ids, &rect, 0);
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
                            entry_ids = &g_menu_list_row_text_ids[0];
                            *entry_ids = g_menu_list_entries[j + 1];
                            rect.x = 0x100;
                            rect.y = g_menu_list_visible_rows * g_menu_list_row_height + 0x30;
                            rect.w = g_menu_list_vram_width;
                            rect.h = g_menu_list_row_height;
                            text_id_rows_to_vram_render(g_menu_list_text_table, entry_ids, &rect, 0);
                        }
                    }
                } else if ((g_menu_list_selected_index < (g_menu_list_entry_count - 1)) && (input & PSX_PAD_DOWN)) {
                    g_menu_list_selected_index += 1;
                }
            } else {
                g_menu_list_scroll_progress = 0;
            }
        }
    }
    if (g_menu_list_scroll_direction < 0) {
        if (g_menu_list_scroll_progress >= g_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset -= g_menu_scroll_fast_step * speed;
        } else {
            g_menu_list_scroll_progress += speed;
            g_menu_list_scroll_offset -= g_menu_scroll_slow_step * speed;
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
        if (g_menu_list_scroll_progress >= g_menu_scroll_accel_delay) {
            g_menu_list_scroll_offset += g_menu_scroll_fast_step * speed;
        } else {
            g_menu_list_scroll_progress += speed;
            g_menu_list_scroll_offset += g_menu_scroll_slow_step * speed;
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
            menu_gfx_enqueue_textured_quad(&rect, 0, g_menu_list_scroll_offset + 0x2b, 0, g_menu_gfx_semitransparency,
                g_status_screen_gfx_background_texture_page, g_menu_window_clut, g_menu_gfx_otag_index);
        } else {
            rect.x = command->list.x;
            rect.y = command->list.y;
            rect.w = g_menu_list_vram_width * 4;
            rect.h = g_menu_list_row_height * g_menu_list_visible_rows;
            menu_gfx_enqueue_textured_quad(&rect, 0, 0x30, 0, g_menu_gfx_semitransparency,
                g_status_screen_gfx_background_texture_page, g_menu_window_clut, g_menu_gfx_otag_index);
        }
    }
    if (g_menu_use_alternate_palette == 0) {
        if (g_menu_scroll_base_index != 0) {
            rect.x = g_status_screen_menu_window_right_x;
            rect.y = command->list.arrow_up_y;
            rect.w = 8;
            rect.h = 0x10;
            menu_gfx_enqueue_textured_quad(&rect, 0xd8, 0, 0, g_menu_gfx_semitransparency, g_menu_cursor_texture_page,
                g_menu_cursor_mode0_foreground_clut, 0x1e);
        }
        if (g_menu_scroll_base_index < (g_menu_list_entry_count - g_menu_list_visible_rows)) {
            rect.x = g_status_screen_menu_window_right_x;
            rect.y = command->list.arrow_down_y;
            rect.w = 8;
            rect.h = 0x10;
            menu_gfx_enqueue_textured_quad(&rect, 0xe0, 0, 0, g_menu_gfx_semitransparency, g_menu_cursor_texture_page,
                g_menu_cursor_mode0_foreground_clut, 0x1e);
        }
        if (g_menu_list_entry_count > g_menu_list_visible_rows) {
            y = command->list.arrow_up_y + 0x14;
            j = command->list.arrow_down_y - y;
            y = (j * g_menu_list_selected_index) / (g_menu_list_entry_count - 1);
            j = command->list.arrow_up_y + 0xf;
            j += y;
            rect.w = 8;
            rect.h = 8;
            rect.x = g_status_screen_menu_window_right_x;
            rect.y = j;
            menu_gfx_enqueue_textured_quad(&rect, 0x10, 0x10, 0, g_menu_gfx_semitransparency,
                g_menu_cursor_texture_page, g_menu_cursor_mode0_foreground_clut, g_menu_gfx_otag_index);
        }
        if (command->list.show_cursor != 0) {
            marker.x = command->list.x - command->list.cursor_x_offset;
            marker.y
                = command->list.y + ((g_menu_list_selected_index - g_menu_scroll_base_index) * g_menu_list_row_height);
            menu_update_and_draw_animated_cursor(&marker, &g_menu_list_cursor_anim_state, g_menu_use_alternate_palette);
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
