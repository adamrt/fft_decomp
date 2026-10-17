/* The scrolling three-column ability list thread. */

#include "fft/ability_list.h"
#include "fft/ai.h"
#include "fft/formation_ability.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_cursor.h"
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

/*
 * Scrolling three-column ability list thread (WORLD's body, reached from at_list_ability_preview_thread).
 *
 * Draws the visible rows into a text image, then runs the input loop on two
 * alternating packet pages. Cursor moves past either end start a one-row
 * scroll whose new row is drawn into the image ring at row `ring * 16`;
 * screen_redraw_text_page_on_scroll_in_rect handles page scrolling, and
 * the scroll-bar thumb tracks entry->selected_index. Right/Left toggle the
 * third column's text when the first two columns are text. Parameter 2 keeps
 * the thread alive after the list closes.
 *
 * Retail bug (QUIRKS.md): the frame sprite pointer is passed to SetSemiTrans
 * by address, so the bit lands in a stack byte past the pointer and the sprite
 * is unchanged; the call also keeps `frame` on the stack.
 */
void ability_list_scrolling_thread(void) {
    image_layout_t window;
    RECT image_rect;
    RECT texture_window;
    SPRT icons0[2];
    SPRT icons1[2];
    SPRT icons2[2];
    RECT clear;
    s32 row_offset;
    SPRT* frame;
    s16 toggle;
    s32 ring;
    s32 cursor;
    menu_list_page_t* base;
    menu_scroll_text_layout_t* layout;
    s32 scroll_accum;
    s32 scroll_delta;
    s32 hold_time;
    s32 redraw_pending;
    s32 row_buffer_live;
    menu_entry_t* entry;
    menu_list_page_t* page;
    u8* buffer;
    s32 i;
    s32 column;
    s32 index;
    s16 value;
    s32 step;
    s32 top;
    s32 arrow_u;
    s32 arrow_y;
    s32 repeat;
    s32 span;

    toggle = 0;
    page = g_formation_ability_list_pages[g_thread_current_id];
    entry = (menu_entry_t*)thread_get_current_parameter_1();
    layout = entry->context;
    entry->window.h = layout->columns.row_count * 16 + 0x1C;
    entry->inner_height = layout->columns.row_count * 16 + 0x1C;
    row_buffer_live = 0;
    entry->text_image.source.h = layout->columns.row_count * 16 + 0x1C;
    window.source.x = 0x108 - entry->inner_width;
    window.source.y = 0xFE;
    window.x_screen_offset = 8;
    window.y_screen_offset = 0xC;
    hold_time = 0;
    window.source.w = entry->inner_width - 8;
    window.source.h = entry->window.h - 0x1A;
    base = page;

    /* The frame sprite starts two lines above the first row (v0 = ring * 16 + accum - 2), so the
     * two lines above the text ring's first row must stay blank. */
    clear.x = window.source.x / 4 + entry->vram_origin.x;
    clear.y = (entry->vram_origin.y & 0xFF00) + 0xFE;
    clear.w = window.source.w / 4;
    clear.h = 2;
    ClearImage(&clear, 0, 0, 0);

    texture_window.x = 0;
    texture_window.y = 0x80;
    texture_window.w = 0x100;
    texture_window.h = 0x80;
    for (i = 0; i < 2; i++) {
        page[i].icons.icons[0] = &icons0[i];
        page[i].icons.icons[1] = &icons1[i];
        page[i].icons.icons[2] = &icons2[i];
        menu_build_window_sprites(&image_rect, entry, &page[i].icons);
    }
    SetDrawMode(&page->icon_mode, 0, 0, GetTPage(0, 2, 0x3C0, 0x100), &g_gfx_texture_window);
    SetDrawMode(&page->text_mode, 1, 0, GetTPage(0, 1, entry->vram_origin.x & 0xFFC0, entry->vram_origin.y & 0xFF00),
        &texture_window);

    /* Scroll arrows, the scroll-bar thumb and the frame sprite. */
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
    gfx_set_image_primitive_layout(&page->frame, &entry->vram_origin, &entry->window, &window);
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

    menu_build_and_upload_window_frame_image(
        entry->inner_width, entry->inner_height, &image_rect, MENU_WINDOW_FRAME_ALTERNATE_UPLOAD);
    buffer = menu_alloc_memory(entry->inner_width * entry->inner_height / 2);
    menu_clear_render_buffer(buffer, window.source.w * window.source.h / 2);

    /* Restore the saved scroll position, falling back to the top if the cursor would leave the view. */
    row_offset = layout->row_offset;
    if (layout->columns.hidden_rows < row_offset) {
        row_offset = layout->columns.hidden_rows;
    }
    cursor = entry->selected_index - row_offset;
    if (cursor >= layout->columns.row_count) {
        row_offset = 0;
        cursor = 0;
    }

    /* The first page's buffer is freed after the first yield, once its upload has run. */
    menu_draw_text_columns_narrow(entry, &row_offset, buffer);
    redraw_pending = 1;
    ring = 0;
    image_rect.x = window.source.x / 4 + entry->vram_origin.x;
    image_rect.y = 0x80;
    image_rect.w = window.source.w / 4;
    image_rect.h = window.source.h;
    LoadImage(&image_rect, (u32*)buffer);

    /* The loop draws from the two pages alternately. The copy also took page 0's icon pointers,
     * so point each page back at its own sprites. */
    mem_copy_bytes_nonempty(&base[1], base, sizeof(menu_list_page_t));
    for (i = 0; i < 2; i++) {
        base[i].icons.icons[0] = &icons0[i];
        base[i].icons.icons[1] = &icons1[i];
        base[i].icons.icons[2] = &icons2[i];
    }

    i = 0;
    scroll_accum = 0;
    scroll_delta = 0;
    for (;;) {
        page = &base[i & 1];
        frame = &page->frame;
        SetSemiTrans(&frame, 1);

        /* Input waits for a running row scroll to finish and skips the first frame. */
        if (scroll_delta == 0 && menu_can_accept_input() != 0 && i != 0) {
            if (layout->columns.mode[0] == MENU_TEXT_COLUMN_TEXT && layout->columns.mode[1] == MENU_TEXT_COLUMN_TEXT) {
                if (g_script_event_input & (PSX_PAD_RIGHT | PSX_PAD_LEFT)) {
                    toggle ^= 1;
                }
                layout->columns.text_ids[2] = &toggle;
            }

            /* Square with up or down pages through the list instead (the redraw call after the yield). */
            if ((g_script_event_input & PSX_PAD_UP)
                && (!(g_script_event_current_input & PSX_PAD_SQUARE) || g_menu_input_mode != MENU_INPUT_PLAYER)) {
                if (cursor != 0) {
                    cursor--;
                    sound_queue_sfx_cursor_move();
                } else if (row_offset != 0) {
                    scroll_delta = -1;
                    sound_queue_sfx_cursor_move();
                } else if (layout->columns.hidden_rows == 0) {
                    cursor = layout->columns.row_count - 1;
                    sound_queue_sfx_cursor_move();
                }
            } else if ((g_script_event_input & PSX_PAD_DOWN)
                && (!(g_script_event_current_input & PSX_PAD_SQUARE) || g_menu_input_mode != MENU_INPUT_PLAYER)) {
                if (cursor != layout->columns.row_count - 1) {
                    cursor++;
                    sound_queue_sfx_cursor_move();
                } else if (row_offset != layout->columns.hidden_rows) {
                    scroll_delta++;
                    sound_queue_sfx_cursor_move();
                } else if (row_offset == 0) {
                    cursor = 0;
                    sound_queue_sfx_cursor_move();
                }
            } else {
                hold_time = 0;
            }

            /* Draw the row scrolling into view into the ring slot below the visible rows, or above them. */
            if (scroll_delta != 0) {
                if (scroll_delta > 0) {
                    image_rect.y = (((ring + layout->columns.row_count) * 16) & 0x7F) + entry->vram_origin.y;
                } else {
                    image_rect.y = (((ring - 1) * 16) & 0x7F) + entry->vram_origin.y;
                }
                image_rect.h = 0x10;
                image_rect.y += 0x80;
                row_buffer_live = 1;
                buffer = menu_alloc_memory(window.source.w * 8);
                menu_clear_render_buffer(buffer, window.source.w * 8);
                g_menu_window_text_pen.stride = window.source.w;

                for (column = 0; column < 3; column++) {
                    index = cursor + row_offset + scroll_delta;
                    menu_set_text_origin(layout->columns.x[column], 0);
                    if (layout->columns.mode[column] != MENU_TEXT_COLUMN_HIDDEN) {
                        g_menu_text_palette_offset = layout->columns.text_colors[column][index];
                    }
                    g_menu_window_text_pen.color = 0;
                    if (g_menu_text_palette_offset == 4) {
                        g_menu_window_text_pen.color = 0x44444444;
                    }
                    if (layout->columns.mode[column] == MENU_TEXT_COLUMN_TEXT) {
                        menu_display_text_entry(
                            layout->columns.text_ids[column][index], buffer, &g_menu_window_text_pen);
                        g_menu_text_palette_offset = layout->columns.text_colors[column][index];
                        if (g_thread_contexts[g_thread_current_id].function_parameter_4 != 0
                            && (g_ai_workspace_ptr->ability_list.flags[index] & MENU_ABILITY_FLAG_REFLECTABLE)) {
                            menu_set_text_origin(layout->columns.x[column] + 0x50, 0);
                            menu_display_text_entry(0x5088, buffer, &g_menu_window_text_pen);
                        }
                    } else if (layout->columns.mode[column] == MENU_TEXT_COLUMN_NUMBER) {
                        menu_display_number_entry(
                            layout->columns.text_ids[column][index], 2, buffer, &g_menu_window_text_pen);
                    } else if (layout->columns.mode[column] == MENU_TEXT_COLUMN_NUMBER_GLYPHS) {
                        if ((column == 2 || (column == 1 && layout->columns.mode[2] == MENU_TEXT_COLUMN_HIDDEN))
                            && layout->columns.text_ids[column][index] >= 0
                            && layout->columns.text_ids[column][index] < 2
                            && (g_ai_workspace_ptr->ability_list.skill_ct[index] == 0xFF
                                || g_ai_workspace_ptr->ability_list.skill_ct[index] == 0)) {
                            g_menu_window_text_pen.rect.y += 2;
                            menu_draw_stacked_glyph_pair(buffer, &g_menu_window_text_pen);
                        } else {
                            value = layout->columns.text_ids[column][index];
                            if (value & 0x7FF) {
                                if ((value & 0x7FF) >= 100) {
                                    g_menu_text_palette_offset = 0;
                                    g_menu_window_text_pen.rect.x += 3;
                                    menu_display_text_entry(0x509D, buffer, &g_menu_window_text_pen);
                                } else {
                                    menu_draw_large_number_glyphs(value, 2, buffer, &g_menu_window_text_pen);
                                }
                            }
                        }
                    }
                }

                g_menu_text_palette_offset = 0;
                g_menu_window_text_pen.color = 0;
                LoadImage(&image_rect, (u32*)buffer);
                /* Blank the two lines above the new row for the same reason as the setup clear. */
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
        if (scroll_delta == 0 && menu_can_accept_input() != 0) {
            buffer = menu_redraw_text_page_on_scroll_in_rect(entry, &row_offset, &ring, &cursor, &window.source);
            /* The returned buffer doubles as the flag: 0 when no page was redrawn. */
            redraw_pending = (s32)buffer;
            ClearImage(&clear, 0, 0, 0);
        }
        entry->selected_index = cursor + row_offset;
        layout->row_offset = row_offset;
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }

        menu_select_icon_cluts(&page->icons.base);
        menu_select_icon_sprite_clut(page->icons.icons[0]);
        menu_select_icon_sprite_clut(page->icons.icons[1]);
        menu_select_icon_sprite_clut(page->icons.icons[2]);
        if (thread_is_previous_running() != 0 || g_menu_input_lock_mode == 1) {
            frame->clut = 0x7F7D;
        } else {
            frame->clut = 0x7FFC;
        }
        menu_update_icon_cursor_sprites(entry, &page->icons.base, i, cursor);

        /* Scroll arrows show only where rows are hidden; the thumb tracks the selected row. */
        if (layout->columns.hidden_rows != 0 && thread_is_previous_running() == 0) {
            if (row_offset != 0) {
                gfx_draw_or_append_gpu_primitive(&page->arrows[0]);
                gfx_draw_or_append_gpu_primitive(&page->arrow_marks[0]);
            }
            if (row_offset != layout->columns.hidden_rows) {
                gfx_draw_or_append_gpu_primitive(&page->arrows[1]);
                gfx_draw_or_append_gpu_primitive(&page->arrow_marks[1]);
            }
            span = page->arrows[1].y0;
            top = page->arrows[0].y0 + page->arrows[0].h;
            span -= top;
            page->thumb.x0 = page->arrows[0].x0;
            page->thumb.y0
                = top + span * entry->selected_index / (layout->columns.row_count + layout->columns.hidden_rows) - 2;
            gfx_draw_or_append_gpu_primitive(&page->thumb);
        }
        if (g_screen_scrolling_list_depth < 2 && scroll_delta == 0) {
            menu_handle_action(entry, cursor);
        }
        menu_handle_cancel_input(entry);

        /* Advance a row scroll every other frame at speed 1 and every frame at speed 2, faster once
         * the button has been held past the acceleration delay. */
        if ((g_script_event_speed == 1 && !(script_get_event_initial_repeat_counter() & 1))
            || g_script_event_speed == 2) {
            if (hold_time < g_menu_scroll_accel_delay) {
                step = g_menu_scroll_slow_step;
            } else {
                step = g_menu_scroll_fast_step;
            }
            scroll_accum += scroll_delta * (step << 1);
        }
        hold_time += g_script_event_speed;

        /* A whole 16-pixel row has scrolled: commit it. A direction held past the initial repeat
         * delay is fed back into the input so the scroll keeps going. */
        if (scroll_accum < -0xF || scroll_accum > 0xF) {
            ring += scroll_delta;
            row_offset += scroll_delta;
            repeat = script_get_event_initial_repeat_counter();
            scroll_accum = 0;
            scroll_delta = 0;
            if (repeat >= g_input_repeat_initial_delay / g_script_event_speed) {
                g_script_event_input |= g_script_event_current_input & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
            menu_free_memory(buffer);
            row_buffer_live = 0;
        }

        frame->v0 = (ring * 16 + scroll_accum - 2) & 0x7F;
        /* The frame zooms open over the first 12 ticks. */
        if (i * g_script_event_speed < 12) {
            gfx_set_image_primitive_layout(&page->frame, &entry->vram_origin, &entry->window, &window);
            menu_zoom_cursor_frame(entry, frame, i * g_script_event_speed);
        }
        if (layout->columns.mode[1] == MENU_TEXT_COLUMN_TEXT && toggle == 1) {
            menu_text_layout_t* columns = entry->context;

            page->icons.base.sprites[1].x0 += columns->x[1];
            columns = entry->context;
            page->icons.base.sprites[2].x0 += columns->x[1];
        }
        page->icons.base.sprites[1].y0 += 4;
        page->icons.base.sprites[2].y0 += 4;

        if (entry->header_id >= 5) {
            gfx_draw_or_append_gpu_primitive(page->icons.icons[0]);
            gfx_draw_or_append_gpu_primitive(page->icons.icons[1]);
            if (g_thread_contexts[g_thread_current_id].function_parameter_4 != 0) {
                gfx_draw_or_append_gpu_primitive(page->icons.icons[2]);
            }
        }
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[3]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[1]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[2]);
        gfx_draw_or_append_gpu_primitive(&page->icon_mode);
        gfx_draw_or_append_gpu_primitive(frame);
        gfx_draw_or_append_gpu_primitive(&page->text_mode);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.draw_mode);
        gfx_draw_or_append_gpu_primitive(&page->icons.base.sprites[0]);
        gfx_draw_or_append_gpu_primitive(&page->icons.base);
        i++;
    }

    if (row_buffer_live != 0) {
        menu_free_memory(buffer);
    }
    thread_yield();
    g_screen_scrolling_list_depth--;
    if (thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}
