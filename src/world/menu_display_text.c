/* Rendering a text entry glyph by glyph into a menu window image, with its commands and numbers. */

#include "fft/gfx.h"
#include "fft/main_unit.h"
#include "fft/menu_text.h"
#include "fft/script_variable.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Render a WORLD text entry glyph by glyph into a menu window image.
 *
 * `text` of 0 or 1 selects `text_id` from the text tables; 1 also lets the
 * 0xFB command continue instead of ending the text. Walks the same command
 * stream as text_measure, drawing lines from g_menu_text_param_22
 * up to g_menu_text_param_24 and stopping at the window's right margin.
 * Numbers (0xE4/0xE6) are drawn one digit per 6-pixel column. Indexing the
 * divisor table inside the digit loop, rather than walking a pointer, is
 * what gives the target's strength-reduced pointer and register choice. */
void menu_display_text(s32 text_id, u8* image, glyph_blit_t* origin, u8* text) {
    party_unit_t* party;
    u8* cursor;
    s32 i;
    u8* fill = g_text_player_name_buffer;
    u8 terminator;
    text_reader_state_t* state;
    s32 variable_id;
    s32 line;
    s32 started;

    /* Refresh the player name buffer that the Ramza format code reads: end bytes, then party unit 0's name. */
    party = main_party_get_unit(0);
    terminator = TEXT_END_WAIT_FOR_CONFIRM;
    for (i = 19; i >= 0; i--) {
        fill[i] = terminator;
    }
    for (i = 0; i < UNIT_NAME_SIZE; i++) {
        g_text_player_name_buffer[i] = party->name[i];
    }

    if (text == 0 || text == (u8*)1) {
        cursor = text_find_entry(text_id);
    } else {
        cursor = text;
    }

    state = (text_reader_state_t*)getScratchAddr(128);
    variable_id = EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION;
    state->return_cursor = (u8*)-1;
    state->value_cursor = g_text_substitution_values;
    state->pending_width = 0xFFFF;
    state->remaining_bytes = 0;
    state->column = 0;
    state->max_column = 0;
    state->row = 0;

    g_menu_text_pen.rect.x = origin->rect.x;
    g_text_glyph_first_row = 0;
    g_menu_text_pen.rect.y = origin->rect.y;
    g_text_glyph_row_limit = 0x10;
    g_menu_text_pen.stride = origin->stride;
    line = 0;

    while (1) {
        state->command = *cursor;
        if (state->command >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
            /* A two-byte glyph: the prefix's low nibble selects a page of glyphs. */
            if ((state->command & 0xF0) == TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                state->command = (*cursor & 0xF) * TEXT_GLYPHS_PER_PREFIX;
                cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                state->command = *cursor + state->command;
            } else {
                if (state->command == TEXT_SET_GLYPH_DELAY) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                } else if (state->command == TEXT_SET_PORTRAIT) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, *cursor);
                } else if (state->command == TEXT_SET_VARIABLE_BASE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    variable_id = *cursor;
                } else if (state->command == TEXT_STORE_VARIABLE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    i = script_get_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
                    if (i != 0) {
                        script_set_variable(variable_id + i * 10, *cursor);
                        script_set_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
                    } else {
                        script_set_variable(variable_id, *cursor);
                    }
                } else if (state->command == TEXT_SET_PALETTE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    g_menu_text_palette_offset = *cursor;
                } else if (state->command == TEXT_CHOICE_MARK) {
                    /* The text ends at the choice mark unless the caller passed text 1. */
                    if (text != (u8*)1) {
                        break;
                    }
                } else if (state->command == TEXT_SPACE) {
                    g_menu_text_pen.rect.x += 4;
                } else if (state->command == TEXT_NEWLINE) {
                    /* Lines before g_menu_text_param_22 are skipped, so only drawn lines move the pen down. */
                    g_menu_text_pen.rect.x = origin->rect.x;
                    if (line >= g_menu_text_param_22) {
                        g_text_glyph_first_row = 0;
                        g_menu_text_pen.rect.y += 0x10;
                    }
                    line++;
                } else if (state->command == TEXT_SET_NUMBER_WIDTH) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    state->pending_width = *cursor;
                } else if (state->command == TEXT_PRINT_NEXT_VALUE || state->command == TEXT_PRINT_INDEXED_VALUE) {
                    if (state->command == TEXT_PRINT_NEXT_VALUE) {
                        state->value = *state->value_cursor;
                        state->value_cursor++;
                    } else {
                        cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                        state->value = g_text_substitution_values[*cursor];
                    }

                    /* Print in the pending width when one was set, padding leading zeros with blank columns. */
                    i = state->pending_width - 1;
                    if (state->pending_width == 0xFFFF) {
                        started = 0;
                        i = text_count_decimal_digits(state->value) - 1;
                    } else {
                        state->pending_width = 0xFFFF;
                        started = 0;
                    }
                    for (; i >= 0; i--) {
                        if (state->value >= g_text_decimal_divisors[i] || i == 0 || started != 0) {
                            state->command = state->value / g_text_decimal_divisors[i];
                            started = 1;
                            state->value = state->value - state->command * g_text_decimal_divisors[i];
                            if (line >= g_menu_text_param_24) {
                                /* Leaves the digit loop and the text loop. */
                                goto done;
                            }
                            if (line >= g_menu_text_param_22) {
                                text_render_glyph_to_4bpp_image(
                                    g_text_glyph_bitmap_data + state->command * TEXT_GLYPH_BYTES, image,
                                    &g_menu_text_pen, g_menu_text_palette_offset);
                            }
                        }
                        g_menu_text_pen.rect.x += 6;
                    }
                }

                /* Every command above also reaches these tests; only a format code, a backreference
                 * or an end byte matches one. */
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

                /* Compressed text: replay remaining_bytes from earlier in the entry, then resume after it. */
                if ((state->command & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (state->command & 0xF) < 4) {
                    state->return_cursor = cursor + 3;
                    state->scratch = (cursor[0] & 3) * 8;
                    state->remaining_bytes = cursor[1] >> 5;
                    state->remaining_bytes = state->remaining_bytes + state->scratch + 4;
                    state->scratch = (cursor[1] & 0xF) * 0xFE;
                    state->scratch = cursor[2] + state->scratch;
                    cursor -= state->scratch;
                    continue;
                }

                /* 0xFE and 0xFF end the text, or return from a substituted string. */
                if ((state->command & 0xFE) == 0xFE) {
                    if (state->return_cursor == (u8*)-1) {
                        break;
                    }
                    cursor = state->return_cursor;
                    state->return_cursor = (u8*)-1;
                    continue;
                }
                cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                continue;
            }
        }

        /* A glyph: draw it inside the visible lines, and stop at the window's right margin. */
        cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
        if (line >= g_menu_text_param_24) {
            break;
        }
        if (line >= g_menu_text_param_22) {
            text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + state->command * TEXT_GLYPH_BYTES, image,
                &g_menu_text_pen, g_menu_text_palette_offset);
        }
        g_menu_text_pen.rect.x += g_text_glyph_widths[state->command];
        if (g_menu_text_pen.rect.x >= g_menu_text_pen.stride - 8) {
            break;
        }
    }

    /* Reset the palette and the visible line range for the next caller. */
done:
    g_menu_text_palette_offset = 0;
    g_menu_text_param_22 = 0;
    g_menu_text_param_24 = 0xFF;
}
