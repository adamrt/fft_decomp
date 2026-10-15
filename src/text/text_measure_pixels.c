#include "fft/main.h"
#include "fft/menu.h"
#include "psx/libetc.h"

/* Measure a text entry in pixels, recording each line's width and returning
 * the widest line and line count. Spaces are 4 pixels and numeric digits 6;
 * other glyphs use their prefix page's width table. */
void text_measure_pixels(s16* width, s16* rows, const u8* text) {
    text_backref_t* backref;
    party_unit_t* party;
    u8* cursor;
    s32 i;
    u8* fill = g_text_player_name_buffer;
    u8 terminator;
    text_reader_state_t* state;

    cursor = (u8*)text;
    party = main_party_get_unit(0);
    terminator = TEXT_END_WAIT_FOR_CONFIRM;
    for (i = 19; i >= 0; i--) {
        fill[i] = terminator;
    }
    for (i = 0; i < 16; i++) {
        g_text_player_name_buffer[i] = party->name[i];
    }
    state = (text_reader_state_t*)getScratchAddr(0);
    state->return_cursor = (u8*)-1;
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
                if (state->return_cursor == (u8*)-1) {
                    state->column = 0;
                    state->row++;
                    break;
                }
                cursor = state->return_cursor;
                state->return_cursor = (u8*)-1;
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
                state->column += text_count_decimal_digits(state->value) * 6;
            } else {
                state->column += state->pending_width * 6;
                state->pending_width = 0xFFFF;
            }
            state->value_cursor++;
        } else if (state->command == TEXT_PRINT_INDEXED_VALUE) {
            cursor = text_advance_cursor_with_backreferences(state, cursor);
            state->value = g_text_substitution_values[*cursor];
            if (state->pending_width == 0xFFFF) {
                state->column += text_count_decimal_digits(state->value) * 6;
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
        state->value = text_resolve_format_string_id(state->command, *state->value_cursor);
        if (state->value != -1) {
            state->return_cursor = cursor + 1;
            if (state->command == TEXT_FORMAT_RAMZA_NAME) {
                cursor = g_text_player_name_buffer;
            } else {
                cursor = text_find_entry(state->value);
            }
            state->value_cursor++;
            continue;
        }
        if ((state->command & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (state->command & 0xF) < 4) {
            state->return_cursor = cursor + 3;
            backref = (text_backref_t*)cursor;
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
