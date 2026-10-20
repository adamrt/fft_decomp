/* Resolve text substitutions, measure formatted lines and render dialog record images. */

#include "fft/menu.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/party.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Convert a text-format substitution opcode into its string-table ID. */
s32 text_format_id_resolve(s32 format_code, s32 value) {
    if (format_code == TEXT_FORMAT_RAMZA_NAME) {
        return TEXT_ID_UNIT_NAME_BASE;
    }
    if (format_code == TEXT_FORMAT_UNIT_NAME) {
        return value + TEXT_ID_UNIT_NAME_BASE;
    }
    if (format_code == TEXT_FORMAT_LOCATION_NAME) {
        return value + TEXT_ID_LOCATION_NAME_BASE;
    }
    if (format_code == TEXT_FORMAT_ITEM_NAME) {
        return value + TEXT_ID_ITEM_NAME_BASE;
    }
    if (format_code == TEXT_FORMAT_ABILITY_NAME) {
        return value + TEXT_ID_ABILITY_NAME_BASE;
    }
    if (format_code == TEXT_FORMAT_EXPLICIT_ID) {
        return value;
    }
    return -1;
}

/* Measure a text entry in characters, following substitutions and backreferences: the widest
 * line's column count goes to *columns and the line count to *rows. Spaces count one column
 * and numeric values their digit count (or the pending number width). */
