/* Display message building, dialog input state, thread slot release, typewriter glyph drawing, format string ids and
 * text measurement. */

#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/option.h"
#include "fft/party.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_party.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Typed views for this scanner; each expansion re-reads the event-block base. */
#define DISPLAY_MESSAGE_INSTRUCTION_AT(base, offset) ((event_display_message_instruction_t*)((base) + (offset)))
#define CHANGE_DIALOG_INSTRUCTION_AT(base, offset)   ((event_change_dialog_instruction_t*)((base) + (offset)))

/* Unreferenced word preceding the glyph pen; no consumer establishes a pointer meaning. */
static u32 g_message_typewriter_prefix_word = 0;
static glyph_blit_t g_message_typewriter_pen = { 0 };
static RECT g_message_typewriter_vram_rect = { 0 };

static u8 g_message_typewriter_glyph_images[3][8][0x54] = { 0 };

static u8 g_message_typewriter_previous_images[3][0x54] = { 0 };

static u8 g_message_typewriter_column_scratch[14] = { 0 };

/* For a dialog kept open for ChangeDialog (DIALOG_TYPE_KEEP_OPEN), measure the widest text the
 * window will show: the DisplayMessage at offset and each later ChangeDialog for the same window
 * position, up to the ChangeDialog with text id 0xFFFF that closes it. Stores the width in
 * *out_width; other dialogs leave it alone.
 *
 * Every access re-reads g_script_event_block as retail does; instruction pointer
 * temporaries make the function 16 bytes shorter. */
void message_build_display(s32 offset, s32 flags, s32* out_width) {
    s16 width;
    s16 height;
    s32 max_width;
    s32 next;
    s32 size;

    if (flags & DIALOG_TYPE_KEEP_OPEN) {
        flags &= DIALOG_TYPE_POSITION_MASK;
        text_measure_pixels(&width, &height,
            text_find_entry(((DISPLAY_MESSAGE_INSTRUCTION_AT((u8*)g_script_event_block, offset)->text_id_le[1] << 8)
                                + DISPLAY_MESSAGE_INSTRUCTION_AT((u8*)g_script_event_block, offset)->text_id_le[0])
                - 1));
        max_width = width;
        next = offset + 1;
        size = g_script_event_instruction_sizes[EVENT_OPCODE_DISPLAY_MESSAGE];
        offset = next + size;
        while ((offset = script_find_instruction_byte_offset(offset, EVENT_OPCODE_CHANGE_DIALOG)) != 0) {
            if (CHANGE_DIALOG_INSTRUCTION_AT((u8*)g_script_event_block, offset)->opcode == EVENT_OPCODE_CHANGE_DIALOG) {
                if (CHANGE_DIALOG_INSTRUCTION_AT((u8*)g_script_event_block, offset)->text_id_le[0] == 0xFF
                    && CHANGE_DIALOG_INSTRUCTION_AT((u8*)g_script_event_block, offset)->text_id_le[1] == 0xFF
                    && (CHANGE_DIALOG_INSTRUCTION_AT((u8*)g_script_event_block, offset)->window_selector & flags)
                        == flags) {
                    break;
                }
                if ((CHANGE_DIALOG_INSTRUCTION_AT((u8*)g_script_event_block, offset)->window_selector & flags)
                    == flags) {
                    text_measure_pixels(&width, &height,
                        text_find_entry(
                            ((CHANGE_DIALOG_INSTRUCTION_AT((u8*)g_script_event_block, offset)->text_id_le[1] << 8)
                                + CHANGE_DIALOG_INSTRUCTION_AT((u8*)g_script_event_block, offset)->text_id_le[0])
                            - 1));
                    if (max_width < width) {
                        max_width = width;
                    }
                }
            }
            next = offset + 1;
            size = g_script_event_instruction_sizes[EVENT_OPCODE_CHANGE_DIALOG];
            offset = next + size;
        }
        *out_width = max_width;
    }
}

#ifdef FFT_WORLD
/* Menu twin of script_route_event_input: clears the new-press mask,
 * yields a frame, then reads the pad and derives the new-press mask with
 * the shared initial/repeat/secondary key-repeat delays. During a tutorial
 * with `EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS` bits 0x5000 set the
 * repeat counters restart. */
