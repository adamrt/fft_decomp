/* VRAM render records, panel render threads, list cursor state, scrollable text windows and the sound queue. */

#include "fft/gfx.h"
#include "fft/main_file.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/window.h"
#include "fft/wldcore.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_sound.h"
#include "fft/wldcore_text.h"
#include "fft/wldcore_window.h"
#include "fft/world_menu.h"
#include "fft/world_sprite.h"
#include "psx/libetc.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

static s16 g_window_panel_row_actions[16]
    = { 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000 };

static void text_window_render_page(text_window_t* state, s32 row);
static void text_window_update_page_prompt(text_window_t* state);
static void text_window_update_scroll_indicators(text_window_t* state);
static void window_init_animated_record(s32 index, s32 priority, s32 sequence);
static void window_number_panel_noop(void);

/* Point image render record `index` at a VRAM rectangle given in halfwords: its
 * texture page, page-relative bounds in 8-bit texels, CLUT and a neutral 0x80 tint.
 * Clears the blank and hidden flags and adds `flags`. */
void window_init_vram_render_record(s32 index, RECT bounds, point32_t clut, s32 flags) {
    RECT* record_bounds;
    CVECTOR* rgb;

    g_window_render_records[index].flags
        = (g_window_render_records[index].flags & ~(DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN)) | flags;
    g_window_render_records[index].clut = clut;
    g_window_render_records[index].tpage = GetTPage(0, 0, bounds.x, bounds.y);
    record_bounds = &g_window_render_records[index].bounds;
    record_bounds->x = (bounds.x & 0x3F) * 2;
    record_bounds->y = bounds.y & 0xFF;
    record_bounds->w = bounds.w * 2;
    record_bounds->h = bounds.h;
    rgb = &g_window_render_records[index].color;
    rgb->b = rgb->g = rgb->r = 0x80;
    g_window_render_records[index].anim_counter = 0;
}

/* Initialize panel state and start its WORLD rendering thread. */
void window_start_panel_thread(s32 thread_id, s32 extent, window_panel_layout_t* layout, point32_t* origin) {
    window_init_panel_state(thread_id, extent, layout, origin);
    thread_start(thread_id, screen_text_list_thread);
    thread_set_parameters(thread_id, &g_window_panel_render_state, 0, 0);
}

/* Initialize the panel renderer's shared geometry and data pointers.
 *
 * Width is rounded for its tiled image; stored origins are screen-relative.
 * Layout and origin values are loaded as full 32-bit values before narrowing to s16. */
void window_init_panel_state(s32 unused, s32 extent, window_panel_layout_t* layout, point32_t* origin) {
    s32 width = layout->text_width;
    s32 rows = layout->visible_rows;
    s32 text_width;

    g_window_panel_label_state.row_count = rows;
    g_window_panel_label_state.hidden_rows = extent - layout->visible_rows;
    g_window_panel_label_state.x[0] = g_window_panel_label_state.x[1] = 4;
    g_window_panel_label_state.mode[0] = MENU_TEXT_COLUMN_TEXT;
    g_window_panel_label_state.mode[1] = MENU_TEXT_COLUMN_HIDDEN;
    g_window_panel_label_state.mode[2] = MENU_TEXT_COLUMN_HIDDEN;
    g_window_panel_label_state.text_ids[0] = g_menu_stack_entry_values;
    g_window_panel_label_state.text_colors[0] = g_menu_stack_row_flags;
    text_width = (width + 0x18) & 0xFFFC;
    g_window_panel_render_state.vram_origin.x = 0x200;
    g_window_panel_render_state.vram_origin.y = 0x100;
    g_window_panel_render_state.inner_width = text_width;
    g_window_panel_render_state.inner_height = 0;
    g_window_panel_render_state.window.x = origin->x - 0x80;
    g_window_panel_render_state.window.y = origin->y - 0x78;
    g_window_panel_render_state.window.w = text_width;
    g_window_panel_render_state.window.h = 0;
    g_window_panel_render_state.text_image.x_load = 0;
    g_window_panel_render_state.text_image.y_load = 0;
    g_window_panel_render_state.text_image.width = text_width;
    g_window_panel_render_state.text_image.height = 0;
    g_window_panel_render_state.text_image.x_screen_offset = 0;
    g_window_panel_render_state.text_image.y_screen_offset = 0;
    g_window_panel_render_state.text_id = 0;
    g_window_panel_render_state.max_row_index = layout->visible_rows;
    g_window_panel_render_state.thread_count = 0;
    g_window_panel_render_state.row_actions = g_window_panel_row_actions;
    g_window_panel_render_state.header_id = 1;
    g_window_panel_render_state.context = &g_window_panel_label_state; /* a column layout */
    g_window_panel_render_state.value = &g_window_panel_value;
}

