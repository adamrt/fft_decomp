/* The dialog text thread, opening a dialog window and drawing a text entry glyph by glyph. */

#include "fft/game/character_identity.h"
#include "fft/game/job.h"
#include "fft/game/party.h"
#include "fft/game/party_unit.h"
#include "fft/gfx/gfx_image.h"
#include "fft/map/map_unit.h"
#include "fft/menu/menu_gfx.h"
#include "fft/menu/menu_input.h"
#include "fft/menu/menu_screen.h"
#include "fft/menu/menu_window.h"
#include "fft/party/formation.h"
#include "fft/party/unit_panel.h"
#include "fft/script/script.h"
#include "fft/script/script_variable.h"
#include "fft/sys/memory.h"
#include "fft/sys/sound.h"
#include "fft/sys/thread.h"
#include "fft/text/message.h"
#include "fft/text/text.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* dialog drawing data */
extern u16 g_menu_screen_help_portrait_clut_by_sheet_id[];
extern SVECTOR g_menu_screen_text_partner_coords;

/* The current buffer of the window pair; the bytes add the offset to `win`, which &win[index] reverses. */
#define WINDOW_AT(index) ((menu_window_buffer_t*)((index) * sizeof(menu_window_buffer_t) + (s32)win))

#define NEXT() text = text_advance_cursor_with_separate_backreference_state(&repeat, text, &saved)

/* Aborts (here and below) leave the nested text loops for the shared
 * window close-and-release tail at `end`. */
#define CHECK_ABORT()                                                                                                  \
    if (thread_current_get_task() == THREAD_TASK_STOP_REQUEST                                                          \
        && g_thread_current_id != MENU_SCREEN_THREAD_SLOT_TUTORIAL_TEXT) {                                             \
        goto end;                                                                                                      \
    }

#define YIELD_OR_INPUT()                                                                                               \
    if (g_formation_screen_active && rec.box_type == DIALOG_BOX_TYPE_HELP) {                                           \
        menu_screen_update_dialog_input();                                                                             \
    } else {                                                                                                           \
        thread_yield();                                                                                                \
    }

/* Dialog text thread: opens a dialog window and draws the text entry named
 * by thread parameter 2 glyph by glyph, handling the control bytes.
 *
 * Parameter 1 is the dialog type (box type, position, options; types with
 * bit 0x10 carry a portrait), parameter 3 the speaker's unit and parameter 4
 * the portrait or roster slot; the thread's task words supply offsets and a
 * forced text width. The box is sized from the measured text, placed (a
 * tailed portrait box above or below its speaker), opened with a tween,
 * filled glyph by glyph with waits for confirmation at full pages, page
 * breaks and the end, and finally closed and released. */
