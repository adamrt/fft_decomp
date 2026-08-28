#include "fft/battle.h"
#include "fft/battle_text.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/script_variables.h"
#include "fft/text.h"
#include "fft/world.h"
#include "psx/types.h"

/* Text-stream state in the scratchpad, 0x200 past the one used by the
 * BATTLE text measurers. */
#define TEXT_STATE ((world_text_backreference_state_t*)0x1F800200)

extern battle_text_pen_t g_battle_menu_text_pen;
extern u8* battle_text_find_next_character(world_text_backreference_state_t* reader, u8* p);

#define TEXT_NEXT(state, cursor) battle_text_find_next_character((state), (cursor))

/* Renders a text entry's glyphs into a menu image at the pen position.
 *
 * The thread target installed by battle_menu_display_text_entry. text 0 or 1
 * selects text_id's entry.
 * Handles the substitution, script-variable, palette, width and numeric
 * control codes; glyph drawing is limited to the characters counted from
 * g_menu_text_param_22 up to g_menu_text_param_24, which are reset on exit.
 *
 * Reading each control operand through the byte local and the u8
 * variable_base keep the outer loop large enough that loop.c does not hoist
 * the 0xFFFF pending-width constant, as in the target. */
void battle_menu_display_text(u32 text_id, s32 image, battle_text_pen_t* origin, u8* text) {
    world_text_backreference_state_t* state;
    u8* cursor;
    u8 operand;
    s32 width;
    u32* divisor;
    u32* powers;
    battle_text_pen_t* pen;
    s32 digit;
    s32 started;
    s32 count;
    u8 variable_base;

    main_party_get_data_pointer(0);
    if (text == (u8*)0 || text == (u8*)1) {
        cursor = battle_text_init_entry(text_id);
    } else {
        cursor = text;
    }
    state = TEXT_STATE;
    powers = g_battle_text_decimal_divisors;
    variable_base = 0x18;
    TEXT_STATE->return_cursor = (u8*)-1;
    TEXT_STATE->value_cursor = g_battle_text_substitution_values;
    TEXT_STATE->pending_width = 0xFFFF;
    TEXT_STATE->remaining_bytes = 0;
    TEXT_STATE->column = 0;
    TEXT_STATE->max_column = 0;
    TEXT_STATE->row = 0;
    g_battle_menu_text_pen.x = origin->x;
    pen = &g_battle_menu_text_pen;
    count = 0;
    g_battle_text_substitution_value_27 = 0;
    g_battle_menu_text_pen.y = origin->y;
    g_battle_text_substitution_value_28 = 0x10;
    g_battle_menu_text_pen.right_limit = origin->right_limit;

    while (1) {
        state->command = *cursor;
        if (state->command >= 0xD0) {
            if ((state->command & 0xF0) == 0xD0) {
                state->command = (*cursor & 0xF) * 0xD0;
                cursor = TEXT_NEXT(state, cursor);
                state->command = *cursor + state->command;
            } else {
                if (state->command == TEXT_SET_GLYPH_COLOR) {
                    cursor = TEXT_NEXT(state, cursor);
                } else if (state->command == TEXT_SET_PORTRAIT) {
                    cursor = TEXT_NEXT(state, cursor);
                    operand = *cursor;
                    battle_script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, operand);
                } else if (state->command == TEXT_SET_VARIABLE_BASE) {
                    cursor = TEXT_NEXT(state, cursor);
                    operand = *cursor;
                    variable_base = operand;
                } else if (state->command == TEXT_STORE_VARIABLE) {
                    cursor = TEXT_NEXT(state, cursor);
                    digit = battle_script_get_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
                    if (digit != 0) {
                        battle_script_set_variable(variable_base + digit * 10, *cursor);
                        battle_script_set_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
                    } else {
                        battle_script_set_variable(variable_base, *cursor);
                    }
                } else if (state->command == TEXT_SET_PALETTE) {
                    cursor = TEXT_NEXT(state, cursor);
                    operand = *cursor;
                    g_menu_text_palette_offset = operand;
                } else if (state->command == TEXT_CHOICE_MARK) {
                } else if (state->command == TEXT_SPACE) {
                    pen->x += 4;
                } else if (state->command == TEXT_NEWLINE) {
                    pen->x = origin->x;
                    if (count >= g_menu_text_param_22) {
                        g_battle_text_substitution_value_27 = 0;
                        pen->y += 16;
                    }
                    count++;
                } else if (state->command == TEXT_SET_NUMBER_WIDTH) {
                    cursor = TEXT_NEXT(state, cursor);
                    operand = *cursor;
                    state->pending_width = operand;
                } else if (state->command == TEXT_PRINT_NEXT_VALUE || state->command == TEXT_PRINT_INDEXED_VALUE) {
                    if (state->command == TEXT_PRINT_NEXT_VALUE) {
                        state->value = *state->value_cursor;
                        state->value_cursor++;
                    } else {
                        cursor = TEXT_NEXT(state, cursor);
                        operand = *cursor;
                        state->value = g_battle_text_substitution_values[operand];
                    }
                    width = state->pending_width;
                    if (width == 0xFFFF) {
                        started = 0;
                        digit = battle_text_count_decimal_digits(state->value) - 1;
                    } else {
                        state->pending_width = 0xFFFF;
                        digit = width - 1;
                        started = 0;
                    }
                    if (digit >= 0) {
                        divisor = (u32*)(digit * 4 + (s32)powers);
                        do {
                            if ((u32)state->value >= *divisor || digit == 0 || started != 0) {
                                state->command = (u32)state->value / *divisor;
                                started = 1;
                                state->value = state->value - state->command * *divisor;
                                if (count >= g_menu_text_param_24) {
                                    goto done;
                                }
                                if (count >= g_menu_text_param_22) {
                                    battle_text_render_glyph_to_4bpp_image(
                                        g_text_glyph_bitmap_data + state->command * 35, image,
                                        &g_battle_menu_text_pen.x, g_menu_text_palette_offset);
                                }
                            }
                            divisor--;
                            digit--;
                            pen->x += 6;
                        } while (digit >= 0);
                    }
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
                if ((state->command & 0xFE) == 0xFE) {
                    if (state->return_cursor == (u8*)-1) {
                        break;
                    }
                    cursor = state->return_cursor;
                    state->return_cursor = (u8*)-1;
                    continue;
                }
                cursor = TEXT_NEXT(state, cursor);
                continue;
            }
        }
        cursor = TEXT_NEXT(state, cursor);
        if (count >= g_menu_text_param_24) {
            break;
        }
        if (count >= g_menu_text_param_22) {
            battle_text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + state->command * 35, image,
                &g_battle_menu_text_pen.x, g_menu_text_palette_offset);
        }
        pen->x += g_text_glyph_widths[state->command];
        if ((s16)pen->x >= pen->right_limit - 8) {
            break;
        }
    }
done:
    g_menu_text_palette_offset = 0;
    g_menu_text_param_22 = 0;
    g_menu_text_param_24 = 0xFF;
}
