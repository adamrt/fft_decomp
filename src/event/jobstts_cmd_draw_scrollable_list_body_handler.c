/* The header declares g_jobstts_menu_list_scroll_direction as u8, but every read here is a signed
 * `lb`; this candidate needs the header changed to s8. Until then the rename
 * keeps the header's declaration out of the way. */
#include "fft/jobstts.h"
#include "fft/main_runtime.h"
#include "fft/main_sound.h"
#include "psx/gpu.h"
#include "psx/pad.h"

/* Page-scroll direction latch: -1 up, 1 down, 0 idle. */

/* Scrollable-list body command.
 *
 * Handles line and page scrolling (animating the VRAM text strip and
 * rendering the newly exposed row), draws the list image, the arrows, the
 * scrollbar thumb and the selection marker, then replays the per-row command
 * group once per visible row, plus one while a scroll offset is in effect.
 * An empty list just skips command[4] + 1 groups. JOBSTTS copy of
 * EQUIP's equip_cmd_draw_scrollable_list_body_handler and BUNIT's bunit_cmd_run_scrollable_list_handler with an
 * event-speed multiplier. The unused arrays reproduce the target's frame: it references only rect (0x20) and marker
 * (0x38). The scroll-down path follows WORLD's world_menu_script_draw_scrollable_list spelling; its `(s16)` row-height
 * cast adds the other 8 bytes of frame and keeps the row count in a register of its own.
 */