void message_dialog_thread(void) {
    text_dialog_record_t rec;
    RECT from;
    RECT to;
    s32 repeat;
    const u8* saved;
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
    s32 party_name_request = 0;
    menu_window_buffer_t* win;
    menu_window_buffer_t* cursor_window;
    menu_window_buffer_t* arrow_window;
    menu_window_buffer_t* scroll_window;
    menu_window_buffer_t* hidden_window;
    menu_window_buffer_t* select_window;
    menu_window_buffer_t* select_hidden_window;
    party_unit_t* party;
    const job_data_t* job;
    const u8* text;
    s16 cursor_x;
    s32 column;
    s32 character;
    s32 i;
    s32 tick;
    s32 value;
    s32 digits;
    u16 started;
    u16 selection;
    s32* substitution;
    s32 portrait_id;
    s32 window_u;
    s32 half_w;
    s32 half_h;
    s32 center;
    u16 param;
    s32 no_value;
    const text_backref_t* backref;

    thread_current_set_task(THREAD_TASK_RESUME);
    script_variable_set(EVENT_SCRIPT_VAR_TYPEWRITER_VRAM_X_OFFSET, 0);
    value_index = 0;
    variable_base = EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION;
    thread = &g_threads[g_thread_current_id];

    /* Box type 0x70 becomes a portrait box placed by the speaker's facing relative to the camera. */
    param = thread_current_get_parameter_1();
    rec.dialog_type = param;
    if ((param & 0x70) == 0x70) {
        rec.dialog_type = (param & 0x1C) | 0x10;
        rec.dialog_type |= map_unit_check_camera_facing_by_event_unit_id(thread_current_get_parameter_3()) + 1;
    }

    rec._unread_2a = 0;
    rec.origin_x = 8;
    rec.origin_y = 8;
    rec.box_type = rec.dialog_type & DIALOG_TYPE_BOX_TYPE_MASK;
    rec.position = rec.dialog_type & DIALOG_TYPE_POSITION_MASK;
    rec.options = rec.dialog_type & DIALOG_TYPE_OPTIONS_MASK;
    rec.keep_open = rec.dialog_type & DIALOG_TYPE_KEEP_OPEN;
    rec.arrow_offset = thread->task_words[DIALOG_THREAD_WORD_ARROW_OFFSET];
    rec.x_offset = thread->task_words[DIALOG_THREAD_WORD_X_OFFSET];
    rec.y_offset = thread->task_words[DIALOG_THREAD_WORD_Y_OFFSET];

    /* Help text is read through WORLD's text sections; the saved pointers come back at `end`. */
    if (rec.box_type == DIALOG_BOX_TYPE_HELP && rec.options == 0xC) {
        text_save_section_pointers();
        text_relocate_pointer_table(&g_menu_screen_text_bank);
    }
    if (rec.box_type == DIALOG_BOX_TYPE_HELP && !g_formation_screen_active) {
        text_save_section_pointers();
        text_relocate_pointer_table(&g_menu_screen_text_bank);
        /* Message 0x807 shows the selected party unit's name in a portrait box instead. */
        if (thread->function_parameter_2 == 0x807) {
            i = formation_get_selected_unit_name_id();
            if (i >= 0x2FF) {
                i = 0x2FF;
            }
            party_name_request = 1;
            thread->function_parameter_2 = i | 0xA000;
            rec.box_type = DIALOG_BOX_TYPE_PORTRAIT;
            rec.options = DIALOG_OPTIONS_NO_TAIL;
            rec.portrait = g_unit_panel_selected_unit.battle_unit_id;
        }
    }
    if (rec.box_type == DIALOG_BOX_TYPE_HELP && g_formation_screen_active == 1) {
        g_menu_input_lock_mode = g_formation_screen_active;
    }

    /* Types with bit 0x10 map the unit's sprite set, or a generic unit's job, to a portrait and palette. */
    if (rec.dialog_type & 0x10) {
        if (!party_name_request) {
            rec.portrait = thread->function_parameter_4;
        }
        party = party_unit_get(rec.portrait);
        if (party->sprite_set >= CHARACTER_IDENTITY_SELECTOR_FIRST) {
            if (party->sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
                s32 sheet_offset = (party->job_id - JOB_ID_GENERIC_FIRST) * 2;
                portrait_id = sheet_offset + JOB_SHEET_GENERIC_MALE_BASE;
                if (party->job_id == JOB_ID_MIME) {
                    portrait_id = sheet_offset + JOB_SHEET_GENERIC_MALE_MIME_BASE;
                }
            } else if (party->sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
                s32 sheet_offset = (party->job_id - JOB_ID_GENERIC_FIRST) * 2;
                portrait_id = sheet_offset + JOB_SHEET_GENERIC_FEMALE_BASE;
                if (party->job_id == JOB_ID_MIME) {
                    portrait_id = sheet_offset + JOB_SHEET_GENERIC_FEMALE_MIME_BASE;
                }
            } else {
                job = job_get_data(party->job_id);
                portrait_id = job->spritesheet_id;
            }
            if (party->job_id == JOB_ID_BARD) {
                portrait_id = JOB_SHEET_BARD;
            }
            if (party->job_id == JOB_ID_DANCER) {
                portrait_id = JOB_SHEET_DANCER;
            }
        } else {
            portrait_id = party->sprite_set;
        }
        portrait_id = g_menu_screen_help_portrait_clut_by_sheet_id[portrait_id];
        if (party->sprite_set == CHARACTER_IDENTITY_ENTD_NONE) {
            portrait_id = 0;
        }
        if (party->sprite_set == CHARACTER_IDENTITY_MONSTER) {
            portrait_id |= job->job_portrait_palette << 10;
        }
        rec.portrait = portrait_id;
    } else {
        rec.portrait = thread->function_parameter_4 - 1;
    }

    /* Size the box from the measured text; several box types cap their line count. */
    text = text_find_entry(thread_current_get_parameter_2());
    text_measure_pixels(&rec.mark, &rec.lines, text);
    if (thread->task_words[DIALOG_THREAD_WORD_TEXT_WIDTH] != 0) {
        rec.mark = thread->task_words[DIALOG_THREAD_WORD_TEXT_WIDTH];
    }
    if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
        rec.x_offset += 8;
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
    rec.width = rec.mark + 24;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        if (rec.portrait < 10000) {
            rec.width = rec.mark + 64;
        }
        if (rec.options == DIALOG_OPTIONS_TAIL) {
            rec.height = rec.lines * 16 + 24;
        }
        if (rec.options == DIALOG_OPTIONS_ALTERNATE_TAIL) {
            rec.height += 16;
        }
    }

    /* A tailed portrait box sits above or below its speaker; `right` and `bottom` become the
     * point the window opens from and closes to. */
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.options != DIALOG_OPTIONS_NO_TAIL) {
        center = rec.width >> 1;
        value = script_resolve_map_unit_id(thread_current_get_parameter_3());
        map_unit_project_screen_position(value, &g_unit_scratch_coords);
        cursor_x = center;
        rec.tail_offset = 0;
        rec.x = g_unit_scratch_coords.vx - center - 4;

        /* Automatic position: below the speaker when it is off the top of the screen or its
         * swap partner stands higher on screen, otherwise above. */
        if (rec.position == DIALOG_POSITION_AUTO) {
            if (thread_current_get_parameter_3() == g_message_speaker_swap_unit_a) {
                value = g_message_speaker_swap_unit_b;
            } else if (thread_current_get_parameter_3() == g_message_speaker_swap_unit_b) {
                value = g_message_speaker_swap_unit_a;
            }
            map_unit_project_screen_position(script_resolve_map_unit_id(value), &g_menu_screen_text_partner_coords);
            if (g_unit_scratch_coords.vy < 0 || g_menu_screen_text_partner_coords.vy < g_unit_scratch_coords.vy) {
                rec.position = DIALOG_POSITION_BOTTOM;
            } else {
                rec.position = DIALOG_POSITION_TOP;
            }
            rec.dialog_type = (rec.dialog_type & 0xFFF0) | rec.position;
        }
        if (rec.position == DIALOG_POSITION_TOP) {
            rec.x += 12;
            rec.y = g_unit_scratch_coords.vy - rec.height - 40;
        }
        if (rec.position == DIALOG_POSITION_BOTTOM) {
            rec.x -= 8;
            rec.y = g_unit_scratch_coords.vy;
        }

        /* Keep the box within x 136..384; the tail moves by whatever the clamp removed. */
        if (rec.x < 136) {
            rec.tail_offset = rec.x - 136;
            rec.x = 136;
        } else if (rec.x + rec.width > 384) {
            rec.tail_offset = rec.x + rec.width - 384;
            rec.x = 384 - rec.width;
        }

        /* The script's offsets apply after that clamp, so clamp again. */
        rec.x += rec.x_offset;
        rec.y += rec.y_offset;
        if (rec.x < 136) {
            rec.tail_offset += rec.x - 136;
            rec.x = 136;
        } else if (rec.x + rec.width > 384) {
            rec.tail_offset += rec.x + rec.width - 384;
            rec.x = 384 - rec.width;
        }
        rec.tail_offset += rec.arrow_offset;
        if (rec.y < 22) {
            rec.y = 22;
        } else if (rec.y + rec.height > 228) {
            rec.y = 228 - rec.height;
        }

        /* Keep the tail at least 24 pixels from either end of the box; cursor_x holds half its width. */
        if (rec.tail_offset > cursor_x - 24) {
            rec.tail_offset = cursor_x - 24;
        }
        if (rec.tail_offset < -cursor_x + 24) {
            rec.tail_offset = -cursor_x + 24;
        }

        if (rec.position == DIALOG_POSITION_TOP) {
            bottom = rec.y + rec.height - 8;
        }
        if (rec.position == DIALOG_POSITION_BOTTOM) {
            bottom = rec.y + 8;
        }
        right = rec.x + rec.tail_offset + (rec.width >> 1);
    } else {
        /* Other boxes center horizontally; positions 1, 2 and 3 are the top, bottom and middle. */
        rec.tail_offset = 0;
        half_w = rec.width >> 1;
        rec.x = (256 - half_w) & 0xFFFE;
        half_h = rec.height >> 1;
        if (rec.position == DIALOG_POSITION_TOP) {
            rec.y = 22;
        }
        if (rec.position == DIALOG_POSITION_BOTTOM) {
            rec.y = 228 - rec.height;
        }
        if (rec.position == DIALOG_POSITION_CENTER) {
            rec.y = (120 - half_h) & 0xFFFE;
        }
        rec.x += rec.x_offset;
        rec.y += rec.y_offset;
        right = half_w + rec.x - 6;
        bottom = half_h + rec.y - 2;
    }

    /* Allocate the window pair and render the box image into its VRAM rectangle. */
    win = menu_alloc_window_buffer_pair();
    win->message_id = thread_current_get_parameter_2();
    win->dialog_selector = rec.position;
    win->box_type = rec.box_type;
    rec.text = thread_current_get_parameter_2();
    rec.rect.w = rec.width;
    rec.rect.h = rec.height;
    gfx_alloc_texture_rect(&rec.rect, win->quads, (u32*)-1);
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        if (text[0] == TEXT_SET_PORTRAIT) {
            win->portrait_code = text[1];
        } else {
            win->portrait_code = rec.portrait;
        }
        value = thread_current_get_parameter_3();
        menu_build_and_upload_window_image(rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
        win->portrait_battle_unit_id = value;
    } else if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
        win[0].quads[0].clut = 0x7CBC;
        win[1].quads[0].clut = 0x7CBC;
        ClearImage(&rec.rect, 0, 0, 0);
    } else {
        rec.first_line = 0;
        rec.last_line = rec.lines;
        message_render_record_image(&rec);
    }

    /* The formation screen shifts its dialogs by (-256, -120); then clamp the open point to the box area. */
    if (g_formation_screen_active) {
        right -= 256;
        bottom -= 120;
        rec.x -= 256;
        rec.y -= 120;
    }
    if (right < 136) {
        right = 136;
    }
    if (right > 384) {
        right = 384;
    }
    if (bottom < 22) {
        bottom = 22;
    }
    if (bottom > 228) {
        bottom = 228;
    }

    /* The box quad starts collapsed at the open point and grows to `from`. */
    setXY4(&win->quads[0], right, bottom, right, bottom, right, bottom, right, bottom);
    from.x = rec.x;
    from.y = rec.y;
    from.w = rec.width;
    from.h = rec.height;
    rec.arrow_x = rec.x + rec.width - 30;
    rec.arrow_y = rec.y + rec.height - 24;
    to.x = 0;

    /* A portrait box also tweens its second quad, the portrait, to `to`: at the left when
     * tail_offset is negative (then drawn flipped), otherwise at the right. */
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        setXY4(&win->quads[1], right, bottom, right, bottom, right, bottom, right, bottom);
        if (rec.tail_offset < 0) {
            to.x = rec.x + 8;
            if (rec.portrait < 8) {
                rec.origin_x += 40;
            }
        } else {
            to.x = rec.x + rec.width - 48;
            rec.arrow_x -= 32;
        }
        to.y = rec.y + 7;
        if (rec.options != DIALOG_OPTIONS_NO_TAIL) {
            if (rec.position == DIALOG_POSITION_BOTTOM) {
                to.y = rec.y + 15;
                rec.origin_y += 8;
            } else {
                rec.arrow_y -= 8;
            }
        }
        if (rec.options == DIALOG_OPTIONS_ALTERNATE_TAIL) {
            rec.arrow_y -= 8;
        }
        to.w = 31;
        to.h = 48;
        if (rec.tail_offset < 0) {
            win[1].portrait_flipped = 1;
            win[0].portrait_flipped = 1;
        }
        if (rec.portrait >= 10000) {
            to.x = 0;
        }
    }

    /* Help and Check boxes carry their title sprite at the top-left corner. */
    if (rec.box_type == DIALOG_BOX_TYPE_HELP) {
        win->sprites[3].u0 = 80;
        win->sprites[3].v0 = 32;
        win->sprites[3].x0 = from.x + 3;
        win->sprites[3].y0 = from.y - 2;
        win->sprites[3].w = 20;
        win->sprites[3].h = 9;
        win->sprites[3].clut = 0x7CBC;
    }
    if (rec.box_type == DIALOG_BOX_TYPE_CHECK) {
        win->sprites[3].u0 = 64;
        win->sprites[3].v0 = 120;
        win->sprites[3].x0 = from.x + 3;
        win->sprites[3].y0 = from.y - 2;
        win->sprites[3].w = 28;
        win->sprites[3].h = 8;
        win->sprites[3].clut = 0x7CBC;
    }

    /* The pair's two buffers alternate by frame, so the second starts as a copy of the first. */
    mem_copy_bytes_nonempty(&win[1], win, sizeof(win[1]));
    win[0].active = 1;
    win[1].active = 1;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        menu_window_tween_quads(win, &from, &to, thread->task_words[DIALOG_THREAD_WORD_WINDOW_OPTIONS] & 0xF);
    } else {
        menu_window_animate_crop(win, &from, &to);
    }

    /* From here rec.x and rec.y address the window's image in VRAM, not the screen. */
    window_u = win->quads[0].u0;
    rec.x = window_u + 8;
    rec.y = win->quads[0].v0 + 8;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.tail_offset < 0 && rec.portrait < 8) {
        rec.x = window_u + 48;
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
    /* Both arms are identical, as in the BATTLE twin whose first arm centers
     * the line; the split keeps the preceding stores ahead of these loads. */
    if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
        cursor_x = rec.x;
    } else {
        cursor_x = rec.x;
    }
    width_override = 0xFFFF;
    cursor_y = rec.y;
    rec.mark = -1;
    saved = (const u8*)-1;
    repeat = 0;

    /* Draw one text byte per pass; `saved` is the return point after a substituted or replayed run. */
    while (1) {
        /* The bytes keep 0xFFFF (no text id, no width override) in a variable reset each pass. */
        no_value = 0xFFFF;
        character = *text;
        CHECK_ABORT();

        if (character == TEXT_NEWLINE || character == TEXT_END_WAIT_FOR_CONFIRM || character == TEXT_END_AUTO_CLOSE
            || character == TEXT_PAGE_BREAK) {
            if (character == TEXT_END_AUTO_CLOSE) {
                break;
            }
            if (character == TEXT_END_WAIT_FOR_CONFIRM) {
                /* The end of a substituted string returns to the text that named it. */
                if (saved != (const u8*)-1) {
                    text = saved;
                    saved = (const u8*)-1;
                    continue;
                }
                while (1) {
                    if (column >= rec.lines) {
                        break;
                    }
                    column++;
                    rec.line++;
                }
                rec.line--;
            } else {
                NEXT();
            }
            if (character != TEXT_PAGE_BREAK) {
                column++;
                rec.line++;
            }
            rec.first_line = rec.line - rec.lines;

            /* Wait for confirmation when the box is full, at the end of the text and at a page break. */
            if (column >= rec.lines || character == TEXT_END_WAIT_FOR_CONFIRM || character == TEXT_PAGE_BREAK) {
                if (character != TEXT_PAGE_BREAK) {
                    column = 0;
                }
                if (!no_wait) {
                    for (tick = 0;; tick += g_script_event_speed) {
                        YIELD_OR_INPUT();
                        if (g_script_event_input & (PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT)) {
                            break;
                        }
                        CHECK_ABORT();
                        /* A page break bobs the cursor sprite after the text; other waits show the arrow. */
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
                                g_menu_screen_dialog_waiting = 1;
                            }
                            if (character == TEXT_NEWLINE
                                || (character == TEXT_END_WAIT_FOR_CONFIRM
                                    && rec.box_type == DIALOG_BOX_TYPE_CENTERED)) {
                                WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].u0 = (tick & 0x30) - 0x58;
                                WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].v0 = 16;
                                arrow_window = WINDOW_AT(g_menu_packet_buffer_index);
                                arrow_window->sprites[2].x0 = rec.arrow_x;
                                arrow_window->sprites[2].y0 = rec.arrow_y;
                            }

                            /* Holding Square scrolls back through the lines already shown. */
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
                                        WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].x0 = 512;
                                        rec.first_line--;
                                        rec.last_line = rec.first_line + rec.lines;
                                        message_render_record_image(&rec);
                                    }
                                    if ((g_script_event_input & PSX_PAD_DOWN)
                                        && rec.first_line != rec.line - rec.lines) {
                                        g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                                        WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].x0 = 512;
                                        rec.first_line++;
                                        rec.last_line = rec.first_line + rec.lines;
                                        message_render_record_image(&rec);
                                    }
                                    if (!(g_script_event_current_input & PSX_PAD_SQUARE)) {
                                        win[0].icon_flags = 0;
                                        win[1].icon_flags = 0;
                                        break;
                                    }
                                    CHECK_ABORT();
                                    WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].x0 = 512;
                                    YIELD_OR_INPUT();
                                }
                            }

                            /* Scroll back down to the newest lines. The else-break keeps the test
                             * on top, as in the target; a `while` is rotated to test at the bottom. */
                            for (;;) {
                                if (rec.line - rec.lines != rec.first_line) {
                                    WINDOW_AT(g_menu_packet_buffer_index)->sprites[2].x0 = 512;
                                    rec.first_line++;
                                    rec.last_line = rec.first_line + rec.lines;
                                    message_render_record_image(&rec);
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }

                /* Hide the cursor, arrow and scroll icons in both buffers, one frame apart. */
                g_menu_screen_dialog_waiting = 0;
                hidden_window = WINDOW_AT(g_menu_packet_buffer_index);
                hidden_window->sprites[0].x0 = 512;
                hidden_window->sprites[1].x0 = 512;
                hidden_window->sprites[2].x0 = 512;
                hidden_window->icon_flags = 0;
                YIELD_OR_INPUT();
                hidden_window = WINDOW_AT(g_menu_packet_buffer_index);
                hidden_window->sprites[0].x0 = 512;
                hidden_window->sprites[1].x0 = 512;
                hidden_window->sprites[2].x0 = 512;
                hidden_window->icon_flags = 0;

                /* With type bit 0x80 the window stays open until ChangeDialog resumes it with new text;
                 * text id 0xFFFF closes it. */
                if (character == TEXT_END_WAIT_FOR_CONFIRM) {
                    if (!rec.keep_open) {
                        break;
                    }
                    thread_current_set_task(THREAD_TASK_DIALOG_AWAIT_TEXT);
                    while (thread_current_get_task() != THREAD_TASK_RESUME) {
                        CHECK_ABORT();
                        thread_yield();
                    }
                    win->message_id = win[1].message_id = rec.text = thread_current_get_parameter_2();
                    if ((rec.text & no_value) == no_value) {
                        break;
                    }

                    column = 0;
                    text = text_find_entry(rec.text);
                    no_wait = 0;
                    menu_build_and_upload_window_image(
                        rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
                    delay = 1;
                    cursor_x = rec.x;
                    width_override = no_value;
                    rec.mark = -1;
                    cursor_y = rec.y;
                    rec.color = 0;
                    rec.line = 0;
                    rec.portrait = thread->function_parameter_4 - 1;
                    g_menu_screen_dialog_waiting = 0;
                    win->portrait_code = rec.portrait;
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

                /* The box is full: start a fresh page; cursor_y lands on its first line after the += below. */
                if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
                    menu_build_and_upload_window_image(
                        rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
                } else if (rec.box_type != DIALOG_BOX_TYPE_CENTERED) {
                    g_sound_pending_sfx_id = SOUND_SFX_TEXT_PAGE;
                    rec.first_line = rec.line;
                    rec.last_line = rec.line + rec.lines;
                    message_render_record_image(&rec);
                }
                cursor_y = rec.y - 16;
                thread_yield();
            }

            cursor_x = rec.x;
            no_wait = 0;
            cursor_y += 16;
        } else if (character == TEXT_CHOICE_MARK) {
            /* Choices start on this line; remember it and the cursor's x for the selection sprite. */
            rec.mark = column;
            mark_x = cursor_x - rec.x - 16;
            NEXT();
        } else if (character == TEXT_CHOICE_SELECT) {
            /* Pick one of the lines from the mark to the current line; store it relative to the mark. */
            if (rec.mark != -1) {
                selection = rec.mark;
                for (i = 0;; i += g_script_event_speed) {
                    g_menu_screen_dialog_waiting = 1;
                    select_window = WINDOW_AT(g_menu_packet_buffer_index);
                    select_window->sprites[0].x0
                        = mark_x + (select_window->quads[0].x0 + rec.x - select_window->quads[0].u0) - ((i >> 4) & 3);
                    select_window->sprites[0].y0
                        = (select_window->quads[0].y0 + rec.y - select_window->quads[0].v0) + selection * 16;
                    select_window->sprites[1].x0 = select_window->sprites[0].x0 + 2;
                    select_window->sprites[1].y0 = select_window->sprites[0].y0 + 2;
                    thread_yield();
                    if (thread_current_get_task() == THREAD_TASK_STOP_REQUEST) {
                        goto end;
                    }
                    if (thread_current_get_task() == THREAD_TASK_WAIT_FOR_RESUME) {
                        continue;
                    }
                    if ((g_script_event_input & PSX_PAD_UP) && selection != rec.mark) {
                        g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                        selection--;
                    }
                    if ((g_script_event_input & PSX_PAD_DOWN) && selection != column) {
                        g_sound_pending_sfx_id = SOUND_SFX_CURSOR_MOVE;
                        selection++;
                    }
                    if (g_script_event_input & PSX_PAD_CIRCLE) {
                        if (rec.box_type != DIALOG_BOX_TYPE_CENTERED) {
                            g_sound_pending_sfx_id = SOUND_SFX_CONFIRM;
                        }
                        break;
                    }
                }

                select_hidden_window = WINDOW_AT(g_menu_packet_buffer_index);
                select_hidden_window->sprites[0].x0 = 512;
                select_hidden_window->sprites[1].x0 = 512;
                thread_yield();
                select_hidden_window = WINDOW_AT(g_menu_packet_buffer_index);
                select_hidden_window->sprites[0].x0 = 512;
                select_hidden_window->sprites[1].x0 = 512;
                g_script_variables[EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION] = selection - rec.mark;
            }
            NEXT();
            g_menu_screen_dialog_waiting = 0;
        } else if (character == TEXT_WAIT_FOR_RESUME) {
            thread_current_set_task(THREAD_TASK_WAIT_FOR_RESUME);
            while (thread_current_get_task() != THREAD_TASK_RESUME) {
                if (thread_current_get_task() == THREAD_TASK_STOP_REQUEST) {
                    goto end;
                }
                thread_yield();
            }

            /* A window kept open (type bit 0x80) restarts on the text it was resumed with. */
            if (rec.keep_open) {
                win->message_id = win[1].message_id = rec.text = thread_current_get_parameter_2();
                if ((rec.text & no_value) == no_value) {
                    break;
                }
                column = 0;
                text = text_find_entry(rec.text);
                no_wait = 0;
                menu_build_and_upload_window_image(rec.width, rec.height, &rec.rect, rec.dialog_type, rec.tail_offset);
                delay = 1;
                width_override = no_value;
                cursor_x = rec.x;
                cursor_y = rec.y;
                value_index = 0;
                rec.color = 0;
                rec.mark = -1;
                rec.line = 0;
                repeat = 0;
                g_menu_screen_dialog_waiting = 0;
                saved = (const u8*)-1;
                thread_yield();
                continue;
            }
            thread_yield();
            NEXT();
            /* The next line break goes on without waiting for confirmation. */
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
            /* A format code draws a whole string, then returns to `saved`. */
            substitution = &g_text_substitution_values[value_index];
            value = text_resolve_format_id(character, *substitution);
            if (value != -1) {
                saved = text + 1;
                value_index++;
                if (character == TEXT_FORMAT_RAMZA_NAME) {
                    text = g_text_player_name_buffer;
                } else {
                    text = text_find_entry(value);
                }
                continue;
            }

            if (character == TEXT_SET_NUMBER_WIDTH) {
                NEXT();
                width_override = *text;
                NEXT();
            } else if (character == TEXT_PRINT_NEXT_VALUE || character == TEXT_PRINT_INDEXED_VALUE) {
                if (character == TEXT_PRINT_NEXT_VALUE) {
                    value = *substitution;
                    value_index++;
                } else {
                    NEXT();
                    value = g_text_substitution_values[*text];
                }

                /* Print the number in `digits` places, padding leading zeros with spaces. */
                digits = text_count_decimal_digits(value);
                started = 0;
                if (width_override != no_value) {
                    digits = width_override;
                    width_override = no_value;
                }
                for (i = digits - 1; i >= 0; i--) {
                    character = value / g_text_decimal_divisors[i];
                    if (value < g_text_decimal_divisors[i] && i != 0 && !started) {
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
                    message_draw_glyph_timed(cursor_x, cursor_y, delay);
                    if (character == TEXT_SPACE) {
                        cursor_x += 4;
                    } else {
                        cursor_x += g_text_glyph_widths[character];
                    }
                }
                NEXT();
            } else if ((character & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (character & 0xF) < 4) {
                /* Compressed text: replay `repeat` bytes from earlier in the entry, then resume at `saved`.
                 * The bytes require building the offset in these steps. */
                backref = (const text_backref_t*)text;
                saved = text + 3;
                value = backref->length_high * 8;
                repeat = value + backref->length_low + 4;
                value = backref->offset_high;
                value *= 0xFE;
                value = value + backref->offset_low;
                text -= value;
            } else {
                if (character < TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                    character = *text;
                } else if ((character & 0xF0) == TEXT_EXTENDED_GLYPH_PREFIX_FIRST) {
                    /* A two-byte glyph: the prefix's low nibble selects a page; the second byte may
                     * itself start a backreference. */
                    character = (*text & 0xF) * TEXT_GLYPHS_PER_PREFIX;
                    NEXT();
                    if ((*text & 0xF0) == TEXT_BACKREFERENCE_PREFIX_FIRST && (*text & 0xF) < 4) {
                        backref = (const text_backref_t*)text;
                        saved = text + 3;
                        value = backref->length_high * 8;
                        repeat = value + backref->length_low + 4;
                        value = backref->offset_high;
                        value *= 0xFE;
                        value = value + backref->offset_low;
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
                message_draw_glyph_timed(cursor_x, cursor_y, delay);
                if (character == TEXT_SPACE) {
                    cursor_x += 4;
                } else {
                    cursor_x += g_text_glyph_widths[character];
                }
                NEXT();
            }
        }
    }

    /* Close: a portrait box shrinks back to its open point; then release the window and texture. */
end:
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        from.w = 0;
        from.h = 0;
        from.x = right;
        from.y = bottom;
        mem_copy_bytes_nonempty(&to, &from, sizeof(to));
        menu_window_tween_quads(win, &from, &to, 4);
    }
    g_menu_screen_dialog_waiting = 0;
    win->active = 0;
    menu_release_window_buffer_pair(win);
    gfx_free_texture_rect(&rec.rect);
    message_release_typewriter_slot();
    YIELD_OR_INPUT();

    if (rec.box_type == DIALOG_BOX_TYPE_HELP && g_formation_screen_active == 1) {
        g_menu_input_lock_mode = 0;
    }
    if (((rec.box_type == DIALOG_BOX_TYPE_HELP || party_name_request) && rec.options == 0xC)
        || ((rec.box_type == DIALOG_BOX_TYPE_HELP || party_name_request) && !g_formation_screen_active)) {
        text_restore_section_pointers();
    }

    /* Options 0xC on the screen text slot is a direct call from Select's help thread, so return to it. */
    if (g_thread_current_id != THREAD_SCREEN_SLOT_TEXT || rec.options != 0xC) {
        thread_current_exit();
    }
}
