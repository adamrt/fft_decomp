/* Window images and text, panel render threads, saved list cursors and scrollable text windows. */

#include "fft/gfx.h"
#include "fft/input.h"
#include "fft/menu.h"
#include "fft/menu_stack.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

extern struct screen_number_range g_window_panel_number_state;

static s16 g_window_panel_row_actions[16]
    = { 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000 };

static void window_get_image_vram_position(s32 mode, RECT bounds, point32_t* output);
static void text_window_render_page(text_window_t* state, s32 row);
static void text_window_update_page_prompt(text_window_t* state);
static void text_window_update_scroll_indicators(text_window_t* state);
static void window_init_animated_record(s32 index, s32 priority, s32 sequence);
static void window_number_panel_noop(void);

/* Set up render record `index` for a window image of `bounds`: unhide it, place it at bounds.x -
 * 0x80, bounds.y - 0x78, give it the window CLUT at (0, 0x1E1) and the texture page of the mode's
 * VRAM area, then draw the empty window frame into `image`. */
void window_build_render_record_image(s32 index, RECT bounds, s32 coordinate_mode, u32* image) {
    point32_t* clut;
    CVECTOR* color;
    point32_t transformed;

    g_window_gs_image_records[index].flags
        &= ~(DISPLAY_OBJECT_8BIT_CLUT | DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN);
    g_window_gs_image_records[index].base.x = bounds.x - 0x80;
    g_window_gs_image_records[index].base.y = bounds.y - 0x78;
    g_window_gs_image_records[index].coordinate_mode = coordinate_mode;
    /* Struct view of clut_x/clut_y: as plain s32 stores they could alias the
     * stacked image argument, whose load the target schedules after them. */
    clut = &g_window_gs_image_records[index].clut;
    clut->y = 0x1E1;
    clut->x = 0;
    g_window_gs_image_records[index].bounds = bounds;
    window_get_image_vram_position(coordinate_mode, bounds, &transformed);
    g_window_gs_image_records[index].tpage = GetTPage(0, 0, transformed.x, transformed.y);
    color = &g_window_gs_image_records[index].color;
    color->b = color->g = color->r = 0x80;
    g_window_gs_image_records[index].anim_counter = 0;
    screen_build_window_frame_image(bounds.w, bounds.h, (u16*)image);
}

/* The VRAM position of a window image at `bounds` in coordinate mode `mode`.
 * Images are 4-bit, four texels to a VRAM halfword: modes 0 and 1 put them in
 * the texture pages at x 0x180 and 0x1c0, mode 2 in the page at (0x240, 0x100).
 *
 * The callee receives the adjacent position and dimensions registers as one
 * eight-byte aggregate. Dimensions are preserved by the ABI but unused here.
 */
static void window_get_image_vram_position(s32 mode, RECT bounds, point32_t* output) {
    switch (mode) {
    case 0:
    case 1:
        output->x = bounds.x / 4 + 0x180 + mode * 64;
        output->y = bounds.y;
        break;
    case 2:
        output->x = (bounds.x / 4) + 0x240;
        output->y = bounds.y + 0x100;
        break;
    }
}

/* Upload a render record's window image at the VRAM position its coordinate
 * mode selects. The record's bounds are passed to the transform by value. */
void window_load_render_record_image(s32 index, u32* image) {
    point32_t transformed;
    RECT rect;

    window_get_image_vram_position(
        g_window_gs_image_records[index].coordinate_mode, g_window_gs_image_records[index].bounds, &transformed);
    rect.x = transformed.x;
    rect.y = transformed.y;
    rect.w = g_window_gs_image_records[index].bounds.w / 4;
    rect.h = g_window_gs_image_records[index].bounds.h;
    LoadImage(&rect, image);
}

/* Draw a text entry into the window image, using its record's width as the stride.
 * `point` is passed by value: GCC spills the two 32-bit register values to their
 * home slots before narrowing them into the origin record. */
void window_display_text_entry(s32 record_index, s32 text_id, point32_t point, u32* buffer) {
    glyph_blit_t origin;

    origin.rect.x = point.x;
    origin.rect.y = point.y;
    origin.stride = g_window_gs_image_records[record_index].bounds.w;
    menu_display_text_entry(text_id, (u8*)buffer, &origin);
}

