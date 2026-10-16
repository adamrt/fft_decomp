/* The deployment select menu, deployment strings, shaded panel strips and the map title graphic. */

#include "fft/event_attack.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_unit.h"
#include "fft/menu.h"
#include "fft/menu_panel.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void attack_deploy_run_select_menu(void) {
    POLY_FT4 left[2];
    POLY_FT4 right[2];
    s32 i;
    s32 selected;
    u32* input;
    s32 parent_id;
    thread_t* thread;
    thread_t* parent;
    help_navigation_record_t* node;
    help_navigation_record_t* nodes;
    s32 x;
    s32 y;
    s32 text_id;
    u32 buttons;
    s32 bob;
    s32 buffer;
    s32 u;
    s32 u_mid;
    s32 shadow_offset;

    u_mid = 0xb8;
    g_attack_panel_status_animation[0] = 1;
    g_attack_panel_status_animation[1] = 1;
    g_sound_pending_sfx_id = MAIN_SFX_CONFIRM;
    for (i = 0; i < 2; i++) {
        menu_init_default_poly_ft4(&left[i]);
        SetSemiTrans(&left[i], 0);
        left[i].tpage = GetTPage(0, 0, 0x3c0, 0x100);
        left[i].clut = 0x7d7c;
        menu_init_default_poly_ft4(&right[i]);
        SetSemiTrans(&right[i], 1);
        right[i].tpage = GetTPage(0, 2, 0x3c0, 0x100);
        right[i].clut = 0x7dbc;
    }
    if (thread_get_current_parameter_1() != 0) {
        g_attack_deploy_select_menu_nodes[5].destination[1] = 5;
        g_attack_deploy_select_menu_nodes[9].destination[1] = 9;
        g_attack_deploy_select_menu_nodes[10].destination[1] = 10;
    } else {
        g_attack_deploy_select_menu_nodes[5].destination[1] = 12;
        g_attack_deploy_select_menu_nodes[9].destination[1] = 17;
        g_attack_deploy_select_menu_nodes[10].destination[1] = 17;
    }
    g_attack_deploy_select_menu_nodes[8].help_base = g_attack_deploy_selected_roster_index * 11 + 10 - 0x3000;
    for (i = 0; i < 10; i++) {
        g_attack_deploy_select_menu_nodes[i + 18].help_base = g_attack_deploy_selected_roster_index * 11 - 0x3000 + i;
    }
    selected = 0;
    nodes = g_attack_deploy_select_menu_nodes;
    g_text_section_pointers[TEXT_SECTION_DEPLOYMENT_UNIT_HELP] = g_attack_deploy_unit_text_section;
    g_text_section_pointers[TEXT_SECTION_OVERLAY_SCREEN] = g_attack_deploy_help_text_section;
    parent_id = g_thread_current_id - 1;
    input = script_get_controller_input_pointer(0);
    i = 0;
    thread = &g_threads[g_thread_current_id];
    parent = &g_threads[parent_id];
    while (1) {
        if (parent->is_running == 0) {
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                g_menu_input_lock_mode = 0;
            }
            g_menu_input_lock_mode = 0;
            if (thread->task_words[1] != 0) {
                thread->task_words[1] = 0;
                thread->task_words[0] = 0;
            }
            node = &nodes[selected];
            y = node->y;
            x = node->x;
            if (thread_find_running_by_task(THREAD_TASK_DEBUG_UNIT_EDITOR) != 0) {
                y += 8;
            }
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                y += 8;
            }
            buttons = *input;
            y += 8;
            if (buttons & PSX_PAD_CROSS) {
                g_sound_pending_sfx_id = MAIN_SFX_CANCEL;
                *input = 0;
                break;
            }
            if (buttons & PSX_PAD_CIRCLE) {
                if (node->value != 0) {
                    text_id = node->help_base + *node->value;
                    if (node->help_base == -0x6800 && g_menu_selected_unit_fields.state.generic_monster != 0) {
                        text_id = *node->value | 0x7800;
                        if ((*node->value & 0x7ff) == 0x7ff || (*node->value & 0x7ff) == 0) {
                            *input = 0;
                        } else {
                            g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                            thread_start(parent_id, text_run_dialog_thread_variant);
                            thread_set_parameters(parent_id, 0x3b, text_id, 0);
                        }
                    } else if (node->help_base == 0x7800 && (u16)*node->value - 1 >= 0x301U) {
                        *input = 0;
                    } else if (node->help_base == 0x6800 && (u16)*node->value - 1 >= 0x101U) {
                        *input = 0;
                    } else if (node->help_base == -0x6800 && (u16)*node->value - 1 >= 0x101U) {
                        *input = 0;
                    } else {
                        g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                        thread_start(parent_id, text_run_dialog_thread_variant);
                        thread_set_parameters(parent_id, 0x3b, text_id, 0);
                    }
                } else {
                    text_id = node->help_base;
                    g_thread_call_target = (void (*)(void))text_find_entry;
                    if ((*(u8*)thread_call_on_main_stack(text_id) & 0xfe) != 0xfe) {
                        g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                        thread_start(parent_id, text_run_dialog_thread_variant);
                        thread_set_parameters(parent_id, 0x3b, text_id, 0);
                    }
                }
            } else if (buttons & PSX_PAD_DPAD_MASK) {
                text_id = 0;
                if (!(buttons & PSX_PAD_UP)) {
                    if (buttons & PSX_PAD_DOWN) {
                        text_id = 1;
                    } else {
                        text_id = 3;
                        if (buttons & PSX_PAD_RIGHT) {
                            text_id = 2;
                        }
                    }
                }
                if (selected != node->destination[text_id]) {
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                }
                selected = node->destination[text_id];
            }
        } else {
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                g_menu_input_lock_mode = 1;
            }
            g_menu_input_lock_mode = 1;
            if (thread->task_words[1] == 0) {
                thread->task_words[1] = 2;
                thread->task_words[0] = 0;
            }
        }
        if (thread->task_words[1] != 0) {
            bob = menu_get_cursor_bob_offset(1);
        } else {
            bob = menu_get_cursor_bob_offset(0);
        }
        thread->task_words[0] += g_script_event_speed;
        bob -= 4;
        buffer = i & 1;
        if (node->vertical_cursor != 0) {
            u = 0xa8;
            left[buffer].u0 = u;
            left[buffer].u1 = u;
            left[buffer].u2 = u_mid;
            left[buffer].u3 = u_mid;
            left[buffer].v0 = 0;
            left[buffer].v1 = 0x10;
            left[buffer].v2 = 0;
            left[buffer].v3 = 0x10;
            u = 0xb8;
            right[buffer].u0 = u;
            right[buffer].u1 = u;
            right[buffer].u2 = 0xc8;
            right[buffer].u3 = 0xc8;
            right[buffer].v0 = 0;
            right[buffer].v1 = 0x10;
            right[buffer].v2 = 0;
            right[buffer].v3 = 0x10;
            setXY4(&left[buffer], x, y + bob, x + 0x10, y + bob, x, y + bob + 0x10, x + 0x10, y + bob + 0x10);
            (right + buffer)->x0 = x + 2;
            (right + buffer)->y0 = y + bob + 2;
            (right + buffer)->x1 = x + 0x12;
            (right + buffer)->y1 = y + bob + 2;
            (right + buffer)->x2 = x + 2;
            (right + buffer)->y2 = y + bob + 0x12;
            (right + buffer)->x3 = x + 0x12;
            (right + buffer)->y3 = y + bob + 0x12;
        } else {
            u = 0xa8;
            (left + buffer)->u0 = u;
            (left + buffer)->v0 = 0;
            (left + buffer)->u1 = u_mid;
            (left + buffer)->v1 = 0;
            (left + buffer)->u2 = 0xa8;
            (left + buffer)->v2 = 0x10;
            (left + buffer)->u3 = u_mid;
            (left + buffer)->v3 = 0x10;
            u = 0xb8;
            (right + buffer)->u0 = u;
            (right + buffer)->v0 = 0;
            (right + buffer)->u1 = 0xc8;
            (right + buffer)->v1 = 0;
            (right + buffer)->u2 = u_mid;
            (right + buffer)->v2 = 0x10;
            (right + buffer)->u3 = 0xc8;
            (right + buffer)->v3 = 0x10;
            (left + buffer)->x0 = x + bob;
            (left + buffer)->y0 = y;
            (left + buffer)->x1 = x + bob + 0x10;
            (left + buffer)->y1 = y;
            (left + buffer)->x2 = x + bob;
            (left + buffer)->y2 = y + 0x10;
            (left + buffer)->x3 = x + bob + 0x10;
            (left + buffer)->y3 = y + 0x10;
            shadow_offset = bob + 2;
            (right + buffer)->x0 = x + shadow_offset;
            (right + buffer)->y0 = y + 2;
            (right + buffer)->x1 = x + shadow_offset + 0x10;
            (right + buffer)->y1 = y + 2;
            (right + buffer)->x2 = x + shadow_offset;
            (right + buffer)->y2 = y + 0x12;
            (right + buffer)->x3 = x + shadow_offset + 0x10;
            (right + buffer)->y3 = y + 0x12;
        }
        if (thread->task_words[1] != 0) {
            left[buffer].clut = 0x7dfc;
            right[buffer].clut = 0x7e3c;
        } else {
            left[buffer].clut = 0x7d7c;
            right[buffer].clut = 0x7dbc;
        }
        gfx_draw_or_append_gpu_primitive(&left[buffer]);
        gfx_draw_or_append_gpu_primitive(&right[buffer]);
        thread_wait_frames(1);
        i++;
    }
    g_attack_panel_status_animation[0] = 0;
    g_attack_panel_status_animation[1] = 0;
    thread_exit_current();
}

