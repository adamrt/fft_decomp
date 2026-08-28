#include "fft/battle_text.h"
#include "fft/menu.h"
#include "fft/script_variables.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/types.h"

/* Text image record handed to world_menu_render_text_image_at_fixed_origin;
 * the leading fields follow world_menu_text_image_t. */
typedef struct world_text_box_record {
    void* buffer;    /* 0x00 */
    u16 width;       /* 0x04 */
    u16 height;      /* 0x06 */
    RECT rect;       /* 0x08 */
    s32 text;        /* 0x10 */
    u16 color;       /* 0x14 */
    u16 dialog_type; /* 0x16 */
    u16 box_type;    /* 0x18: dialog_type & 0x70 */
    u16 position;    /* 0x1a */
    u16 options;     /* 0x1c: dialog_type & 0xc */
    u8 unk_1e[2];    /* 0x1e */
    u16 line;        /* 0x20 */
    u16 first_line;  /* 0x22 */
    u16 last_line;   /* 0x24 */
    u8 unk_26[6];    /* 0x26 */
    s16 mark;        /* 0x2c: characters per line, later the 0xFB cursor column */
    s16 lines;       /* 0x2e */
    s16 tail_offset; /* 0x30: world_menu_build_and_upload_window_image tail offset */
    u8 unk_32[0x50 - 0x32];
} world_text_box_record_t;

/* Record `index` of the window buffer pair; the offset is the first addu
 * operand in the target, which plain `&win[index]` does not reproduce. */
#define WINDOW_AT(index) ((menu_window_buffer_t*)((index) * sizeof(menu_window_buffer_t) + (s32)win))

/* Advance the text cursor; a finished back-reference resumes at `saved`. */
#define NEXT()                                                                                                         \
    text++;                                                                                                            \
    if (repeat > 0) {                                                                                                  \
        repeat--;                                                                                                      \
        if (repeat == 0) {                                                                                             \
            text = saved;                                                                                              \
            saved = (u8*)-1;                                                                                           \
        }                                                                                                              \
    }

/* Message text box thread (task 0x26).
 *
 * Draws the text entry named by thread parameter 2 glyph by glyph into a
 * window buffer pair, waiting for input at page breaks (0xF8/0xFE) and
 * restarting when parameter 2 changes. Parameter 1 selects the window style
 * (bits 4-6) and vertical position (bits 0-1). `no_value` is assigned inside
 * the loop so loop.c hoists it as an invariant: the resulting spilled
 * constant is what reproduces the target's 0xFFFF reloads and its reload
 * register rotation. */
