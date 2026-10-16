/* Entry value announcement, clamped icon lists, entry cursor quads, the number, labeled number and text entry threads,
 * signed numbers and entry chains. */

#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/main_sound.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static void screen_draw_signed_number(s32 value, s32 digits, u8* image, glyph_blit_t* pen);

/* Select's help-text thread: take the menu entry's selected row (for table kinds 2 and
 * -1, the next running menu thread's entry), look up its text id and show it in a dialog,
 * then unlock menu input and restore the saved input state. */
void screen_help_text_thread(void) {
    menu_entry_t* entry = (menu_entry_t*)thread_get_current_parameter_1();
    thread_t* thread;
    s32 kind = entry->select_text_table;
    s32 index;
    s32 value;
    s32 next;

    if (kind == 2 || kind == -1) {
        next = g_thread_current_id + 1;
        thread = &g_threads[next];
        while (1) {
            if (thread->is_running != 0) {
                entry = (menu_entry_t*)thread->function_parameter_1;
                if (entry->select_text_table != -1) {
                    break;
                }
            }
            thread++;
        }
        kind = entry->select_text_table;
    }
    index = entry->selected_index;
    if (kind == 0x11) {
        menu_text_binding_t* binding = entry->context;

        value = binding->row_text_ids[index];
        value &= 0x7FF;
        if (value == 0x2B) {
            value = 0x63;
        }
        value |= 0x8000;
    } else {
        value = g_screen_entry_text_id_tables[kind][index];
    }
    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
    thread_set_parameters(g_thread_current_id, 0x3F, value, 0);
    text_run_dialog_thread();
    g_screen_new_buttons = 0;
    g_menu_input_lock_mode = 0;
    script_load_event_input_state(&g_screen_input_state);
    thread_exit_current();
}

/* Menu icon list thread with a clamped cursor: builds the text image once,
 * steps the cursor while the previous thread is idle and submits the four
 * icon sprites of the alternating record each frame. Up steps the cursor up,
 * but the Down test is inverted (a retail bug, QUIRKS.md): the cursor moves
 * down on every frame the Down bit is clear, until it reaches max_row_index. */
void screen_clamped_icon_list_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    s32 cursor;
    menu_icon_record_t* record;
    menu_icon_sprites_t* sprites;

    g_screen_list_controller_input = script_get_controller_input_pointer(0);
    param = (menu_entry_t*)thread_get_current_parameter_1();
    cursor = param->selected_index;
    record = &records[0];
    if (cursor == -1) {
        cursor = 0;
    }
    menu_build_window_sprites(&rect, param, record);
    mem_copy_bytes_nonempty(&records[1], record, sizeof(records[1]));
    buffer = menu_build_and_upload_window_frame_image(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = param->inner_width;
    menu_set_text_origin(8, 9);
    g_menu_text_palette_offset = 0;
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, buffer);
    for (i = 0;; i++) {
        record = &records[i & 1];
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        param->selected_index = cursor;
        if (menu_should_close_thread(g_screen_list_controller_input) != 0) {
            break;
        }
        if (thread_is_previous_running() == 0) {
            if ((*g_screen_list_controller_input & PSX_PAD_UP) && cursor != 0) {
                cursor--;
            }
            if (!(*g_screen_list_controller_input & PSX_PAD_DOWN) && cursor != param->max_row_index) {
                cursor++;
            }
            menu_handle_action(param, cursor);
            menu_handle_cancel_input(param);
        }
        sprites = &record->base;
        menu_select_icon_cluts(sprites);
        menu_update_icon_cursor_sprites(param, &record->base, i, cursor);
        gfx_draw_or_append_gpu_primitive(&sprites->sprites[3].tag);
        gfx_draw_or_append_gpu_primitive(&sprites->sprites[2].tag);
        gfx_draw_or_append_gpu_primitive(&sprites->sprites[1].tag);
        gfx_draw_or_append_gpu_primitive(&sprites->draw_mode);
    }
    thread_yield();
    thread_exit_current();
}

/* Place a number entry's bobbing digit cursor and its shadow over digit cursor (counted from the
 * right edge), freezing the bob while input belongs to another thread. */
