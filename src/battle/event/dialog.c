/* The dialog text thread, opening a dialog window and drawing a text entry glyph by glyph. */

#include "fft/camera.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/memory.h"
#include "fft/menu_gfx.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_event.h"
#include "fft/unit_runtime.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s32 g_text_awaiting_input;

/* &win[index], the window of the current packet buffer; the byte arithmetic
 * matches the target where plain indexing does not. */
#define WINDOW_AT(index) ((menu_window_buffer_t*)((index) * sizeof(menu_window_buffer_t) + (s32)win))

#define NEXT() text = text_advance_cursor_with_separate_backreference_state(&repeat, text, &saved)

#define CHECK_ABORT()                                                                                                  \
    if (thread_current_task_get() == THREAD_TASK_STOP_REQUEST) {                                                       \
        goto end;                                                                                                      \
    }

/* Dialog text thread: opens a dialog window, draws a text entry into it glyph
 * by glyph while handling the control bytes, then closes the window and exits.
 *
 * Thread parameters: 1 is the DisplayMessage dialog type byte, 2 the text id,
 * 3 the speaker's script unit id (portrait boxes) and 4 the portrait index
 * plus one. The DIALOG_THREAD_WORD_* task words add position offsets, a tail
 * shift, the opening tween and a width override. A stop request ends the
 * thread at any wait. With DIALOG_TYPE_KEEP_OPEN the window outlives the
 * end of its text: the thread waits for a ChangeDialog resume carrying the
 * next text id in parameter 2, and 0xFFFF there closes the window.
 *
 * The 0xFE/0xFD waits compare against resume_task rather than the constant
 * THREAD_TASK_RESUME (1): with a constant, loop.c pools every constant 1
 * of the main loop into one hoisted (and spilled) pseudo, where the target
 * keeps each wait's 1 in a saved register. The 0xE4/0xE6 dispatch shares the digit printer through a
 * label; the WORLD twin's `character == 0xE4 || character == 0xE6` form is not threaded here
 * and loses the substitution-table base CSE. */