void screen_input_update_dialog_state(void) {
    u32 input;
    s32 initial_delay;
    s32 repeat_delay;
    s32 secondary_repeat_delay;
    u32 previous_input;
    u32 counter;

    g_script_event_input = 0;
    script_load_event_input_state(&g_script_event_input);
    thread_yield();
    input = PadRead(1);
    if (input == 0
        || (tutorial_is_current_event() != 0
            && (script_get_variable(EVENT_SCRIPT_VAR_DISABLED_CONTROLLER_INPUTS) & 0x5000))) {
        g_script_event_initial_repeat_counter = 0;
        g_script_event_repeat_counter = 0;
        g_script_event_secondary_repeat_counter = 0;
    }

    previous_input = g_script_event_current_input;
    g_script_event_current_input = input;
    g_script_event_input = 0;
    g_script_event_previous_input = previous_input;

    if (previous_input != input || (input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK)) == 0) {
        g_script_event_input = ~previous_input & input;
        g_script_event_initial_repeat_counter = 0;
    } else {
        initial_delay = g_input_repeat_initial_delay / g_script_event_speed;
        counter = ++g_script_event_initial_repeat_counter;
        if (counter == initial_delay) {
            g_script_event_input = input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_DPAD_MASK);
        }
        if (counter >= initial_delay) {
            repeat_delay = g_input_repeat_period / g_script_event_speed;
            counter = ++g_script_event_repeat_counter;
            g_script_event_secondary_repeat_counter++;
            if (counter >= repeat_delay) {
                g_script_event_repeat_counter = 0;
                if ((input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input = input & (PSX_PAD_RIGHT | PSX_PAD_LEFT);
                } else {
                    g_script_event_input = input & PSX_PAD_DPAD_MASK;
                }
            }
            secondary_repeat_delay = g_input_secondary_repeat_period / g_script_event_speed;
            if (g_script_event_secondary_repeat_counter >= secondary_repeat_delay) {
                g_script_event_secondary_repeat_counter = 0;
                if ((g_script_event_current_input & PSX_PAD_SQUARE) != 0) {
                    g_script_event_input
                        |= g_script_event_current_input & (PSX_PAD_L1 | PSX_PAD_R1 | PSX_PAD_UP | PSX_PAD_DOWN);
                } else {
                    g_script_event_input |= g_script_event_current_input & (PSX_PAD_L1 | PSX_PAD_R1);
                }
            }
        } else {
            g_script_event_repeat_counter = 0;
            g_script_event_secondary_repeat_counter = 0;
        }
    }
}
#endif

/* Free the typewriter slot (one of three) the current thread owns, if any, and reset its
 * glyph count; a closing dialog thread calls it. */
void message_release_typewriter_slot(void) {
    s32 index = 0;
    s32 current_thread_id = g_thread_current_id;
    s32* owner_thread_id;
    s32* slot_state = g_text_slot_glyph_counts;

    owner_thread_id = g_text_slot_owner_thread_ids;
    for (;;) {
        if (current_thread_id == *owner_thread_id) {
            *owner_thread_id = 0;
            *slot_state = 0;
            return;
        }
        slot_state++;
        index++;
        owner_thread_id++;
        if (index >= 3) {
            return;
        }
    }
}

/* The running thread's task id; loops that may be cut short poll it for THREAD_TASK_STOP_REQUEST. */
s32 thread_get_current_task_id(void) {
    return g_thread_contexts[g_thread_current_id].task_id;
}

/* Draw the glyph in g_message_typewriter_glyph straight into VRAM at the pen position through one of
 * three per-thread staging slots, then pace the message: delay 0 waits for the draw, 2 or more
 * waits that many steps per glyph, and 1 follows the message speed option (Circle held or Fast
 * shows 8 glyphs per wait). Queues the text sound unless muted. */