void world_text_message_box_thread(void) {
    world_text_box_record_t rec;
    RECT from;
    RECT to;
    s32 column;
    s32 message;
    u8* base_entry;
    s16 x;
    s16 y;
    u16 cursor_x;
    u16 cursor_y;
    s16 right;
    s16 bottom;
    u16 color;
    u16 no_wait;
    u16 script_variable_base;
    s32 width_override;
    u16 value_index;
    menu_window_buffer_t* win;
    menu_window_buffer_t* cursor_window;
    menu_window_buffer_t* arrow_window;
    menu_window_buffer_t* hidden_window;
    u8* text;
    u8* saved;
    s32 repeat;
    s32 code;
    s32 value;
    s32 i;
    s32 digits;
    u16 started;
    u16 param;
    u16 half_w;
    u16 half_h;
    s32* substitution;
    s32 no_value;

    world_thread_set_current_task_id(NATIVE_THREAD_TASK_MESSAGE_BOX);
    value_index = 0;
    script_variable_base = 0x18;
    g_world_menu_event_state_flag = 0;
    param = (s32)world_thread_get_current_parameter_1();
    rec.box_type = param & 0x70;
    rec.dialog_type = param;
    rec.position = param & 3;
    rec.options = param & 0xC;
    text = world_text_find_entry(world_thread_get_current_parameter_2());
    base_entry = world_text_find_entry(world_thread_get_current_parameter_2() & 0xF800);
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        rec.mark = 0x12;
        rec.lines = 3;
    } else {
        rec.mark = 0x14;
        rec.lines = 2;
    }
    rec.height = rec.lines * 16 + 16;
    rec.width = rec.mark * 10 + 0x18;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        rec.width = rec.mark * 10 + 0x40;
    }
    rec.tail_offset = 0;
    half_w = rec.width >> 1;
    x = (0x100 - half_w) & ~1;
    half_h = rec.height >> 1;
    if (rec.position == 1) {
        y = 0xC;
    }
    if (rec.position == 2) {
        y = 0xE4 - rec.height;
    }
    if (rec.position == 3) {
        y = (0x78 - half_h) & ~1;
    }
    right = x + half_w - 6;
    bottom = y + half_h - 2;
    win = world_menu_alloc_window_buffer_pair();
    win->message_id = world_thread_get_current_parameter_2();
    if (text[0] == TEXT_SET_PORTRAIT) {
        win[0].portrait_code = text[1];
        win[1].portrait_code = text[1];
        world_script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, text[1]);
    } else {
        win[0].portrait_code = world_script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
        win[1].portrait_code = world_script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
    }
    win[0].is_message_box = 1;
    win[1].is_message_box = 1;
    rec.text = world_thread_get_current_parameter_2();
    message = world_thread_get_current_parameter_2();
    rec.rect.w = rec.width;
    rec.rect.h = rec.height;
    world_gfx_alloc_texture_grid_rect(&rec.rect, (world_texture_prim_t*)win->quads, (u32*)-1);
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        world_menu_build_and_upload_window_image(
            rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.tail_offset);
    } else {
        world_menu_build_and_upload_window_frame_image(rec.width, rec.height, &rec.rect, 0);
    }
    if (g_world_formation_screen_active != 0) {
        x -= 0x100;
        y -= 0x78;
        right -= 0x100;
        bottom -= 0x78;
    }
    win->quads[1].x0 = right;
    win->quads[1].y0 = bottom;
    win->quads[1].x1 = right;
    win->quads[1].y1 = bottom;
    win->quads[1].x2 = right;
    win->quads[1].y2 = bottom;
    win->quads[1].x3 = right;
    win->quads[1].y3 = bottom;
    win->quads[0].x0 = right;
    win->quads[0].y0 = bottom;
    win->quads[0].x1 = right;
    win->quads[0].y1 = bottom;
    win->quads[0].x2 = right;
    win->quads[0].y2 = bottom;
    win->quads[0].x3 = right;
    win->quads[0].y3 = bottom;
    from.x = x;
    from.y = y;
    from.w = rec.width;
    from.h = rec.height;
    if (rec.position == 1) {
        to.x = x + 8;
        to.y = y + 7;
        to.w = 0x1F;
        to.h = 0x30;
        world_gfx_flip_poly_ft4_u_if_negative(&win->quads[1], -1);
    } else {
        to.x = x + rec.width - 0x30;
        to.y = y + 8;
        to.w = 0x1F;
        to.h = 0x30;
    }
    world_script_copy_bytes(&win[1], &win[0], 0x118);
    win[0].active = 1;
    win[1].active = 1;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        world_menu_animate_window_quads_to_rects(win, &from, &to);
    } else {
        /* The target also passes &to in a2 to this two-argument callee. */
        ((void (*)(menu_window_buffer_t*, RECT*, RECT*))world_menu_animate_window_quad_crop_full)(win, &from, &to);
    }
    x = win->quads[0].u0 + 8;
    y = win->quads[0].v0 + 8;
    /* One word compare of box_type == 0x10 and position == 1, as in the target. */
    if (*(s32*)&rec.box_type == 0x10010) {
        x = win->quads[0].u0 + 0x30;
    }
    color = 1;
    saved = (u8*)-1;
    repeat = 0;
    no_wait = 0;
    column = 0;
    rec.color = 0;
    rec.mark = -1;
    rec.line = 0;
    cursor_x = x;
    cursor_y = y;
    width_override = 0xFFFF;
    while (1) {
        no_value = 0xFFFF;
        code = *text;
        if (world_thread_get_current_parameter_2() != message) {
            if ((world_thread_get_current_parameter_2() & no_value) != no_value) {
                while (base_entry != world_text_find_entry(world_thread_get_current_parameter_2() & 0xF800)) {
                    world_thread_yield();
                }
            }
            message = world_thread_get_current_parameter_2();
            if ((message & no_value) == no_value) {
                g_world_menu_event_state_flag = 0;
                break;
            }
            text = world_text_find_entry(message);
            if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
                world_menu_build_and_upload_window_image(
                    rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.tail_offset);
            } else {
                world_menu_build_and_upload_window_frame_image(rec.width, rec.height, &rec.rect, 0);
            }
            color = 1;
            saved = (u8*)-1;
            cursor_x = x;
            cursor_y = y;
            no_wait = 0;
            column = 0;
            value_index = 0;
            rec.color = 0;
            rec.mark = -1;
            rec.line = 0;
            g_world_menu_event_state_flag = 0;
            width_override = no_value;
            rec.text = message;
            repeat = 0;
            if (text[0] == TEXT_SET_PORTRAIT) {
                win[0].portrait_code = text[1];
                win[1].portrait_code = text[1];
                world_script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, text[1]);
            } else {
                win[0].portrait_code = world_script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
                win[1].portrait_code = world_script_get_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT);
            }
            world_thread_yield();
            continue;
        }
        if (code == TEXT_NEWLINE || code == TEXT_END_WAIT_FOR_CONFIRM || code == TEXT_END_AUTO_CLOSE
            || code == TEXT_PAGE_BREAK) {
            if (code == TEXT_END_AUTO_CLOSE) {
                break;
            }
            if (code == TEXT_END_WAIT_FOR_CONFIRM) {
                g_world_menu_event_state_flag = 1;
                if (saved != (u8*)-1) {
                    text = saved;
                    saved = (u8*)-1;
                    continue;
                }
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
            if (code != TEXT_PAGE_BREAK) {
                column++;
                rec.line++;
            }
            rec.first_line = rec.line - rec.lines;
            if (column >= rec.lines || code == TEXT_END_WAIT_FOR_CONFIRM || code == TEXT_PAGE_BREAK) {
                if (code != TEXT_PAGE_BREAK) {
                    column = 0;
                }
                if (no_wait == 0) {
                    for (i = 0;; i += g_world_event_speed) {
                        world_thread_yield();
                        if (world_thread_get_current_parameter_2() != message) {
                            break;
                        }
                        if (code == TEXT_PAGE_BREAK) {
                            cursor_window = WINDOW_AT(g_world_menu_packet_buffer_index);
                            cursor_window->sprites[0].x0 = cursor_x
                                + (x + cursor_window->quads[0].x0 - cursor_window->quads[0].u0)
                                + (s16)(((i >> 4) & 3) - 8);
                            cursor_window->sprites[0].y0 = cursor_y
                                + (y + cursor_window->quads[0].y0 - cursor_window->quads[0].v0)
                                - (cursor_window->quads[0].v0 + 8);
                            cursor_window->sprites[1].x0 = cursor_window->sprites[0].x0 + 2;
                            cursor_window->sprites[1].y0 = cursor_window->sprites[0].y0 + 2;
                        } else {
                            if (code == TEXT_NEWLINE) {
                                WINDOW_AT(g_world_menu_packet_buffer_index)->sprites[2].u0 = (i & 0x30) - 0x58;
                                WINDOW_AT(g_world_menu_packet_buffer_index)->sprites[2].v0 = 0x10;
                                arrow_window = WINDOW_AT(g_world_menu_packet_buffer_index);
                                arrow_window->sprites[2].x0 = cursor_x + arrow_window->quads[0].x0;
                                arrow_window->sprites[2].y0
                                    = cursor_y + arrow_window->quads[0].y0 - arrow_window->quads[0].v0;
                            }
                            /* The else-break keeps the test on top, as in the target; a
                               `while` is rotated to test at the bottom. */
                            for (;;) {
                                if (rec.line - rec.lines != rec.first_line) {
                                    WINDOW_AT(g_world_menu_packet_buffer_index)->sprites[2].x0 = 0;
                                    rec.first_line++;
                                    rec.last_line = rec.first_line + rec.lines;
                                    world_menu_render_text_image_at_fixed_origin((world_menu_text_image_t*)&rec);
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }
                hidden_window = WINDOW_AT(g_world_menu_packet_buffer_index);
                hidden_window->sprites[0].x0 = 0x200;
                hidden_window->sprites[1].x0 = 0x200;
                hidden_window->sprites[2].x0 = 0x200;
                world_thread_yield();
                hidden_window = WINDOW_AT(g_world_menu_packet_buffer_index);
                hidden_window->sprites[0].x0 = 0x200;
                hidden_window->sprites[1].x0 = 0x200;
                hidden_window->sprites[2].x0 = 0x200;
                if (code == TEXT_PAGE_BREAK) {
                    continue;
                }
                if (world_thread_get_current_parameter_2() != message) {
                    continue;
                }
                if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
                    world_menu_build_and_upload_window_image(
                        rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.tail_offset);
                } else {
                    world_menu_build_and_upload_window_frame_image(rec.width, rec.height, &rec.rect, 0);
                }
                cursor_y = y - 0x10;
                world_thread_yield();
            }
            no_wait = 0;
            cursor_x = x;
            cursor_y += 0x10;
        } else if (code == TEXT_CHOICE_MARK) {
            rec.mark = column;
            NEXT();
        } else if (code == TEXT_CHOICE_SELECT) {
            NEXT();
        } else if (code == TEXT_WAIT_FOR_RESUME) {
            NEXT();
            no_wait = 1;
        } else if (code == TEXT_RELEASE_WAITING_THREADS) {
            world_thread_yield();
            NEXT();
        } else if (code == TEXT_SET_PORTRAIT) {
            NEXT();
            win[0].portrait_code = *text;
            win[1].portrait_code = *text;
            world_script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, *text);
            NEXT();
        } else if (code == TEXT_SET_VARIABLE_BASE) {
            NEXT();
            script_variable_base = *text;
            NEXT();
        } else if (code == TEXT_STORE_VARIABLE) {
            NEXT();
            i = world_script_get_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
            if (i != 0) {
                world_script_set_variable(script_variable_base + i * 10, *text);
                world_script_set_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
            } else {
                world_script_set_variable(script_variable_base, *text);
            }
            NEXT();
        } else if (code == TEXT_SET_GLYPH_COLOR) {
            NEXT();
            color = *text;
            NEXT();
        } else if (code == TEXT_SET_PALETTE) {
            NEXT();
            rec.color = *text;
            NEXT();
        } else {
            substitution = &g_world_text_substitution_values[value_index];
            value = world_text_adjust_value_for_event_code(code, *substitution);
            if (value != -1) {
                saved = text + 1;
                value_index++;
                text = world_text_find_entry(value);
                continue;
            }
            if (code == TEXT_SET_NUMBER_WIDTH) {
                NEXT();
                width_override = *text;
                NEXT();
            } else if (code == TEXT_PRINT_NEXT_VALUE || code == TEXT_PRINT_INDEXED_VALUE) {
                if (code == TEXT_PRINT_NEXT_VALUE) {
                    value = *substitution;
                    value_index++;
                } else {
                    NEXT();
                    value = g_world_text_substitution_values[*text];
                }
                digits = world_text_count_decimal_digits(value);
                started = 0;
                if (width_override != no_value) {
                    digits = width_override;
                    width_override = no_value;
                }
                for (i = digits - 1; i >= 0; i--) {
                    code = value / g_world_text_message_box_digit_divisors[i];
                    if (value < g_world_text_message_box_digit_divisors[i] && i != 0 && started == 0) {
                        code = TEXT_SPACE;
                    } else {
                        started = 1;
                        value -= code * g_world_text_message_box_digit_divisors[i];
                    }
                    g_world_text_message_box_glyph.code = code;
                    g_world_text_message_box_glyph.pixel_x = cursor_x;
                    g_world_text_message_box_glyph.y = cursor_y;
                    g_world_text_message_box_glyph.mode = rec.dialog_type & 0x70;
                    g_world_text_message_box_glyph.color = rec.color;
                    world_text_draw_glyph_with_typewriter_delay_2((s16)cursor_x, (s16)cursor_y, color);
                    if (code == TEXT_SPACE) {
                        cursor_x += 4;
                    } else {
                        cursor_x += g_world_text_glyph_widths[code];
                    }
                }
                NEXT();
            } else if ((code & 0xF0) == 0xF0 && (code & 0xF) < 4) {
                saved = text + 3;
                value = (text[0] & 3) * 8;
                repeat = text[1] >> 5;
                repeat += value;
                repeat += 4;
                value = text[1] & 0xF;
                value *= 0xFE;
                value = value + text[2];
                text -= value;
            } else {
                if (code < 0xD0) {
                    code = *text;
                } else if ((code & 0xF0) == 0xD0) {
                    code = (*text & 0xF) * 0xD0;
                    NEXT();
                    code = code + *text;
                }
                g_world_text_message_box_glyph.code = code;
                g_world_text_message_box_glyph.mode = rec.box_type;
                g_world_text_message_box_glyph.color = rec.color;
                g_world_text_message_box_glyph.pixel_x = cursor_x;
                g_world_text_message_box_glyph.y = cursor_y;
                world_text_draw_glyph_with_typewriter_delay_2((s16)cursor_x, (s16)cursor_y, color);
                if (code == TEXT_SPACE) {
                    cursor_x += 4;
                } else {
                    cursor_x += g_world_text_glyph_widths[code];
                }
                NEXT();
            }
        }
    }
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        from.w = 0;
        from.h = 0;
        from.x = right;
        from.y = bottom;
        world_script_copy_bytes(&to, &from, 8);
        world_menu_animate_window_quads_to_rects(win, &from, &to);
    }
    win->active = 0;
    world_menu_release_window_buffer_pair(win);
    world_gfx_free_texture_grid_rect(&rec.rect);
    world_thread_clear_current_slot();
    world_thread_yield();
    world_thread_exit_current();
}