/* Point the battle text sections at an overlay text file: 32 offsets
 * followed by the section data they are relative to. The event-message
 * section is left as it is. */
static void attack_text_init_battle_pointers(attack_text_file_t* file) {
    s32* offsets = file->section_offsets;
    s32 i = 0;
    u8* data;
    u8** table;

    /* Section data follows the 32-entry offset header. */
    data = file->section_data;
    table = g_text_section_pointers;
    do {
        if (i != TEXT_SECTION_EVENT_MESSAGES) {
            *table = data + *offsets;
        }
        offsets++;
        i++;
        table++;
    } while (i < 32);
}

/* Appends one 0xfe/0xff-terminated string. Overflowing the arena jumps
 * straight to the cleanup and skips the help-text copy. */
#define APPEND_TEXT(text_id)                                                                                           \
    src = text_find_entry(text_id);                                                                                    \
    *dst++ = *src++;                                                                                                   \
    while ((dst[-1] & 0xfe) != 0xfe) {                                                                                 \
        if (dst >= g_attack_scenario_table + 0x4000) {                                                                 \
            goto end;                                                                                                  \
        }                                                                                                              \
        *dst++ = *src++;                                                                                               \
    }

/* An id of 0 or with all eleven low bits set stores an empty string. */
#define APPEND_TEXT_ID(id, base)                                                                                       \
    if (id == 0 || ((id) & 0x7ff) == 0x7ff) {                                                                          \
        *dst++ = 0xfe;                                                                                                 \
    } else {                                                                                                           \
        APPEND_TEXT((id) + base);                                                                                      \
    }

