/* Rendering a text entry's glyphs into a menu image, with its substitution, variable, palette and width codes. */

#include "fft/gfx.h"
#include "fft/party.h"
#include "fft/script_variable.h"
#include "fft/text.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/types.h"

/* Renders a text entry's glyphs into a menu image at the pen position.
 *
 * The thread target installed by menu_display_text_entry. A `text` of 0 or 1
 * selects text_id's entry; any other value is the text itself.
 * Handles the substitution, script-variable, palette, width and numeric
 * control codes. Only lines g_menu_text_param_22 up to, not including,
 * g_menu_text_param_24 are drawn; earlier lines still advance the pen
 * horizontally, and reaching the end line stops rendering. Both line limits
 * and the text palette are reset on exit. Rendering also stops when the pen
 * comes within 8 pixels of the image's right edge.
 *
 * Reading each control operand through the byte local and the u8
 * variable_base keep the outer loop large enough that loop.c does not hoist
 * the 0xFFFF pending-width constant, as in the target. */
void menu_display_text(s32 text_id, u8* image, glyph_blit_t* origin, const u8* text) {
    text_reader_state_t* state;
    const u8* cursor;
    u8 operand;
    s32 width;
    u32* divisor;
    u32* powers;
    glyph_blit_t* pen;
    s32 digit;
    s32 started;
    s32 line;
    u8 variable_base;

    /* The result is unused; WORLD's body copies the player's name from roster slot 0 here. */
    party_get_unit(0);

    if (text == 0 || text == (const u8*)1) {
        cursor = text_find_entry(text_id);
    } else {
        cursor = text;
    }

    /* The renderer uses the scratchpad block after the text measurer. */
    state = (text_reader_state_t*)getScratchAddr(128);
    powers = g_text_decimal_divisors;
    variable_base = EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION;
    state->return_cursor = (const u8*)-1;
    state->value_cursor = g_text_substitution_values;
    state->pending_width = 0xFFFF;
    state->remaining_bytes = 0;
    state->column = 0;
    state->max_column = 0;
    state->row = 0;

    g_menu_text_pen.rect.x = origin->rect.x;
    pen = &g_menu_text_pen;
    line = 0;
    g_text_glyph_first_row = 0;
    g_menu_text_pen.rect.y = origin->rect.y;
    g_text_glyph_row_limit = 0x10;
    g_menu_text_pen.stride = origin->stride;

    while (1) {
        state->command = *cursor;
        if (state->command >= TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
            if ((state->command & 0xF0) == TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                state->command = (*cursor & 0xF) * TEXT_GLYPHS_PER_PREFIX;
                cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                state->command = *cursor + state->command;
            } else {
                if (state->command == TEXT_SET_GLYPH_DELAY) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                } else if (state->command == TEXT_SET_PORTRAIT) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    operand = *cursor;
                    script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, operand);
                } else if (state->command == TEXT_SET_VARIABLE_BASE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    operand = *cursor;
                    variable_base = operand;
                } else if (state->command == TEXT_STORE_VARIABLE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    digit = script_get_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
                    if (digit != 0) {
                        script_set_variable(variable_base + digit * 10, *cursor);
                        script_set_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
                    } else {
                        script_set_variable(variable_base, *cursor);
                    }
                } else if (state->command == TEXT_SET_PALETTE) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    operand = *cursor;
                    g_menu_text_palette_offset = operand;
                } else if (state->command == TEXT_CHOICE_MARK) {
                } else if (state->command == TEXT_SPACE) {
                    pen->rect.x += 4;
                } else if (state->command == TEXT_NEWLINE) {
                    pen->rect.x = origin->rect.x;
                    /* The pen moves down only from the first shown line; earlier lines are not drawn. */
                    if (line >= g_menu_text_first_line) {
                        g_text_glyph_first_row = 0;
                        pen->rect.y += 16;
                    }
                    line++;
                } else if (state->command == TEXT_SET_NUMBER_WIDTH) {
                    cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                    operand = *cursor;
                    state->pending_width = operand;
                } else if (state->command == TEXT_PRINT_NEXT_VALUE || state->command == TEXT_PRINT_INDEXED_VALUE) {
                    if (state->command == TEXT_PRINT_NEXT_VALUE) {
                        state->value = *state->value_cursor;
                        state->value_cursor++;
                    } else {
                        cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                        operand = *cursor;
                        state->value = g_text_substitution_values[operand];
                    }

                    width = state->pending_width;
                    if (width == 0xFFFF) {
                        started = 0;
                        digit = text_count_decimal_digits(state->value) - 1;
                    } else {
                        state->pending_width = 0xFFFF;
                        digit = width - 1;
                        started = 0;
                    }

                    if (digit >= 0) {
                        /* Byte arithmetic on the table address; `&powers[digit]` compiles differently. */
                        divisor = (u32*)(digit * 4 + (s32)powers);
                        do {
                            /* Leading zeros draw nothing but still advance the pen. */
                            if (state->value >= *divisor || digit == 0 || started != 0) {
                                state->command = state->value / *divisor;
                                started = 1;
                                state->value = state->value - state->command * *divisor;
                                if (line >= g_menu_text_line_limit) {
                                    goto done;
                                }
                                if (line >= g_menu_text_first_line) {
                                    text_render_glyph_to_4bpp_image(
                                        g_text_glyph_bitmap_data + state->command * TEXT_GLYPH_BYTES, image,
                                        &g_menu_text_pen, g_menu_text_palette_offset);
                                }
                            }
                            divisor--;
                            digit--;
                            pen->rect.x += 6;
                        } while (digit >= 0);
                    }
                }

                /* Every control code then reaches the substitution, back-reference and end checks. */
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

                /* A back-reference (0xf0-0xf3) replays earlier text. Its three bytes pack a length of
                 * 4 to 35 bytes and a distance back of up to 15 * 254 + 255; reading resumes after
                 * the code once the copy is used up. */
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

                /* 0xFE and 0xFF end the entry, or return from a substituted one. */
                if ((state->command & 0xFE) == 0xFE) {
                    if (state->return_cursor == (const u8*)-1) {
                        break;
                    }
                    cursor = state->return_cursor;
                    state->return_cursor = (const u8*)-1;
                    continue;
                }

                cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
                continue;
            }
        }

        cursor = menu_text_advance_cursor_with_backreferences(state, cursor);
        if (line >= g_menu_text_line_limit) {
            break;
        }
        if (line >= g_menu_text_first_line) {
            text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + state->command * TEXT_GLYPH_BYTES, image,
                &g_menu_text_pen, g_menu_text_palette_offset);
        }

        pen->rect.x += g_text_glyph_widths[state->command];
        if (pen->rect.x >= pen->stride - 8) {
            break;
        }
    }

done:
    g_menu_text_palette_offset = 0;
    g_menu_text_first_line = 0;
    g_menu_text_line_limit = 0xFF;
}
