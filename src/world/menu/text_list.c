/* Text page redraws on scroll and AT list building for the screen. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/screen.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

static menu_list_page_t g_text_list_pages[2] = { 0 };

/* Page a three-column text screen while Square is held: Up and Down move by a page within the
 * hidden rows, and a pending redraw request also counts. When anything changed, redraw the columns
 * into a new image, upload it (resetting the image ring to its first row) and return it; otherwise
 * return 0. */
static u8* text_page_scroll_update(menu_entry_t* entry, s32* row_offset, s32* ring, s32* unused_cursor) {
    menu_text_layout_t* layout = entry->context;
    u32* input0;
    u32* input1;
    RECT rect;
    RECT clear;
    s32 changed;
    s32 offset;
    u8* buffer;

    input0 = script_get_controller_input_pointer(0);
    input1 = script_get_controller_input_pointer(1);
    rect.x = entry->vram_origin.x;
    rect.y = entry->vram_origin.y;
    rect.w = entry->inner_width >> 2;
    rect.h = entry->inner_height;
    changed = 0;
    if (!(*input1 & PSX_PAD_SQUARE)
        && g_threads[g_thread_current_id].task_words[MENU_LIST_THREAD_WORD_REDRAW_REQUEST] == 0) {
        return 0;
    }
    if ((*input0 & PSX_PAD_UP) && *row_offset != 0) {
        offset = *row_offset - layout->row_count;
        *row_offset = offset;
        if (offset < 0) {
            *row_offset = 0;
        }
        g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
        changed = 1;
    } else if ((*input0 & PSX_PAD_DOWN) && *row_offset != layout->hidden_rows) {
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
    if (changed == 0) {
        return 0;
    }
    buffer = menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
    menu_clear_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
    screen_draw_text_columns(entry, row_offset, buffer);
    *ring = 0;
    LoadImage(&rect, buffer);
    mem_copy_bytes_nonempty(&clear, &rect, sizeof(clear));
    clear.h = 2;
    clear.x = rect.x;
    clear.y = ((rect.y - 2) & 0xFF) + (rect.y & 0xFF00);
    clear.w = rect.w;
    ClearImage(&clear, 0, 0, 0);
    return buffer;
}

/*
 * Scrolling three-column text list thread (an older sibling of
 * ability_list_scrolling_thread with a fixed page pair, g_text_list_pages, and
 * screen_window_frame_thread in the frame thread slot). The menu-screen library's
 * variant of at_list_run: the formation screens, the world map menus and the title's music
 * test run it.
 *
 * Draws the visible rows into a text image, then runs the input loop on two
 * alternating packet pages. Cursor moves past either end start a one-row
 * scroll whose new row is drawn into the image ring; page scrolling is left
 * to text_page_scroll_update, and the scroll-bar thumb tracks
 * entry->selected_index. The list layout is read into `binding` for sizing and
 * copied to `layout` before the loop: with one variable the allocator gives
 * the image buffer the last callee-saved register instead of the layout.
 */
void screen_text_list_thread(void) {
    image_layout_t image_parameters;
    RECT image_rect;
    RECT clear;
    RECT texture_window;
    SPRT icons0[2];
    SPRT icons1[2];
    SPRT icons2[2];
    s32 row_offset;
    s32 ring;
    s32 cursor;
    u32* input0;
    u32* input1;
    SPRT* frame;
    u8* buffer;
    s32 accum;
    s32 timer;
    s32 redraw_pending;
    menu_list_page_t* page;
    menu_entry_t* entry;
    menu_text_layout_t* layout;
    menu_text_layout_t* binding;
    s32 delta;
    s32 i;
    s32 column;
    s32 index;
    s32 arrow_u;
    s32 arrow_y;
    s32 step;
    s32 top;
    s32 span;
    s32 repeat;

    page = g_text_list_pages;
    timer = 0;
    input0 = script_get_controller_input_pointer(0);
    input1 = script_get_controller_input_pointer(1);
    row_offset = 0;
    cursor = 0;

    entry = (menu_entry_t*)thread_current_parameter_1_get();
    binding = entry->context;
    entry->window.h = binding->row_count * 16 + 0x1C;
    entry->inner_height = binding->row_count * 16 + 0x1C;
    entry->text_image.source.h = binding->row_count * 16 + 0x1C;

    image_parameters.source.x = 0;
    image_parameters.source.y = 0x7E;
    image_parameters.x_screen_offset = 8;
    image_parameters.y_screen_offset = 0xC;
    image_parameters.source.w = entry->window.w - 0x12;
    image_parameters.source.h = entry->window.h - 0x1A;

    gfx_set_draw_mode_for_texture_page(&page->icon_mode, MENU_TEXTURE_PAGE_FRAME);
    texture_window.x = 0;
    texture_window.y = entry->vram_origin.y & 0xFF;
    texture_window.w = 0x100;
    texture_window.h = 0x100;

    for (i = 0; i < 2; i++) {
        g_text_list_pages[i].icons.icons[0] = &icons0[i];
        g_text_list_pages[i].icons.icons[1] = &icons1[i];
        g_text_list_pages[i].icons.icons[2] = &icons2[i];
        menu_build_window_sprites(&image_rect, entry, &g_text_list_pages[i].icons);
    }

    menu_init_scaled_draw_area_packets(&page->draw_areas);
    SetDrawMode(&page->text_mode, 1, 0, GetTPage(0, 0, entry->vram_origin.x & 0xFFC0, entry->vram_origin.y & 0xFF00),
        &texture_window);

    for (i = 0, arrow_u = 0xD8; i < 2; i++) {
        menu_init_sprt(&page->arrows[i]);
        page->arrows[i].clut = 0x7D7C;
        page->arrows[i].w = 8;
        page->arrows[i].h = 0x10;
        page->arrows[i].u0 = arrow_u;
        page->arrows[i].v0 = 0;
        page->arrows[i].x0 = entry->window.x + entry->window.w - 0xF;
        arrow_y = i * (entry->window.h - 0x32) + 0x10;
        page->arrows[i].y0 = entry->window.y + arrow_y;
        arrow_u += 8;
    }

    menu_init_sprt(&page->thumb);
    page->thumb.u0 = 0x10;
    page->thumb.v0 = 0x10;
    page->thumb.w = 8;
    page->thumb.h = 8;
    page->thumb.clut = 0x7D7C;
    menu_init_sprt(&page->frame);
    gfx_set_image_primitive_layout(&page->frame, &entry->vram_origin, &entry->window, &image_parameters);

    menu_init_sprite_array(page->arrow_marks, 2, MENU_CLUT_NORMAL);
    page->arrow_marks[0].w = 6;
    page->arrow_marks[0].h = 0xC;
    page->arrow_marks[1].w = 4;
    page->arrow_marks[1].h = 4;
    page->arrow_marks[0].u0 = 0xE8;
    page->arrow_marks[1].u0 = 0xE8;
    page->arrow_marks[0].v0 = 0;
    page->arrow_marks[0].x0 = page->arrows[0].x0 - 1;
    page->arrow_marks[0].y0 = page->arrows[0].y0 + 3;
    page->arrow_marks[1].v0 = 0xC;
    page->arrow_marks[1].x0 = page->arrows[1].x0;
    page->arrow_marks[1].y0 = page->arrows[1].y0 + 0xE;

    /* The first page's buffer is freed after the first yield, once its upload has run. */
    buffer = menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
    menu_clear_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
    redraw_pending = 1;
    thread_start(SCREEN_THREAD_SLOT_AT_LIST_FRAME, screen_window_frame_thread);
    thread_parameters_set(SCREEN_THREAD_SLOT_AT_LIST_FRAME, (s32)&entry->window, 0, 0);
    row_offset = entry->select_text_table;
    cursor = entry->selected_index - row_offset;
    screen_draw_text_columns(entry, &row_offset, buffer);
    ring = 0;
    image_rect.w = g_menu_window_text_pen.stride >> 2;
    LoadImage(&image_rect, (u32*)buffer);

    /* The window sprite starts two lines above the first row (v0 = ring * 16 + accum - 2), so the
     * two lines above the text ring's first row must stay blank. */
    clear.h = 2;
    clear.x = image_rect.x;
    clear.y = ((image_rect.y - 2) & 0xFF) + (image_rect.y & 0xFF00);
    clear.w = image_rect.w;
    ClearImage(&clear, 0, 0, 0);

    /* The loop draws from the two pages alternately. The copy also took page 0's icon pointers,
     * so point each page back at its own sprites. */
    mem_copy_bytes_nonempty(&g_text_list_pages[1], g_text_list_pages, sizeof(menu_list_page_t));
    for (i = 0; i < 2; i++) {
        g_text_list_pages[i].icons.icons[0] = &icons0[i];
        g_text_list_pages[i].icons.icons[1] = &icons1[i];
        g_text_list_pages[i].icons.icons[2] = &icons2[i];
    }

    layout = binding;
    delta = 0;
    accum = 0;
    *input0 = 0;
    i = 0;
    *input1 = 0;
    for (;;) {
        page = &g_text_list_pages[i & 1];
        frame = &page->frame;

        /* The frame thread dims its border while the thread before this one runs. */
        if (thread_is_previous_running() != 0 && g_menu_input_lock_mode == 0) {
            thread_parameters_set(SCREEN_THREAD_SLOT_AT_LIST_FRAME, (s32)&entry->window, 1, 0);
        } else {
            thread_parameters_set(SCREEN_THREAD_SLOT_AT_LIST_FRAME, (s32)&entry->window, 0, 0);
        }

        if (delta == 0 && menu_can_accept_input() != 0 && i != 0) {
            /* Square with up or down pages through the list instead (text_page_scroll_update). */
            if ((*input0 & PSX_PAD_UP) && !(*input1 & PSX_PAD_SQUARE)) {
                if (cursor != 0) {
                    cursor--;
                    g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                } else if (row_offset != 0) {
                    delta = -1;
                    g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                } else if (layout->hidden_rows == 0) {
                    cursor = layout->row_count - 1;
                    if (layout->row_count != 1) {
                        g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                    }
                }
            } else if ((*input0 & PSX_PAD_DOWN) && !(*input1 & PSX_PAD_SQUARE)) {
                if (cursor != layout->row_count - 1) {
                    cursor++;
                    g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                } else if (row_offset != layout->hidden_rows) {
                    delta++;
                    g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                } else if (row_offset == 0) {
                    /* row_offset equals hidden_rows here, so the cursor wraps only when no rows are hidden. */
                    cursor = 0;
                    if (layout->row_count != 1) {
                        g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                    }
                }
            } else {
                timer = 0;
            }

            /* Draw the row scrolling into view into the ring slot below the visible rows, or above them.
             * Its buffer is freed when the scroll completes. */
            if (delta != 0) {
                if (delta > 0) {
                    image_rect.y = (((ring + layout->row_count) * 16) & 0xFF) + entry->vram_origin.y;
                } else {
                    image_rect.y = (((ring - 1) * 16) & 0xFF) + entry->vram_origin.y;
                }
                image_rect.h = 0x10;
                buffer = menu_alloc_memory(0x800);
                menu_clear_render_buffer(buffer, 0x800);
                g_menu_window_text_pen.stride = entry->inner_width;

                for (column = 0; column < 3; column++) {
                    index = cursor + row_offset + delta;
                    menu_set_text_origin(layout->x[column], 0);
                    if (layout->mode[column] != MENU_TEXT_COLUMN_HIDDEN) {
                        g_menu_text_palette_offset = layout->text_colors[column][index];
                    }
                    if (g_menu_text_palette_offset == 4) {
                        g_menu_window_text_pen.color = 0x44444444;
                    }
                    if (layout->mode[column] == MENU_TEXT_COLUMN_TEXT) {
                        menu_display_text_entry(layout->text_ids[column][index], buffer, &g_menu_window_text_pen);
                    } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER) {
                        menu_display_number_entry(layout->text_ids[column][index], 2, buffer, &g_menu_window_text_pen);
                    } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER_GLYPHS) {
                        menu_draw_large_number_glyphs(
                            layout->text_ids[column][index], 2, buffer, &g_menu_window_text_pen);
                    }
                }

                g_menu_window_text_pen.color = 0;
                g_menu_text_palette_offset = 0;
                LoadImage(&image_rect, (u32*)buffer);
                clear.h = 2;
                clear.x = image_rect.x;
                clear.y = ((image_rect.y - 2) & 0xFF) + (image_rect.y & 0xFF00);
                clear.w = image_rect.w;
                ClearImage(&clear, 0, 0, 0);
            }
        }

        thread_yield();
        /* A redrawn page's buffer is freed a frame late, after its upload has run. */
        if (redraw_pending != 0) {
            menu_free_memory(buffer);
            redraw_pending = 0;
        }
        if (delta == 0 && menu_can_accept_input() != 0) {
            buffer = text_page_scroll_update(entry, &row_offset, &ring, &cursor);
            /* The returned buffer doubles as the flag: 0 when no page was redrawn. */
            redraw_pending = (s32)buffer;
            ClearImage(&clear, 0, 0, 0);
        }

        entry->selected_index = cursor + row_offset;
        entry->select_text_table = row_offset;
        if (menu_should_close_thread(input0) != 0) {
            break;
        }

        if (delta == 0) {
            menu_handle_action(entry, cursor);
        }
        menu_handle_cancel_input(entry);

        if ((g_script_event_speed == 1 && !(script_event_initial_repeat_counter_get() & 1))
            || g_script_event_speed == 2) {
            if (timer < g_menu_scroll_accel_delay) {
                step = g_menu_scroll_slow_step;
            } else {
                step = g_menu_scroll_fast_step;
            }
            accum += delta * (step << 1);
        }
        timer += g_script_event_speed;

        /* A whole 16-pixel row has scrolled: commit it. A direction held past the initial repeat
         * delay is fed back into the input so the scroll keeps going. */
        if (accum < -0xF || accum > 0xF) {
            ring += delta;
            row_offset += delta;
            repeat = script_event_initial_repeat_counter_get();
            delta = 0;
            accum = 0;
            if (repeat >= g_input_repeat_initial_delay / g_script_event_speed) {
                *input0 |= *input1 & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
            menu_free_memory(buffer);
        }

        frame->v0 = ring * 16 + accum - 2;
        if (thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
            frame->clut = 0x7F7D;
        } else {
            frame->clut = 0x7FFC;
        }

        if (layout->hidden_rows != 0 && menu_can_accept_input() != 0) {
            if (row_offset != 0) {
                gfx_draw_or_append_gpu_primitive(&page->arrows[0]);
                gfx_draw_or_append_gpu_primitive(&page->arrow_marks[0]);
            }
            if (row_offset != layout->hidden_rows) {
                gfx_draw_or_append_gpu_primitive(&page->arrows[1]);
                gfx_draw_or_append_gpu_primitive(&page->arrow_marks[1]);
            }

            span = page->arrows[1].y0;
            top = page->arrows[0].y0 + page->arrows[0].h;
            span -= top;
            page->thumb.x0 = page->arrows[0].x0;
            page->thumb.y0 = top + span * entry->selected_index / (layout->row_count + layout->hidden_rows) - 2;
            gfx_draw_or_append_gpu_primitive(&page->thumb);
        }

        gfx_build_scaled_draw_area_pair_swapped(&page->draw_areas, &entry->window, i, g_gfx_active_buffer_index);
        menu_select_icon_sprite_clut(page->icons.icons[0]);
        if (entry->header_id >= 5) {
            gfx_draw_or_append_gpu_primitive(page->icons.icons[0]);
        }
        gfx_draw_or_append_gpu_primitive(&page->draw_areas.areas[1]);
        gfx_draw_or_append_gpu_primitive(&page->icon_mode);
        gfx_draw_or_append_gpu_primitive(frame);
        gfx_draw_or_append_gpu_primitive(&page->text_mode);
        gfx_draw_or_append_gpu_primitive(&page->draw_areas);

        menu_select_icon_cluts(&page->icons.base);
        menu_update_icon_cursor_sprites(entry, &page->icons.base, i, cursor);
        i++;
        /* An x of 1000 keeps icon sprite 0 off screen. */
        page->icons.base.sprites[0].x0 = 1000;
        page->icons.base.sprites[1].y0 += 4;
        page->icons.base.sprites[2].y0 += 4;
        menu_submit_icon_primitives(&page->icons.base);
    }

    /* A nonzero third parameter ends the frame thread. */
    thread_parameters_set(SCREEN_THREAD_SLOT_AT_LIST_FRAME, 0, 0, 1);
    entry->select_text_table = row_offset;
    /* A row scroll still under way owns its row buffer. */
    if (accum & 0xF) {
        menu_free_memory(buffer);
    }
    thread_yield();
    thread_current_exit();
}