void message_dialog_thread(void) {
    /* rec.dialog_type is the DisplayMessage dialog type byte, split into box_type,
     * position, options and keep_open below. */
    text_dialog_record_t rec;
    RECT from;
    RECT to;
    s32 repeat;
    const u8* saved;
    s32 column;
    s32 width_override;
    s16 cursor_y;
    s16 right;
    s16 bottom;
    u16 mark_x;
    u16 delay;
    u16 no_wait;
    u16 variable_base;
    u16 value_index;
    thread_t* thread;
    menu_window_buffer_t* win;
    menu_window_buffer_t* cursor_window;
    menu_window_buffer_t* arrow_window;
    menu_window_buffer_t* scroll_window;
    menu_window_buffer_t* hidden_window;
    menu_window_buffer_t* hidden_window_next;
    menu_window_buffer_t* select_window;
    menu_window_buffer_t* select_hidden_window;
    menu_window_buffer_t* select_hidden_window_next;
    const u8* text;
    s16 cursor_x;
    s32 character;
    s32 i;
    s32 tick;
    s32 value;
    s32 digits;
    u16 started;
    u16 selection;
    s32* substitution;
    s16 event_id;
    s32 window_u;
    s32 half_w;
    s32 half_h;
    s32 center;
    u16 param;
    s32 no_value; /* 0xFFFF: no width override, no next text, and minus one for the u16 counters */
    s32 resume_task;

    thread_current_task_set(THREAD_TASK_RESUME);
    script_variable_set(EVENT_SCRIPT_VAR_TYPEWRITER_VRAM_X_OFFSET, 0);
    value_index = 0;
    variable_base = EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION;
    thread = &g_threads[g_thread_current_id];

    param = thread_current_parameter_1_get();
    rec.dialog_type = param;
    /* Box type 0x70 is a portrait box above the speaker, or below it when the speaker
     * faces away from the camera. */
    if ((param & 0x70) == 0x70) {
        rec.dialog_type = (param & 0x1C) | 0x10;
        rec.dialog_type |= unit_camera_facing_check(thread_current_parameter_3_get()) + 1;
    }

    rec._unknown_2a = 0;
    rec.origin_x = 8;
    rec.origin_y = 8;
    rec.box_type = rec.dialog_type & DIALOG_TYPE_BOX_TYPE_MASK;
    rec.position = rec.dialog_type & DIALOG_TYPE_POSITION_MASK;
    rec.options = rec.dialog_type & DIALOG_TYPE_OPTIONS_MASK;
    rec.keep_open = rec.dialog_type & DIALOG_TYPE_KEEP_OPEN;
    rec.arrow_offset = thread->task_words[DIALOG_THREAD_WORD_ARROW_OFFSET];
    rec.x_offset = thread->task_words[DIALOG_THREAD_WORD_X_OFFSET];
    rec.y_offset = thread->task_words[DIALOG_THREAD_WORD_Y_OFFSET];
    /* Both arms compute the same portrait index. */
    if (rec.dialog_type & 0x10) {
        rec.portrait = thread->function_parameter_4 - 1;
    } else {
        rec.portrait = thread->function_parameter_4 - 1;
    }

    text = text_entry_find(thread_current_parameter_2_get());
    text_pixels_measure(&rec.mark, &rec.lines, text);
    if (thread->task_words[DIALOG_THREAD_WORD_TEXT_WIDTH] != 0) {
        rec.mark = thread->task_words[DIALOG_THREAD_WORD_TEXT_WIDTH];
    }

    if (rec.box_type == DIALOG_BOX_TYPE_EIGHT_LINE) {
        rec.lines = 8;
    }
    if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
        rec.lines = 6;
    }
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        rec.lines = 3;
    }
    if (rec.box_type == DIALOG_BOX_TYPE_CHECK && rec.lines > 4) {
        rec.lines = 4;
    }
    if (rec.box_type == DIALOG_BOX_TYPE_HELP && rec.lines > 4) {
        rec.lines = 4;
    }
    if (rec.box_type == DIALOG_BOX_TYPE_TWO_LINE && rec.lines > 2) {
        rec.lines = 2;
    }

    rec.height = rec.lines * 16 + 16;
    rec.width = rec.mark + 0x18;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        if (rec.portrait < 8) {
            rec.width = rec.mark + 0x40;
        }
        if (rec.options == DIALOG_OPTIONS_TAIL) {
            rec.height = rec.lines * 16 + 0x18;
        }
        if (rec.options == DIALOG_OPTIONS_ALTERNATE_TAIL) {
            rec.height += 0x10;
        }
    }

    /* A portrait box with a tail is placed beside the speaker's projected position. */
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.options != DIALOG_OPTIONS_NO_TAIL) {
        center = rec.width >> 1;
        value = script_runtime_id_resolve(thread_current_parameter_3_get());
        unit_screen_position_project(value, &g_unit_scratch_coords);
        cursor_x = center;
        rec.tail_offset = 0;
        rec.x = g_unit_scratch_coords.vx - center - 4;
        /* Automatic position: below the speaker when it projects above the screen or its swap
         * partner projects higher, else above. g_camera_script_rotation is scratch here. */
        if (rec.position == DIALOG_POSITION_AUTO) {
            if (thread_current_parameter_3_get() == g_message_speaker_swap_unit_a) {
                value = g_message_speaker_swap_unit_b;
            } else if (thread_current_parameter_3_get() == g_message_speaker_swap_unit_b) {
                value = g_message_speaker_swap_unit_a;
            }
            unit_screen_position_project(script_runtime_id_resolve(value), &g_camera_script_rotation);
            if (g_unit_scratch_coords.vy < 0 || g_camera_script_rotation.vy < g_unit_scratch_coords.vy) {
                rec.position = DIALOG_POSITION_BOTTOM;
            } else {
                rec.position = DIALOG_POSITION_TOP;
            }
            rec.dialog_type = (rec.dialog_type & 0xFFF0) | rec.position;
        }

        if (rec.position == DIALOG_POSITION_TOP) {
            rec.x += 0xC;
            rec.y = g_unit_scratch_coords.vy - rec.height - 0x28;
        }
        if (rec.position == DIALOG_POSITION_BOTTOM) {
            rec.x -= 8;
            rec.y = g_unit_scratch_coords.vy;
        }

        /* Clamp the box to x 0x88..0x180, before and after the script offsets; the tail
         * offset absorbs each shift so the tail still points at the speaker. */
        if (rec.x < 0x88) {
            rec.tail_offset = rec.x - 0x88;
            rec.x = 0x88;
        } else if (rec.x + rec.width > 0x180) {
            rec.tail_offset = rec.x + rec.width - 0x180;
            rec.x = 0x180 - rec.width;
        }

        rec.x += rec.x_offset;
        rec.y += rec.y_offset;
        if (rec.x < 0x88) {
            rec.tail_offset += rec.x - 0x88;
            rec.x = 0x88;
        } else if (rec.x + rec.width > 0x180) {
            rec.tail_offset += rec.x + rec.width - 0x180;
            rec.x = 0x180 - rec.width;
        }

        rec.tail_offset += rec.arrow_offset;
        if (rec.y < 0x16) {
            rec.y = 0x16;
        } else if (rec.y + rec.height > 0xE4) {
            rec.y = 0xE4 - rec.height;
        }

        /* Keep the tail at least 0x18 pixels inside either end of the box. */
        if (rec.tail_offset > cursor_x - 0x18) {
            rec.tail_offset = cursor_x - 0x18;
        }
        if (rec.tail_offset < -cursor_x + 0x18) {
            rec.tail_offset = -cursor_x + 0x18;
        }

        /* The window grows out of the tail's spot on the box edge. */
        if (rec.position == DIALOG_POSITION_TOP) {
            bottom = rec.y + rec.height - 8;
        }
        if (rec.position == DIALOG_POSITION_BOTTOM) {
            bottom = rec.y + 8;
        }
        right = rec.x + rec.tail_offset + (rec.width >> 1);
    } else {
        rec.tail_offset = 0;
        half_w = rec.width >> 1;
        rec.x = (0x100 - half_w) & 0xFFFE;
        half_h = rec.height >> 1;
        if (rec.position == DIALOG_POSITION_TOP) {
            rec.y = 0x16;
        }
        if (rec.position == DIALOG_POSITION_BOTTOM) {
            rec.y = 0xE4 - rec.height;
        }
        if (rec.position == DIALOG_POSITION_CENTER) {
            rec.y = (0x78 - half_h) & 0xFFFE;
        }

        rec.x += rec.x_offset;
        rec.y += rec.y_offset;
        /* Other boxes grow out of their center. */
        right = half_w + rec.x - 6;
        bottom = half_h + rec.y - 2;
    }

    win = menu_alloc_window_buffer_pair();
    win->message_id = thread_current_parameter_2_get();
    win->dialogue_selector = rec.position;
    win->box_type = rec.box_type;
    rec.text = thread_current_parameter_2_get();
    rec.rect.w = rec.width;
    rec.rect.h = rec.height;
    gfx_alloc_texture_rect(&rec.rect, win->quads, (u32*)-1);

    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        if (text[0] == TEXT_SET_PORTRAIT) {
            win->portrait_code = text[1];
        } else {
            win->portrait_code = rec.portrait;
        }
        value = unit_runtime_battle_id_get(script_runtime_id_resolve(thread_current_parameter_3_get()));
        menu_build_and_upload_window_image(rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
        win->portrait_battle_id = value;
    } else if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
        win[0].quads[0].clut = 0x7CBC;
        win[1].quads[0].clut = 0x7CBC;
        ClearImage(&rec.rect, 0, 0, 0);
    } else {
        rec.first_line = 0;
        rec.last_line = rec.lines;
        message_record_image_render(&rec);
    }

    /* Clamp the opening point to the dialog area and collapse the window onto it. */
    if (right < 0x88) {
        right = 0x88;
    }
    if (right > 0x180) {
        right = 0x180;
    }
    if (bottom < 0x16) {
        bottom = 0x16;
    }
    if (bottom > 0xE4) {
        bottom = 0xE4;
    }
    setXY4(&win->quads[0], right, bottom, right, bottom, right, bottom, right, bottom);

    from.x = rec.x;
    from.y = rec.y;
    from.w = rec.width;
    from.h = rec.height;
    rec.arrow_x = rec.x + rec.width - 0x1E;
    rec.arrow_y = rec.y + rec.height - 0x18;

    to.x = 0;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        setXY4(&win->quads[1], right, bottom, right, bottom, right, bottom, right, bottom);
        if (rec.tail_offset < 0) {
            to.x = rec.x + 8;
            if (rec.portrait < 8) {
                rec.origin_x += 0x28;
            }
        } else {
            to.x = rec.x + rec.width - 0x30;
            rec.arrow_x -= 0x20;
        }
        to.y = rec.y + 7;
        if (rec.options != DIALOG_OPTIONS_NO_TAIL) {
            if (rec.position == DIALOG_POSITION_BOTTOM) {
                to.y = rec.y + 0xF;
                rec.origin_y += 8;
            } else {
                rec.arrow_y -= 8;
            }
        }
        if (rec.options == DIALOG_OPTIONS_ALTERNATE_TAIL) {
            rec.arrow_y -= 8;
        }
        to.w = 0x1F;
        to.h = 0x30;
        if (rec.tail_offset < 0) {
            win[1].portrait_flipped = 1;
            win[0].portrait_flipped = 1;
        }
        if (rec.portrait >= 8) {
            to.x = 0;
            rec.arrow_x = rec.x + rec.width - 0x1E;
        }
    }

    if (rec.box_type == DIALOG_BOX_TYPE_HELP) {
        win->sprites[3].u0 = 0x50;
        win->sprites[3].v0 = 0x20;
        win->sprites[3].x0 = from.x + 3;
        win->sprites[3].y0 = from.y - 2;
        win->sprites[3].w = 0x14;
        win->sprites[3].h = 9;
        win->sprites[3].clut = 0x7CBC;
    }

    if (rec.box_type == DIALOG_BOX_TYPE_CHECK) {
        win->sprites[3].u0 = 0x40;
        win->sprites[3].v0 = 0x78;
        win->sprites[3].x0 = from.x + 3;
        win->sprites[3].y0 = from.y - 2;
        win->sprites[3].w = 0x1C;
        win->sprites[3].h = 8;
        win->sprites[3].clut = 0x7CBC;
    }

    /* Copy the finished window into its pair for the other packet buffer. */
    mem_copy_bytes_nonempty(&win[1], win, 0x118);
    win[0].active = 1;
    win[1].active = 1;

    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        menu_window_quads_tween(win, &from, &to, thread->task_words[DIALOG_THREAD_WORD_WINDOW_OPTIONS] & 0xF);
    } else {
        menu_window_crop_animate(win, &from, &to);
    }

    window_u = win->quads[0].u0;
    rec.x = window_u + 8;
    rec.y = win->quads[0].v0 + 8;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.tail_offset < 0 && rec.portrait < 8) {
        rec.x = window_u + 0x30;
    }
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.options != DIALOG_OPTIONS_NO_TAIL
        && rec.position == DIALOG_POSITION_BOTTOM) {
        rec.y += 8;
    }

    no_wait = 0;
    delay = 1;
    column = 0;
    rec.color = 0;
    rec.line = 0;
    g_text_current_line = 0;

    /* Centered boxes center each line on its measured width, except in two
     * events (unless primitives draw immediately), where lines start flush left. */
    if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
        event_id = script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT);
        if ((event_id == EVENT_ID_WIEGRAF_PACT_WITH_VELIUS || event_id == EVENT_ID_THE_LYING_HEART)
            && g_gfx_draw_primitives_immediately == 0) {
            cursor_x = rec.x;
        } else {
            u32 line_width = g_text_line_widths[0] + 0x18;
            cursor_x = rec.x + ((rec.width - line_width) >> 1);
        }
        /* Round the pen x to a multiple of 4: up from phase 2 or 3, down from phase 1. */
        if (cursor_x & 2) {
            cursor_x += 4 - (cursor_x & 3);
        } else if (cursor_x & 1) {
            cursor_x -= cursor_x & 3;
        }
    } else {
        cursor_x = rec.x;
    }

    width_override = 0xFFFF;
    cursor_y = rec.y;
    rec.mark = -1;
    saved = (const u8*)-1;
    repeat = 0;

    while (1) {
        no_value = 0xFFFF;
        character = *text;
        CHECK_ABORT();

        if (character == TEXT_NEWLINE || character == TEXT_END_WAIT_FOR_CONFIRM || character == TEXT_END_AUTO_CLOSE
            || character == TEXT_PAGE_BREAK) {
            if (character == TEXT_END_AUTO_CLOSE) {
                break;
            }
            if (character == TEXT_END_WAIT_FOR_CONFIRM) {
                /* The end of a substituted entry returns to the text that referenced it. */
                if (saved != (const u8*)-1) {
                    text = saved;
                    saved = (const u8*)-1;
                    continue;
                }
                /* Skip to the end of the page; adding no_value (0xFFFF) to the u16 line count
                 * undoes the line step below. */
                while (1) {
                    if (column >= rec.lines) {
                        break;
                    }
                    column++;
                    rec.line++;
                }
                rec.line += no_value;
            } else {
                NEXT();
            }

            if (character != TEXT_PAGE_BREAK) {
                column++;
                rec.line++;
            }

            rec.first_line = rec.line - rec.lines;
            if (column >= rec.lines || character == TEXT_END_WAIT_FOR_CONFIRM || character == TEXT_PAGE_BREAK) {
                if (character != TEXT_PAGE_BREAK) {
                    column = 0;
                }
                if (no_wait == 0) {
                    /* Wait for Circle, Cross or Select; tick drives the cursor and arrow animation. */
                    for (tick = 0;; tick += g_script_event_speed) {
                        /* A centered box dims its text instead of waiting for a button. */
                        if (rec.box_type == DIALOG_BOX_TYPE_CENTERED && character != TEXT_PAGE_BREAK) {
                            for (tick = 0x80; tick > 0x30; tick -= 8) {
                                WINDOW_AT(g_menu_packet_buffer_index)->color.r = tick;
                                WINDOW_AT(g_menu_packet_buffer_index)->color.g = tick;
                                WINDOW_AT(g_menu_packet_buffer_index)->color.b = tick;
                                thread_yield();
                            }
                            break;
                        }
                        thread_yield();
                        if (g_script_event_input & (PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT)) {
                            break;
                        }
                        CHECK_ABORT();

                        /* At a page break the cursor and its shadow (+2, +2) bob beside the pen. */
                        if (character == TEXT_PAGE_BREAK) {
                            if (rec.box_type != DIALOG_BOX_TYPE_PORTRAIT) {
                                cursor_window = WINDOW_AT(g_menu_packet_buffer_index);
                                cursor_window->sprites[0].x0 = cursor_x
                                    + (cursor_window->quads[0].x0 + rec.x - cursor_window->quads[0].u0)
                                    + (s16)(((tick >> 4) & 3) - 8);
                                cursor_window->sprites[0].y0 = cursor_y
                                    + (cursor_window->quads[0].y0 + rec.y - cursor_window->quads[0].v0)
                                    - (cursor_window->quads[0].v0 + 8);
                                cursor_window->sprites[1].x0 = cursor_window->sprites[0].x0 + 2;
                                cursor_window->sprites[1].y0 = cursor_window->sprites[0].y0 + 2;
                            }
                        } else {
                            if (character == TEXT_END_WAIT_FOR_CONFIRM && rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
                                g_text_awaiting_input = 1;
                            }

                            /* A full page shows the next-page arrow, cycling its frames with tick. */
                            if (character == TEXT_NEWLINE) {
                                WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].u0 = (tick & 0x30) - 0x58;
                                WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].v0 = 0x10;
                                arrow_window = WINDOW_AT(g_menu_packet_buffer_index);
                                arrow_window->sprites[2].x0 = rec.arrow_x;
                                arrow_window->sprites[2].y0 = rec.arrow_y;
                            }

                            /* Holding Square scrolls back through earlier lines with Up and Down. */
                            if (g_script_event_current_input & PSX_PAD_SQUARE) {
                                while (1) {
                                    if (rec.line != rec.lines) {
                                        scroll_window = WINDOW_AT(g_menu_packet_buffer_index);
                                        scroll_window->scroll_range = rec.line - rec.lines;
                                        scroll_window->scroll_position = rec.first_line;
                                        scroll_window->icon_flags = MENU_WINDOW_ICON_SCROLL_MARKER;
                                        if (rec.first_line != 0) {
                                            scroll_window->icon_flags
                                                = MENU_WINDOW_ICON_SCROLL_UP | MENU_WINDOW_ICON_SCROLL_MARKER;
                                        }
                                        if (rec.first_line != rec.line - rec.lines) {
                                            WINDOW_AT(g_menu_packet_buffer_index)->icon_flags
                                                |= MENU_WINDOW_ICON_SCROLL_DOWN;
                                        }
                                    }
                                    if ((g_script_event_input & PSX_PAD_UP) && rec.first_line != 0) {
                                        g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                                        WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                        rec.first_line += no_value;
                                        rec.last_line = rec.first_line + rec.lines;
                                        message_record_image_render(&rec);
                                    }
                                    if ((g_script_event_input & PSX_PAD_DOWN)
                                        && rec.first_line != rec.line - rec.lines) {
                                        g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                                        WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                        rec.first_line++;
                                        rec.last_line = rec.first_line + rec.lines;
                                        message_record_image_render(&rec);
                                    }
                                    if (!(g_script_event_current_input & PSX_PAD_SQUARE)) {
                                        break;
                                    }
                                    CHECK_ABORT();
                                    WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                    thread_yield();
                                }
                                win[0].icon_flags = 0;
                                win[1].icon_flags = 0;
                            }

                            /* Scroll forward to the newest lines again. */
                            for (;;) {
                                if (rec.line - rec.lines != rec.first_line) {
                                    WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                    rec.first_line++;
                                    rec.last_line = rec.first_line + rec.lines;
                                    message_record_image_render(&rec);
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }

                g_text_awaiting_input = 0;
                /* Hide the cursor, arrow and scroll icons in both packet buffers, a frame apart. */
                hidden_window = WINDOW_AT(g_menu_packet_buffer_index);
                hidden_window->sprites[0].x0 = 0x200;
                hidden_window->sprites[1].x0 = 0x200;
                hidden_window->sprites[2].x0 = 0x200;
                hidden_window->icon_flags = 0;
                thread_yield();
                hidden_window_next = WINDOW_AT(g_menu_packet_buffer_index);
                hidden_window_next->sprites[0].x0 = 0x200;
                hidden_window_next->sprites[1].x0 = 0x200;
                hidden_window_next->sprites[2].x0 = 0x200;
                hidden_window_next->icon_flags = 0;

                if (character == TEXT_END_WAIT_FOR_CONFIRM) {
                    /* With DIALOG_TYPE_KEEP_OPEN the window stays open for ChangeDialog, which
                     * puts the next text id in parameter 2 (0xFFFF closes it) and resumes the thread. */
                    if (rec.keep_open == 0) {
                        break;
                    }
                    thread_current_task_set(THREAD_TASK_DIALOG_AWAIT_TEXT);
                    resume_task = THREAD_TASK_RESUME;
                    while (thread_current_task_get() != resume_task) {
                        if (thread_current_task_get() == THREAD_TASK_STOP_REQUEST) {
                            goto end;
                        }
                        thread_yield();
                    }

                    win->message_id = win[1].message_id = rec.text = thread_current_parameter_2_get();
                    if ((rec.text & no_value) == no_value) {
                        break;
                    }
                    text = text_entry_find(rec.text);
                    no_wait = 0;
                    menu_build_and_upload_window_image(
                        rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
                    delay = 1;
                    column = 0;
                    cursor_x = rec.x;
                    width_override = no_value;
                    rec.color = 0;
                    rec.mark = -1;
                    cursor_y = rec.y;
                    rec.line = 0;
                    rec.portrait = thread->function_parameter_4 - 1;
                    win->portrait_code = rec.portrait;
                    g_text_awaiting_input = 0;
                    value_index = 0;
                    saved = (const u8*)-1;
                    repeat = 0;
                    win[1].portrait_code = rec.portrait;
                    thread_yield();
                    continue;
                }

                if (character == TEXT_PAGE_BREAK) {
                    continue;
                }

                if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
                    menu_build_and_upload_window_image(
                        rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
                } else if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
                    ClearImage(&rec.rect, 0, 0, 0);
                    thread_yield();
                    for (tick = 0; tick < 2; tick++) {
                        win[tick].color.r = 0x80;
                        win[tick].color.g = 0x80;
                        win[tick].color.b = 0x80;
                    }
                } else {
                    g_sound_pending_sfx_id = SOUND_SFX_TEXT_PAGE;
                    rec.first_line = rec.line;
                    rec.last_line = rec.line + rec.lines;
                    message_record_image_render(&rec);
                }

                /* Start one row up; the newline step below brings the pen to the top row. */
                cursor_y = rec.y - 0x10;
                thread_yield();
            }

            if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
                event_id = script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT);
                if ((event_id == EVENT_ID_WIEGRAF_PACT_WITH_VELIUS || event_id == EVENT_ID_THE_LYING_HEART)
                    && g_gfx_draw_primitives_immediately == 0) {
                    cursor_x = rec.x;
                } else {
                    u32 line_width;
                    g_text_current_line++;
                    line_width = g_text_line_widths[g_text_current_line] + 0x18;
                    cursor_x = rec.x + ((rec.width - line_width) >> 1);
                }
                /* Unlike the first line's rounding, any phase rounds up here; the else arm never
                 * runs. */
                if (cursor_x & 3) {
                    cursor_x += 4 - (cursor_x & 3);
                } else if (cursor_x & 1) {
                    cursor_x -= cursor_x & 3;
                }
                text_debug_value_print(rec.line + rec.lines);
            } else {
                cursor_x = rec.x;
            }

            cursor_y += 0x10;
            no_wait = 0;
        } else if (character == TEXT_CHOICE_MARK) {
            rec.mark = column;
            mark_x = cursor_x - rec.x - 0x10;
            NEXT();
        } else if (character == TEXT_CHOICE_SELECT) {
            if (rec.mark != -1) {
                /* The options run from the line marked by 0xFB to the current line. */
                selection = rec.mark;
                for (i = 0;; i += g_script_event_speed) {
                    g_text_awaiting_input = 1;
                    select_window = WINDOW_AT(g_menu_packet_buffer_index);
                    select_window->sprites[0].x0
                        = mark_x + (select_window->quads[0].x0 + rec.x - select_window->quads[0].u0) - ((i >> 4) & 3);
                    select_window->sprites[0].y0
                        = (select_window->quads[0].y0 + rec.y - select_window->quads[0].v0) + selection * 16;
                    select_window->sprites[1].x0 = select_window->sprites[0].x0 + 2;
                    select_window->sprites[1].y0 = select_window->sprites[0].y0 + 2;
                    thread_yield();
                    if (thread_current_task_get() == THREAD_TASK_STOP_REQUEST) {
                        goto end;
                    }
                    if (thread_current_task_get() == THREAD_TASK_WAIT_FOR_RESUME) {
                        continue;
                    }
                    if ((g_script_event_input & PSX_PAD_UP) && selection != rec.mark) {
                        g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                        selection += no_value;
                    }
                    if ((g_script_event_input & PSX_PAD_DOWN) && selection != column) {
                        g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                        selection++;
                    }
                    if (g_script_event_input & PSX_PAD_CIRCLE) {
                        g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
                        break;
                    }
                }

                select_hidden_window = WINDOW_AT(g_menu_packet_buffer_index);
                select_hidden_window->sprites[0].x0 = 0x200;
                select_hidden_window->sprites[1].x0 = 0x200;
                thread_yield();
                select_hidden_window_next = WINDOW_AT(g_menu_packet_buffer_index);
                select_hidden_window_next->sprites[0].x0 = 0x200;
                select_hidden_window_next->sprites[1].x0 = 0x200;

                g_script_variables[EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION] = selection - rec.mark;
            }
            NEXT();
            g_text_awaiting_input = 0;
        } else if (character == TEXT_WAIT_FOR_RESUME) {
            thread_current_task_set(THREAD_TASK_WAIT_FOR_RESUME);
            resume_task = THREAD_TASK_RESUME;
            while (thread_current_task_get() != resume_task) {
                if (thread_current_task_get() == THREAD_TASK_STOP_REQUEST) {
                    goto end;
                }
                thread_yield();
            }

            /* With DIALOG_TYPE_KEEP_OPEN a resume also loads the next text from
             * parameter 2, as at 0xFE. */
            if (rec.keep_open != 0) {
                win->message_id = win[1].message_id = rec.text = thread_current_parameter_2_get();
                if ((rec.text & no_value) == no_value) {
                    break;
                }
                text = text_entry_find(rec.text);
                no_wait = 0;
                menu_build_and_upload_window_image(rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
                delay = 1;
                column = 0;
                width_override = no_value;
                cursor_x = rec.x;
                cursor_y = rec.y;
                value_index = 0;
                rec.color = 0;
                rec.mark = -1;
                rec.line = 0;
                saved = (const u8*)-1;
                repeat = 0;
                g_text_awaiting_input = 0;
                thread_yield();
                continue;
            }

            thread_yield();
            NEXT();
            /* The line break after a resume doesn't wait for a button. */
            no_wait = 1;
        } else if (character == TEXT_RELEASE_WAITING_THREADS) {
            for (i = 0; i < VARIANT_THREAD_TABLE_SLOT_COUNT; i++) {
                if (g_thread_current_id != i && g_thread_contexts[i].task_id == THREAD_TASK_WAIT_FOR_RESUME
                    && thread_is_running_asm(i)) {
                    g_thread_contexts[i].task_id = THREAD_TASK_RESUME;
                }
            }
            thread_yield();
            NEXT();
        } else if (character == TEXT_SET_PORTRAIT) {
            NEXT();
            win[0].portrait_code = *text;
            win[1].portrait_code = *text;
            script_variable_set(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, *text);
            NEXT();
        } else if (character == TEXT_SET_VARIABLE_BASE) {
            NEXT();
            variable_base = *text;
            NEXT();
        } else if (character == TEXT_STORE_VARIABLE) {
            NEXT();
            i = script_variable_get(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
            if (i != 0) {
                script_variable_set(variable_base + i * 10, *text);
                script_variable_set(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
            } else {
                script_variable_set(variable_base, *text);
            }
            NEXT();
        } else if (character == TEXT_SET_GLYPH_DELAY) {
            NEXT();
            delay = *text;
            NEXT();
        } else if (character == TEXT_SET_PALETTE) {
            NEXT();
            rec.color = *text;
            NEXT();
        } else {
            /* A substitution code with a value jumps into that entry (or the player's name);
             * saved brings the text back here afterward. */
            substitution = &g_text_substitution_values[value_index];
            value = text_format_id_resolve(character, *substitution);
            if (value != -1) {
                saved = text + 1;
                value_index++;
                if (character == TEXT_FORMAT_RAMZA_NAME) {
                    text = g_text_player_name_buffer;
                } else {
                    text = text_entry_find(value);
                }
                continue;
            }

            if (character == TEXT_SET_NUMBER_WIDTH) {
                NEXT();
                width_override = *text;
                NEXT();
            } else if (character == TEXT_PRINT_NEXT_VALUE) {
                value = *substitution;
                value_index++;
                goto print_value;
            } else if (character == TEXT_PRINT_INDEXED_VALUE) {
                NEXT();
                value = g_text_substitution_values[*text];
            print_value:
                digits = text_decimal_digits_count(value);
                started = 0;
                if (width_override != no_value) {
                    digits = width_override;
                    width_override = no_value;
                }

                /* Leading zeros print as 4-pixel spaces, except in the ones place. */
                for (i = digits - 1; i >= 0; i--) {
                    character = value / g_text_decimal_divisors[i];
                    if (value < g_text_decimal_divisors[i] && i != 0 && started == 0) {
                        character = TEXT_SPACE;
                    } else {
                        started = 1;
                        value -= character * g_text_decimal_divisors[i];
                    }
                    {
                        text_glyph_state_t* glyph = &g_message_typewriter_glyph;

                        glyph->x = cursor_x;
                        glyph->y = cursor_y;
                        glyph->glyph = character;
                        glyph->mode = rec.dialog_type & DIALOG_TYPE_BOX_TYPE_MASK;
                        glyph->palette = rec.color;
                    }
                    message_glyph_draw_timed(cursor_x, cursor_y, delay);
                    if (character == TEXT_SPACE) {
                        cursor_x += 4;
                    } else {
                        cursor_x += g_text_glyph_widths[character];
                    }
                }
                NEXT();
            } else if ((character & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (character & 0xF) < 4) {
                /* Back-reference: replay repeat bytes from value bytes back, then resume at saved. */
                saved = text + 3;
                value = (text[0] & 3) * 8;
                repeat = value + (text[1] >> 5) + 4;
                value = text[1] & 0xF;
                value *= 0xFE;
                value = value + text[2];
                text -= value;
            } else {
                if (character < TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                    character = *text;
                } else if ((character & 0xF0) == TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                    character = (*text & 0xF) * TEXT_GLYPHS_PER_PREFIX;
                    NEXT();
                    /* The glyph's second byte may itself start a back-reference. */
                    if ((*text & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (*text & 0xF) < 4) {
                        saved = text + 3;
                        value = (text[0] & 3) * 8;
                        repeat = value + (text[1] >> 5) + 4;
                        value = text[1] & 0xF;
                        value *= 0xFE;
                        value = value + text[2];
                        text -= value;
                    }
                    character += *text;
                }

                {
                    text_glyph_state_t* glyph = &g_message_typewriter_glyph;

                    glyph->x = cursor_x;
                    glyph->y = cursor_y;
                    glyph->glyph = character;
                    glyph->mode = rec.box_type;
                    glyph->palette = rec.color;
                }
                message_glyph_draw_timed(cursor_x, cursor_y, delay);
                if (character == TEXT_SPACE) {
                    cursor_x += 4;
                } else {
                    cursor_x += g_text_glyph_widths[character];
                }
                NEXT();
            }
        }
    }

end:
    /* A portrait window shrinks back into the point it opened from. */
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        from.w = 0;
        from.h = 0;
        from.x = right;
        from.y = bottom;
        mem_copy_bytes_nonempty(&to, &from, 8);
        menu_window_quads_tween(win, &from, &to, 4);
    }

    g_text_awaiting_input = 0;
    win->active = 0;
    menu_release_window_buffer_pair(win);
    gfx_free_texture_rect(&rec.rect);
    message_typewriter_slot_release();
    thread_yield();
    thread_current_exit();
}
