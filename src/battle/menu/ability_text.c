/* Three-column ability-list text, page redraws and repeat timing. */

#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Draw three-column text 8 pixels narrower than the entry, adding ability
 * reflection and charge-time markers where requested. */
void ability_list_text_draw(menu_entry_t* entry, s32* row_offset, u8* buffer) {
    menu_text_layout_t* layout = entry->context;
    s32 row;
    s32 column;
    s32 index;
    s16 value;

    for (row = 0; row < layout->row_count; row++) {
        g_menu_window_text_pen.stride = entry->inner_width - 8;
        for (column = 0; column < 3; column++) {
            index = row + *row_offset;
            if (layout->mode[column] == MENU_TEXT_COLUMN_HIDDEN) {
                continue;
            }
            menu_set_text_origin(layout->x[column], row * 0x10);
            if (layout->mode[column] != MENU_TEXT_COLUMN_HIDDEN) {
                g_menu_text_palette_offset = layout->text_colors[column][index];
            }
            /* A row drawn in palette 4 (the highlighted row) also gets a background of
             * color index 4 in every pixel nibble. */
            g_menu_window_text_pen.color = 0;
            if (g_menu_text_palette_offset == 4) {
                g_menu_window_text_pen.color = 0x44444444;
            }
            if (layout->mode[column] == MENU_TEXT_COLUMN_TEXT) {
                menu_display_text_entry(layout->text_ids[column][index], buffer, &g_menu_window_text_pen);
                if (g_threads[g_thread_current_id].function_parameter_4 != 0
                    && (g_event_work->ability_list.flags[index] & MENU_ABILITY_FLAG_REFLECTABLE)) {
                    g_menu_text_palette_offset = layout->text_colors[column][index];
                    /* 32-bit x and y, as above. */
                    menu_set_text_origin(layout->x[column] + 0x50, row * 0x10);
                    menu_display_text_entry(TEXT_ID_ABILITY_REFLECTABLE_MARKER, buffer, &g_menu_window_text_pen);
                }
            } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER) {
                menu_display_number_entry(layout->text_ids[column][index], 2, buffer, &g_menu_window_text_pen);
            } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER_GLYPHS) {
                if ((column == 2 || (column == 1 && layout->mode[2] == MENU_TEXT_COLUMN_HIDDEN))
                    && layout->text_ids[column][index] >= 0 && layout->text_ids[column][index] < 2
                    && (g_event_work->ability_list.skill_ct[index] == 0xFF
                        || g_event_work->ability_list.skill_ct[index] == 0)) {
                    g_menu_window_text_pen.rect.y += 2;
                    menu_draw_charge_time_now(buffer, &g_menu_window_text_pen);
                } else {
                    value = layout->text_ids[column][index];
                    if (value & 0x7FF) {
                        if ((value & 0x7FF) >= 100) {
                            g_menu_text_palette_offset = 0;
                            g_menu_window_text_pen.rect.x += 3;
                            menu_display_text_entry(TEXT_ID_INFINITY, buffer, &g_menu_window_text_pen);
                        } else {
                            menu_draw_large_number_glyphs(value, 2, buffer, &g_menu_window_text_pen);
                        }
                    }
                }
            }
            g_menu_text_palette_offset = 0;
            g_menu_window_text_pen.color = 0;
        }
    }
}

/* Redraw a scrolled page, using the companion overlay buffer at nesting depth 2. */
u8* menu_redraw_text_page_on_scroll_in_rect(
    menu_entry_t* entry, s32* row_offset, s32* redraw, s32* unused, RECT* area) {
    menu_text_layout_t* layout = entry->context;
    u32* pressed;
    u32* held;
    RECT rect;
    RECT clear;
    s32 changed;
    s32 offset;
    void* buffer;

    pressed = script_get_controller_input_pointer(0);
    held = script_get_controller_input_pointer(1);
    rect.x = (area->x / 4) + entry->vram_origin.x;
    rect.y = 0x80;
    rect.w = area->w / 4;
    rect.h = area->h;
    changed = 0;
    if (g_menu_input_mode == MENU_INPUT_PLAYER
        && ((*held & PSX_PAD_SQUARE)
            || g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] != 0)) {
        /* Page up or down by one screen of rows, clamped to the list. */
        if ((*pressed & PSX_PAD_UP) && *row_offset != 0) {
            offset = *row_offset - layout->row_count;
            *row_offset = offset;
            if (offset < 0) {
                *row_offset = 0;
            }
            g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
            changed = 1;
        } else if ((*pressed & PSX_PAD_DOWN) && *row_offset != layout->hidden_rows) {
            offset = layout->row_count + *row_offset;
            *row_offset = offset;
            if (layout->hidden_rows < offset) {
                *row_offset = layout->hidden_rows;
            }
            g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
            changed = 1;
        } else if (g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] != 0) {
            changed = 1;
            g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] = 0;
        }
        if (changed != 0) {
            if (g_menu_scrolling_list_depth == 2) {
                buffer = g_heap_companion_overlay_load_address + 0x2000;
            } else {
                buffer = menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
            }
            menu_clear_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
            ability_list_text_draw(entry, row_offset, buffer);
            *redraw = 0;
            LoadImage(&rect, buffer);
            /* Clear the two VRAM rows above the image, wrapping within its 256-row page. */
            mem_copy_bytes_nonempty(&clear, &rect, sizeof(clear));
            clear.h = 2;
            clear.x = rect.x;
            clear.y = ((rect.y - 2) & 0xFF) + (rect.y & 0xFF00);
            clear.w = rect.w;
            ClearImage(&clear, 0, 0, 0);
            return buffer;
        }
    }
    return 0;
}

/* Frames the current repeatable buttons have been held unchanged (0 after a change);
 * the scrolling lists pace and repeat their scrolling by it. */
u32 script_event_initial_repeat_counter_get(void) {
    return g_script_event_initial_repeat_counter;
}