/* Save the active menu position in a persistent cursor slot. */
void menu_stack_store_cursor(s32 index) {
    g_menu_stack_cursor_states[index].selected_index = g_window_panel_render_state.selected_index;
    g_menu_stack_cursor_states[index].scroll_offset = g_window_panel_render_state.select_text_table;
}

/* Clamp a saved cursor slot to the current list and restore it as active. */
void menu_stack_clamp_cursor(s32 index, s32 count, s32 visible) {
    s32 end;

    if (count < g_menu_stack_cursor_states[index].selected_index + 1) {
        g_menu_stack_cursor_states[index].selected_index = count - 1;
        g_menu_stack_cursor_states[index].scroll_offset = count - visible;
    }
    end = g_menu_stack_cursor_states[index].scroll_offset;
    end += visible;
    if (count - end < 0) {
        g_menu_stack_cursor_states[index].scroll_offset = count - visible;
    }
    {
        s32 value;

        value = g_menu_stack_cursor_states[index].selected_index;
        g_window_panel_render_state.selected_index = value;
        value = g_menu_stack_cursor_states[index].scroll_offset;
        g_window_panel_render_state.select_text_table = value;
    }
}

/* Reset persistent cursor slot `index` to the first row. */
void menu_stack_clear_cursor(s32 index) {
    g_menu_stack_cursor_states[index].scroll_offset = 0;
    g_menu_stack_cursor_states[index].selected_index = 0;
}

/* Initialize and start a numeric panel renderer.
 *
 * The packed 32-bit value supplies two displayed 16-bit values; their magnitude selects
 * a one- through four-digit panel width before the optional label is measured.
 * `width` holds the digit count first and the panel's pixel width after. */
void window_start_number_panel_thread(s32 thread_id, u32 packed_values, s32 text_id, point32_t* origin) {
    s16 text_width;
    s16 text_height;
    s32 width;
    s32 min = packed_values & 0xffff;
    s32 max = packed_values >> 16;
    s32 x;
    s32 y;
    s32 label_width;

    g_window_panel_number_state.bounds.min = min;
    g_window_panel_number_state.bounds.max = max;
    g_window_panel_number_state._unknown_04[0] = -1;
    g_window_panel_number_state._unknown_04[1] = -1;
    g_window_panel_number_state.on_change = window_number_panel_noop;
    g_menu_stack_entry_values[0] = g_menu_stack_entry_values[1] = 2000;
    if (min < 10 && max < 10) {
        width = 1;
    } else {
        width = 4;
        if (max < 1000) {
            width = 3;
        }
        if (max < 100) {
            width = 2;
        }
    }

    text_width = 0;
    if (text_id != 0) {
        text_measure_pixels(&text_width, &text_height, text_find_entry(text_id));
    }

    label_width = text_width + 0x18;
    width = (width * 6 + label_width) & 0xfffc;
    g_window_panel_render_state.vram_origin.x = 0x200;
    g_window_panel_render_state.vram_origin.y = 0x100;
    g_window_panel_render_state.inner_width = width;
    g_window_panel_render_state.inner_height = 0x20;
    x = origin->x;
    g_window_panel_render_state.window.x = x;
    y = origin->y;
    g_window_panel_render_state.window.y = y + 8;
    g_window_panel_render_state.window.w = width;
    g_window_panel_render_state.window.h = 0x20;
    g_window_panel_render_state.text_image.x_load = 0;
    g_window_panel_render_state.text_image.y_load = 0;
    g_window_panel_render_state.text_image.width = width;
    g_window_panel_render_state.text_image.height = 0x20;
    g_window_panel_render_state.text_image.x_screen_offset = 0;
    g_window_panel_render_state.text_image.y_screen_offset = 0;
    g_window_panel_render_state.text_id = text_id;
    g_window_panel_render_state.max_row_index = 0;
    g_window_panel_render_state.thread_count = 0;
    g_window_panel_render_state.row_actions = g_menu_stack_entry_values;
    g_window_panel_render_state.header_id = 0;
    g_window_panel_render_state.context = &g_window_panel_number_state; /* a value range */
    g_window_panel_render_state.value = &g_window_panel_value;
    g_window_panel_render_state.selected_index = 0;
    g_window_panel_render_state.select_text_table = 0;
    thread_start(thread_id, screen_labeled_number_entry_thread);
    thread_set_parameters(thread_id, (s32)&g_window_panel_render_state, 0, 0);
}

