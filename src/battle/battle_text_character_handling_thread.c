#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_menu_window.h"
#include "fft/battle_text.h"
#include "fft/etc.h"
#include "fft/event.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/option.h"
#include "fft/script_variables.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

typedef union battle_texture_prim battle_texture_prim_t;

/* Members of g_battle_text_typewriter_glyph (+0x02, +0x04, +0x0c, +0x0e),
 * bound separately: member stores add a spill slot to this function. */
extern s16 g_battle_text_typewriter_glyph_y;
extern s16 g_battle_text_typewriter_glyph_character;
extern s16 g_battle_text_typewriter_glyph_dialogue_type;
extern s16 g_battle_text_typewriter_glyph_palette;

extern void battle_gfx_alloc_tpage7_vram(RECT* rect, union battle_texture_prim* sprite, u32* image_data);
extern void battle_menu_tween_window_quads(menu_window_buffer_t* buffers, RECT* to, RECT* to2, s32 table);
extern void battle_menu_animate_window_quad_crop(menu_window_buffer_t* buffers, RECT* rect);

#define WINDOW_AT(index) ((menu_window_buffer_t*)((index) * sizeof(menu_window_buffer_t) + (s32)win))

#define NEXT() text = battle_text_advance_cursor_with_separate_backreference_state(&repeat, text, &saved)

#define CHECK_ABORT()                                                                                                  \
    if (battle_thread_get_current_task_id() == NATIVE_THREAD_TASK_STOP_REQUEST) {                                      \
        goto end;                                                                                                      \
    }

/* Dialog text thread: opens a dialog window and draws the text entry named
 * by thread parameter 2 glyph by glyph, handling the control bytes.
 *
 * The 0xFE/0xFD waits compare against resume_task rather than the constant
 * NATIVE_THREAD_TASK_RESUME (1): with a constant, loop.c pools every constant 1
 * of the main loop into one hoisted (and spilled) pseudo, where the target
 * keeps each wait's 1 in a saved register. The 0xE4/0xE6 dispatch shares the digit printer through a
 * label; the WORLD twin's `character == 0xE4 || character == 0xE6` form is not threaded here
 * and loses the substitution-table base CSE. */
