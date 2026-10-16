/* The AT list window thread, with its scroll arrows and paged text columns. */

#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/main.h"
#include "fft/main_gfx.h"
#include "fft/main_sound.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_at_list.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_panel.h"
#include "fft/menu_window.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Thread that runs a scrolling list window; parameter 1 is its menu entry, whose context is the
 * menu_text_layout_t of text columns to show.
 *
 * The visible rows are drawn into a ring of 16-pixel rows in VRAM, which the window sprite
 * shows through a moving v offset, and a window frame thread is started around it. Each frame
 * moves the cursor, scrolls one row at a time smoothly (drawing the incoming row into the ring
 * slot before it scrolls into view), runs the entry's action and cancel handlers, and draws the
 * scroll arrows and thumb while rows are hidden. Two page records alternate by frame. When the
 * menu closes, the frame thread is stopped and the scroll position is kept in the entry.
 *
 * The target's lone dead `lw t1,0xe0(sp)` (reload of the spilled `layout`)
 * at +0x4d4 comes from a conditional on `layout` whose body is a dead store: flow deletes
 * the store, the now-empty branch survives until after reload, and the post-reload jump
 * pass deletes the branch but not its operand reload. */
void menu_run_at_list(void) {
    image_load_parameters_t image_parameters;
    RECT image_rect;
    RECT clear;
    RECT texture_window;
    SPRT icons0[2];
    SPRT icons1[2];
    SPRT icons2[2];
    s32 row_offset;
    s32 ring;
    s32 cursor;
    u32* input;
    u32* input2;
    SPRT* frame;
    menu_list_page_t* page;
    menu_text_layout_t* layout;
    s32 accum;
    s32 timer;
    s32 redraw_pending;
    menu_entry_t* entry;
    s32 delta;
    s32 i;
    s32 column;
    s32 index;
    s32 arrow_u;
    s32 step;
    s32 top;
    s32 repeat;
    u8* buffer;
    s32 arrow_y;
    s32 span;

    page = g_at_list_pages;
    timer = 0;
    input = script_get_controller_input_pointer(0);
    input2 = script_get_controller_input_pointer(1);
    row_offset = 0;
    cursor = 0;

    entry = (menu_entry_t*)thread_get_current_parameter_1();
    layout = entry->context;
    entry->window.h = layout->row_count * 16 + 0x1C;
    entry->inner_height = layout->row_count * 16 + 0x1C;
    entry->text_image.height = layout->row_count * 16 + 0x1C;

    image_parameters.x_load = 0;
    image_parameters.y_load = 0x7E;
    image_parameters.x_screen_offset = 8;
    image_parameters.y_screen_offset = 0xC;
    image_parameters.width = entry->window.w - 0x12;
    image_parameters.height = entry->window.h - 0x1A;

    gfx_set_draw_mode_for_texture_page(&page->icon_mode, MENU_TEXTURE_PAGE_FRAME);
    texture_window.x = 0;
    texture_window.y = entry->vram_origin.y & 0xFF;
    texture_window.w = 0x100;
    texture_window.h = 0x100;

    for (i = 0; i < 2; i++) {
        g_at_list_pages[i].icons.icons[0] = &icons0[i];
        g_at_list_pages[i].icons.icons[1] = &icons1[i];
        g_at_list_pages[i].icons.icons[2] = &icons2[i];
        menu_build_window_sprites(&image_rect, entry, &g_at_list_pages[i].icons);
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
    gfx_init_image_loading(&page->frame, &entry->vram_origin, &entry->window, &image_parameters);

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

    buffer = g_menu_render_buffer;
    menu_clear_render_buffer(buffer, entry->inner_width * entry->inner_height / 2);
    redraw_pending = 1;
    thread_start(WORLD_AT_LIST_THREAD_SLOT_WINDOW_FRAME, menu_window_frame_thread);
    thread_set_parameters(WORLD_AT_LIST_THREAD_SLOT_WINDOW_FRAME, (s32)&entry->window.x, 0, 0);
    row_offset = entry->select_text_table;
    cursor = entry->selected_index - row_offset;
    menu_draw_text_columns(entry, &row_offset, buffer);
    ring = 0;
    image_rect.w = g_menu_text_state.pen.stride >> 2;
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
    mem_copy_bytes_nonempty(&g_at_list_pages[1], g_at_list_pages, sizeof(menu_list_page_t));
    for (i = 0; i < 2; i++) {
        g_at_list_pages[i].icons.icons[0] = &icons0[i];
        g_at_list_pages[i].icons.icons[1] = &icons1[i];
        g_at_list_pages[i].icons.icons[2] = &icons2[i];
    }

    delta = 0;
    accum = 0;
    *input = 0;
    i = 0;
    *input2 = 0;
    for (;;) {
        page = &g_at_list_pages[i & 1];
        frame = &page->frame;

        /* The frame thread dims its border while the thread before this one runs. */
        if (thread_is_previous_running() != 0 && g_menu_input_lock_mode == 0) {
            thread_set_parameters(WORLD_AT_LIST_THREAD_SLOT_WINDOW_FRAME, (s32)&entry->window.x, 1, 0);
        } else {
            thread_set_parameters(WORLD_AT_LIST_THREAD_SLOT_WINDOW_FRAME, (s32)&entry->window.x, 0, 0);
        }

        if (delta == 0 && menu_can_accept_input() != 0 && i != 0) {
            /* Dead store (column is reinitialized before use); only the reload
             * of layout for this branch survives in the target. */
            if (layout == 0) {
                column = 0;
            }

            /* Square with up or down pages through the list instead (menu_redraw_text_page_on_scroll). */
            if ((*input & PSX_PAD_UP) && !(*input2 & PSX_PAD_SQUARE)) {
                if (cursor != 0) {
                    cursor--;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != 0) {
                    delta = -1;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                } else if (layout->hidden_rows == 0) {
                    cursor = layout->row_count - 1;
                    if (layout->row_count != 1) {
                        g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                    }
                }
            } else if ((*input & PSX_PAD_DOWN) && !(*input2 & PSX_PAD_SQUARE)) {
                if (cursor != layout->row_count - 1) {
                    cursor++;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset != layout->hidden_rows) {
                    delta++;
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                } else if (row_offset == 0) {
                    /* row_offset equals hidden_rows here, so the cursor wraps only when no rows are hidden. */
                    cursor = 0;
                    if (layout->row_count != 1) {
                        g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                    }
                }
            } else {
                timer = 0;
            }

            /* Draw the row scrolling into view into the ring slot below the visible rows, or above them. */
            if (delta != 0) {
                if (delta > 0) {
                    image_rect.y = (((ring + layout->row_count) * 16) & 0xFF) + entry->vram_origin.y;
                } else {
                    image_rect.y = (((ring - 1) * 16) & 0xFF) + entry->vram_origin.y;
                }
                image_rect.h = 0x10;
                buffer = g_menu_render_buffer;
                menu_clear_render_buffer(buffer, 0x800);
                g_menu_text_state.pen.stride = entry->inner_width;

                for (column = 0; column < 3; column++) {
                    index = cursor + row_offset + delta;
                    menu_set_text_origin(layout->x[column], 0);
                    if (layout->mode[column] != MENU_TEXT_COLUMN_HIDDEN) {
                        g_menu_text_palette_offset = layout->text_colors[column][index];
                    }
                    if (g_menu_text_palette_offset == 4) {
                        g_menu_text_state.pen.color = 0x44444444;
                    }
                    if (layout->mode[column] == MENU_TEXT_COLUMN_TEXT) {
                        menu_display_text_entry(layout->text_ids[column][index], buffer, &g_menu_text_state.pen);
                    } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER) {
                        menu_display_number_entry(layout->text_ids[column][index], 2, buffer, &g_menu_text_state.pen);
                    } else if (layout->mode[column] == MENU_TEXT_COLUMN_NUMBER_GLYPHS) {
                        menu_draw_number_glyphs(layout->text_ids[column][index], 2, buffer, &g_menu_text_state.pen);
                    }
                }

                g_menu_text_state.pen.color = 0;
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
        if (redraw_pending != 0) {
            redraw_pending = 0;
        }
        if (delta == 0 && menu_can_accept_input() != 0) {
            redraw_pending = (s32)menu_redraw_text_page_on_scroll(entry, &row_offset, &ring, &cursor);
            ClearImage(&clear, 0, 0, 0);
        }

        entry->selected_index = cursor + row_offset;
        entry->select_text_table = row_offset;
        if (menu_should_close_thread(input) != 0) {
            break;
        }

        if (delta == 0) {
            menu_handle_action(entry, cursor);
        }
        menu_handle_cancel_input(entry);

        if ((g_script_event_speed == 1 && !(script_get_event_initial_repeat_counter() & 1))
            || g_script_event_speed == 2) {
            if (timer < g_main_menu_scroll_accel_delay) {
                step = g_main_menu_scroll_slow_step << 1;
            } else {
                step = g_main_menu_scroll_fast_step << 1;
            }
            accum += delta * step;
        }
        timer += g_script_event_speed;

        /* A whole 16-pixel row has scrolled: commit it. A direction held past the initial repeat
         * delay is fed back into the input so the scroll keeps going. */
        if (accum < -0xF || accum > 0xF) {
            ring += delta;
            row_offset += delta;
            repeat = script_get_event_initial_repeat_counter();
            delta = 0;
            accum = 0;
            if (repeat >= g_main_input_repeat_initial_delay / g_script_event_speed) {
                *input |= *input2 & (PSX_PAD_UP | PSX_PAD_DOWN);
            }
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

        gfx_build_scaled_draw_area_pair_swapped(&page->draw_areas, &entry->window, i, g_main_gfx_active_buffer_index);
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
    thread_set_parameters(WORLD_AT_LIST_THREAD_SLOT_WINDOW_FRAME, 0, 0, 1);
    entry->select_text_table = row_offset;
    thread_yield();
    thread_exit_current();
}