/* The on_change callback of the number panel's value range: nothing to do. */
static void window_number_panel_noop(void) {
}

/* Open the scrollable text window for state->text_id: add the page-prompt, scroll-arrow
 * and scroll-thumb indicators and a popping-in image record, place the indicators at
 * the text's right edge, measure the text and render its first page. */
void text_window_open(text_window_t* state) {
    s16 text_metrics[0x20];
    s32 index;
    s32 render_index;
    s32 padded_width;
    s32 window_x;
    s32 window_y;
    s32 lower_y;
    s32 rows;
    s32 columns;
    s32 last_page_row;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    state->left_window_index = index;
    window_init_animated_record(index, state->priority, WINDOW_ANIM_TEXT_PAGE_PROMPT);
    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    state->middle_window_index = index;
    window_init_animated_record(index, state->priority, WINDOW_ANIM_TEXT_SCROLL_UP);
    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    state->right_window_index = index;
    window_init_animated_record(index, state->priority, WINDOW_ANIM_TEXT_SCROLL_DOWN);
    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    state->lower_window_index = index;
    window_init_animated_record(index, state->priority, WINDOW_ANIM_TEXT_SCROLL_THUMB);
    render_index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    state->render_record_index = render_index;
    g_window_render_records[render_index].flags |= DISPLAY_OBJECT_POP_IN;
    g_window_render_records[state->render_record_index].priority = state->priority;
    g_window_render_records[state->render_record_index].base = state->base;

    padded_width = (state->text_width + 0x18) & 0xfffc;
    window_x = state->base.x + padded_width;
    window_y = state->base.y + state->rows_per_page * 16 + 16;
    if (padded_width % 8 == 4) {
        window_x -= 14;
    } else {
        window_x -= 12;
    }
    g_window_records[state->left_window_index].position.x = window_x - 14;
    g_window_records[state->left_window_index].position.y = window_y - 22;
    lower_y = window_y - 26;
    g_window_records[state->middle_window_index].position.x = window_x;
    do {
        g_window_records[state->middle_window_index].position.y = state->base.y + 8;
    } while (0);
    g_window_records[state->right_window_index].position.x = window_x;
    g_window_records[state->right_window_index].position.y = lower_y;
    g_window_records[state->lower_window_index].position.x = window_x;
    g_window_records[state->lower_window_index].position.y = lower_y;

    text_measure(&text_metrics[0], &text_metrics[1], text_find_entry(state->text_id));
    rows = text_metrics[1];
    last_page_row = (rows - 1) / state->rows_per_page * state->rows_per_page;
    state->page_start_row = 0;
    columns = text_metrics[0];
    state->scroll_state = TEXT_WINDOW_SCROLL_PAGING;
    state->measured_rows = rows;
    state->measured_columns = columns;
    state->last_page_row = last_page_row;
    text_window_render_page(state, state->page_start_row);
}

/* Initializes window record `index` hidden and animating even while the map is
 * frozen, with its animation sequence and OT priority, and restarts the
 * animation. */
static void window_init_animated_record(s32 index, s32 priority, s32 sequence) {
    g_window_records[index].sequence = sequence;
    g_window_records[index].priority = priority;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    g_window_records[index].flags |= DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_ANIMATE_WHILE_PAUSED;
}