/* Padded image size using proportional pixel widths. No retail caller. */
void window_measure_text_image(s32 text_id, size32_t* output) {
    s16 measured_width;
    s16 rows;

    text_measure_pixels(&measured_width, &rows, text_find_entry(text_id));
    output->width = (measured_width + 24) & 0xfffc;
    output->height = rows * 16 + 16;
}

/* Build and upload a window image, draw the entry at (8, 8) unless its id is zero,
 * and start the record's pop-in animation. */
void window_init_text_record(s32 record_index, RECT bounds, s32 coordinate_mode, s32 text_id, u32* image) {
    /* The pair is stack-resident because it is passed by value. */
    point32_t text_origin;

    window_build_render_record_image(record_index, bounds, coordinate_mode, image);
    text_origin.x = 8;
    text_origin.y = 8;
    if (text_id != 0) {
        window_display_text_entry(record_index, text_id, text_origin, image);
    }
    window_load_render_record_image(record_index, image);
    g_window_gs_image_records[record_index].flags |= DISPLAY_OBJECT_POP_IN;
}

/* Point image render record `index` at a VRAM rectangle given in halfwords: its
 * texture page, page-relative bounds in 8-bit texels, CLUT and a neutral 0x80 tint.
 * Clears the blank and hidden flags and adds `flags`. */
void window_init_vram_render_record(s32 index, RECT bounds, point32_t clut, s32 flags) {
    RECT* record_bounds;
    CVECTOR* rgb;

    g_window_gs_image_records[index].flags
        = (g_window_gs_image_records[index].flags & ~(DISPLAY_OBJECT_BLANK | DISPLAY_OBJECT_HIDDEN)) | flags;
    g_window_gs_image_records[index].clut = clut;
    g_window_gs_image_records[index].tpage = GetTPage(0, 0, bounds.x, bounds.y);
    record_bounds = &g_window_gs_image_records[index].bounds;
    record_bounds->x = (bounds.x & 0x3F) * 2;
    record_bounds->y = bounds.y & 0xFF;
    record_bounds->w = bounds.w * 2;
    record_bounds->h = bounds.h;
    rgb = &g_window_gs_image_records[index].color;
    rgb->b = rgb->g = rgb->r = 0x80;
    g_window_gs_image_records[index].anim_counter = 0;
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
    g_window_panel_render_state.text_image.source.x = 0;
    g_window_panel_render_state.text_image.source.y = 0;
    g_window_panel_render_state.text_image.source.w = text_width;
    g_window_panel_render_state.text_image.source.h = 0;
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
    g_window_panel_number_state.on_confirm = window_number_panel_noop;
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
    g_window_panel_render_state.text_image.source.x = 0;
    g_window_panel_render_state.text_image.source.y = 0;
    g_window_panel_render_state.text_image.source.w = width;
    g_window_panel_render_state.text_image.source.h = 0x20;
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

/* The on_confirm callback of the number panel's value range: nothing to do. */
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
    state->page_prompt_record = index;
    window_init_animated_record(index, state->priority, WINDOW_ANIM_TEXT_PAGE_PROMPT);
    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    state->up_arrow_record = index;
    window_init_animated_record(index, state->priority, WINDOW_ANIM_TEXT_SCROLL_UP);
    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    state->down_arrow_record = index;
    window_init_animated_record(index, state->priority, WINDOW_ANIM_TEXT_SCROLL_DOWN);
    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    state->scroll_thumb_record = index;
    window_init_animated_record(index, state->priority, WINDOW_ANIM_TEXT_SCROLL_THUMB);
    render_index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    state->render_record_index = render_index;
    g_window_gs_image_records[render_index].flags |= DISPLAY_OBJECT_POP_IN;
    g_window_gs_image_records[state->render_record_index].priority = state->priority;
    g_window_gs_image_records[state->render_record_index].base = state->base;

    padded_width = (state->text_width + 0x18) & 0xfffc;
    window_x = state->base.x + padded_width;
    window_y = state->base.y + state->rows_per_page * 16 + 16;
    if (padded_width % 8 == 4) {
        window_x -= 14;
    } else {
        window_x -= 12;
    }
    g_window_records[state->page_prompt_record].position.x = window_x - 14;
    g_window_records[state->page_prompt_record].position.y = window_y - 22;
    lower_y = window_y - 26;
    g_window_records[state->up_arrow_record].position.x = window_x;
    do {
        g_window_records[state->up_arrow_record].position.y = state->base.y + 8;
    } while (0);
    g_window_records[state->down_arrow_record].position.x = window_x;
    g_window_records[state->down_arrow_record].position.y = lower_y;
    g_window_records[state->scroll_thumb_record].position.x = window_x;
    g_window_records[state->scroll_thumb_record].position.y = lower_y;

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
            g_window_records[state->up_arrow_record].flags |= DISPLAY_OBJECT_HIDDEN;
            g_window_records[state->down_arrow_record].flags |= DISPLAY_OBJECT_HIDDEN;
            g_window_records[state->scroll_thumb_record].flags |= DISPLAY_OBJECT_HIDDEN;
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
            g_window_records[state->page_prompt_record].flags |= DISPLAY_OBJECT_HIDDEN;
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

    g_menu_text_first_line = row;
    g_menu_text_line_limit = row + state->rows_per_page;
    bounds.x = state->image_position.x;
    bounds.y = state->image_position.y;
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
            g_window_gs_image_records[state->extra_render_index].base.y = top;
            g_window_gs_image_records[state->extra_render_index].bounds.y = state->extra_bounds.y;
            g_window_gs_image_records[state->extra_render_index].bounds.h = state->extra_bounds.h;
            g_window_gs_image_records[state->extra_render_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        } else if (top + state->extra_bounds.h >= limit) {
            g_window_gs_image_records[state->extra_render_index].base.y = limit;
            g_window_gs_image_records[state->extra_render_index].bounds.y = state->extra_bounds.y + offset;
            g_window_gs_image_records[state->extra_render_index].bounds.h = state->extra_bounds.h - offset;
            g_window_gs_image_records[state->extra_render_index].flags &= ~DISPLAY_OBJECT_HIDDEN;
        } else {
            g_window_gs_image_records[state->extra_render_index].flags |= DISPLAY_OBJECT_HIDDEN;
        }
    }
    point.x = 8;
    point.y = 8;
    window_display_text_entry(state->render_record_index, state->text_id, point, g_window_image_buffer);
    window_load_render_record_image(state->render_record_index, g_window_image_buffer);
    g_window_gs_image_records[state->render_record_index].base = state->base;
}