/*
 * Builds the deployment screen's per-roster-unit text strings.
 *
 * Loads the text file, then for each of the 16 roster slots loads the unit
 * into the editor and appends its 11 strings (five item names, five
 * ability or skillset names, the job name); an empty or unavailable slot
 * gets 11 empty strings. Finally publishes the two text pointers and copies
 * the 0x1400-byte help text.
 */
void attack_text_build_deployment_strings(void) {
    u8* dst = g_attack_scenario_table;
    attack_text_file_t* file = main_heap_alloc(0x18000);
    s32 i;
    s32 j;
    u8* src;
    s32 k;
    party_unit_t* unit;

    g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
    thread_call_on_main_stack(DISC_SECTOR_EVENT_ATCHELP_LZW, 0x16800, file);
    text_save_section_pointers();
    attack_text_init_battle_pointers(file);
    for (i = 0; i < 16; i++) {
        unit = main_party_get_unit(i);
        if (unit->party_id == PARTY_ID_NONE || i >= 0x10 || unit->proposition_status != 0) {
            for (k = 0; k < 11; k++) {
                *dst++ = 0xfe;
            }
            continue;
        }
        attack_editor_load_party_unit(1, i);
        for (j = 0; j < 5; j++) {
            APPEND_TEXT_ID(g_attack_editor_item_and_ability_ids[j], 0x6800);
        }
        if (g_menu_selected_unit_fields.state.generic_monster != 0) {
            for (j = 0; j < 5; j++) {
                APPEND_TEXT_ID(g_attack_editor_item_and_ability_ids[j + 5], 0x7800);
            }
        } else {
            for (j = 0; j < 2; j++) {
                APPEND_TEXT_ID(g_attack_editor_item_and_ability_ids[j + 5], 0x9800);
            }
            for (j = 2; j < 5; j++) {
                APPEND_TEXT_ID(g_attack_editor_item_and_ability_ids[j + 5], 0x7800);
            }
        }
        APPEND_TEXT(g_attack_panel_selected_unit_data.job_id + 0x6000);
    }
    g_attack_deploy_unit_text_section = g_attack_scenario_table;
    g_attack_deploy_help_text_section = g_attack_event_condition_blocks;
    src = text_find_entry(0x5800);
    dst = g_attack_event_condition_blocks;
    for (i = 0; i < 0x1400; i++) {
        *dst++ = *src++;
    }
end:
    text_restore_section_pointers();
    main_heap_call_free(file);
}