static void screen_place_number_entry_cursor(
    menu_entry_t* entry, POLY_FT4* quad, POLY_FT4* shadow, s32 unused, s32 cursor) {
    thread_t* thread = &g_threads[g_thread_current_id];
    s32 x;
    s32 y;

    if (cursor >= 0) {
        if (g_menu_formation_display_mode == 1) {
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 2;
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
        }
        if (thread_is_previous_running() == 0 && g_menu_input_lock_mode == 0) {
            if (thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] != 0
                && thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] >= 0x1B) {
                thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 0;
                thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
            }
        } else if (thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] == 0) {
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] = 2;
            thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] = 0;
        }
        if (thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_ACTIVE] != 0) {
            y = menu_get_cursor_bob_offset(1);
        } else {
            y = menu_get_cursor_bob_offset(0);
        }
        thread->task_words[MENU_CURSOR_THREAD_WORD_BOB_TIMER] += g_script_event_speed;
        x = -(cursor + 1) * 6;
        y -= 0x17;
        setXY4(quad, entry->window.x + (entry->inner_width + x) - 0x13, entry->window.y - y + 4,
            entry->window.x + (entry->inner_width + x) - 3, entry->window.y - y + 4,
            entry->window.x + (entry->inner_width + x) - 0x13, entry->window.y - y + 0x14,
            entry->window.x + (entry->inner_width + x) - 3, entry->window.y - y + 0x14);
        setXY4(shadow, entry->window.x + (entry->inner_width + x) - 0x11, entry->window.y - y + 6,
            entry->window.x + (entry->inner_width + x) - 1, entry->window.y - y + 6,
            entry->window.x + (entry->inner_width + x) - 0x11, entry->window.y - y + 0x16,
            entry->window.x + (entry->inner_width + x) - 1, entry->window.y - y + 0x16);
    }
}

/* Numeric entry thread: edits the 16-bit value at param->value one decimal digit
 * at a time within the range bound at param->context. Left/right move the digit cursor, up/down
 * step the value by the g_screen_number_entry_digit_steps step of that digit (clamped to the range),
 * and a changed value is redrawn as a signed number in the window. When the
 * cancel/confirm handlers change the 32-bit input mask, a -1 left in the value
 * restores the original; otherwise the value is stored and the range's
 * on_change callback runs. */
