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
#include "fft/unit_list.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Dispatch slots before this program's end opcode; zero entries stay unhandled. */
menu_opcode_handler_t g_menu_opcode_handlers[UNIT_LIST_SCRIPT_END] = {
    menu_opcode_00_draw_window_frame,         /* 0x00 */
    menu_opcode_01_if_group,                  /* 0x01 */
    menu_opcode_02_if_else_group,             /* 0x02 */
    0,                                        /* 0x03 */
    menu_opcode_04_draw_textured_quad,        /* 0x04 */
    menu_opcode_04_draw_textured_quad,        /* 0x05 */
    menu_opcode_06_draw_callback_quad,        /* 0x06 */
    0,                                        /* 0x07 */
    0,                                        /* 0x08 */
    menu_opcode_09_set_fixed_clut,            /* 0x09 */
    menu_opcode_0a_set_clut,                  /* 0x0a */
    menu_opcode_0b_set_draw_color,            /* 0x0b */
    menu_opcode_0c_set_texture_page,          /* 0x0c */
    menu_opcode_0d_draw_formatted_number,     /* 0x0d */
    menu_opcode_0e_set_semi_trans,            /* 0x0e */
    menu_opcode_0f_set_draw_priority,         /* 0x0f */
    menu_opcode_10_draw_scrollable_list,      /* 0x10 */
    menu_opcode_11_draw_background_tiles,     /* 0x11 */
    menu_opcode_12_draw_opening_window_frame, /* 0x12 */
    menu_opcode_13_clear_temporary_draw_area, /* 0x13 */
    menu_opcode_14_set_draw_area_if_unlocked, /* 0x14 */
    menu_opcode_15_repeat_draw_area,          /* 0x15 */
    menu_opcode_16_advance_window_scale_step, /* 0x16 */
    menu_opcode_17_draw_right_aligned_number, /* 0x17 */
    menu_opcode_18_add_blend_draw_mode,       /* 0x18 */
    menu_opcode_0d_draw_formatted_number,     /* 0x19 */
    menu_opcode_0d_draw_formatted_number,     /* 0x1a */
    menu_opcode_0d_draw_formatted_number,     /* 0x1b */
};

/* Draw a row-callback value as a row of digit glyphs (opcodes 0x19-0x1B).
 *
 * Getter, width, spacing, palette, coordinates and value come from the command,
 * and the getter is given the command's value, or the list row in a scrollable
 * list. MENU_NUMBER_VALUE_DISABLED in the result selects the disabled CLUT,
 * MENU_NUMBER_VALUE_DASHES draws dashes, and a negative value gets a minus
 * glyph ahead of the first cell. Style 0 uses the 6x11 digits, 1 the 8x14 ones;
 * other styles draw no digits.
 *
 * Register allocation hinges on GCC 2.6.3 allocno priorities: `negative` is
 * an s8 (the promoted subreg gives the target's `move v0,s7` copy before the
 * test) and is cleared after the minus-glyph call, and the if/else spellings
 * of `color` and `value` set the live lengths that put `color` in s6 ahead of
 * `negative` in s7. */