/*
 * Builds and submits a 2x2 set of shaded POLY_GT4 panel strips at the current
 * thread's position.
 *
 * Row 0 is the solid part of the strip and row 1 a narrow edge whose shade
 * fades to 0; `reverse` selects which side of `width` is drawn. Column 0 is
 * the same quad offset by one pixel with a different blend mode.
 *
 * `height` is a local rather than a literal so the narrowed y2/y3 sums reuse
 * the y0 sum (a literal lets fold reassociate them). `rest` is assigned inside
 * its branch: that keeps the target's loop hoisting, where
 * `x + (0x80 + width)` is recomputed once per row instead of leaving both loops.
 */
static void attack_gfx_build_shaded_panel_strips(POLY_GT4* polys, s32 width, s32 shade, s32 reverse) {
    POLY_GT4* poly;
    s32 row;
    s32 rest;
    s32 x = thread_get_current_parameter_1();
    s32 y = thread_get_current_parameter_2();
    s32 col;
    s32 dx;
    s32 dy;
    s32 edge;
    s32 height;

    poly = polys;
    height = 0x14;
    for (row = 0; row < 2; row++) {
        for (col = 0; col < 2; col++) {
            SetPolyGT4(poly);
            SetShadeTex(poly, 0);
            SetSemiTrans(poly, 1);
            if (col != 0) {
                if (GetGraphType() == 1 || GetGraphType() == 2) {
                    poly->tpage = 0x87;
                } else {
                    poly->tpage = 0x27;
                }
                dx = 0;
                dy = 0;
            } else {
                if (GetGraphType() == 1 || GetGraphType() == 2) {
                    poly->tpage = 0x107;
                } else {
                    poly->tpage = 0x47;
                }
                dx = 1;
                dy = 1;
            }
            poly->clut = 0x3fdc;
            if (row == 0) {
                if (reverse == 0) {
                    setUVWH(poly, 0, 0xe0, width, 0x14);
                    setXYWH(poly, x + 0x80 + dx, y + 0x60 + dy, width, height);
                } else {
                    rest = 0xff - width;
                    setUVWH(poly, width, 0xe0, rest, 0x14);
                    setXYWH(poly, x + (0x80 + width) + dx, y + 0x60 + dy, rest, height);
                }
                setRGB0(poly, shade, shade, shade);
                setRGB1(poly, shade, shade, shade);
                setRGB2(poly, shade, shade, shade);
                setRGB3(poly, shade, shade, shade);
            } else {
                if (reverse == 0) {
                    setRGB0(poly, shade, shade, shade);
                    setRGB1(poly, 0, 0, 0);
                    setRGB2(poly, shade, shade, shade);
                    setRGB3(poly, 0, 0, 0);
                } else {
                    setRGB1(poly, shade, shade, shade);
                    setRGB0(poly, 0, 0, 0);
                    setRGB3(poly, shade, shade, shade);
                    setRGB2(poly, 0, 0, 0);
                }
                if (reverse == 0) {
                    edge = 0x20;
                    if (width >= 0xdf) {
                        edge = 0xfe - width;
                    }
                    setUVWH(poly, width, 0xe0, edge, 0x14);
                    setXYWH(poly, x + 0x80 + width + dx, y + 0x60 + dy, edge, height);
                } else {
                    edge = 0x20;
                    if (width < 0x20) {
                        edge = width;
                    }
                    setUVWH(poly, width - edge, 0xe0, edge, 0x14);
                    setXYWH(poly, x + (0x80 + width) - edge + dx, y + 0x60 + dy, edge, height);
                }
            }
            poly++;
        }
    }
    if (reverse != 0) {
        gfx_draw_or_append_gpu_primitive(&polys[1]);
        gfx_draw_or_append_gpu_primitive(&polys[0]);
        gfx_draw_or_append_gpu_primitive(&polys[3]);
        gfx_draw_or_append_gpu_primitive(&polys[2]);
    } else {
        gfx_draw_or_append_gpu_primitive(&polys[3]);
        gfx_draw_or_append_gpu_primitive(&polys[2]);
        gfx_draw_or_append_gpu_primitive(&polys[1]);
        gfx_draw_or_append_gpu_primitive(&polys[0]);
    }
}