void message_draw_glyph_with_delay(s32 pen_x, s32 pen_y, s32 delay) {
    s32 slot;
    s32 i;
    s32 j;
    s32 phase;
    u8 ring;
    s32 glyph;
    s32 kind;
    s32 next_glyph;
    s32 next_kind;
    s32 pitch;
    u8* image;
    u8* previous;
    s32 row;
    s32 offset;
    text_glyph_state_t* request = &g_message_typewriter_glyph;
    s32 column;

    if (g_message_typewriter_glyph.mode == DIALOG_BOX_TYPE_PORTRAIT || g_message_typewriter_glyph.mode == 0) {
        /* Use the staging slot this thread owns or claim a free one (16 means none);
         * with all three taken, wait a frame and retry. */
        for (;;) {
            slot = 16;
            for (i = 0; i < 3; i++) {
                if (g_thread_current_id == g_text_slot_owner_thread_ids[i]) {
                    slot = i;
                    g_text_slot_owner_thread_ids[i] = g_thread_current_id;
                    break;
                }
            }
            if (i == 3) {
                for (row = 0; row < 3; row++) {
                    if (g_text_slot_owner_thread_ids[row] == 0) {
                        slot = row;
                        g_text_slot_owner_thread_ids[row] = g_thread_current_id;
                        g_text_slot_source_columns[slot] = 0;
                        g_text_slot_row_pitches[slot] = 0;
                        break;
                    }
                }
            }
            if (slot != 16) {
                break;
            }
            thread_yield();
        }

        /* The glyphs drawn since the slot's last wait each get their own staging image, and
         * `phase` is the glyph's pixel within a 4-pixel VRAM word. */
        ring = g_text_slot_glyph_counts[slot];
        phase = request->x & 3;
        image = g_message_typewriter_glyph_images[slot][ring];
        previous = g_message_typewriter_previous_images[slot];

        /* The upload covers whole VRAM words: `pitch` pixels across, `w` words. */
        glyph = request->glyph;
        kind = g_text_glyph_widths[glyph];
        if (kind == 4 || glyph == TEXT_SPACE) {
            if (phase == 2) {
                pitch = 8;
                g_message_typewriter_vram_rect.w = 2;
            } else {
                pitch = 4;
                g_message_typewriter_vram_rect.w = 1;
            }
        } else if (kind == 2) {
            pitch = 4;
            g_message_typewriter_vram_rect.w = 1;
        } else if (kind == 6) {
            pitch = 8;
            g_message_typewriter_vram_rect.w = 2;
        } else {
            pitch = 12;
            g_message_typewriter_vram_rect.w = 3;
        }

        /* At phase 2 the upload starts two pixels left of the glyph; copy those pixels from the
         * previous glyph's image so the upload keeps them. */
        if (phase == 2) {
            offset = 0;
            for (i = 0; i < 14; i++) {
                j = offset / 2;
                offset += g_text_slot_row_pitches[slot];
                g_message_typewriter_column_scratch[i] = (previous + j)[g_text_slot_source_columns[slot]];
            }
            for (i = 0; i < 14; i++) {
                image[i * pitch / 2] = g_message_typewriter_column_scratch[i];
            }
        }

        /* Record which byte column of this image the next glyph shares with it. */
        next_glyph = request->glyph;
        next_kind = g_text_glyph_widths[next_glyph];
        if (next_kind == 4 || next_glyph == TEXT_SPACE) {
            g_text_slot_source_columns[slot] = phase == 2 ? 2 : 1;
        } else if (next_kind == 2) {
            if (phase == 2) {
                g_text_slot_source_columns[slot] = 1;
            } else {
                g_text_slot_source_columns[slot] = 0;
            }
        } else if (next_kind == 6) {
            g_text_slot_source_columns[slot] = phase == 2 ? 3 : 2;
        } else {
            g_text_slot_source_columns[slot] = phase == 2 ? 5 : 4;
        }
        g_text_slot_row_pitches[slot] = pitch;

        /* Prefill with the box background: zero in mode 0, else the repeating 16-pixel tile in
         * the glyph sheet, aligned to the pen x. */
        for (row = 0; row < 14; row++) {
            offset = row * pitch;
            for (i = phase >> 1; i < pitch / 2; i++) {
                if (request->mode == 0) {
                    (image + i)[offset / 2] = 0;
                } else {
                    column = (((request->x + (i << 1)) & 0xF) + 8) / 2;
                    (image + i)[offset / 2] = (g_menu_glyph_sheet->pixels + column)[(row << 7) + 0x400];
                }
            }
        }

        if (request->glyph != TEXT_SPACE) {
            g_message_typewriter_pen.rect.x = phase;
            g_message_typewriter_pen.rect.y = 0;
            g_message_typewriter_pen.stride = pitch;
            script_set_variable(EVENT_SCRIPT_VAR_PRINTED_CHARACTER_COUNT,
                script_get_variable(EVENT_SCRIPT_VAR_PRINTED_CHARACTER_COUNT) + 1);
            text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + request->glyph * TEXT_GLYPH_BYTES, image,
                &g_message_typewriter_pen, request->palette);
            g_message_typewriter_vram_rect.x
                = (pen_x >> 2) + (s16)(script_get_variable(EVENT_SCRIPT_VAR_TYPEWRITER_VRAM_X_OFFSET) + 0x1C0);
            g_message_typewriter_vram_rect.y = pen_y;
            g_message_typewriter_vram_rect.h = 14;
            LoadImage(&g_message_typewriter_vram_rect, (u32*)image);
        }

        g_text_slot_glyph_counts[slot] = g_text_slot_glyph_counts[slot] + 1;
        /* Keep the 14 rows of 12 pixels for the next glyph's shared column. */
        mem_copy_bytes_nonempty(previous, image, 0x54);

        if (delay == 0) {
            DrawSync(0);
            g_text_slot_glyph_counts[slot] = 0;
        } else if (delay >= 2) {
            for (i = 0; i < delay * (3 - g_script_event_speed); i++) {
                if (thread_get_current_task_id() == THREAD_TASK_STOP_REQUEST) {
                    break;
                }
                thread_wait_frames(1);
            }
            g_text_slot_glyph_counts[slot] = 0;
        } else {
            if (PadRead(1) & PSX_PAD_CIRCLE) {
                g_text_glyphs_per_wait = 8;
            } else if (g_game_options.fields.message_display_speed == GAME_MESSAGE_DISPLAY_SPEED_FAST) {
                g_text_glyphs_per_wait = 8;
            } else if (g_game_options.fields.message_display_speed == GAME_MESSAGE_DISPLAY_SPEED_REGULAR) {
                g_text_glyphs_per_wait = 2;
            } else {
                g_text_glyphs_per_wait = 1;
            }
            if (g_text_slot_glyph_counts[slot] >= g_text_glyphs_per_wait) {
                for (i = 0; i < 3 - g_script_event_speed; i++) {
                    if (thread_get_current_task_id() == THREAD_TASK_STOP_REQUEST) {
                        break;
                    }
                    thread_wait_frames(1);
                }
                g_text_slot_glyph_counts[slot] = 0;
            }
        }

        if (script_get_variable(EVENT_SCRIPT_VAR_MUTE_TEXT_AUDIO_CUE) == 0 && request->mode != 0) {
            g_sound_pending_sfx_id = SOUND_SFX_TEXT_GLYPH;
        }
    }
}

/* Convert a text-format substitution opcode into its string-table ID. */
s32 text_resolve_format_string_id(s32 format_code, s32 value) {
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
    party = party_get_unit(0);
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
                state->column += text_count_decimal_digits(state->value);
            } else {
                state->column += state->pending_width;
                state->pending_width = 0xFFFF;
            }
            state->value_cursor++;
        } else if (state->command == TEXT_PRINT_INDEXED_VALUE) {
            cursor = text_advance_cursor_with_backreferences(state, cursor);
            state->value = g_text_substitution_values[*cursor];
            if (state->pending_width == 0xFFFF) {
                state->column += text_count_decimal_digits(state->value);
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
void text_measure_pixels(s16* width, s16* rows, const u8* text) {
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
    party = party_get_unit(0);
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
void message_render_record_image(text_dialog_record_t* record) {
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
        g_menu_text_param_22 = record->first_line;
        g_menu_text_param_24 = record->last_line;
        menu_display_text_entry(record->text, record->buffer, &g_menu_window_text_pen);
        LoadImage(&record->rect, record->buffer);
        thread_yield();
        menu_free_memory(record->buffer);
    }
}