void text_measure(s16* columns, s16* rows, const u8* text) {
    const text_backref_t* backref;
    party_unit_t* party;
    const u8* cursor;
    s32 i;
    u8* fill = g_text_player_name_buffer;
    u8 terminator;
    text_reader_state_t* state;

    /* Stage the first party unit's name (Ramza) for TEXT_FORMAT_RAMZA_NAME, in a buffer of
     * terminators. */
    cursor = text;
    party = party_unit_get(0);
    terminator = TEXT_END_WAIT_FOR_CONFIRM;
    for (i = 19; i >= 0; i--) {
        fill[i] = terminator;
    }
    for (i = 0; i < UNIT_NAME_SIZE; i++) {
        g_text_player_name_buffer[i] = party->name[i];
    }
    state = (text_reader_state_t*)getScratchAddr(0);
    state->return_cursor = (const u8*)-1;
    state->value_cursor = g_text_substitution_values;
    state->remaining_bytes = 0;
    state->column = 0;
    state->max_column = 0;
    state->row = 0;
    state->pending_width = 0xFFFF;

    while (1) {
        state->command = *cursor;
        if (state->command == TEXT_NEWLINE || state->command == TEXT_END_WAIT_FOR_CONFIRM
            || state->command == TEXT_END_AUTO_CLOSE) {
            if (state->max_column < state->column) {
                state->max_column = state->column;
            }
            if (state->command == TEXT_END_WAIT_FOR_CONFIRM || state->command == TEXT_END_AUTO_CLOSE) {
                if (state->return_cursor == (const u8*)-1) {
                    state->column = 0;
                    state->row++;
                    break;
                }
                cursor = state->return_cursor;
                state->return_cursor = (const u8*)-1;
                continue;
            }
            state->column = 0;
            state->row++;
        } else if (state->command == TEXT_SPACE) {
            state->column++;
        } else if (state->command == TEXT_SET_NUMBER_WIDTH) {
            cursor = text_advance_cursor_with_backreferences(state, cursor);
            state->pending_width = *cursor;
        } else if (state->command == TEXT_PRINT_NEXT_VALUE) {
            state->value = *state->value_cursor;
            if (state->pending_width == 0xFFFF) {
                state->column += text_decimal_digits_count(state->value);
            } else {
                state->column += state->pending_width;
                state->pending_width = 0xFFFF;
            }
            state->value_cursor++;
        } else if (state->command == TEXT_PRINT_INDEXED_VALUE) {
            cursor = text_advance_cursor_with_backreferences(state, cursor);
            state->value = g_text_substitution_values[*cursor];
            if (state->pending_width == 0xFFFF) {
                state->column += text_decimal_digits_count(state->value);
            } else {
                state->column += state->pending_width;
                state->pending_width = 0xFFFF;
            }
        } else if (state->command == TEXT_SET_GLYPH_DELAY || state->command == TEXT_SET_PALETTE
            || state->command == TEXT_SET_VARIABLE_BASE || state->command == TEXT_STORE_VARIABLE
            || state->command == TEXT_SET_PORTRAIT) {
            cursor = text_advance_cursor_with_backreferences(
                state, text_advance_cursor_with_backreferences(state, cursor));
            continue;
        }
        state->value = text_format_id_resolve(state->command, *state->value_cursor);
        if (state->value != -1) {
            state->return_cursor = cursor + 1;
            if (state->command == TEXT_FORMAT_RAMZA_NAME) {
                cursor = g_text_player_name_buffer;
            } else {
                cursor = text_entry_find(state->value);
            }
            state->value_cursor++;
            continue;
        }
        if ((state->command & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (state->command & 0xF) < 4) {
            state->return_cursor = cursor + 3;
            backref = (const text_backref_t*)cursor;
            state->scratch = backref->length_high * 8;
            state->remaining_bytes = backref->length_low;
            state->remaining_bytes = state->remaining_bytes + state->scratch + 4;
            state->scratch = backref->offset_high * 0xFE;
            state->scratch = backref->offset_low + state->scratch;
            cursor -= state->scratch;
            continue;
        }
        if ((state->command & 0xF0) < TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
            state->column++;
        }
        cursor = text_advance_cursor_with_backreferences(state, cursor);
    }
    *columns = state->max_column;
    *rows = state->row;
}

/* Measure a text entry in pixels, recording each line's width and returning
 * the widest line and line count. Spaces are 4 pixels and numeric digits 6;
 * other glyphs use their prefix page's width table. */
void text_pixels_measure(s16* width, s16* rows, const u8* text) {
    const text_backref_t* backref;
    party_unit_t* party;
    const u8* cursor;
    s32 i;
    u8* fill = g_text_player_name_buffer;
    u8 terminator;
    text_reader_state_t* state;

    /* Stage the first party unit's name (Ramza) for TEXT_FORMAT_RAMZA_NAME, in a buffer of
     * terminators. */
    cursor = text;
    party = party_unit_get(0);
    terminator = TEXT_END_WAIT_FOR_CONFIRM;
    for (i = 19; i >= 0; i--) {
        fill[i] = terminator;
    }
    for (i = 0; i < UNIT_NAME_SIZE; i++) {
        g_text_player_name_buffer[i] = party->name[i];
    }
    state = (text_reader_state_t*)getScratchAddr(0);
    state->return_cursor = (const u8*)-1;
    state->value_cursor = g_text_substitution_values;
    state->remaining_bytes = 0;
    state->column = 0;
    state->max_column = 0;
    state->row = 0;
    state->pending_width = 0xFFFF;
    state->glyph_page = 0;

    while (1) {
        state->command = *cursor;
        if (state->command == TEXT_NEWLINE || state->command == TEXT_END_WAIT_FOR_CONFIRM
            || state->command == TEXT_END_AUTO_CLOSE) {
            g_text_line_widths[state->row] = state->column;
            if (state->max_column < state->column) {
                state->max_column = state->column;
            }
            if (state->command == TEXT_END_WAIT_FOR_CONFIRM || state->command == TEXT_END_AUTO_CLOSE) {
                if (state->return_cursor == (const u8*)-1) {
                    state->column = 0;
                    state->row++;
                    break;
                }
                cursor = state->return_cursor;
                state->return_cursor = (const u8*)-1;
                continue;
            }
            state->column = 0;
            state->row++;
        } else if (state->command == TEXT_SPACE) {
            state->column += 4;
        } else if (state->command == TEXT_SET_NUMBER_WIDTH) {
            cursor = text_advance_cursor_with_backreferences(state, cursor);
            state->pending_width = *cursor;
        } else if (state->command == TEXT_PRINT_NEXT_VALUE) {
            state->value = *state->value_cursor;
            if (state->pending_width == 0xFFFF) {
                state->column += text_decimal_digits_count(state->value) * 6;
            } else {
                state->column += state->pending_width * 6;
                state->pending_width = 0xFFFF;
            }
            state->value_cursor++;
        } else if (state->command == TEXT_PRINT_INDEXED_VALUE) {
            cursor = text_advance_cursor_with_backreferences(state, cursor);
            state->value = g_text_substitution_values[*cursor];
            if (state->pending_width == 0xFFFF) {
                state->column += text_decimal_digits_count(state->value) * 6;
            } else {
                state->column += state->pending_width * 6;
                state->pending_width = 0xFFFF;
            }
        } else if (state->command == TEXT_SET_GLYPH_DELAY || state->command == TEXT_SET_PALETTE
            || state->command == TEXT_SET_VARIABLE_BASE || state->command == TEXT_STORE_VARIABLE
            || state->command == TEXT_SET_PORTRAIT) {
            cursor = text_advance_cursor_with_backreferences(
                state, text_advance_cursor_with_backreferences(state, cursor));
            continue;
        }
        state->value = text_format_id_resolve(state->command, *state->value_cursor);
        if (state->value != -1) {
            state->return_cursor = cursor + 1;
            if (state->command == TEXT_FORMAT_RAMZA_NAME) {
                cursor = g_text_player_name_buffer;
            } else {
                cursor = text_entry_find(state->value);
            }
            state->value_cursor++;
            continue;
        }
        if ((state->command & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (state->command & 0xF) < 4) {
            state->return_cursor = cursor + 3;
            backref = (const text_backref_t*)cursor;
            state->scratch = backref->length_high * 8;
            state->remaining_bytes = backref->length_low;
            state->remaining_bytes = state->remaining_bytes + state->scratch + 4;
            state->scratch = backref->offset_high * 0xFE;
            state->scratch = backref->offset_low + state->scratch;
            cursor -= state->scratch;
            continue;
        }
        if ((state->command & 0xF0) >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
            if ((state->command & 0xF0) == TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                state->glyph_page = state->command;
            } else {
                state->glyph_page = 0;
            }
        } else {
            state->column += g_text_glyph_widths[(state->glyph_page & 0xF) * TEXT_GLYPHS_PER_PREFIX + state->command];
            state->glyph_page = 0;
        }
        cursor = text_advance_cursor_with_backreferences(state, cursor);
    }
    *width = state->max_column;
    *rows = state->row;
}

/* Render a text record into a freshly built window image at the record's own
 * origin, upload it to the record's rectangle and free the buffer.
 *
 * A portrait dialog gets the window with its tail, every other type the plain
 * frame; a centered box is skipped. The thread yields once between the upload and
 * the free. */
void message_record_image_render(text_dialog_record_t* record) {
    if (record->box_type != DIALOG_BOX_TYPE_CENTERED) {
        if (record->box_type == DIALOG_BOX_TYPE_PORTRAIT) {
            record->buffer = menu_build_window_image(
                record->width, record->height, &record->rect, record->dialog_type, record->tail_offset, 1);
        } else {
            record->buffer = menu_build_and_upload_window_frame_image(
                record->width, record->height, &record->rect, MENU_WINDOW_FRAME_BUILD);
        }
        g_menu_window_text_pen.rect.x = record->origin_x;
        g_menu_window_text_pen.rect.y = record->origin_y;
        g_menu_window_text_pen.stride = record->rect.w * 4;
        g_menu_text_first_line = record->first_line;
        g_menu_text_line_limit = record->last_line;
        menu_display_text_entry(record->text, record->buffer, &g_menu_window_text_pen);
        LoadImage(&record->rect, record->buffer);
        thread_yield();
        menu_free_memory(record->buffer);
    }
}
