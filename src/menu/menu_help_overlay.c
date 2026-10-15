/* Menu close, battle/require help runners and the first running menu thread. */

#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/event_helpmenu.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void menu_close(void) {
    mem_copy_bytes_nonempty(g_text_section_pointers, g_text_saved_pointers, 0x80);
    main_heap_free(g_main_heap_high_overlay_load_address);
    g_menu_help_open = 0;
    thread_exit_current();
}

/* Opens help for the running menu's pending request, or runs the bouncing
 * help cursor over the navigation table for that menu or the unit view. */
void menu_run_battle_help(void) {
    POLY_FT4 cursor_quads[2];
    POLY_FT4 shadow_quads[2];
    help_navigation_record_t* navigation;
    s32 menu_index;
    s32 x;
    thread_t* help_thread;
    help_navigation_record_t* node;
    s32 y;
    s32 bob;
    s32 buffer;
    s32 u;
    s32 u_mid;
    s32 shadow_offset;
    s32 help_id; /* also the request table mode flag and the pad direction index */
    s16 request_kind;
    s32 text_section;
    s32 view_mode;
    s32 input;
    s32 quad_index;
    s32 frame; /* first walks the thread slots looking for a help request */
    /* Unbinding this id swaps its register with the navigation pointer. */
    register s32 help_thread_id __asm__("$21");
    thread_t* scan_thread;
    menu_entry_t* request;
    thread_t* self;
    menu_unit_status_record_t* setup_banner;
    menu_active_unit_t* setup_unit_data;
    menu_status_panel_slot_storage_t* setup_billboard;
    map_selected_tile_t* setup_cursor_tile;
    s32 missing_value;

    u_mid = 0xB8;
    if (g_menu_modal_open != 0) {
        thread_exit_current();
    }
    mem_copy_bytes_nonempty(g_text_saved_pointers, g_text_section_pointers, sizeof(g_text_saved_pointers));
    setup_banner = &g_menu_help_active_banner;
    setup_unit_data = &g_menu_help_active_unit_data;
    setup_billboard = &g_menu_help_billboard;
    setup_cursor_tile = &g_menu_help_menu_cursor_tile;
    g_text_section_pointers[HELP_TEXT_SECTION_UNIT_STATUS]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_UNIT_STATUS];
    g_text_section_pointers[HELP_TEXT_SECTION_MENU_COMMANDS]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_MENU_COMMANDS];
    g_text_section_pointers[HELP_TEXT_SECTION_TERRAIN]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_TERRAIN];
    g_text_section_pointers[HELP_TEXT_SECTION_JOBS]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_JOBS];
    g_text_section_pointers[HELP_TEXT_SECTION_ITEMS]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_ITEMS];
    g_text_section_pointers[HELP_TEXT_SECTION_ABILITIES]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_ABILITIES];
    g_text_section_pointers[HELP_TEXT_SECTION_SKILLSETS]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_SKILLSETS];
    g_text_section_pointers[HELP_TEXT_SECTION_STATUSES]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_STATUSES];
    self = &g_threads[g_thread_current_id];
    menu_copy_billboard_data_out(setup_banner, setup_unit_data, setup_billboard, setup_cursor_tile);
    /* This fixed lower-menu 16-bit slot can contain another overlay's data. */
    if ((g_menu_selected_unit_stat_summary.team_kind != 0) || (g_menu_post_battle_unit_changes_active != 0)) {
        g_menu_help_require_navigation[2].destination[2] = 7;
        g_menu_help_require_navigation[7].destination[3] = 2;
    }
    help_thread_id = thread_find_first_running_menu_thread();
    g_menu_help_controller_state_ptr = script_get_controller_input_pointer(0);
    menu_index = 0;
    for (quad_index = 0; quad_index < 2; quad_index++) {
        menu_init_default_poly_ft4(&cursor_quads[quad_index]);
        SetSemiTrans(&cursor_quads[quad_index], 0);
        cursor_quads[quad_index].tpage = GetTPage(0, 0, 0x3C0, 0x100);
        cursor_quads[quad_index].clut = 0x7D7C;
        menu_init_default_poly_ft4(&shadow_quads[quad_index]);
        SetSemiTrans(&shadow_quads[quad_index], 1);
        shadow_quads[quad_index].tpage = GetTPage(0, 2, 0x3C0, 0x100);
        shadow_quads[quad_index].clut = 0x7DBC;
    }
    if (help_thread_id < 9) {
        frame = help_thread_id;
        missing_value = -1;
        scan_thread = &g_threads[frame];
        for (; frame < 9; frame++) {
            request = (menu_entry_t*)scan_thread->function_parameter_1;
            if (request->select_text_table != missing_value) {
                break;
            }
            scan_thread++;
        }
        if (frame != 9) {
            if (g_menu_help_active_text_id_tables == (u16* const*)missing_value) {
                g_menu_help_active_text_id_tables = g_menu_help_menu_text_id_tables;
            }
            request_kind = request->select_text_table;
            if (request_kind != 0x10) {
                if (request_kind == 0x100) {
                    help_id = *menu_entry_result(request);
                    help_id &= 0x7FF;
                    help_id |= 0x7800;
                    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                    thread_start(help_thread_id - 1, text_run_dialog_thread_variant);
                    thread_set_parameters(help_thread_id - 1, 0x3B, help_id, 0);
                    *g_menu_help_controller_state_ptr = 0;
                    thread_wait_frames(1);
                    menu_close();
                } else if (request_kind == 4) {
                    help_id = g_menu_skillsets[request->selected_index];
                    if (help_id == ACTION_MENU_PSEUDO_SKILLSET_ANYTHING) {
                        *g_menu_help_controller_state_ptr = 0;
                        thread_wait_frames(1);
                        menu_close();
                    }
                    help_id |= 0x9800;
                    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                    thread_start(help_thread_id - 1, text_run_dialog_thread_variant);
                    thread_set_parameters(help_thread_id - 1, 0x3B, help_id, 0);
                } else if (request_kind == 0) {
                    *g_menu_help_controller_state_ptr = 0;
                    thread_wait_frames(1);
                    menu_close();
                } else if (request_kind == 5) {
                    menu_text_layout_t* layout = request->context;
                    help_id = layout->text_ids[0][request->selected_index];
                    help_id &= 0x7FF;
                    help_id |= 0x7800;
                    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                    thread_start(help_thread_id - 1, text_run_dialog_thread_variant);
                    thread_set_parameters(help_thread_id - 1, 0x3B, help_id, 0);
                } else if (request_kind == 0x11) {
                    menu_text_layout_t* layout = request->context;
                    help_id = layout->text_ids[0][request->selected_index];
                    help_id &= 0x7FF;
                    if (help_id == 0) {
                        *g_menu_help_controller_state_ptr = 0;
                        thread_wait_frames(1);
                        menu_close();
                    }
                    help_id |= 0x6800;
                    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                    thread_start(help_thread_id - 1, text_run_dialog_thread_variant);
                    thread_set_parameters(help_thread_id - 1, 0x3B, help_id, 0);
                } else if (request_kind == 0x17) {
                    menu_text_layout_t* layout = request->context;
                    help_id = layout->text_ids[0][request->selected_index];
                    help_id &= 0x7FF;
                    if (help_id == 0) {
                        *g_menu_help_controller_state_ptr = 0;
                        thread_wait_frames(1);
                        menu_close();
                    }
                    help_id += ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE;
                    help_id |= 0x7800;
                    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                    thread_start(help_thread_id - 1, text_run_dialog_thread_variant);
                    thread_set_parameters(help_thread_id - 1, 0x3B, help_id, 0);
                } else if (request_kind == 0x12) {
                    menu_text_layout_t* layout = request->context;
                    /* The list stores its column toggle in the third text pointer. */
                    help_id = *layout->text_ids[2];
                    if (help_id == 0) {
                        help_id = layout->text_ids[0][request->selected_index];
                    } else {
                        help_id = layout->text_ids[1][request->selected_index];
                    }
                    help_id &= 0x7FF;
                    if (help_id == 0) {
                        *g_menu_help_controller_state_ptr = 0;
                        thread_wait_frames(1);
                        menu_close();
                    }
                    help_id |= 0xA000;
                    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                    thread_start(help_thread_id - 1, text_run_dialog_thread_variant);
                    thread_set_parameters(help_thread_id - 1, 0x3B, help_id, 0);
                } else if (request_kind == 0x13) {
                    menu_text_layout_t* layout = request->context;
                    help_id = layout->text_ids[1][request->selected_index];
                    text_section = help_id & 0xF800;
                    if (text_section == TEXT_ID_ABILITY_NAME_BASE) {
                        help_id &= 0x7FF;
                        help_id |= 0x7800;
                    } else if (text_section == TEXT_ID_SKILLSET_NAME_SOURCE_BASE) {
                        help_id &= 0x7FF;
                        help_id |= 0x9800;
                    } else if (text_section == TEXT_ID_ITEM_NAME_BASE) {
                        help_id &= 0x7FF;
                        if ((help_id - ITEM_ID_ASURA_KNIFE) < 0xAU) {
                            help_id += ABILITY_ID_DRAW_OUT_ASURA - ITEM_ID_ASURA_KNIFE;
                            help_id |= 0x7800;
                        } else {
                            help_id |= 0x6800;
                        }
                    } else {
                        help_id = 0x807;
                    }
                    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                    thread_start(help_thread_id - 1, text_run_dialog_thread_variant);
                    thread_set_parameters(help_thread_id - 1, 0x3B, help_id, 0);
                } else {
                    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                    g_menu_help_topic_text_ids = g_menu_help_active_text_id_tables[request_kind];
                    thread_start(help_thread_id - 1, text_run_dialog_thread_variant);
                    thread_set_parameters(
                        help_thread_id - 1, 0x3B, g_menu_help_topic_text_ids[request->selected_index], 0);
                }
                /* Wait for the text thread in the preceding slot. */
                frame = help_thread_id;
                do {
                    thread_wait_frames(1);
                } while (g_threads[frame - 1].is_running != 0);
                *g_menu_help_controller_state_ptr = 0;
                thread_wait_frames(1);
                menu_close();
            }
        } else {
            *g_menu_help_controller_state_ptr = 0;
            thread_wait_frames(1);
            menu_close();
        }
    }
    if (help_thread_id < 9) {
        navigation = g_menu_help_require_navigation;
        help_thread_id -= 1;
    } else {
        view_mode = g_menu_unit_view_mode;
        if (view_mode == UNIT_VIEW_MODE_TILE_INFO || view_mode == UNIT_VIEW_MODE_HOVERED_UNIT) {
            g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
            thread_start(THREAD_SCREEN_SLOT_MESSAGE, text_run_dialog_thread_variant);
            thread_set_parameters(
                THREAD_SCREEN_SLOT_MESSAGE, 0x3B, g_menu_help_menu_cursor_tile.surface_type + 0x5800, 0);
            g_threads[THREAD_SCREEN_SLOT_MESSAGE].task_words[3] = -0x20;
            thread_wait_until_inactive(THREAD_SCREEN_SLOT_MESSAGE);
            *g_menu_help_controller_state_ptr = 0;
            thread_wait_frames(1);
            menu_close();
        } else if (view_mode == UNIT_VIEW_MODE_HOVERED_UNIT) {
            /* Unreachable: the arm above already takes modes 1 and 2. */
            navigation = g_menu_help_terrain_navigation;
            help_thread_id = 2;
        } else if (view_mode == UNIT_VIEW_MODE_UNIT_SUMMARY) {
            navigation = g_menu_help_unit_box_navigation;
            help_thread_id = 2;
        } else if (view_mode == UNIT_VIEW_MODE_RIGHT_UNIT) {
            navigation = g_menu_help_attack_preview_navigation;
            help_thread_id = 2;
        }
    }
    frame = 0;
    help_thread = &g_threads[help_thread_id];
    while (1) {
        if (help_thread->is_running == 0) {
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                g_menu_input_lock_mode = 0;
            }
            if (self->task_words[1] != 0) {
                self->task_words[1] = 0;
                self->task_words[0] = 0;
            }
            node = &navigation[menu_index];
            y = node->y;
            x = node->x;
            if (thread_find_running_by_task(THREAD_TASK_DEBUG_UNIT_EDITOR) != 0) {
                y += 8;
            }
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                y += 8;
            }
            input = *g_menu_help_controller_state_ptr;
            if (input & PSX_PAD_CROSS) {
                g_sound_pending_sfx_id = MAIN_SFX_CANCEL;
                break;
            }
            if (input & PSX_PAD_CIRCLE) {
                if (node->value != 0) {
                    help_id = node->help_base + *node->value;
                    if (node->help_base == -0x6800 && g_menu_help_billboard.state.generic_monster != 0) {
                        help_id = *node->value | 0x7800;
                        if ((*node->value & 0x7FF) == 0x7FF || (*node->value & 0x7FF) == 0) {
                            *g_menu_help_controller_state_ptr = 0;
                        } else {
                            g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                            thread_start(help_thread_id, text_run_dialog_thread_variant);
                            thread_set_parameters(help_thread_id, 0x3B, help_id, 0);
                        }
                    } else if (node->help_base == 0x7800 && (u32)((u16)*node->value - 1) >= 0x301) {
                        *g_menu_help_controller_state_ptr = 0;
                    } else if (node->help_base == 0x6800 && (u32)((u16)*node->value - 1) >= 0x101) {
                        *g_menu_help_controller_state_ptr = 0;
                    } else if (node->help_base == -0x6800 && (u32)((u16)*node->value - 1) >= 0x101) {
                        *g_menu_help_controller_state_ptr = 0;
                    } else {
                        g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                        thread_start(help_thread_id, text_run_dialog_thread_variant);
                        thread_set_parameters(help_thread_id, 0x3B, help_id, 0);
                    }
                } else {
                    help_id = node->help_base;
                    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                    thread_start(help_thread_id, text_run_dialog_thread_variant);
                    thread_set_parameters(help_thread_id, 0x3B, help_id, 0);
                }
            } else if (input & PSX_PAD_DPAD_MASK) {
                help_id = 0;
                if (!(input & PSX_PAD_UP)) {
                    if (input & PSX_PAD_DOWN) {
                        help_id = 1;
                    } else {
                        help_id = 3;
                        if (input & PSX_PAD_RIGHT) {
                            help_id = 2;
                        }
                    }
                }
                if (menu_index != node->destination[help_id]) {
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                }
                menu_index = node->destination[help_id];
            }
        } else {
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                g_menu_input_lock_mode = 1;
            }
            if (self->task_words[1] == 0) {
                self->task_words[1] = 2;
                self->task_words[0] = 0;
            }
        }
        if (self->task_words[1] != 0) {
            bob = menu_get_cursor_bob_offset(1);
        } else {
            bob = menu_get_cursor_bob_offset(0);
        }
        self->task_words[0] += g_script_event_speed;
        bob -= 4;
        buffer = frame & 1;
        if (node->vertical_cursor != 0) {
            u = 0xA8;
            cursor_quads[buffer].u0 = u;
            cursor_quads[buffer].u1 = u;
            cursor_quads[buffer].u2 = u_mid;
            cursor_quads[buffer].u3 = u_mid;
            cursor_quads[buffer].v0 = 0;
            cursor_quads[buffer].v1 = 0x10;
            cursor_quads[buffer].v2 = 0;
            cursor_quads[buffer].v3 = 0x10;
            u = 0xB8;
            shadow_quads[buffer].u0 = u;
            shadow_quads[buffer].u1 = u;
            shadow_quads[buffer].u2 = 0xC8;
            shadow_quads[buffer].u3 = 0xC8;
            shadow_quads[buffer].v0 = 0;
            shadow_quads[buffer].v1 = 0x10;
            shadow_quads[buffer].v2 = 0;
            shadow_quads[buffer].v3 = 0x10;
            setXY4(&cursor_quads[buffer], x, y + bob, x + 0x10, y + bob, x, y + bob + 0x10, x + 0x10, y + bob + 0x10);
            setXY4(shadow_quads + buffer, x + 2, y + bob + 2, x + 0x12, y + bob + 2, x + 2, y + bob + 0x12, x + 0x12,
                y + bob + 0x12);
        } else {
            u = 0xA8;
            setUV4(cursor_quads + buffer, u, 0, u_mid, 0, 0xA8, 0x10, u_mid, 0x10);
            u = 0xB8;
            setUV4(shadow_quads + buffer, u, 0, 0xC8, 0, u_mid, 0x10, 0xC8, 0x10);
            setXY4(cursor_quads + buffer, x + bob, y, x + bob + 0x10, y, x + bob, y + 0x10, x + bob + 0x10, y + 0x10);
            shadow_offset = bob + 2;
            setXY4(shadow_quads + buffer, x + shadow_offset, y + 2, x + shadow_offset + 0x10, y + 2, x + shadow_offset,
                y + 0x12, x + shadow_offset + 0x10, y + 0x12);
        }
        if (self->task_words[1] != 0) {
            cursor_quads[buffer].clut = 0x7DFC;
            shadow_quads[buffer].clut = 0x7E3C;
        } else {
            cursor_quads[buffer].clut = 0x7D7C;
            shadow_quads[buffer].clut = 0x7DBC;
        }
        gfx_draw_or_append_gpu_primitive(&cursor_quads[buffer]);
        gfx_draw_or_append_gpu_primitive(&shadow_quads[buffer]);
        thread_wait_frames(1);
        frame++;
    }
    *g_menu_help_controller_state_ptr = 0;
    thread_wait_frames(1);
    menu_close();
}