/*
 * Per-frame scrollable text window handler. Returns 1 while the window stays
 * open and 0 once it has closed.
 *
 * PAGING: Circle turns the page and Cross closes the window, as does turning
 * past the last page; closing releases the four indicator records and the
 * image record. Square switches to MANUAL, where Up and Down scroll back
 * through the rows before the page, one row at a time, while Square is held.
 * On release RETURN scrolls forward a row per frame until the page is back.
 *
 * The state 2 page-start path repeats the update call instead of jumping to
 * the shared tail: its argument copy is then a no-op against the incoming $a0,
 * so only the other paths load $a0 (in their jump delay slots), as in the
 * target.
 */
s32 text_window_step(text_window_t* state) {
    if (state->scroll_state == TEXT_WINDOW_SCROLL_RETURN) {
        if (state->scroll_row == state->page_start_row) {
            state->scroll_state = TEXT_WINDOW_SCROLL_PAGING;
            text_window_update_page_prompt(state);
            return 1;
        }
        state->scroll_timer += 1;
        if (state->scroll_timer <= 0) {
            return 1;
        }
        state->scroll_timer = 0;
        state->scroll_row += 1;
        text_window_render_page(state, state->scroll_row);
        return 1;
    }

    if (state->scroll_state == TEXT_WINDOW_SCROLL_MANUAL) {
        if ((g_input_current_buttons & PSX_PAD_SQUARE) == 0) {
            state->scroll_state = TEXT_WINDOW_SCROLL_RETURN;
            state->scroll_timer = 0;
            g_window_records[state->middle_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
            g_window_records[state->right_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
            g_window_records[state->lower_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
            return 1;
        }
        if ((world_map_update_input_repeat(PSX_PAD_UP) != 0) && (state->scroll_row != 0)) {
            state->scroll_row--;
        } else if (world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
            if (state->page_start_row > state->scroll_row) {
                state->scroll_row++;
            } else {
                return 1;
            }
        } else {
            return 1;
        }
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
        text_window_render_page(state, state->scroll_row);
        text_window_update_scroll_indicators(state);
        return 1;
    }

    if (state->scroll_state == TEXT_WINDOW_SCROLL_PAGING) {
        if (g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
                state->page_start_row += state->rows_per_page;
            }
            if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS)
                || (state->page_start_row > state->last_page_row)) {
                g_window_record_count -= 4;
                g_window_render_record_count -= 1;
                g_window_render_object_count -= 5;
                return 0;
            }
            sound_play_effect(SOUND_SFX_TEXT_PAGE);
            text_window_render_page(state, state->page_start_row);
        } else if (g_world_map_input_new_button_presses & PSX_PAD_SQUARE) {
            state->scroll_state = TEXT_WINDOW_SCROLL_MANUAL;
            state->scroll_row = state->page_start_row;
            text_window_update_scroll_indicators(state);
            g_window_records[state->left_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
            return 1;
        }
        text_window_update_page_prompt(state);
        return 1;
    }

    return 0;
}

/* Renders the page of a scrollable text window that starts at `row` and
 * clips its optional extra render record against the window top.
 *
 * The final base copy is one point32 block copy (both loads precede both
 * stores); two field assignments reorder it. */
static void text_window_render_page(text_window_t* state, s32 row) {
    RECT bounds;
    point32_t point;
    s32 i;
    s32 offset;
    s32 top;
    s32 limit;

    g_menu_text_param_22 = row;
    g_menu_text_param_24 = row + state->rows_per_page;
    bounds.x = state->image_x;
    bounds.y = state->image_y;
    bounds.w = (state->text_width + 0x18) & 0xFFFC;
    bounds.h = state->rows_per_page * 16 + 16;
    window_build_render_record_image(
        state->render_record_index, bounds, state->image_coordinate_mode, g_window_image_buffer);
    for (i = 0; i < 2; i++) {
        if (state->text_substitutions[i] != -1) {
            g_text_substitution_values[i] = state->text_substitutions[i];
        }
    }
    if (state->extra_render_index != -1) {
        offset = row * 16;
        top = state->extra_base.y - offset;
        limit = state->base.y + 8;
        if (top >= limit) {
            g_window_render_records[state->extra_render_index].base.y = top;
            g_window_render_records[state->extra_render_index].bounds.y = state->extra_bounds.y;
            g_window_render_records[state->extra_render_index].bounds.h = state->extra_bounds.h;
            g_window_render_records[state->extra_render_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        } else if (top + state->extra_bounds.h >= limit) {
            g_window_render_records[state->extra_render_index].base.y = limit;
            g_window_render_records[state->extra_render_index].bounds.y = state->extra_bounds.y + offset;
            g_window_render_records[state->extra_render_index].bounds.h = state->extra_bounds.h - offset;
            g_window_render_records[state->extra_render_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        } else {
            g_window_render_records[state->extra_render_index].flags |= DISPLAY_OBJECT_HIDDEN;
        }
    }
    point.x = 8;
    point.y = 8;
    window_display_text_entry(state->render_record_index, state->text_id, point, g_window_image_buffer);
    window_load_render_record_image(state->render_record_index, g_window_image_buffer);
    g_window_render_records[state->render_record_index].base = state->base;
}

/* Show the text-page prompt while more pages remain. */
static void text_window_update_page_prompt(text_window_t* state) {
    if (state->page_start_row < state->last_page_row) {
        g_window_records[state->left_window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    } else {
        g_window_records[state->left_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
    }
}

/*
 * Position the scroll-thumb window between the up and down arrow windows and
 * hide the arrows that are not usable at the current scroll position.
 */
static void text_window_update_scroll_indicators(text_window_t* state) {
    s32 top;
    s32 bottom;

    if (state->page_start_row == 0) {
        g_window_records[state->middle_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_records[state->right_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_records[state->lower_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
        return;
    }

    top = g_window_records[state->right_window_index].position.y;
    bottom = g_window_records[state->middle_window_index].position.y;
    g_window_records[state->lower_window_index].position.y
        = bottom + (top - bottom - 0x12) * state->scroll_row / state->page_start_row + 0x12;
    g_window_records[state->lower_window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    if (state->scroll_row != 0) {
        g_window_records[state->middle_window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    } else {
        g_window_records[state->middle_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
    }
    if (state->scroll_row != state->page_start_row) {
        g_window_records[state->right_window_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
    } else {
        g_window_records[state->right_window_index].flags |= DISPLAY_OBJECT_HIDDEN;
    }
}

/* Play sound effect `sound_id` (WLDCORE's wrapper for sound_play_sfx). */
void sound_play_effect(s32 sound_id) {
    sound_play_sfx(sound_id);
}

/* Queue an audio command, servicing frame work while the queue is full.
 *
 * Accessing the count as a g_audio_queue member is what makes GCC reload it
 * after each array store. */
void audio_queue_add(audio_command_e command, s32 value) {
    while (g_audio_queue.count >= AUDIO_QUEUE_CAPACITY) {
        file_poll_load(&g_file_cd_state);
        audio_queue_process();
        VSync(0);
    }

    g_audio_queue.commands[g_audio_queue.count] = command;
    g_audio_queue.values[g_audio_queue.count] = value;
    g_audio_queue.count++;
}

/* Services the audio command FIFO: dequeues the next command when idle and
 * runs the current one until it has to wait for loading or a fade.
 *
 * PLAY_MUSIC loads track `value & 0xff` into slot `value >> 8`, or just
 * switches to it when the slot already holds that track. FADE_OUT fades the
 * music out when it is playing, FADE_IN switches to the current slot and
 * fades in to `volume`, and UNLOAD releases a slot (0: the current one); the
 * WAIT commands wait for the load or count down the fade. The dispatch loop is a
 * backward goto: a `for (;;)` loop lets loop.c hoist the queue addresses
 * into saved registers, which the target never does. The if/else in case 4
 * keeps CSE from carrying the value's address across the join. Cases 2,
 * 0x10 and 0x11 jump into the default arm's reset as the target does; a
 * duplicated reset is cross-jumped into the last copy instead. */
void audio_queue_process(void) {
    s32 i;
    s32 value;
    s32 slot;
    s32 track;
    s32* slot_track;

next:
    switch (g_audio_queue.current_command) {
    case AUDIO_COMMAND_NONE:
        if (g_audio_queue.count == 0) {
            return;
        }
        g_audio_queue.current_command = g_audio_queue.commands[0];
        g_audio_queue.current_value = g_audio_queue.values[0];
        for (i = 0; i < g_audio_queue.count - 1; i++) {
            g_audio_queue.commands[i] = g_audio_queue.commands[i + 1];
            g_audio_queue.values[i] = g_audio_queue.values[i + 1];
        }
        g_audio_queue.count--;
        break;
    case AUDIO_COMMAND_PLAY_MUSIC:
        if (g_file_still_loading != 0) {
            return;
        }
        slot = (g_audio_queue.current_value & 0xF00) >> 8;
        track = g_audio_queue.current_value & 0xFF;
        slot_track = &g_audio_queue.music.tracks[slot - 1];
        if (*slot_track == track) {
            g_audio_queue.current_slot = slot;
            g_audio_queue.current_command = AUDIO_COMMAND_NONE;
            music_switch_track(slot, 0, 0);
            return;
        }
        if (*slot_track != 0) {
            music_unload_scenario_slot(slot);
        }
        if (music_load_into_slot(track, slot) != 0) {
            g_audio_queue.current_command = AUDIO_COMMAND_WAIT_LOAD;
            g_audio_queue.current_slot = slot;
            *slot_track = track;
            return;
        }
        g_audio_queue.current_command = AUDIO_COMMAND_NONE;
        return;
    case AUDIO_COMMAND_FADE_OUT:
        if ((g_audio_queue.music.flags & AUDIO_MUSIC_PLAYING) == 0) {
            goto clear_command;
        }
        music_set_target(0, g_audio_queue.current_value << 2);
        g_audio_queue.current_command = AUDIO_COMMAND_WAIT_FADE_OUT;
        return;
    case AUDIO_COMMAND_FADE_IN:
        music_switch_track(g_audio_queue.current_slot, 0, 0);
        music_set_target(g_audio_queue.volume, g_audio_queue.current_value << 2);
        g_audio_queue.current_command = AUDIO_COMMAND_WAIT_FADE_IN;
        return;
    case AUDIO_COMMAND_UNLOAD:
        value = g_audio_queue.current_value;
        if (value != 0) {
            slot = value;
        } else {
            slot = g_audio_queue.current_slot;
        }
        music_unload_scenario_slot(slot);
        if (slot == g_audio_queue.current_slot) {
            g_audio_queue.music.flags &= ~AUDIO_MUSIC_PLAYING;
        }
        g_audio_queue.music.tracks[slot - 1] = 0;
    default:
    clear_command:
        g_audio_queue.current_command = AUDIO_COMMAND_NONE;
        break;
    case AUDIO_COMMAND_WAIT_LOAD:
        if (g_file_still_loading == 0) {
            goto clear_command;
        }
        return;
    case AUDIO_COMMAND_WAIT_FADE_OUT:
        if (--g_audio_queue.current_value != 0) {
            return;
        }
        g_audio_queue.music.flags &= ~AUDIO_MUSIC_PLAYING;
        music_stop();
        goto clear_command;
    case AUDIO_COMMAND_WAIT_FADE_IN:
        if (--g_audio_queue.current_value != 0) {
            return;
        }
        g_audio_queue.current_command = AUDIO_COMMAND_NONE;
        g_audio_queue.music.flags |= AUDIO_MUSIC_PLAYING;
        break;
    }

    goto next;
}

/* Queue a music change: fade out what is playing, start `value` (a packed
 * slot and music id) and fade it in over 0x10 steps. */
void audio_queue_start_music(s32 value) {
    if (g_audio_queue.music.flags & AUDIO_MUSIC_PLAYING) {
        audio_queue_add(AUDIO_COMMAND_FADE_OUT, 0x10);
    }
    audio_queue_add(AUDIO_COMMAND_PLAY_MUSIC, value);
    audio_queue_add(AUDIO_COMMAND_FADE_IN, 0x10);
}

/* Twin of file_wait_for_pending with the audio queue as the wait condition:
 * run frames until no command is queued or running. The two 32-bit busy
 * fields are read as g_audio_queue members rather than as separate
 * scalars. */
void audio_queue_wait_until_empty(void) {
    while (g_audio_queue.count != 0 || g_audio_queue.current_command != AUDIO_COMMAND_NONE) {
        file_poll_load(&g_file_cd_state);
        audio_queue_process();
        VSync(0);
    }
}