void battle_text_character_handling_thread(void) {
    /* dialog_type is the DisplayMessage Dialog Type byte; box_type is its 0x70 bits.
     * The offsets at 0x44..0x4c are the instruction's arrow position and coordinates. */
    world_text_dialog_record_t rec;
    RECT from;
    RECT to;
    s32 repeat;
    u8* saved;
    s32 column;
    s32 width_override;
    u16 cursor_y;
    s16 right;
    s16 bottom;
    u16 mark_x;
    u16 color;
    u16 no_wait;
    u16 variable_base;
    u16 value_index;
    native_thread_t* thread;
    menu_window_buffer_t* win;
    menu_window_buffer_t* cursor_window;
    menu_window_buffer_t* arrow_window;
    menu_window_buffer_t* scroll_window;
    menu_window_buffer_t* hidden_window;
    menu_window_buffer_t* hidden_window_next;
    menu_window_buffer_t* select_window;
    menu_window_buffer_t* select_hidden_window;
    menu_window_buffer_t* select_hidden_window_next;
    u8* text;
    s16 cursor_x;
    s32 character;
    s32 i;
    s32 k;
    s32 value;
    s32 digits;
    u16 started;
    u16 selection;
    s32* substitution;
    s32 event_id;
    s32 window_u;
    s32 half_w;
    s32 half_h;
    s32 center;
    u16 param;
    s32 no_value;
    s32 resume_task;

    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_RESUME);
    battle_script_set_variable(EVENT_SCRIPT_VAR_TYPEWRITER_VRAM_X_OFFSET, 0);
    value_index = 0;
    variable_base = 0x18;
    thread = (native_thread_t*)((g_battle_current_thread_id << 10) + (s32)g_battle_threads);
    param = (s32)battle_thread_get_current_parameter_1();
    rec.dialog_type = param;
    if ((param & 0x70) == 0x70) {
        rec.dialog_type = (param & 0x1C) | 0x10;
        rec.dialog_type |= battle_unit_check_facing_relative_to_camera(battle_thread_get_current_parameter_3()) + 1;
    }
    rec.unk_2a = 0;
    rec.origin_x = 8;
    rec.origin_y = 8;
    rec.box_type = rec.dialog_type & 0x70;
    rec.position = rec.dialog_type & 3;
    rec.options = rec.dialog_type & 0xC;
    rec.no_box = rec.dialog_type & 0x80;
    rec.arrow_offset = thread->task_words[4];
    rec.x_offset = thread->task_words[2];
    rec.y_offset = thread->task_words[3];
    if (rec.dialog_type & 0x10) {
        rec.portrait = thread->function_parameter_4 - 1;
    } else {
        rec.portrait = thread->function_parameter_4 - 1;
    }
    text = battle_text_init_entry(battle_thread_get_current_parameter_2());
    battle_text_measure_pixels(&rec.mark, &rec.lines, text);
    if (thread->task_words[6] != 0) {
        rec.mark = thread->task_words[6];
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
        if (rec.options == 0) {
            rec.height = rec.lines * 16 + 0x18;
        }
        if (rec.options == 4) {
            rec.height += 0x10;
        }
    }
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.options != 8) {
        center = rec.width >> 1;
        value = battle_get_misc_id(battle_thread_get_current_parameter_3());
        battle_unit_project_misc_to_screen(value, g_battle_scratch_coords);
        cursor_x = center;
        rec.offset = 0;
        rec.x = g_battle_scratch_coords[0] - center - 4;
        if (rec.position == 0) {
            if (battle_thread_get_current_parameter_3() == (u16)g_battle_text_speaker_swap_unit_a) {
                value = (u16)g_battle_text_speaker_swap_unit_b;
            } else if (battle_thread_get_current_parameter_3() == (u16)g_battle_text_speaker_swap_unit_b) {
                value = (u16)g_battle_text_speaker_swap_unit_a;
            }
            battle_unit_project_misc_to_screen(battle_get_misc_id(value), (s16*)&g_battle_camera_script_rotation);
            if (g_battle_scratch_coords[1] < 0
                || ((s16*)&g_battle_camera_script_rotation)[1] < g_battle_scratch_coords[1]) {
                rec.position = 2;
            } else {
                rec.position = 1;
            }
            rec.dialog_type = (rec.dialog_type & 0xFFF0) | rec.position;
        }
        if (rec.position == 1) {
            rec.x += 0xC;
            rec.y = g_battle_scratch_coords[1] - rec.height - 0x28;
        }
        if (rec.position == 2) {
            rec.x -= 8;
            rec.y = g_battle_scratch_coords[1];
        }
        if (rec.x < 0x88) {
            rec.offset = rec.x - 0x88;
            rec.x = 0x88;
        } else if (rec.x + rec.width > 0x180) {
            rec.offset = rec.x + rec.width - 0x180;
            rec.x = 0x180 - rec.width;
        }
        rec.x += rec.x_offset;
        rec.y += rec.y_offset;
        if (rec.x < 0x88) {
            rec.offset += rec.x - 0x88;
            rec.x = 0x88;
        } else if (rec.x + rec.width > 0x180) {
            rec.offset += rec.x + rec.width - 0x180;
            rec.x = 0x180 - rec.width;
        }
        rec.offset += rec.arrow_offset;
        if (rec.y < 0x16) {
            rec.y = 0x16;
        } else if (rec.y + rec.height > 0xE4) {
            rec.y = 0xE4 - rec.height;
        }
        if (rec.offset > cursor_x - 0x18) {
            rec.offset = cursor_x - 0x18;
        }
        if (rec.offset < -cursor_x + 0x18) {
            rec.offset = -cursor_x + 0x18;
        }
        if (rec.position == 1) {
            bottom = rec.y + rec.height - 8;
        }
        if (rec.position == 2) {
            bottom = rec.y + 8;
        }
        right = rec.x + rec.offset + (rec.width >> 1);
    } else {
        rec.offset = 0;
        half_w = rec.width >> 1;
        rec.x = (0x100 - half_w) & 0xFFFE;
        half_h = rec.height >> 1;
        if (rec.position == 1) {
            rec.y = 0x16;
        }
        if (rec.position == 2) {
            rec.y = 0xE4 - rec.height;
        }
        if (rec.position == 3) {
            rec.y = (0x78 - half_h) & 0xFFFE;
        }
        rec.x += rec.x_offset;
        rec.y += rec.y_offset;
        right = half_w + rec.x - 6;
        bottom = half_h + rec.y - 2;
    }
    win = battle_menu_alloc_window_buffer_pair();
    win->message_id = battle_thread_get_current_parameter_2();
    win->dialogue_selector = rec.position;
    win->box_type = rec.box_type;
    rec.text = battle_thread_get_current_parameter_2();
    rec.rect.w = rec.width;
    rec.rect.h = rec.height;
    battle_gfx_alloc_tpage7_vram(&rec.rect, (battle_texture_prim_t*)win->quads, (u32*)-1);
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        if (text[0] == TEXT_SET_PORTRAIT) {
            win->portrait_code = text[1];
        } else {
            win->portrait_code = rec.portrait;
        }
        value = battle_unit_get_battle_index_by_misc_id(battle_get_misc_id(battle_thread_get_current_parameter_3()));
        battle_menu_build_and_upload_window_image(rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.offset);
        win->portrait_battle_id = value;
    } else if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
        win[0].quads[0].clut = 0x7CBC;
        win[1].quads[0].clut = 0x7CBC;
        ClearImage(&rec.rect, 0, 0, 0);
    } else {
        rec.first_line = 0;
        rec.last_line = rec.lines;
        battle_menu_render_text_image_at_record_origin((battle_menu_text_image_t*)&rec);
    }
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
    win->quads[0].x0 = right;
    win->quads[0].y0 = bottom;
    win->quads[0].x1 = right;
    win->quads[0].y1 = bottom;
    win->quads[0].x2 = right;
    win->quads[0].y2 = bottom;
    win->quads[0].x3 = right;
    win->quads[0].y3 = bottom;
    from.x = rec.x;
    from.y = rec.y;
    from.w = rec.width;
    from.h = rec.height;
    rec.arrow_x = rec.x + rec.width - 0x1E;
    rec.arrow_y = rec.y + rec.height - 0x18;
    to.x = 0;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        win->quads[1].x0 = right;
        win->quads[1].y0 = bottom;
        win->quads[1].x1 = right;
        win->quads[1].y1 = bottom;
        win->quads[1].x2 = right;
        win->quads[1].y2 = bottom;
        win->quads[1].x3 = right;
        win->quads[1].y3 = bottom;
        if (rec.offset < 0) {
            to.x = rec.x + 8;
            if (rec.portrait < 8) {
                rec.origin_x += 0x28;
            }
        } else {
            to.x = rec.x + rec.width - 0x30;
            rec.arrow_x -= 0x20;
        }
        to.y = rec.y + 7;
        if (rec.options != 8) {
            if (rec.position == 2) {
                to.y = rec.y + 0xF;
                rec.origin_y += 8;
            } else {
                rec.arrow_y -= 8;
            }
        }
        if (rec.options == 4) {
            rec.arrow_y -= 8;
        }
        to.w = 0x1F;
        to.h = 0x30;
        if (rec.offset < 0) {
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
    battle_copy_bytes(&win[1], win, 0x118);
    win[0].active = 1;
    win[1].active = 1;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        battle_menu_tween_window_quads(win, &from, &to, thread->task_words[5] & 0xF);
    } else {
        /* The target passes a third argument the two-parameter callee ignores. */
        ((void (*)(menu_window_buffer_t*, RECT*, RECT*))battle_menu_animate_window_quad_crop)(win, &from, &to);
    }
    window_u = win->quads[0].u0;
    rec.x = window_u + 8;
    rec.y = win->quads[0].v0 + 8;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.offset < 0 && rec.portrait < 8) {
        rec.x = window_u + 0x30;
    }
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.options != 8 && rec.position == 2) {
        rec.y += 8;
    }
    no_wait = 0;
    color = 1;
    column = 0;
    rec.color = 0;
    rec.line = 0;
    g_battle_text_current_line = 0;
    if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
        event_id = (s16)battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
        if ((event_id == 0xF9 || event_id == 0x12A) && g_battle_etc_graphics_enabled == 0) {
            cursor_x = rec.x;
        } else {
            s32 line_width = g_battle_text_line_widths[0] + 0x18;
            cursor_x = rec.x + ((u32)(rec.width - line_width) >> 1);
        }
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
    saved = (u8*)-1;
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
                    for (k = 0;; k += g_battle_event_speed) {
                        if (rec.box_type == DIALOG_BOX_TYPE_CENTERED && character != TEXT_PAGE_BREAK) {
                            for (k = 0x80; k > 0x30; k -= 8) {
                                WINDOW_AT(g_battle_menu_packet_buffer_index)->r = k;
                                WINDOW_AT(g_battle_menu_packet_buffer_index)->g = k;
                                WINDOW_AT(g_battle_menu_packet_buffer_index)->b = k;
                                battle_thread_yield();
                            }
                            break;
                        }
                        battle_thread_yield();
                        if (g_battle_script_event_input & (PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT)) {
                            break;
                        }
                        CHECK_ABORT();
                        if (character == TEXT_PAGE_BREAK) {
                            if (rec.box_type != DIALOG_BOX_TYPE_PORTRAIT) {
                                cursor_window = WINDOW_AT(g_battle_menu_packet_buffer_index);
                                cursor_window->sprites[0].x0 = cursor_x
                                    + (cursor_window->quads[0].x0 + rec.x - cursor_window->quads[0].u0)
                                    + (s16)(((k >> 4) & 3) - 8);
                                cursor_window->sprites[0].y0 = cursor_y
                                    + (cursor_window->quads[0].y0 + rec.y - cursor_window->quads[0].v0)
                                    - (cursor_window->quads[0].v0 + 8);
                                cursor_window->sprites[1].x0 = cursor_window->sprites[0].x0 + 2;
                                cursor_window->sprites[1].y0 = cursor_window->sprites[0].y0 + 2;
                            }
                        } else {
                            if (character == TEXT_END_WAIT_FOR_CONFIRM && rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
                                g_battle_text_awaiting_input = 1;
                            }
                            if (character == TEXT_NEWLINE) {
                                WINDOW_AT(g_battle_menu_packet_buffer_index)->sprites[2].u0 = (k & 0x30) - 0x58;
                                WINDOW_AT(g_battle_menu_packet_buffer_index)->sprites[2].v0 = 0x10;
                                arrow_window = WINDOW_AT(g_battle_menu_packet_buffer_index);
                                arrow_window->sprites[2].x0 = rec.arrow_x;
                                arrow_window->sprites[2].y0 = rec.arrow_y;
                            }
                            if (g_battle_script_event_current_input & PSX_PAD_SQUARE) {
                                while (1) {
                                    if (rec.line != rec.lines) {
                                        scroll_window = WINDOW_AT(g_battle_menu_packet_buffer_index);
                                        scroll_window->scroll_range = rec.line - rec.lines;
                                        scroll_window->scroll_position = rec.first_line;
                                        scroll_window->icon_flags = 4;
                                        if (rec.first_line != 0) {
                                            scroll_window->icon_flags = 5;
                                        }
                                        if (rec.first_line != rec.line - rec.lines) {
                                            WINDOW_AT(g_battle_menu_packet_buffer_index)->icon_flags |= 2;
                                        }
                                    }
                                    if ((g_battle_script_event_input & PSX_PAD_UP) && rec.first_line != 0) {
                                        g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                                        WINDOW_AT(g_battle_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                        rec.first_line += no_value;
                                        rec.last_line = rec.first_line + rec.lines;
                                        battle_menu_render_text_image_at_record_origin((battle_menu_text_image_t*)&rec);
                                    }
                                    if ((g_battle_script_event_input & PSX_PAD_DOWN)
                                        && rec.first_line != rec.line - rec.lines) {
                                        g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                                        WINDOW_AT(g_battle_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                        rec.first_line++;
                                        rec.last_line = rec.first_line + rec.lines;
                                        battle_menu_render_text_image_at_record_origin((battle_menu_text_image_t*)&rec);
                                    }
                                    if (!(g_battle_script_event_current_input & PSX_PAD_SQUARE)) {
                                        break;
                                    }
                                    CHECK_ABORT();
                                    WINDOW_AT(g_battle_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                    battle_thread_yield();
                                }
                                win[0].icon_flags = 0;
                                win[1].icon_flags = 0;
                            }
                            for (;;) {
                                if (rec.line - rec.lines != rec.first_line) {
                                    WINDOW_AT(g_battle_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                    rec.first_line++;
                                    rec.last_line = rec.first_line + rec.lines;
                                    battle_menu_render_text_image_at_record_origin((battle_menu_text_image_t*)&rec);
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }
                g_battle_text_awaiting_input = 0;
                hidden_window = WINDOW_AT(g_battle_menu_packet_buffer_index);
                hidden_window->sprites[0].x0 = 0x200;
                hidden_window->sprites[1].x0 = 0x200;
                hidden_window->sprites[2].x0 = 0x200;
                hidden_window->icon_flags = 0;
                battle_thread_yield();
                hidden_window_next = WINDOW_AT(g_battle_menu_packet_buffer_index);
                hidden_window_next->sprites[0].x0 = 0x200;
                hidden_window_next->sprites[1].x0 = 0x200;
                hidden_window_next->sprites[2].x0 = 0x200;
                hidden_window_next->icon_flags = 0;
                if (character == TEXT_END_WAIT_FOR_CONFIRM) {
                    if (rec.no_box == 0) {
                        break;
                    }
                    battle_thread_set_current_task_id(NATIVE_THREAD_TASK_DIALOG_AWAIT_TEXT);
                    resume_task = NATIVE_THREAD_TASK_RESUME;
                    while (battle_thread_get_current_task_id() != resume_task) {
                        if (battle_thread_get_current_task_id() == NATIVE_THREAD_TASK_STOP_REQUEST) {
                            goto end;
                        }
                        battle_thread_yield();
                    }
                    win->message_id = win[1].message_id = rec.text = battle_thread_get_current_parameter_2();
                    if ((rec.text & no_value) == no_value) {
                        break;
                    }
                    text = battle_text_init_entry(rec.text);
                    no_wait = 0;
                    battle_menu_build_and_upload_window_image(
                        rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.offset);
                    color = 1;
                    column = 0;
                    cursor_x = rec.x;
                    width_override = no_value;
                    rec.color = 0;
                    rec.mark = -1;
                    cursor_y = rec.y;
                    rec.line = 0;
                    rec.portrait = thread->function_parameter_4 - 1;
                    win->portrait_code = rec.portrait;
                    g_battle_text_awaiting_input = 0;
                    value_index = 0;
                    saved = (u8*)-1;
                    repeat = 0;
                    win[1].portrait_code = rec.portrait;
                    battle_thread_yield();
                    continue;
                }
                if (character == TEXT_PAGE_BREAK) {
                    continue;
                }
                if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
                    battle_menu_build_and_upload_window_image(
                        rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.offset);
                } else if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
                    ClearImage(&rec.rect, 0, 0, 0);
                    battle_thread_yield();
                    for (k = 0; k < 2; k++) {
                        win[k].r = 0x80;
                        win[k].g = 0x80;
                        win[k].b = 0x80;
                    }
                } else {
                    g_sound_effect_id_to_play = MAIN_SFX_TEXT_PAGE;
                    rec.first_line = rec.line;
                    rec.last_line = rec.line + rec.lines;
                    battle_menu_render_text_image_at_record_origin((battle_menu_text_image_t*)&rec);
                }
                cursor_y = rec.y - 0x10;
                battle_thread_yield();
            }
            if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
                event_id = (s16)battle_script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT);
                if ((event_id == 0xF9 || event_id == 0x12A) && g_battle_etc_graphics_enabled == 0) {
                    cursor_x = rec.x;
                } else {
                    s32 line_width;
                    g_battle_text_current_line++;
                    line_width = g_battle_text_line_widths[g_battle_text_current_line] + 0x18;
                    cursor_x = rec.x + ((u32)(rec.width - line_width) >> 1);
                }
                if (cursor_x & 3) {
                    cursor_x += 4 - (cursor_x & 3);
                } else if (cursor_x & 1) {
                    cursor_x -= cursor_x & 3;
                }
                battle_text_print_fnt_value_and_next(rec.line + rec.lines);
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
                selection = rec.mark;
                for (i = 0;; i += g_battle_event_speed) {
                    g_battle_text_awaiting_input = 1;
                    select_window = WINDOW_AT(g_battle_menu_packet_buffer_index);
                    select_window->sprites[0].x0
                        = mark_x + (select_window->quads[0].x0 + rec.x - select_window->quads[0].u0) - ((i >> 4) & 3);
                    select_window->sprites[0].y0
                        = (select_window->quads[0].y0 + rec.y - select_window->quads[0].v0) + selection * 16;
                    select_window->sprites[1].x0 = select_window->sprites[0].x0 + 2;
                    select_window->sprites[1].y0 = select_window->sprites[0].y0 + 2;
                    battle_thread_yield();
                    if (battle_thread_get_current_task_id() == NATIVE_THREAD_TASK_STOP_REQUEST) {
                        goto end;
                    }
                    if (battle_thread_get_current_task_id() == NATIVE_THREAD_TASK_WAIT_FOR_RESUME) {
                        continue;
                    }
                    if ((g_battle_script_event_input & PSX_PAD_UP) && selection != rec.mark) {
                        g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                        selection += no_value;
                    }
                    if ((g_battle_script_event_input & PSX_PAD_DOWN) && selection != column) {
                        g_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                        selection++;
                    }
                    if (g_battle_script_event_input & PSX_PAD_CIRCLE) {
                        g_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                        break;
                    }
                }
                select_hidden_window = WINDOW_AT(g_battle_menu_packet_buffer_index);
                select_hidden_window->sprites[0].x0 = 0x200;
                select_hidden_window->sprites[1].x0 = 0x200;
                battle_thread_yield();
                select_hidden_window_next = WINDOW_AT(g_battle_menu_packet_buffer_index);
                select_hidden_window_next->sprites[0].x0 = 0x200;
                select_hidden_window_next->sprites[1].x0 = 0x200;
                g_battle_script_variables[EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION] = selection - rec.mark;
            }
            NEXT();
            g_battle_text_awaiting_input = 0;
        } else if (character == TEXT_WAIT_FOR_RESUME) {
            battle_thread_set_current_task_id(NATIVE_THREAD_TASK_WAIT_FOR_RESUME);
            resume_task = NATIVE_THREAD_TASK_RESUME;
            while (battle_thread_get_current_task_id() != resume_task) {
                if (battle_thread_get_current_task_id() == NATIVE_THREAD_TASK_STOP_REQUEST) {
                    goto end;
                }
                battle_thread_yield();
            }
            if (rec.no_box != 0) {
                win->message_id = win[1].message_id = rec.text = battle_thread_get_current_parameter_2();
                if ((rec.text & no_value) == no_value) {
                    break;
                }
                text = battle_text_init_entry(rec.text);
                no_wait = 0;
                battle_menu_build_and_upload_window_image(
                    rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.offset);
                color = 1;
                column = 0;
                width_override = no_value;
                cursor_x = rec.x;
                cursor_y = rec.y;
                value_index = 0;
                rec.color = 0;
                rec.mark = -1;
                rec.line = 0;
                saved = (u8*)-1;
                repeat = 0;
                g_battle_text_awaiting_input = 0;
                battle_thread_yield();
                continue;
            }
            battle_thread_yield();
            NEXT();
            no_wait = 1;
        } else if (character == TEXT_RELEASE_WAITING_THREADS) {
            for (i = 0; i < 0x10; i++) {
                if (g_battle_current_thread_id != i
                    && g_battle_thread_task_ids[i][0] == NATIVE_THREAD_TASK_WAIT_FOR_RESUME
                    && battle_thread_is_running_8014cc94(i)) {
                    g_battle_thread_task_ids[i][0] = NATIVE_THREAD_TASK_RESUME;
                }
            }
            battle_thread_yield();
            NEXT();
        } else if (character == TEXT_SET_PORTRAIT) {
            NEXT();
            win[0].portrait_code = *text;
            win[1].portrait_code = *text;
            battle_script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, *text);
            NEXT();
        } else if (character == TEXT_SET_VARIABLE_BASE) {
            NEXT();
            variable_base = *text;
            NEXT();
        } else if (character == TEXT_STORE_VARIABLE) {
            NEXT();
            i = battle_script_get_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
            if (i != 0) {
                battle_script_set_variable(variable_base + i * 10, *text);
                battle_script_set_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
            } else {
                battle_script_set_variable(variable_base, *text);
            }
            NEXT();
        } else if (character == TEXT_SET_GLYPH_COLOR) {
            NEXT();
            color = *text;
            NEXT();
        } else if (character == TEXT_SET_PALETTE) {
            NEXT();
            rec.color = *text;
            NEXT();
        } else {
            substitution = &g_battle_text_substitution_values[value_index];
            value = battle_text_resolve_format_string_id(character, *substitution);
            if (value != -1) {
                saved = text + 1;
                value_index++;
                if (character == TEXT_FORMAT_RAMZA_NAME) {
                    text = g_battle_player_name;
                } else {
                    text = battle_text_init_entry(value);
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
                value = g_battle_text_substitution_values[*text];
            print_value:
                digits = battle_text_count_decimal_digits(value);
                started = 0;
                if (width_override != no_value) {
                    digits = width_override;
                    width_override = no_value;
                }
                for (i = digits - 1; i >= 0; i--) {
                    character = value / g_battle_text_decimal_divisors[i];
                    if (value < g_battle_text_decimal_divisors[i] && i != 0 && started == 0) {
                        character = TEXT_SPACE;
                    } else {
                        started = 1;
                        value -= character * g_battle_text_decimal_divisors[i];
                    }
                    g_battle_text_typewriter_glyph.flags_0 = cursor_x;
                    g_battle_text_typewriter_glyph_y = cursor_y;
                    g_battle_text_typewriter_glyph_character = character;
                    g_battle_text_typewriter_glyph_dialogue_type = rec.dialog_type & 0x70;
                    g_battle_text_typewriter_glyph_palette = rec.color;
                    battle_text_draw_dialogue_glyph((s16)cursor_x, (s16)cursor_y, color);
                    if (character == TEXT_SPACE) {
                        cursor_x += 4;
                    } else {
                        cursor_x += g_text_glyph_widths[character];
                    }
                }
                NEXT();
            } else if ((character & 0xF0) == 0xF0 && (character & 0xF) < 4) {
                saved = text + 3;
                value = (text[0] & 3) * 8;
                repeat = value + (text[1] >> 5) + 4;
                value = text[1] & 0xF;
                value *= 0xFE;
                value = value + text[2];
                text -= value;
            } else {
                if (character < 0xD0) {
                    character = *text;
                } else if ((character & 0xF0) == 0xD0) {
                    character = (*text & 0xF) * 0xD0;
                    NEXT();
                    if ((*text & 0xF0) == 0xF0 && (*text & 0xF) < 4) {
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
                g_battle_text_typewriter_glyph.flags_0 = cursor_x;
                g_battle_text_typewriter_glyph_y = cursor_y;
                g_battle_text_typewriter_glyph_character = character;
                g_battle_text_typewriter_glyph_dialogue_type = rec.box_type;
                g_battle_text_typewriter_glyph_palette = rec.color;
                battle_text_draw_dialogue_glyph((s16)cursor_x, (s16)cursor_y, color);
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
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        from.w = 0;
        from.h = 0;
        from.x = right;
        from.y = bottom;
        battle_copy_bytes(&to, &from, 8);
        battle_menu_tween_window_quads(win, &from, &to, 4);
    }
    g_battle_text_awaiting_input = 0;
    win->active = 0;
    battle_menu_free_buffer((s32)win);
    battle_gfx_free_tpage7_vram(&rec.rect);
    battle_menu_release_current_thread_slot();
    battle_thread_yield();
    battle_thread_exit_current();
}