menu_instruction_t* menu_opcode_0d_draw_formatted_number(menu_instruction_t* command) {
    s8 text[16];
    RECT rect;
    RECT glyph;
    RECT minus_rect;
    s32 (*getter)(s32);
    s32 row;
    s32 y;
    s32 value;
    s32 digit;
    s16 blanks;
    s32 text_index;
    s32 i;
    s8 negative;
    rgb24_t* color;
    u16 clut;
    s32 x;
    s32 next_remaining;
    u8 opcode;

    getter = g_menu_row_callbacks[command->number.getter].value;
    negative = 0;
    if (g_menu_list_row_mode == 0) {
        row = command->number.value;
        y = command->number.y;
    } else {
        row = g_menu_scroll_base_index + g_menu_list_row_index;
        y = g_menu_list_row_height * g_menu_list_row_index + command->number.y - g_menu_list_scroll_offset;
        if (g_menu_list_scroll_offset < 0) {
            row--;
            y -= g_menu_list_row_height;
        }
    }
    if (getter != 0) {
        value = getter(row);
    } else {
        value = row;
    }
    if (value < 0) {
        value = -value;
        negative = 1;
    }
    opcode = command->number.opcode;
    if (opcode == MENU_SCRIPT_OP_NUMBER_TINTED || opcode == MENU_SCRIPT_OP_NUMBER_TINTED_NO_LEADING_ZEROS) {
        color = &g_menu_text_color;
    } else {
        color = 0;
    }
    if (command->number.alternate_palette != 0) {
        clut = g_menu_alternate_number_clut;
    } else {
        clut = g_menu_palette_clut;
    }
    if (value & MENU_NUMBER_VALUE_DISABLED) {
        clut = g_menu_disabled_number_clut;
        value &= ~MENU_NUMBER_VALUE_DISABLED;
    }
    if (command->number.style == 1) {
        glyph.x = 0x78;
        glyph.y = 1;
        glyph.w = 8;
        glyph.h = 0xE;
    } else if (command->number.style == 0) {
        glyph.x = 0x78;
        glyph.y = 0x10;
        glyph.w = 6;
        glyph.h = 0xB;
    } else {
        glyph.x = 0x20;
        glyph.y = 0;
        glyph.w = 0xA;
        glyph.h = 0x10;
    }
    if (value & MENU_NUMBER_VALUE_DASHES) {
        value = command->number.width;
        while (value > 0) {
            next_remaining = value - 1;
            rect.x = command->number.x + next_remaining * command->number.spacing;
            rect.y = y + 5;
            rect.w = 6;
            rect.h = 3;
            menu_gfx_enqueue_textured_quad(&rect, 0xBA, 0x13, color, g_menu_gfx_semitransparency,
                g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
            value = next_remaining;
        }
    } else {
        blanks = command->number.width - sprintf(text, g_menu_text_decimal_format, value) - negative;
        text_index = 0;
        for (i = 0; i < command->number.width; i++) {
            x = command->number.x + i * command->number.spacing;
            rect.y = y;
            rect.w = glyph.w;
            rect.h = glyph.h;
            rect.x = x;
            if (negative != 0) {
                minus_rect.x = x - 6;
                minus_rect.y = y + 2;
                minus_rect.w = 6;
                minus_rect.h = 0xB;
                menu_gfx_enqueue_textured_quad(&minus_rect, 0xBA, 0x10, color, g_menu_gfx_semitransparency,
                    g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
                negative = 0;
            } else {
                if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_DIM_LEADING_ZEROS) {
                    clut = g_menu_palette_clut;
                }
                if (blanks != 0) {
                    digit = 0;
                    blanks--;
                    if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_TINTED_NO_LEADING_ZEROS) {
                        continue;
                    }
                    if (command->number.opcode == MENU_SCRIPT_OP_NUMBER_DIM_LEADING_ZEROS
                        && i < command->number.width - 1) {
                        clut = g_menu_disabled_number_clut;
                    }
                } else {
                    digit = text[text_index] - '0';
                    text_index++;
                }
                if (command->number.style < 2) {
                    menu_gfx_enqueue_textured_quad(&rect, glyph.x + glyph.w * digit, glyph.y, color,
                        g_menu_gfx_semitransparency, g_menu_atlas_texture_page, clut, g_menu_gfx_otag_index);
                }
            }
        }
    }
    return menu_next_instruction(command);
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
    s32 x;
    s32 y;

    count = g_menu_row_callbacks[cmd->number.getter].value(cmd->number.value);
    count = sprintf(digits, g_menu_text_decimal_format, count);
    x = cmd->number.x;
    y = cmd->number.y;
    blanks = cmd->number.width - count;
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
            g_menu_cursor_texture_page, 0x3ED3, g_menu_gfx_otag_index);
        glyph_rect.x += 8;
    }
    return menu_next_instruction(cmd);
}

/* Advances the animated cursor at `pos` and draws it, sliding horizontally,
 * with its semi-transparent shadow. `mode` bit 0 starts the reverse curve;
 * mode 0 cancels it. The same routine as menu_update_and_draw_animated_cursor, except
 * that it draws at the fixed ordering-table entries 0x3C and 0x3B. */
void menu_update_and_draw_sliding_cursor(point16_t* pos, u16* state, s32 mode) {
    RECT rect;
    s32 active;
    s32 active_bit = *state >> 15;
    s32 timer;
    s32 offset;
    s8 raw_offset;
    s32 i;
    u16 front_clut;
    u16 shadow_clut;

    active = active_bit;
    if (mode == 0) {
        if (active_bit != 0) {
            *state = 0;
            active = 0;
        }
    }
    active_bit = active;
    if (active_bit == 0 && (mode & 1) != 0) {
        *state = 0x8000;
        active = 1;
    }

    timer = *state;
    timer &= 0x7F;
    timer = timer * menu_get_event_speed();
    active_bit = active;
    if (active_bit != 0) {
        if (timer >= 0x25) {
            timer = 0x24;
        }
    } else {
        if (timer >= 0x2F) {
            timer = 0;
        }
    }

    active_bit = active;
    if (active_bit != 0) {
        for (i = 0; g_menu_cursor_reverse_offset_keyframes[i] < timer; i += 2) { }
        raw_offset = g_menu_cursor_reverse_offset_keyframes[i + 1];
    } else {
        for (i = 0; g_menu_cursor_forward_offset_keyframes[i] < timer; i += 2) { }
        raw_offset = g_menu_cursor_forward_offset_keyframes[i + 1];
    }

    offset = raw_offset;
    timer += menu_get_event_speed();
    active_bit = active;
    if (active_bit != 0) {
        if (timer < 0x25) {
            (*state)++;
        }
    } else {
        if (timer < 0x2F) {
            (*state)++;
        } else {
            *state = 0;
        }
    }

    if (mode != 0) {
        front_clut = g_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_menu_cursor_mode1_background_clut;
    } else {
        front_clut = g_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_menu_cursor_mode0_background_clut;
    }

    rect.x = offset + pos->x - 2;
    rect.y = pos->y;
    rect.w = 0x10;
    rect.h = 0x10;
    menu_gfx_enqueue_textured_quad(&rect, 0xA8, 0, 0, 0, g_menu_cursor_texture_page, front_clut, 0x3C);
    rect.x += 2;
    rect.y += 2;
    menu_gfx_enqueue_textured_quad(&rect, 0xB8, 0, 0, 1, 0x5F, shadow_clut, 0x3B);
}