void screen_number_entry_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    POLY_FT4 cursor_quads[2];
    POLY_FT4 shadow_quads[2];
    screen_number_range_t* range;
    u8* buffer;
    s16 original;
    s32 digits;
    thread_t* thread;
    s32* parameter;
    menu_entry_t* param;
    s16 value;
    s32 i;
    s32 j;
    s32 digit;
    s32 redraw;
    u32 input;
    menu_icon_sprites_t* record;
    POLY_FT4* cursor_quad;
    POLY_FT4* shadow_quad;
    s32* stride;

    g_screen_list_controller_input = script_get_controller_input_pointer(0);
    /* A direct pen pointer folds both member-address adjustments and changes scheduling. */
    stride = &g_menu_text_state.pen.stride;
    thread = &g_threads[g_thread_current_id];
    parameter = &thread->function_parameter_1;
    param = (menu_entry_t*)*parameter;
    range = param->context;
    digits = text_count_decimal_digits(range->bounds.max);

    /* Start from the stored value, or from the minimum when it is out of range. */
    original = *menu_entry_result(param);
    value = original;
    if (value < range->bounds.min || range->bounds.max < value) {
        value = range->bounds.min;
    }

    menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], sizeof(records[1]));

    /* One digit cursor and translucent shadow per frame buffer. */
    for (j = 0; j < 2; j++) {
        menu_init_default_poly_ft4(&cursor_quads[j]);
        SetSemiTrans(&cursor_quads[j], 0);
        cursor_quads[j].tpage = GetTPage(0, 0, 0x3C0, 0x100);
        cursor_quads[j].clut = 0x7D7C;
        cursor_quads[j].u0 = 0xB7;
        cursor_quads[j].u1 = 0xB7;
        cursor_quads[j].u2 = 0xA7;
        cursor_quads[j].u3 = 0xA7;
        cursor_quads[j].v0 = 0;
        cursor_quads[j].v1 = 0x10;
        cursor_quads[j].v2 = 0;
        cursor_quads[j].v3 = 0x10;
        menu_init_default_poly_ft4(&shadow_quads[j]);
        SetSemiTrans(&shadow_quads[j], 1);
        shadow_quads[j].tpage = GetTPage(0, 2, 0x3C0, 0x100);
        shadow_quads[j].clut = 0x7DBC;
        shadow_quads[j].u0 = 0xC7;
        shadow_quads[j].u1 = 0xC7;
        shadow_quads[j].u2 = 0xB7;
        shadow_quads[j].u3 = 0xB7;
        shadow_quads[j].v0 = 0;
        shadow_quads[j].v1 = 0x10;
        shadow_quads[j].v2 = 0;
        shadow_quads[j].v3 = 0x10;
    }

    /* Draw the first value; redraw = 1 frees this image after the first wait. The (8, 9) origin goes
     * unused here; the labeled twin draws its label there. */
    digit = 0;
    buffer = menu_build_and_upload_window_frame_image(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    *stride = param->inner_width;
    menu_set_text_origin(8, 9);
    menu_set_text_origin(param->inner_width - digits * 6 - 0x10, 9);
    screen_draw_signed_number(value, digits, buffer, (glyph_blit_t*)((u8*)stride - offsetof(glyph_blit_t, stride)));
    LoadImage(&rect, buffer);
    redraw = 1;

    for (i = 0;; i++) {
        record = &records[i & 1].base;
        *menu_entry_result(param) = value;
        thread_wait_frames(1);
        /* A redrawn image is freed a frame late, after its upload has run. */
        if (redraw == 1) {
            redraw = 0;
            menu_free_memory(buffer);
        }
        if (menu_should_close_thread(g_screen_list_controller_input) != 0) {
            break;
        }

        /* The handlers consume the input bits they act on, so a changed mask means one of them ran.
         * A cancel leaves -1 in the result, which restores the original value. */
        input = *g_screen_list_controller_input;
        menu_handle_cancel_input(param);
        menu_handle_action(param, 0);
        if (input != *g_screen_list_controller_input) {
            if (*menu_entry_result(param) == -1) {
                *menu_entry_result(param) = original;
            } else {
                screen_number_range_t* current_range;

                *menu_entry_result(param) = value;
                current_range = param->context;
                current_range->on_change();
            }
        }

        /* Digit 0 is the ones digit, so right moves the cursor toward it. Up and down step by the
         * cursor digit's place value, clamped to the range. */
        if (thread[-1].is_running == 0) {
            if ((*g_screen_list_controller_input & PSX_PAD_RIGHT) && digit > 0) {
                digit--;
            }
            if ((*g_screen_list_controller_input & PSX_PAD_LEFT) && digit < digits - 1) {
                digit++;
            }
            if (*g_screen_list_controller_input & PSX_PAD_UP) {
                if (value <= range->bounds.max - g_screen_number_entry_digit_steps[digit]) {
                    value += g_screen_number_entry_digit_steps[digit];
                } else {
                    value = range->bounds.max;
                }
                redraw = 1;
            }
            if (*g_screen_list_controller_input & PSX_PAD_DOWN) {
                if (value >= range->bounds.min + g_screen_number_entry_digit_steps[digit]) {
                    value -= g_screen_number_entry_digit_steps[digit];
                } else {
                    value = range->bounds.min;
                }
                redraw = 1;
            }
        }

        if (redraw == 1) {
            buffer = menu_build_and_upload_window_frame_image(
                param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            *stride = param->inner_width;
            menu_set_text_origin(param->inner_width - digits * 6 - 0x10, 9);
            screen_draw_signed_number(
                value, digits, buffer, (glyph_blit_t*)((u8*)stride - offsetof(glyph_blit_t, stride)));
            LoadImage(&rect, buffer);
        }

        /* The cursor and its shadow switch to dimmed CLUTs while the thread before this one runs. */
        cursor_quad = &cursor_quads[i & 1];
        shadow_quad = &shadow_quads[i & 1];
        screen_place_number_entry_cursor(param, cursor_quad, shadow_quad, i, digit);
        menu_select_sprite_clut(cursor_quad);
        menu_select_sprite_clut(shadow_quad);
        if (thread_is_previous_running() == 0) {
            cursor_quad->clut = 0x7D7C;
            shadow_quad->clut = 0x7DBC;
        } else {
            cursor_quad->clut = 0x7DFC;
            shadow_quad->clut = 0x7E3C;
        }

        menu_update_icon_cursor_sprites(param, record, i, -1);
        menu_select_icon_cluts(record);
        gfx_draw_or_append_gpu_primitive(&cursor_quads[i & 1]);
        gfx_draw_or_append_gpu_primitive(&shadow_quads[i & 1]);
        gfx_draw_or_append_gpu_primitive(&record->sprites[0]);
        gfx_draw_or_append_gpu_primitive(record);
    }

    thread_yield();
    thread_exit_current();
}

/*
 * Labeled numeric entry thread: screen_number_entry_thread with a text label.
 *
 * Draws param->text_id left of the number and edits the 16-bit value at
 * param->value one decimal digit at a time within the range bound at param->context. The value is
 * written back each frame only while g_formation_screen_active is set. The initial label is
 * measured in pixels and each redraw in character columns (times 6).
 * i is shared by the quad setup loop and the frame loop, as in the target.
 */
void screen_labeled_number_entry_thread(void) {
    RECT rect;
    menu_icon_record_t record;
    POLY_FT4 cursor_quads[2];
    POLY_FT4 shadow_quads[2];
    s16 label_size[2];
    screen_number_range_t* range;
    u8* buffer;
    s16 original;
    s32 digits;
    thread_t* thread;
    menu_entry_t* param;
    s16 value;
    s32 i;
    s32 digit;
    s32 redraw;
    u32 input;
    POLY_FT4* cursor_quad;
    POLY_FT4* shadow_quad;

    g_screen_list_controller_input = script_get_controller_input_pointer(0);
    thread = &g_threads[g_thread_current_id];
    param = *(menu_entry_t**)thread;
    range = param->context;
    digits = text_count_decimal_digits(range->bounds.max);

    /* Start from the stored value, or from the minimum when it is out of range. */
    original = *menu_entry_result(param);
    value = original;
    if (value < range->bounds.min || range->bounds.max < value) {
        value = range->bounds.min;
    }

    menu_build_window_sprites(&rect, param, &record);

    /* One digit cursor and translucent shadow per frame buffer. */
    for (i = 0; i < 2; i++) {
        menu_init_default_poly_ft4(&cursor_quads[i]);
        SetSemiTrans(&cursor_quads[i], 0);
        cursor_quads[i].tpage = GetTPage(0, 0, 0x3C0, 0x100);
        cursor_quads[i].clut = 0x7D7C;
        cursor_quads[i].u0 = 0xB7;
        cursor_quads[i].u1 = 0xB7;
        cursor_quads[i].u2 = 0xA7;
        cursor_quads[i].u3 = 0xA7;
        cursor_quads[i].v0 = 0;
        cursor_quads[i].v1 = 0x10;
        cursor_quads[i].v2 = 0;
        cursor_quads[i].v3 = 0x10;
        menu_init_default_poly_ft4(&shadow_quads[i]);
        SetSemiTrans(&shadow_quads[i], 1);
        shadow_quads[i].tpage = GetTPage(0, 2, 0x3C0, 0x100);
        shadow_quads[i].clut = 0x7DBC;
        shadow_quads[i].u0 = 0xC7;
        shadow_quads[i].u1 = 0xC7;
        shadow_quads[i].u2 = 0xB7;
        shadow_quads[i].u3 = 0xB7;
        shadow_quads[i].v0 = 0;
        shadow_quads[i].v1 = 0x10;
        shadow_quads[i].v2 = 0;
        shadow_quads[i].v3 = 0x10;
    }

    /* Draw the label and the first value; redraw = 1 frees this image after the first wait. The
     * origin after the label is overridden at once by the right-aligned number's. */
    digit = 0;
    buffer = menu_build_and_upload_window_frame_image(
        param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = param->inner_width;
    text_measure_pixels(&label_size[0], &label_size[1], text_find_entry(param->text_id));
    menu_set_text_origin(8, 9);
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    menu_set_text_origin((u16)label_size[0] + 8, 9);
    menu_set_text_origin(param->inner_width - digits * 6 - 0x10, 9);
    screen_draw_signed_number(value, digits, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, buffer);
    redraw = 1;

    for (i = 0;; i++) {
        if (g_formation_screen_active != 0) {
            *menu_entry_result(param) = value;
        }
        thread_wait_frames(1);
        /* A redrawn image is freed a frame late, after its upload has run. */
        if (redraw == 1) {
            menu_free_memory(buffer);
            redraw = 0;
        }
        if (menu_should_close_thread(g_screen_list_controller_input) != 0) {
            break;
        }

        /* The handlers consume the input bits they act on, so a changed mask means one of them ran.
         * A cancel leaves -1 in the result, which restores the original value. */
        input = *g_screen_list_controller_input;
        menu_handle_cancel_input(param);
        menu_handle_action(param, 0);
        if (input != *g_screen_list_controller_input) {
            if (*menu_entry_result(param) == -1) {
                *menu_entry_result(param) = original;
            } else {
                screen_number_range_t* current_range;

                *menu_entry_result(param) = value;
                current_range = param->context;
                current_range->on_change();
            }
        }

        /* Digit 0 is the ones digit, so right moves the cursor toward it. Up and down step by the
         * cursor digit's place value, clamped to the range. */
        if (thread[-1].is_running == 0) {
            if ((*g_screen_list_controller_input & PSX_PAD_RIGHT) && digit > 0) {
                digit--;
            }
            if ((*g_screen_list_controller_input & PSX_PAD_LEFT) && digit < digits - 1) {
                digit++;
            }
            if (*g_screen_list_controller_input & PSX_PAD_UP) {
                if (value <= range->bounds.max - g_screen_number_entry_digit_steps[digit]) {
                    value += g_screen_number_entry_digit_steps[digit];
                } else {
                    value = range->bounds.max;
                }
                redraw = 1;
            }
            if (*g_screen_list_controller_input & PSX_PAD_DOWN) {
                if (value >= range->bounds.min + g_screen_number_entry_digit_steps[digit]) {
                    value -= g_screen_number_entry_digit_steps[digit];
                } else {
                    value = range->bounds.min;
                }
                redraw = 1;
            }
        }

        /* A redraw measures the label in character columns (times 6), the setup above in pixels. */
        if (redraw == 1) {
            buffer = menu_build_and_upload_window_frame_image(
                param->inner_width, param->inner_height, &rect, MENU_WINDOW_FRAME_BUILD);
            g_menu_text_state.pen.stride = param->inner_width;
            text_measure(&label_size[0], &label_size[1], text_find_entry(param->text_id));
            menu_set_text_origin(8, 9);
            menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
            menu_set_text_origin((u16)label_size[0] * 6 + 8, 9);
            menu_set_text_origin(param->inner_width - digits * 6 - 0x10, 9);
            screen_draw_signed_number(value, digits, buffer, &g_menu_text_state.pen);
            LoadImage(&rect, buffer);
        }

        /* The cursor and its shadow switch to dimmed CLUTs while the thread before this one runs. */
        cursor_quad = &cursor_quads[i & 1];
        shadow_quad = &shadow_quads[i & 1];
        screen_place_number_entry_cursor(param, cursor_quad, shadow_quad, i, digit);
        menu_select_sprite_clut(cursor_quad);
        menu_select_sprite_clut(shadow_quad);
        if (thread_is_previous_running() == 0) {
            cursor_quad->clut = 0x7D7C;
            shadow_quad->clut = 0x7DBC;
        } else {
            cursor_quad->clut = 0x7DFC;
            shadow_quad->clut = 0x7E3C;
        }

        menu_update_icon_cursor_sprites(param, &record.base, i, -1);
        menu_select_icon_cluts(&record.base);
        gfx_draw_or_append_gpu_primitive(&cursor_quads[i & 1]);
        gfx_draw_or_append_gpu_primitive(&shadow_quads[i & 1]);
        gfx_draw_or_append_gpu_primitive(&record.base.sprites[0]);
        gfx_draw_or_append_gpu_primitive(&record);
    }

    thread_yield();
    thread_exit_current();
}

/*
 * Draw a signed decimal number right-aligned in `digits` columns of 6 pixels
 * starting at the pen, least significant digit first, with a minus glyph drawn
 * before the digits when the value is negative. Glyphs are TEXT_GLYPH_BYTES
 * apart in the font block at g_text_glyph_bitmap_data; the minus is glyph 237.
 *
 * `digits` arrives as a full 32-bit value and only its low byte is used.
 */
static void screen_draw_signed_number(s32 value, s32 digits, u8* image, glyph_blit_t* pen) {
    s32 original = value;
    s32 i;

    pen->rect.x += ((u8)digits - 1) * 6;
    if (original < 0) {
        value = -value;
        pen->rect.x += 6;
    }
    for (i = 0; i < (u8)digits; i++) {
        text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + (value % 10) * TEXT_GLYPH_BYTES, image, pen, 0);
        pen->rect.x -= 6;
        value /= 10;
    }
    if (original < 0) {
        text_render_glyph_to_4bpp_image(g_text_glyph_bitmap_data + 237 * TEXT_GLYPH_BYTES, image, pen, 0);
    }
}

/* Reopen a chain of menu entries, each in the thread slot below the previous one, following each
 * entry's selected row action until a number-entry window is started or the action is not a menu
 * index. */
void screen_start_entry_chain(u32 index, s32 thread) {
    menu_entry_t* entry;

    do {
        entry = &g_menu_thread_menu_data[index];
        thread_start(thread, entry->thread_entry);
        thread_set_parameters(thread, (s32)entry, 0, 0);
        if (entry->thread_entry == screen_labeled_number_entry_thread
            || entry->thread_entry == screen_number_entry_thread) {
            return;
        }
        index = entry->row_actions[entry->selected_index];
        thread--;
    } while (index < 1000);
}

/* Menu thread body: runs the entry's text binding hook, starts the text
 * thread for it, then idles until input arrives or the previous thread's
 * third parameter is raised. */
void screen_text_entry_thread(void) {
    thread_t* thread = &g_threads[g_thread_current_id];
    menu_entry_t* entry = (menu_entry_t*)thread->function_parameter_1;
    menu_text_binding_t* binding;
    s32 text_thread;
    u16 second;
    s32 speaker_id;
    u16 text_id;
    menu_text_binding_t* prepare_binding;

    prepare_binding = entry->context;
    prepare_binding->prepare();
    g_screen_list_controller_input = script_get_controller_input_pointer(0);
    text_id = entry->text_id;
    second = entry->header_id;
    binding = entry->context;
    text_thread = binding->text_thread_id;
    speaker_id = binding->speaker_id;
    thread_start(text_thread, text_run_dialog_thread);
    thread_set_parameters(text_thread, second, text_id, speaker_id);
    thread_wait_until_inactive(text_thread);
    *g_screen_list_controller_input = PSX_PAD_CIRCLE;
    while (1) {
        menu_handle_action(entry, 0);
        if (menu_should_close_thread(g_screen_list_controller_input) != 0) {
            break;
        }
        if ((thread - 1)->function_parameter_3 != 0) {
            break;
        }
        thread_wait_frames(1);
    }
    thread_exit_current();
}

/* Resize the current thread's menu entry's first parent entry to fit a
 * decimal number, keeping its right edge fixed. */
void screen_fit_parent_entry_to_digits(void) {
    menu_entry_t* entry = (menu_entry_t*)g_threads[g_thread_current_id].function_parameter_1;
    u32 index = entry->row_actions[0];
    s32 digits;
    s32 value;
    s32 width;
    s32 old_width;
    s32 margin;

    if (index < 1000) {
        entry = &g_menu_thread_menu_data[index];
        /* Retail bug (QUIRKS.md): `value` is never set, so the callee counts the digits of a
         * stale $a0. Nothing calls this function. */
        digits = text_count_decimal_digits(value);
        width = digits * 6;
        margin = (digits & 1) * 2 + 0x18;
        width += margin;
        old_width = entry->inner_width;
        entry->inner_width = width;
        entry->window.w = width;
        entry->text_image.width = width;
        entry->window.x -= width - old_width;
    }
}
