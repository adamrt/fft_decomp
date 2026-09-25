#include "fft/open.h"
#include "fft/world.h"
#include "psx/gpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Provisional view of the 0x0E-byte record at 0x8013A364 (copied by
 * world_gfx_copy_screen_setup_in/out). The halfword at +0x0A is also read
 * through the scalar alias g_world_unit_view_battle_id; the struct access is required here
 * because an in-struct load keeps its place after the preceding stores. */
typedef struct world_screen_setup_c {
    u8 _unused_00[0xA];
    s16 battle_id; /* 0x0a: active unit banner battle ID */
    u8 _unused_0c[2];
} world_screen_setup_c_t;

/* g_world_text_typewriter_glyph.y, .bits_per_pixel and .palette under
 * separate names: spelling any one of the three stores as a member access
 * breaks this function's match (first difference at offset 0). */
extern s16 g_world_text_typewriter_glyph_y;
extern s16 g_world_text_typewriter_glyph_bits_per_pixel;
extern s16 g_world_text_typewriter_glyph_palette;

#define WINDOW_AT(index) ((menu_window_buffer_t*)((index) * sizeof(menu_window_buffer_t) + (s32)win))

#define NEXT() text = world_script_advance_cursor_with_repeat(&repeat, text, &saved)

/* Aborts (here and below) leave the nested text loops for the shared
   window close-and-release tail at `end`. */
#define CHECK_ABORT()                                                                                                  \
    if (world_thread_get_current_task_id() == NATIVE_THREAD_TASK_STOP_REQUEST && g_world_thread_current_id != 0x10) {  \
        goto end;                                                                                                      \
    }

#define YIELD_OR_INPUT()                                                                                               \
    if (g_world_formation_screen_active != 0 && rec.box_type == DIALOG_BOX_TYPE_HELP) {                                \
        world_input_update_menu_state();                                                                               \
    } else {                                                                                                           \
        world_thread_yield();                                                                                          \
    }

/* Dialog text thread: opens a dialog window and draws the text entry named
 * by thread parameter 2 glyph by glyph, handling the control bytes. */