/* The lowest running slot from THREAD_SLOT_SYSTEM_FUNCTION (4) to THREAD_SLOT_MENU (8),
 * or 9 when none runs. */
s32 thread_find_first_running_menu_thread(void) {
    s32 thread_id;

    for (thread_id = THREAD_SLOT_SYSTEM_FUNCTION; thread_id <= THREAD_SLOT_MENU; thread_id++) {
        if (g_threads[thread_id].is_running == 1) {
            return thread_id;
        }
    }
    return thread_id;
}

/* Run the help menu over the REQUIRE screen. */

void menu_run_require_help(void) {
    POLY_FT4 cursor[2];
    POLY_FT4 shadow[2];
    thread_t* self;
    help_navigation_record_t* navigation;
    help_navigation_record_t* entry;
    POLY_FT4* poly;
    POLY_FT4* shadow_poly;
    s32 menu_index;
    thread_t* text_thread;
    s32 text_thread_id;
    s32 frame;
    s32 buffer;
    s32 x;
    s32 y;
    s32 bob;
    s32 input;
    /* Also carries the navigation direction. */
    s32 help_id;
    s32 u;

    for (frame = 0; frame < 2; frame++) {
        poly = &cursor[frame];
        menu_init_default_poly_ft4(poly);
        SetSemiTrans(poly, 0);
        poly->tpage = GetTPage(0, 0, 0x3C0, 0x100);
        poly->clut = 0x7D7C;
        shadow_poly = &shadow[frame];
        menu_init_default_poly_ft4(shadow_poly);
        SetSemiTrans(shadow_poly, 1);
        shadow_poly->tpage = GetTPage(0, 2, 0x3C0, 0x100);
        shadow_poly->clut = 0x7DBC;
    }
    mem_copy_bytes_nonempty(g_text_saved_pointers, g_text_section_pointers, 0x80);
    g_text_section_pointers[HELP_TEXT_SECTION_UNIT_STATUS]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_UNIT_STATUS];
    g_text_section_pointers[HELP_TEXT_SECTION_MENU_COMMANDS]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_MENU_COMMANDS];
    g_text_section_pointers[HELP_TEXT_SECTION_TERRAIN]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_TERRAIN];
    g_text_section_pointers[HELP_TEXT_SECTION_JOBS]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_JOBS];
    g_text_section_pointers[HELP_TEXT_SECTION_ITEMS]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_ITEMS];
    g_text_section_pointers[HELP_TEXT_SECTION_ABILITIES]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_ABILITIES];
    g_text_section_pointers[HELP_TEXT_SECTION_SKILLSETS]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_SKILLSETS];
    g_text_section_pointers[HELP_TEXT_SECTION_STATUSES]
        = g_menu_help_data_base + g_menu_help_text_section_offsets[HELP_TEXT_SECTION_STATUSES];
    self = &g_threads[g_thread_current_id];
    menu_copy_billboard_data_out(&g_menu_help_active_banner, &g_menu_help_active_unit_data, &g_menu_help_billboard,
        &g_menu_help_menu_cursor_tile);
    if (g_menu_selected_unit_stat_summary.team_kind != 0 || g_menu_post_battle_unit_changes_active != 0) {
        g_menu_help_require_navigation[2].destination[2] = 7;
        g_menu_help_require_navigation[7].destination[3] = 2;
    }
    g_menu_help_controller_state_ptr = script_get_controller_input_pointer(0);
    menu_index = 0;
    g_menu_help_require_navigation[0].x = 0xC4;
    g_menu_help_require_navigation[2].x = 0xE0;
    g_menu_help_require_navigation[0].destination[3] = 0;
    g_menu_help_require_navigation[3].destination[0] = 0;
    navigation = g_menu_help_require_navigation;
    text_thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    text_thread = &g_threads[text_thread_id];
    for (frame = 0;; frame++) {
        if (text_thread->is_running == 0) {
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                g_menu_input_lock_mode = 0;
            }
            if (self->task_words[1] != 0) {
                self->task_words[1] = 0;
                self->task_words[0] = 0;
            }
            entry = &navigation[menu_index];
            y = entry->y;
            x = entry->x;
            if (thread_find_running_by_task(THREAD_TASK_DEBUG_UNIT_EDITOR) != 0) {
                y += 8;
            }
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                y += 8;
            }
            input = *g_menu_help_controller_state_ptr;
            if (input & PSX_PAD_CROSS) {
                g_sound_pending_sfx_id = MAIN_SFX_CANCEL;
                break;
            }
            if (input & PSX_PAD_CIRCLE) {
                if (entry->value != 0) {
                    help_id = entry->help_base + *entry->value;
                    if (entry->help_base == -0x6800 && g_menu_help_billboard.state.generic_monster != 0) {
                        help_id = *entry->value | 0x7800;
                        if ((*entry->value & 0x7FF) == 0x7FF || (*entry->value & 0x7FF) == 0) {
                            *g_menu_help_controller_state_ptr = 0;
                        } else {
                            g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                            thread_start(text_thread_id, text_run_dialog_thread_variant);
                            thread_set_parameters(text_thread_id, 0x3B, help_id, 0);
                        }
                    } else if (entry->help_base == 0x7800 && (*(u16*)entry->value - 1) >= 0x301U) {
                        *g_menu_help_controller_state_ptr = 0;
                    } else if (entry->help_base == 0x6800 && (*(u16*)entry->value - 1) >= 0x101U) {
                        *g_menu_help_controller_state_ptr = 0;
                    } else if (entry->help_base == -0x6800 && (*(u16*)entry->value - 1) >= 0x101U) {
                        *g_menu_help_controller_state_ptr = 0;
                    } else {
                        g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                        thread_start(text_thread_id, text_run_dialog_thread_variant);
                        thread_set_parameters(text_thread_id, 0x3B, help_id, 0);
                    }
                } else {
                    help_id = entry->help_base;
                    g_sound_pending_sfx_id = MAIN_SFX_WINDOW_OPEN;
                    thread_start(text_thread_id, text_run_dialog_thread_variant);
                    thread_set_parameters(text_thread_id, 0x3B, help_id, 0);
                }
            } else if (input & PSX_PAD_DPAD_MASK) {
                if (input & PSX_PAD_UP) {
                    help_id = 0;
                } else if (input & PSX_PAD_DOWN) {
                    help_id = 1;
                } else if (input & PSX_PAD_RIGHT) {
                    help_id = 2;
                } else {
                    help_id = 3;
                }
                if (menu_index != entry->destination[help_id]) {
                    g_sound_pending_sfx_id = MAIN_SFX_CURSOR_MOVE;
                }
                menu_index = entry->destination[help_id];
            }
        } else {
            /* The help text is up: entry, x and y keep the values of the last
               idle frame. */
            if (thread_find_running_by_task(THREAD_TASK_HELP_MENU) != 0) {
                g_menu_input_lock_mode = 1;
            }
            if (self->task_words[1] == 0) {
                self->task_words[1] = 2;
                self->task_words[0] = 0;
            }
        }
        if (self->task_words[1] != 0) {
            bob = menu_get_cursor_bob_offset(1);
        } else {
            bob = menu_get_cursor_bob_offset(0);
        }
        self->task_words[0] += g_script_event_speed;
        bob -= 4;
        buffer = frame & 1;
        if (entry->vertical_cursor != 0) {
            POLY_FT4* quad;
            POLY_FT4* shadow_quad;

            /* The upright cursor is the same texture cell transposed. */
            u = 0xA8;
            quad = &cursor[buffer];
            quad->u0 = u;
            quad->u1 = u;
            quad->u2 = u + 16;
            quad->u3 = u + 16;
            quad->v0 = 0;
            quad->v1 = 16;
            quad->v2 = 0;
            quad->v3 = 16;
            u += 16;
            shadow_quad = &shadow[buffer];
            shadow_quad->u0 = u;
            shadow_quad->u1 = u;
            shadow_quad->u2 = u + 16;
            shadow_quad->u3 = u + 16;
            shadow_quad->v0 = 0;
            shadow_quad->v1 = 16;
            shadow_quad->v2 = 0;
            shadow_quad->v3 = 16;
            setXYWH(quad, x, y + bob, 16, 16);
            /* The shadow follows cursor; its named pointer changes store scheduling. */
            setXYWH(&quad[2], x + 2, y + bob + 2, 16, 16);
        } else {
            POLY_FT4* quads;
            s32 shadow_x;

            /* Retail loads each quad's third u from its own constant, not from `u`. */
            u = 0xA8;
            quads = cursor;
            setUV4(&quads[buffer], u, 0, u + 16, 0, 0xA8, 16, u + 16, 16);
            u += 16;
            setUV4(&shadow[buffer], u, 0, u + 16, 0, 0xB8, 16, u + 16, 16);
            setXYWH(&quads[buffer], x + bob, y, 16, 16);
            shadow_x = x + 2 + bob;
            setXYWH(&shadow[buffer], shadow_x, y + 2, 16, 16);
        }
        if (self->task_words[1] != 0) {
            cursor[buffer].clut = 0x7DFC;
            shadow[buffer].clut = 0x7E3C;
        } else {
            cursor[buffer].clut = 0x7D7C;
            shadow[buffer].clut = 0x7DBC;
        }
        gfx_draw_or_append_gpu_primitive(&cursor[buffer]);
        gfx_draw_or_append_gpu_primitive(&shadow[buffer]);
        thread_wait_frames(1);
    }
    *g_menu_help_controller_state_ptr = 0;
    thread_wait_frames(1);
    menu_close();
}