/* Show the text-page prompt while more pages remain. */
static void text_window_update_page_prompt(text_window_t* state) {
    if (state->page_start_row < state->last_page_row) {
        g_window_records[state->page_prompt_record].flags &= ~DISPLAY_OBJECT_HIDDEN;
    } else {
        g_window_records[state->page_prompt_record].flags |= DISPLAY_OBJECT_HIDDEN;
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
        g_window_records[state->up_arrow_record].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_records[state->down_arrow_record].flags |= DISPLAY_OBJECT_HIDDEN;
        g_window_records[state->scroll_thumb_record].flags |= DISPLAY_OBJECT_HIDDEN;
        return;
    }

    top = g_window_records[state->down_arrow_record].position.y;
    bottom = g_window_records[state->up_arrow_record].position.y;
    g_window_records[state->scroll_thumb_record].position.y
        = bottom + (top - bottom - 0x12) * state->scroll_row / state->page_start_row + 0x12;
    g_window_records[state->scroll_thumb_record].flags &= ~DISPLAY_OBJECT_HIDDEN;
    if (state->scroll_row != 0) {
        g_window_records[state->up_arrow_record].flags &= ~DISPLAY_OBJECT_HIDDEN;
    } else {
        g_window_records[state->up_arrow_record].flags |= DISPLAY_OBJECT_HIDDEN;
    }
    if (state->scroll_row != state->page_start_row) {
        g_window_records[state->down_arrow_record].flags &= ~DISPLAY_OBJECT_HIDDEN;
    } else {
        g_window_records[state->down_arrow_record].flags |= DISPLAY_OBJECT_HIDDEN;
    }
}