/* Advances the animated cursor at `pos` and draws it, bouncing vertically,
 * with its semi-transparent shadow through the shared sprite descriptor.
 *
 * Same state machine and curves as menu_update_and_draw_sliding_cursor; the
 * curve offset moves the quad's y edges instead of its x origin.
 */
void menu_update_and_draw_bouncing_cursor(point16_t* pos, u16* state, s32 mode) {
    u16 unused[4]; /* Retains the original 0x38-byte stack frame. */
    s32 bottom;
    s32 active;
    s32 active_bit;
    s32 timer;
    s32 y_offset;
    s8 raw_offset;
    s32 i;
    u16 front_clut;
    u16 shadow_clut;

    active_bit = *state >> 15;
    active = active_bit;
    if (mode == 0) {
        if (active_bit != 0) {
            *state = 0;
            active = 0;
        }
    }
    active_bit = active;
    if (active_bit == 0 && (mode & 1) != 0) {
        *state = 0x8000;
        active = 1;
    }

    timer = *state;
    timer &= 0x7F;
    timer = timer * menu_get_event_speed();
    active_bit = active;
    if (active_bit != 0) {
        if (timer >= 0x25) {
            timer = 0x24;
        }
    } else {
        if (timer >= 0x2F) {
            timer = 0;
        }
    }

    active_bit = active;
    if (active_bit != 0) {
        for (i = 0; g_menu_cursor_reverse_offset_keyframes[i] < timer; i += 2) { }
        raw_offset = g_menu_cursor_reverse_offset_keyframes[i + 1];
    } else {
        for (i = 0; g_menu_cursor_forward_offset_keyframes[i] < timer; i += 2) { }
        raw_offset = g_menu_cursor_forward_offset_keyframes[i + 1];
    }

    y_offset = raw_offset;
    timer += menu_get_event_speed();
    active_bit = active;
    if (active_bit != 0) {
        if (timer < 0x25) {
            (*state)++;
        }
    } else {
        if (timer < 0x2F) {
            (*state)++;
        } else {
            *state = 0;
        }
    }

    if (mode != 0) {
        front_clut = g_menu_cursor_mode1_foreground_clut;
        shadow_clut = g_menu_cursor_mode1_background_clut;
    } else {
        front_clut = g_menu_cursor_mode0_foreground_clut;
        shadow_clut = g_menu_cursor_mode0_background_clut;
    }

    bottom = y_offset + 0x10;
    setXY4(&g_unit_list_bouncing_cursor_quad, pos->x, y_offset + pos->y, pos->x + 0x10, y_offset + pos->y, pos->x,
        pos->y + bottom, pos->x + 0x10, pos->y + bottom);
    g_unit_list_bouncing_cursor_quad.clut = front_clut;
    g_unit_list_bouncing_cursor_quad.tpage = g_menu_cursor_texture_page;
    gfx_enqueue_textured_quad_corners(&g_unit_list_bouncing_cursor_quad, 0, 0, 0x3C);
    setXY4(&g_unit_list_bouncing_cursor_quad, pos->x, y_offset + pos->y + 2, pos->x + 0x10, y_offset + pos->y + 2,
        pos->x, pos->y + bottom + 2, pos->x + 0x10, pos->y + bottom + 2);
    g_unit_list_bouncing_cursor_quad.clut = shadow_clut;
    g_unit_list_bouncing_cursor_quad.tpage = 0x5F;
    gfx_enqueue_textured_quad_corners(&g_unit_list_bouncing_cursor_quad, 0, 1, 0x3B);
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
        if (script->header.opcode == UNIT_LIST_SCRIPT_END) {
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

/* Set up a scrollable list at a scroll base and draw it with the given menu script. */
void menu_init_and_draw_scrollable_list(
    const s16* entries, s32 selected_index, s32 scroll_base_index, u8* text_table, const menu_instruction_t* script) {
    menu_init_scrollable_list(entries, selected_index, scroll_base_index, text_table);
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
 * Uses the event speed and a sliding cursor. The padding arrays retain the
 * original stack frame.
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
    speed = menu_get_event_speed();
    g_menu_list_row_mode = 1;
    if (g_menu_list_redraw_pending != 0) {
        menu_draw_scrollable_list(command);
    }
    previous_selected = g_menu_list_selected_index;
    if (g_menu_list_scroll_direction == 0) {
        j = menu_input_read_page_scroll_direction();
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
        menu_update_and_draw_sliding_cursor(&marker, &g_menu_list_cursor_anim_state, g_menu_use_alternate_palette);
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