u8* jobstts_cmd_draw_scrollable_list_body_handler(u8* command) {
    urect16_t rect;
    u8 unused_28[16];
    urect16_t marker;
    u8 unused_40[8];
    u8* start;
    u16* ids;
    u16 previous;
    s32 input;
    u8 speed;
    s32 i;
    s32 j;
    s32 total;
    s32 y;
    u16 ot_index;
    s32 count;

    ot_index = g_jobstts_gfx_otag_index;
    speed = 1;
    if (g_jobstts_menu_event_speed != 0) {
        speed = g_jobstts_menu_event_speed;
    }
    if (g_jobstts_menu_list_entry_count <= 0) {
        count = command[4] + 1;
        while (--count != -1) {
            command = (u8*)jobstts_cmd_skip_group(command);
        }
        return command;
    }
    g_jobstts_menu_list_row_rendering = 1;
    previous = g_jobstts_menu_list_selected_index;
    if (g_jobstts_menu_list_redraw_pending != 0) {
        jobstts_menu_draw_scrollable_list(command);
    }
    if (g_jobstts_menu_list_scroll_direction == 0) {
        j = jobstts_input_read_page_scroll_direction();
        if (j != 0) {
            if (j != 2) {
                jobstts_menu_scroll_list_by_page(j, command);
            }
        } else {
            input = g_jobstts_cmd_stream_input;
            if ((input & 0x10000000) || (input & PSX_PAD_UP)) {
                if (g_jobstts_menu_scroll_base_index == g_jobstts_menu_list_selected_index) {
                    if (g_jobstts_menu_scroll_base_index > 0) {
                        g_jobstts_menu_list_scroll_direction = -1;
                        if (g_jobstts_menu_list_text_table != 0) {
                            s32 id;
                            s32 height;

                            height = g_jobstts_menu_list_row_height;
                            ids = &g_jobstts_menu_list_scroll_entry_ids;
                            id = g_jobstts_menu_list_entries[g_jobstts_menu_scroll_base_index - 1];
                            rect.x = 0x100;
                            rect.y = 0x20;
                            rect.w = g_jobstts_menu_list_vram_width;
                            rect.h = height;
                            *ids = id;
                            jobstts_text_render_id_rows_to_vram(
                                (s32)g_jobstts_menu_list_text_table, (s16*)ids, (RECT*)&rect, 0);
                        }
                    }
                } else if (input & PSX_PAD_UP) {
                    g_jobstts_menu_list_selected_index -= 1;
                }
            } else if ((input & 0x40000000) || (input & PSX_PAD_DOWN)) {
                if (g_jobstts_menu_scroll_base_index + g_jobstts_menu_list_visible_rows - 1
                    == g_jobstts_menu_list_selected_index) {
                    if (g_jobstts_menu_scroll_base_index + g_jobstts_menu_list_visible_rows - 1
                        < g_jobstts_menu_list_entry_count - 1) {
                        g_jobstts_menu_list_scroll_direction = 1;
                        if (g_jobstts_menu_list_text_table != 0) {
                            u16* entry_ids;

                            j = g_jobstts_menu_scroll_base_index + g_jobstts_menu_list_visible_rows - 1;
                            entry_ids = &g_jobstts_menu_list_scroll_entry_ids;
                            *entry_ids = g_jobstts_menu_list_entries[j + 1];
                            rect.x = 0x100;
                            rect.y = g_jobstts_menu_list_visible_rows * (s16)g_jobstts_menu_list_row_height + 0x30;
                            rect.w = g_jobstts_menu_list_vram_width;
                            rect.h = g_jobstts_menu_list_row_height;
                            jobstts_text_render_id_rows_to_vram(
                                (s32)g_jobstts_menu_list_text_table, (s16*)entry_ids, (RECT*)&rect, 0);
                        }
                    }
                } else if ((g_jobstts_menu_list_selected_index < (g_jobstts_menu_list_entry_count - 1))
                    && (input & PSX_PAD_DOWN)) {
                    g_jobstts_menu_list_selected_index += 1;
                }
            } else {
                g_jobstts_menu_list_scroll_progress = 0;
            }
        }
    }
    if (g_jobstts_menu_list_scroll_direction < 0) {
        if (g_jobstts_menu_list_scroll_progress >= g_main_menu_scroll_accel_delay) {
            g_jobstts_menu_list_scroll_offset_y -= g_main_menu_scroll_fast_step * speed;
        } else {
            g_jobstts_menu_list_scroll_progress += speed;
            g_jobstts_menu_list_scroll_offset_y -= g_main_menu_scroll_slow_step * speed;
        }
        if ((-(s16)g_jobstts_menu_list_row_height) >= (s16)g_jobstts_menu_list_scroll_offset_y) {
            g_jobstts_menu_list_scroll_offset_y = 0;
            g_jobstts_menu_list_scroll_direction = 0;
            g_jobstts_menu_list_selected_index -= 1;
            g_jobstts_menu_scroll_base_index = (s16)g_jobstts_menu_list_selected_index;
            if (g_jobstts_menu_list_text_table != 0) {
                for (j = 0; j < g_jobstts_menu_list_visible_rows; j++) {
                    s32 height;

                    height = (s16)g_jobstts_menu_list_row_height;
                    rect.x = 0x100;
                    rect.w = g_jobstts_menu_list_vram_width;
                    rect.h = height;
                    rect.y = ((j - 1) * height) + 0x30;
                    jobstts_gfx_enqueue_draw_move((const RECT*)&rect, 0x100, (height * j) + 0x30, ot_index - 1);
                }
            }
        }
    } else if (g_jobstts_menu_list_scroll_direction > 0) {
        if (g_jobstts_menu_list_scroll_progress >= g_main_menu_scroll_accel_delay) {
            g_jobstts_menu_list_scroll_offset_y += g_main_menu_scroll_fast_step * speed;
        } else {
            g_jobstts_menu_list_scroll_progress += speed;
            g_jobstts_menu_list_scroll_offset_y += g_main_menu_scroll_slow_step * speed;
        }
        if ((s16)g_jobstts_menu_list_scroll_offset_y >= (s16)g_jobstts_menu_list_row_height) {
            g_jobstts_menu_list_scroll_offset_y = 0;
            g_jobstts_menu_list_scroll_direction = 0;
            g_jobstts_menu_scroll_base_index++;
            g_jobstts_menu_list_selected_index++;
            if (g_jobstts_menu_list_text_table != 0) {
                rect.x = 0x100;
                rect.y = (s16)g_jobstts_menu_list_row_height + 0x30;
                rect.w = g_jobstts_menu_list_vram_width;
                rect.h = g_jobstts_menu_list_row_height * (u16)g_jobstts_menu_list_visible_rows;
                jobstts_gfx_enqueue_draw_move((const RECT*)&rect, 0x100, 0x30, ot_index - 1);
            }
        }
    }
    if (g_jobstts_menu_list_text_table != 0) {
        if ((s16)g_jobstts_menu_list_scroll_offset_y != 0) {
            rect.x = command[7];
            rect.y = command[8] - 5;
            rect.w = g_jobstts_menu_list_vram_width * 4;
            rect.h = (g_jobstts_menu_list_row_height * (u16)g_jobstts_menu_list_visible_rows) + 0xa;
            jobstts_gfx_enqueue_textured_quad(&rect, 0, (s16)g_jobstts_menu_list_scroll_offset_y + 0x2b, 0,
                g_jobstts_gfx_semitransparency, g_jobstts_gfx_background_texture_page, g_jobstts_text_metric_0,
                (s16)g_jobstts_gfx_otag_index);
        } else {
            rect.x = command[7];
            rect.y = command[8];
            rect.w = g_jobstts_menu_list_vram_width * 4;
            rect.h = g_jobstts_menu_list_row_height * (u16)g_jobstts_menu_list_visible_rows;
            jobstts_gfx_enqueue_textured_quad(&rect, 0, 0x30, 0, g_jobstts_gfx_semitransparency,
                g_jobstts_gfx_background_texture_page, g_jobstts_text_metric_0, (s16)g_jobstts_gfx_otag_index);
        }
    }
    if (g_jobstts_text_layout_mode == 0) {
        if (g_jobstts_menu_scroll_base_index != 0) {
            rect.x = g_jobstts_menu_window_right_x;
            rect.y = command[0xc];
            rect.w = 8;
            rect.h = 0x10;
            jobstts_gfx_enqueue_textured_quad(&rect, 0xd8, 0, 0, g_jobstts_gfx_semitransparency,
                g_jobstts_menu_cursor_texture_page, g_jobstts_menu_cursor_mode0_foreground_clut, 0x1e);
        }
        if (g_jobstts_menu_scroll_base_index < (g_jobstts_menu_list_entry_count - g_jobstts_menu_list_visible_rows)) {
            rect.x = g_jobstts_menu_window_right_x;
            rect.y = command[0xd];
            rect.w = 8;
            rect.h = 0x10;
            jobstts_gfx_enqueue_textured_quad(&rect, 0xe0, 0, 0, g_jobstts_gfx_semitransparency,
                g_jobstts_menu_cursor_texture_page, g_jobstts_menu_cursor_mode0_foreground_clut, 0x1e);
        }
        if (g_jobstts_menu_list_entry_count > g_jobstts_menu_list_visible_rows) {
            y = command[0xc] + 0x14;
            j = command[0xd] - y;
            y = (j * g_jobstts_menu_list_selected_index) / (g_jobstts_menu_list_entry_count - 1);
            j = command[0xc] + 0xf;
            j += y;
            rect.w = 8;
            rect.h = 8;
            rect.x = g_jobstts_menu_window_right_x;
            rect.y = j;
            jobstts_gfx_enqueue_textured_quad(&rect, 0x10, 0x10, 0, g_jobstts_gfx_semitransparency,
                g_jobstts_menu_cursor_texture_page, g_jobstts_menu_cursor_mode0_foreground_clut,
                (s16)g_jobstts_gfx_otag_index);
        }
        if (command[2] != 0) {
            marker.x = command[7] - command[0xb];
            marker.y = command[8]
                + (((s16)g_jobstts_menu_list_selected_index - g_jobstts_menu_scroll_base_index)
                    * (s16)g_jobstts_menu_list_row_height);
            jobstts_menu_update_and_draw_animated_marker(
                &marker, &g_jobstts_menu_list_cursor_anim_state, g_jobstts_text_layout_mode);
        }
    }
    command += command[1];
    g_jobstts_menu_list_row_index = 0;
    total = g_jobstts_menu_list_visible_rows + 1;
    if ((s16)g_jobstts_menu_list_scroll_offset_y == 0) {
        total = g_jobstts_menu_list_visible_rows;
    }
    start = command;
    for (i = 0; i < total; i++) {
        if (((g_jobstts_menu_scroll_base_index + i) > (g_jobstts_menu_list_entry_count - 1))
            && ((s16)g_jobstts_menu_list_scroll_offset_y == 0)) {
            break;
        }
        command = start;
        for (j = 0; j < g_jobstts_menu_list_row_group_count; j++) {
            command = g_jobstts_cmd_handlers[*command](command);
        }
        g_jobstts_menu_list_row_index += 1;
    }
    g_jobstts_menu_list_row_rendering = 0;
    if (previous != (u16)g_jobstts_menu_list_selected_index) {
        g_jobstts_sound_queued_effect_id = MAIN_SFX_CURSOR_MOVE;
    }
    return command;
}