void world_text_character_handling_thread(void) {
    world_text_dialog_record_t rec;
    RECT from;
    RECT to;
    s32 repeat;
    u8* saved;
    s32 width_override;
    u16 cursor_y;
    s16 right;
    s16 bottom;
    u16 mark_x;
    u16 color;
    u16 no_wait;
    u16 variable_id;
    u16 value_index;
    native_thread_t* thread;
    s32 party_name_request;
    menu_window_buffer_t* win;
    menu_window_buffer_t* cursor_window;
    menu_window_buffer_t* arrow_window;
    menu_window_buffer_t* scroll_window;
    menu_window_buffer_t* hidden_window;
    menu_window_buffer_t* hidden_window_2;
    menu_window_buffer_t* choice_window;
    menu_window_buffer_t* choice_hidden_window;
    menu_window_buffer_t* choice_hidden_window_2;
    party_data_t* party;
    job_data_t* job;
    u8* text;
    s16 cursor_x;
    s32 column;
    s32 code;
    s32 i;
    s32 tick;
    s32 value;
    s32 digits;
    u16 started;
    u16 choice;
    s32* substitution;
    u32* divisor;
    s32 portrait_id;
    s32 base;
    s32 half_w;
    s32 half_h;
    s32 center;
    u16 param;
    s32 no_value;

    party_name_request = 0;
    world_thread_set_current_task_id(NATIVE_THREAD_TASK_RESUME);
    world_script_set_variable(EVENT_SCRIPT_VAR_TYPEWRITER_VRAM_X_OFFSET, 0);
    value_index = 0;
    variable_id = 0x18;
    thread = (native_thread_t*)((g_world_thread_current_id << 10) + (s32)g_world_threads);
    param = (s32)world_thread_get_current_parameter_1();
    rec.dialog_type = param;
    if ((param & 0x70) == 0x70) {
        rec.dialog_type = (param & 0x1C) | 0x10;
        rec.dialog_type |= world_unit_check_facing_relative_to_camera(world_thread_get_current_parameter_3()) + 1;
    }
    rec._unknown_2a = 0;
    rec.origin_x = 8;
    rec.origin_y = 8;
    rec.box_type = rec.dialog_type & 0x70;
    rec.position = rec.dialog_type & 3;
    rec.options = rec.dialog_type & 0xC;
    rec.no_box = rec.dialog_type & 0x80;
    rec.arrow_offset = thread->task_words[4];
    rec.x_offset = thread->task_words[2];
    rec.y_offset = thread->task_words[3];
    if (rec.box_type == DIALOG_BOX_TYPE_HELP && rec.options == 0xC) {
        world_text_save_section_pointers();
        world_text_init_format_section_pointers(g_open_text_section_offsets);
    }
    if (rec.box_type == DIALOG_BOX_TYPE_HELP && g_world_formation_screen_active == 0) {
        world_text_save_section_pointers();
        world_text_init_format_section_pointers(g_open_text_section_offsets);
        if (thread->function_parameter_2 == 0x807) {
            i = world_menu_get_party_unit_name_id();
            if (i >= 0x2FF) {
                i = 0x2FF;
            }
            party_name_request = 1;
            thread->function_parameter_2 = i | 0xA000;
            rec.box_type = DIALOG_BOX_TYPE_PORTRAIT;
            rec.options = 8;
            rec.portrait = ((world_screen_setup_c_t*)g_world_active_unit_data)->battle_id;
        }
    }
    if (rec.box_type == DIALOG_BOX_TYPE_HELP && g_world_formation_screen_active == 1) {
        g_world_thread_task_active = g_world_formation_screen_active;
    }
    if (rec.dialog_type & 0x10) {
        if (party_name_request == 0) {
            rec.portrait = thread->function_parameter_4;
        }
        party = main_party_get_data_pointer(rec.portrait);
        if (party->sprite_set >= 0x80) {
            if (party->sprite_set == CHARACTER_IDENTITY_GENERIC_MALE) {
                s32 base = (party->job_id - 0x4A) * 2;
                portrait_id = base + 0x60;
                if (party->job_id == JOB_ID_MIME) {
                    portrait_id = base + 0x5E;
                }
            } else if (party->sprite_set == CHARACTER_IDENTITY_GENERIC_FEMALE) {
                s32 base = (party->job_id - 0x4A) * 2;
                portrait_id = base + 0x61;
                if (party->job_id == JOB_ID_MIME) {
                    portrait_id = base + 0x5F;
                }
            } else {
                job = main_job_get_data_pointer(party->job_id);
                portrait_id = job->spritesheet_id;
            }
            if (party->job_id == JOB_ID_BARD) {
                portrait_id = 0x82;
            }
            if (party->job_id == JOB_ID_DANCER) {
                portrait_id = 0x83;
            }
        } else {
            portrait_id = party->sprite_set;
        }
        portrait_id = g_world_help_portrait_clut_by_sheet_id[portrait_id];
        if (party->sprite_set == 0) {
            portrait_id = 0;
        }
        if (party->sprite_set == CHARACTER_IDENTITY_MONSTER) {
            portrait_id |= job->job_portrait_palette << 10;
        }
        rec.portrait = portrait_id;
    } else {
        rec.portrait = thread->function_parameter_4 - 1;
    }
    text = world_text_find_entry(world_thread_get_current_parameter_2());
    world_text_measure_pixels(&rec.mark, &rec.lines, text);
    if (thread->task_words[6] != 0) {
        rec.mark = thread->task_words[6];
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
    rec.width = rec.mark + 0x18;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        if (rec.portrait < 10000) {
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
        value = world_get_misc_id(world_thread_get_current_parameter_3());
        world_unit_project_misc_to_screen(value, g_world_text_speaker_screen_coords);
        cursor_x = center;
        rec.offset = 0;
        rec.x = g_world_text_speaker_screen_coords[0] - center - 4;
        if (rec.position == 0) {
            if (world_thread_get_current_parameter_3() == (u16)g_world_text_speaker_swap_unit_a) {
                value = (u16)g_world_text_speaker_swap_unit_b;
            } else if (world_thread_get_current_parameter_3() == (u16)g_world_text_speaker_swap_unit_b) {
                value = (u16)g_world_text_speaker_swap_unit_a;
            }
            world_unit_project_misc_to_screen(world_get_misc_id(value), g_world_text_partner_screen_coords);
            if (g_world_text_speaker_screen_coords[1] < 0
                || g_world_text_partner_screen_coords[1] < g_world_text_speaker_screen_coords[1]) {
                rec.position = 2;
            } else {
                rec.position = 1;
            }
            rec.dialog_type = (rec.dialog_type & 0xFFF0) | rec.position;
        }
        if (rec.position == 1) {
            rec.x += 0xC;
            rec.y = g_world_text_speaker_screen_coords[1] - rec.height - 0x28;
        }
        if (rec.position == 2) {
            rec.x -= 8;
            rec.y = g_world_text_speaker_screen_coords[1];
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
    win = world_menu_alloc_window_buffer_pair();
    win->message_id = world_thread_get_current_parameter_2();
    win->dialogue_selector = rec.position;
    win->box_type = rec.box_type;
    rec.text = world_thread_get_current_parameter_2();
    rec.rect.w = rec.width;
    rec.rect.h = rec.height;
    world_gfx_alloc_texture_grid_rect(&rec.rect, (world_texture_prim_t*)win->quads, (u32*)-1);
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        if (text[0] == TEXT_SET_PORTRAIT) {
            win->portrait_code = text[1];
        } else {
            win->portrait_code = rec.portrait;
        }
        value = world_thread_get_current_parameter_3();
        world_menu_build_and_upload_window_image(rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.offset);
        win->portrait_battle_id = value;
    } else if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
        win[0].quads[0].clut = 0x7CBC;
        win[1].quads[0].clut = 0x7CBC;
        ClearImage(&rec.rect, 0, 0, 0);
    } else {
        rec.first_line = 0;
        rec.last_line = rec.lines;
        world_menu_render_text_image_at_record_origin((world_menu_text_image_t*)&rec);
    }
    if (g_world_formation_screen_active != 0) {
        right -= 0x100;
        bottom -= 0x78;
        rec.x -= 0x100;
        rec.y -= 0x78;
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
        if (rec.portrait >= 10000) {
            to.x = 0;
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
    world_script_copy_bytes(&win[1], win, 0x118);
    win[0].active = 1;
    win[1].active = 1;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
        world_menu_tween_window_quads(win, &from, &to, thread->task_words[5] & 0xF);
    } else {
        /* The target also passes &to in a2 to this two-argument callee. */
        ((void (*)(menu_window_buffer_t*, RECT*, RECT*))world_menu_animate_window_quad_crop)(win, &from, &to);
    }
    base = win->quads[0].u0;
    rec.x = base + 8;
    rec.y = win->quads[0].v0 + 8;
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.offset < 0 && rec.portrait < 8) {
        rec.x = base + 0x30;
    }
    if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT && rec.options != 8 && rec.position == 2) {
        rec.y += 8;
    }
    no_wait = 0;
    color = 1;
    column = 0;
    rec.color = 0;
    rec.line = 0;
    g_world_text_current_line = 0;
    /* Both arms are identical, as in the BATTLE twin whose first arm centres
     * the line; the split keeps the preceding stores ahead of these loads. */
    if (rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
        cursor_x = rec.x;
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
        code = *text;
        CHECK_ABORT();
        if (code == TEXT_NEWLINE || code == TEXT_END_WAIT_FOR_CONFIRM || code == TEXT_END_AUTO_CLOSE
            || code == TEXT_PAGE_BREAK) {
            if (code == TEXT_END_AUTO_CLOSE) {
                break;
            }
            if (code == TEXT_END_WAIT_FOR_CONFIRM) {
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
                    for (tick = 0;; tick += g_world_event_speed) {
                        YIELD_OR_INPUT();
                        if (g_world_menu_new_button_input & (PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT)) {
                            break;
                        }
                        CHECK_ABORT();
                        if (code == TEXT_PAGE_BREAK) {
                            if (rec.box_type != DIALOG_BOX_TYPE_PORTRAIT) {
                                cursor_window = WINDOW_AT(g_world_menu_packet_buffer_index);
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
                            if (code == TEXT_END_WAIT_FOR_CONFIRM && rec.box_type == DIALOG_BOX_TYPE_CENTERED) {
                                g_world_menu_event_state_flag = 1;
                            }
                            if (code == TEXT_NEWLINE
                                || (code == TEXT_END_WAIT_FOR_CONFIRM && rec.box_type == DIALOG_BOX_TYPE_CENTERED)) {
                                WINDOW_AT(g_world_menu_packet_buffer_index)->sprites[2].u0 = (tick & 0x30) - 0x58;
                                WINDOW_AT(g_world_menu_packet_buffer_index)->sprites[2].v0 = 0x10;
                                arrow_window = WINDOW_AT(g_world_menu_packet_buffer_index);
                                arrow_window->sprites[2].x0 = rec.arrow_x;
                                arrow_window->sprites[2].y0 = rec.arrow_y;
                            }
                            if (g_world_menu_current_button_input & PSX_PAD_SQUARE) {
                                while (1) {
                                    if (rec.line != rec.lines) {
                                        scroll_window = WINDOW_AT(g_world_menu_packet_buffer_index);
                                        scroll_window->scroll_range = rec.line - rec.lines;
                                        scroll_window->scroll_position = rec.first_line;
                                        scroll_window->icon_flags = 4;
                                        if (rec.first_line != 0) {
                                            scroll_window->icon_flags = 5;
                                        }
                                        if (rec.first_line != rec.line - rec.lines) {
                                            WINDOW_AT(g_world_menu_packet_buffer_index)->icon_flags |= 2;
                                        }
                                    }
                                    if ((g_world_menu_new_button_input & PSX_PAD_UP) && rec.first_line != 0) {
                                        g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                                        WINDOW_AT(g_world_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                        rec.first_line += no_value;
                                        rec.last_line = rec.first_line + rec.lines;
                                        world_menu_render_text_image_at_record_origin((world_menu_text_image_t*)&rec);
                                    }
                                    if ((g_world_menu_new_button_input & PSX_PAD_DOWN)
                                        && rec.first_line != rec.line - rec.lines) {
                                        g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                                        WINDOW_AT(g_world_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                        rec.first_line++;
                                        rec.last_line = rec.first_line + rec.lines;
                                        world_menu_render_text_image_at_record_origin((world_menu_text_image_t*)&rec);
                                    }
                                    if (!(g_world_menu_current_button_input & PSX_PAD_SQUARE)) {
                                        win[0].icon_flags = 0;
                                        win[1].icon_flags = 0;
                                        break;
                                    }
                                    CHECK_ABORT();
                                    WINDOW_AT(g_world_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                    YIELD_OR_INPUT();
                                }
                            }
                            /* The else-break keeps the test on top, as in the target; a
                               `while` is rotated to test at the bottom. */
                            for (;;) {
                                if (rec.line - rec.lines != rec.first_line) {
                                    WINDOW_AT(g_world_menu_packet_buffer_index)->sprites[2].x0 = 0x200;
                                    rec.first_line++;
                                    rec.last_line = rec.first_line + rec.lines;
                                    world_menu_render_text_image_at_record_origin((world_menu_text_image_t*)&rec);
                                } else {
                                    break;
                                }
                            }
                        }
                    }
                }
                g_world_menu_event_state_flag = 0;
                hidden_window = WINDOW_AT(g_world_menu_packet_buffer_index);
                hidden_window->sprites[0].x0 = 0x200;
                hidden_window->sprites[1].x0 = 0x200;
                hidden_window->sprites[2].x0 = 0x200;
                hidden_window->icon_flags = 0;
                YIELD_OR_INPUT();
                hidden_window_2 = WINDOW_AT(g_world_menu_packet_buffer_index);
                hidden_window_2->sprites[0].x0 = 0x200;
                hidden_window_2->sprites[1].x0 = 0x200;
                hidden_window_2->sprites[2].x0 = 0x200;
                hidden_window_2->icon_flags = 0;
                if (code == TEXT_END_WAIT_FOR_CONFIRM) {
                    if (rec.no_box == 0) {
                        break;
                    }
                    world_thread_set_current_task_id(NATIVE_THREAD_TASK_DIALOG_AWAIT_TEXT);
                    while (world_thread_get_current_task_id() != NATIVE_THREAD_TASK_RESUME) {
                        CHECK_ABORT();
                        world_thread_yield();
                    }
                    win->message_id = win[1].message_id = rec.text = world_thread_get_current_parameter_2();
                    if ((rec.text & no_value) == no_value) {
                        break;
                    }
                    column = 0;
                    text = world_text_find_entry(rec.text);
                    no_wait = 0;
                    world_menu_build_and_upload_window_image(
                        rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.offset);
                    color = 1;
                    cursor_x = rec.x;
                    width_override = no_value;
                    rec.mark = -1;
                    cursor_y = rec.y;
                    rec.color = 0;
                    rec.line = 0;
                    rec.portrait = thread->function_parameter_4 - 1;
                    g_world_menu_event_state_flag = 0;
                    win->portrait_code = rec.portrait;
                    value_index = 0;
                    saved = (u8*)-1;
                    repeat = 0;
                    win[1].portrait_code = rec.portrait;
                    world_thread_yield();
                    continue;
                }
                if (code == TEXT_PAGE_BREAK) {
                    continue;
                }
                if (rec.box_type == DIALOG_BOX_TYPE_PORTRAIT) {
                    world_menu_build_and_upload_window_image(
                        rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.offset);
                } else if (rec.box_type != 0) {
                    g_world_sound_effect_id_to_play = MAIN_SFX_TEXT_PAGE;
                    rec.first_line = rec.line;
                    rec.last_line = rec.line + rec.lines;
                    world_menu_render_text_image_at_record_origin((world_menu_text_image_t*)&rec);
                }
                cursor_y = rec.y - 0x10;
                world_thread_yield();
            }
            cursor_x = rec.x;
            no_wait = 0;
            cursor_y += 0x10;
        } else if (code == TEXT_CHOICE_MARK) {
            rec.mark = column;
            mark_x = cursor_x - rec.x - 0x10;
            NEXT();
        } else if (code == TEXT_CHOICE_SELECT) {
            if (rec.mark != -1) {
                choice = rec.mark;
                for (i = 0;; i += g_world_event_speed) {
                    g_world_menu_event_state_flag = 1;
                    choice_window = WINDOW_AT(g_world_menu_packet_buffer_index);
                    choice_window->sprites[0].x0
                        = mark_x + (choice_window->quads[0].x0 + rec.x - choice_window->quads[0].u0) - ((i >> 4) & 3);
                    choice_window->sprites[0].y0
                        = (choice_window->quads[0].y0 + rec.y - choice_window->quads[0].v0) + choice * 16;
                    choice_window->sprites[1].x0 = choice_window->sprites[0].x0 + 2;
                    choice_window->sprites[1].y0 = choice_window->sprites[0].y0 + 2;
                    world_thread_yield();
                    if (world_thread_get_current_task_id() == NATIVE_THREAD_TASK_STOP_REQUEST) {
                        goto end;
                    }
                    if (world_thread_get_current_task_id() == NATIVE_THREAD_TASK_WAIT_FOR_RESUME) {
                        continue;
                    }
                    if ((g_world_menu_new_button_input & PSX_PAD_UP) && choice != rec.mark) {
                        g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                        choice += no_value;
                    }
                    if ((g_world_menu_new_button_input & PSX_PAD_DOWN) && choice != column) {
                        g_world_sound_effect_id_to_play = MAIN_SFX_CURSOR_MOVE;
                        choice++;
                    }
                    if (g_world_menu_new_button_input & PSX_PAD_CIRCLE) {
                        if (rec.box_type != 0) {
                            g_world_sound_effect_id_to_play = MAIN_SFX_CONFIRM;
                        }
                        break;
                    }
                }
                choice_hidden_window = WINDOW_AT(g_world_menu_packet_buffer_index);
                choice_hidden_window->sprites[0].x0 = 0x200;
                choice_hidden_window->sprites[1].x0 = 0x200;
                world_thread_yield();
                choice_hidden_window_2 = WINDOW_AT(g_world_menu_packet_buffer_index);
                choice_hidden_window_2->sprites[0].x0 = 0x200;
                choice_hidden_window_2->sprites[1].x0 = 0x200;
                g_world_script_variables[EVENT_SCRIPT_VAR_SELECTED_DIALOG_OPTION] = choice - rec.mark;
            }
            NEXT();
            g_world_menu_event_state_flag = 0;
        } else if (code == TEXT_WAIT_FOR_RESUME) {
            world_thread_set_current_task_id(NATIVE_THREAD_TASK_WAIT_FOR_RESUME);
            while (world_thread_get_current_task_id() != NATIVE_THREAD_TASK_RESUME) {
                if (world_thread_get_current_task_id() == NATIVE_THREAD_TASK_STOP_REQUEST) {
                    goto end;
                }
                world_thread_yield();
            }
            if (rec.no_box != 0) {
                win->message_id = win[1].message_id = rec.text = world_thread_get_current_parameter_2();
                if ((rec.text & no_value) == no_value) {
                    break;
                }
                column = 0;
                text = world_text_find_entry(rec.text);
                no_wait = 0;
                world_menu_build_and_upload_window_image(
                    rec.width, rec.height, (s32)&rec.rect, rec.dialog_type, rec.offset);
                color = 1;
                width_override = no_value;
                cursor_x = rec.x;
                cursor_y = rec.y;
                value_index = 0;
                rec.color = 0;
                rec.mark = -1;
                rec.line = 0;
                repeat = 0;
                g_world_menu_event_state_flag = 0;
                saved = (u8*)-1;
                world_thread_yield();
                continue;
            }
            world_thread_yield();
            NEXT();
            no_wait = 1;
        } else if (code == TEXT_RELEASE_WAITING_THREADS) {
            for (i = 0; i < 0x11; i++) {
                if (g_world_thread_current_id != i
                    && g_world_thread_task_ids[i][0] == NATIVE_THREAD_TASK_WAIT_FOR_RESUME
                    && world_thread_is_running_80100164(i)) {
                    g_world_thread_task_ids[i][0] = NATIVE_THREAD_TASK_RESUME;
                }
            }
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
            variable_id = *text;
            NEXT();
        } else if (code == TEXT_STORE_VARIABLE) {
            NEXT();
            i = world_script_get_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX);
            if (i != 0) {
                world_script_set_variable(variable_id + i * 10, *text);
                world_script_set_variable(EVENT_SCRIPT_VAR_TEXT_STORE_INDEX, 0);
            } else {
                world_script_set_variable(variable_id, *text);
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
                if (code == TEXT_FORMAT_RAMZA_NAME) {
                    text = g_world_text_player_name_buffer;
                } else {
                    text = world_text_find_entry(value);
                }
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
                    code = value / g_world_text_decimal_divisors[i];
                    if (value < g_world_text_decimal_divisors[i] && i != 0 && started == 0) {
                        code = TEXT_SPACE;
                    } else {
                        started = 1;
                        value -= code * g_world_text_decimal_divisors[i];
                    }
                    g_world_text_typewriter_glyph.x = cursor_x;
                    g_world_text_typewriter_glyph.glyph = code;
                    g_world_text_typewriter_glyph_y = cursor_y;
                    g_world_text_typewriter_glyph_bits_per_pixel = rec.dialog_type & 0x70;
                    g_world_text_typewriter_glyph_palette = rec.color;
                    world_text_draw_glyph_with_typewriter_delay((s16)cursor_x, (s16)cursor_y, color);
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
                repeat = value + (text[1] >> 5) + 4;
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
                    if ((*text & 0xF0) == 0xF0 && (*text & 0xF) < 4) {
                        saved = text + 3;
                        value = (text[0] & 3) * 8;
                        repeat = value + (text[1] >> 5) + 4;
                        value = text[1] & 0xF;
                        value *= 0xFE;
                        value = value + text[2];
                        text -= value;
                    }
                    code += *text;
                }
                g_world_text_typewriter_glyph.x = cursor_x;
                g_world_text_typewriter_glyph.glyph = code;
                g_world_text_typewriter_glyph_y = cursor_y;
                g_world_text_typewriter_glyph_bits_per_pixel = rec.box_type;
                g_world_text_typewriter_glyph_palette = rec.color;
                world_text_draw_glyph_with_typewriter_delay((s16)cursor_x, (s16)cursor_y, color);
                if (code == TEXT_SPACE) {
                    cursor_x += 4;
                } else {
                    cursor_x += g_world_text_glyph_widths[code];
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
        world_script_copy_bytes(&to, &from, 8);
        world_menu_tween_window_quads(win, &from, &to, 4);
    }
    g_world_menu_event_state_flag = 0;
    win->active = 0;
    world_menu_release_window_buffer_pair(win);
    world_gfx_free_texture_grid_rect(&rec.rect);
    world_menu_clear_matching_state_entry();
    YIELD_OR_INPUT();
    if (rec.box_type == DIALOG_BOX_TYPE_HELP && g_world_formation_screen_active == 1) {
        g_world_thread_task_active = 0;
    }
    if (((rec.box_type == DIALOG_BOX_TYPE_HELP || party_name_request != 0) && rec.options == 0xC)
        || ((rec.box_type == DIALOG_BOX_TYPE_HELP || party_name_request != 0)
            && g_world_formation_screen_active == 0)) {
        world_text_restore_section_pointers();
    }
    if (g_world_thread_current_id != 1 || rec.options != 0xC) {
        world_thread_exit_current();
    }
}
