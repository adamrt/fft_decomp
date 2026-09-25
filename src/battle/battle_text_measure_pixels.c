#include "fft/battle.h"
#include "psx/types.h"

#define TEXT_STATE ((world_text_backreference_state_t*)0x1F800000)

struct battle_text_backreference_state;
u8* battle_text_advance_cursor_with_backreferences(struct battle_text_backreference_state* state, u8* cursor);

/* BATTLE twin of world_text_measure_pixels. Measure a text entry in pixels: the widest line and the line count.
 *
 * Pixel-width sibling of world_measure_text. Spaces (0xFA) are 4 pixels,
 * substituted digits 6, and other glyphs come from the per-page width table,
 * where a 0xDx prefix byte selects the page for the following glyph. Each
 * line's width is also recorded in g_battle_text_line_widths. */
void battle_text_measure_pixels(s16* width, s16* rows, const u8* text) {
    party_data_t* party;
    u8* cursor;
    s32 i;
    u8* fill = g_battle_player_name;
    u8 terminator;
    world_text_backreference_state_t* state;

    cursor = (u8*)text;
    party = main_party_get_data_pointer(0);
    terminator = TEXT_END_WAIT_FOR_CONFIRM;
    for (i = 19; i >= 0; i--) {
        fill[i] = terminator;
    }
    for (i = 0; i < 16; i++) {
        g_battle_player_name[i] = party->name[i];
    }
    TEXT_STATE->return_cursor = (u8*)-1;
    TEXT_STATE->value_cursor = g_battle_text_substitution_values;
    TEXT_STATE->remaining_bytes = 0;
    TEXT_STATE->column = 0;
    TEXT_STATE->max_column = 0;
    TEXT_STATE->row = 0;
    TEXT_STATE->pending_width = 0xFFFF;
    TEXT_STATE->glyph_page = 0;

    state = TEXT_STATE;
    while (1) {
        state->command = *cursor;
        if (state->command == TEXT_NEWLINE || state->command == TEXT_END_WAIT_FOR_CONFIRM
            || state->command == TEXT_END_AUTO_CLOSE) {
            g_battle_text_line_widths[state->row] = state->column;
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
            cursor = battle_text_advance_cursor_with_backreferences(
                (struct battle_text_backreference_state*)state, cursor);
            state->pending_width = *cursor;
        } else if (state->command == TEXT_PRINT_NEXT_VALUE) {
            state->value = *state->value_cursor;
            if (state->pending_width == 0xFFFF) {
                state->column += battle_text_count_decimal_digits(state->value) * 6;
            } else {
                state->column += state->pending_width * 6;
                state->pending_width = 0xFFFF;
            }
            state->value_cursor++;
        } else if (state->command == TEXT_PRINT_INDEXED_VALUE) {
            cursor = battle_text_advance_cursor_with_backreferences(
                (struct battle_text_backreference_state*)state, cursor);
            state->value = g_battle_text_substitution_values[*cursor];
            if (state->pending_width == 0xFFFF) {
                state->column += battle_text_count_decimal_digits(state->value) * 6;
            } else {
                state->column += state->pending_width * 6;
                state->pending_width = 0xFFFF;
            }
        } else if ((u32)(state->command - TEXT_SET_GLYPH_COLOR) < 2
            || (u32)(state->command - TEXT_SET_VARIABLE_BASE) < 2 || state->command == TEXT_SET_PORTRAIT) {
            cursor = battle_text_advance_cursor_with_backreferences((struct battle_text_backreference_state*)state,
                battle_text_advance_cursor_with_backreferences((struct battle_text_backreference_state*)state, cursor));
            continue;
        }
        state->value = battle_text_resolve_format_string_id(state->command, *state->value_cursor);
        if (state->value != -1) {
            state->return_cursor = cursor + 1;
            if (state->command == TEXT_FORMAT_RAMZA_NAME) {
                cursor = g_battle_player_name;
            } else {
                cursor = battle_text_init_entry(state->value);
            }
            state->value_cursor++;
            continue;
        }
        if ((state->command & 0xF0) == 0xF0 && (state->command & 0xF) < 4) {
            state->return_cursor = cursor + 3;
            state->scratch = (cursor[0] & 3) * 8;
            state->remaining_bytes = cursor[1] >> 5;
            state->remaining_bytes = state->remaining_bytes + state->scratch + 4;
            state->scratch = (cursor[1] & 0xF) * 0xFE;
            state->scratch = cursor[2] + state->scratch;
            cursor -= state->scratch;
            continue;
        }
        if ((state->command & 0xF0) >= 0xD0) {
            if ((state->command & 0xF0) == 0xD0) {
                state->glyph_page = state->command;
            } else {
                state->glyph_page = 0;
            }
        } else {
            state->column += g_text_glyph_widths[(state->glyph_page & 0xF) * 0xD0 + state->command];
            state->glyph_page = 0;
        }
        cursor = battle_text_advance_cursor_with_backreferences((struct battle_text_backreference_state*)state, cursor);
    }
    *width = state->max_column;
    *rows = state->row;
}