/* Thread body that animates the map-title panel strip with double-buffered
 * primitives: widen it by the thread's step, hold it, then sweep it again
 * with `reverse` set. The hold phase reuses `width` as its frame counter. */
void attack_map_show_title(void) {
    s32 step = thread_get_current_parameter_3();
    s32 buffer = 0;
    s32 width;

    if (step == 0) {
        step = 1;
    }

    width = 8;
    do {
        buffer ^= 1;
        attack_gfx_build_shaded_panel_strips(g_attack_map_title_sprites[buffer], width, 0x80, 0);
        width += step;
        thread_yield();
    } while (width < 0xf8);

    width = 0;
    do {
        buffer ^= 1;
        attack_gfx_build_shaded_panel_strips(g_attack_map_title_sprites[buffer], 0xf8, 0x80, 0);
        width += step;
        thread_yield();
    } while (width < 0x6e);

    width = 8;
    do {
        buffer ^= 1;
        attack_gfx_build_shaded_panel_strips(g_attack_map_title_sprites[buffer], width, 0x80, 1);
        if (width >= 0x81) {
            g_attack_map_title_closing_past_midpoint = 1;
        }
        width += step;
        thread_yield();
    } while (width < 0xf9);

    thread_exit_current();
}

/*
 * Upload the current map's title graphic and its palette.
 *
 * The titles are stored four to a 0x2800-byte file, 0xa00 bytes each, in
 * files five sectors apart from sector 0xdac, indexed by map id - 1. For map
 * 0x100 no file is read and the freshly allocated buffer is uploaded as it
 * is.
 */
void attack_map_load_title_graphic(void) {
    s32 group;
    u8* image = menu_alloc_memory(0x2800);
    s32 title = script_get_variable(EVENT_SCRIPT_VAR_CURRENT_MAP);
    s32 adjusted = title - 1;
    s32 group_input = adjusted;

    if (adjusted < 0) {
        group_input = title + 2;
    }
    group_input = group = group_input >> 2;
    group = adjusted - group * 4;

    if (adjusted != 0xff) {
        title = group_input * 5 + DISC_SECTOR_EVENT_MAPTITLE_BIN;
        g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
        thread_call_on_main_stack(title, 0x2800, image);
    }

    LoadImage(&g_attack_map_title_image_rect, (u32*)(image + group * 0xa00));
    LoadImage(&g_attack_map_title_palette_rect, &g_attack_map_title_palette_data);
    menu_free_memory(image);
}
