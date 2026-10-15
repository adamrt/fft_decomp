#include "fft/battle.h"
#include "fft/battle_camera.h"
#include "fft/battle_event.h"
#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/battle_text.h"
#include "fft/battle_unit.h"
#include "fft/event_debugchr.h"
#include "fft/event_etc.h"
#include "fft/event_helpmenu.h"
#include "fft/event_option.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

void menu_open_option_game_options(void) {
    menu_set_modal_open();
    menu_request_open_companion_executable(COMPANION_OVERLAY_OPTION);
    option_entrypoint(OPTION_MENU_GAME_OPTIONS);
}

/* Thread entry of menu entry 0x1d: load OPTION.OUT and open its AT list. */
void menu_open_option_at_list(void) {
    menu_set_modal_open();
    menu_request_open_companion_executable(COMPANION_OVERLAY_OPTION);
    option_entrypoint(OPTION_MENU_AT_LIST);
}

/* Thread started for system function 0x1f (crystal or treasure): load OPTION.OUT
 * and open its dead-unit panel. */
void menu_open_option_dead_unit_panel(void) {
    menu_set_modal_open();
    menu_request_open_companion_executable(COMPANION_OVERLAY_OPTION);
    option_entrypoint(OPTION_MENU_DEAD_UNIT);
}

/* Wait for the camera and menu transition, then run Triangle-menu selection. */
void menu_run_triangle_selection(void) {
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
    while (camera_is_active() != 0 || g_menu_transition_state != 0) {
        thread_yield();
    }
    script_pulse_tutorial_wait_value(0xFE);
    g_menu_hide_numeric_values = 0;
    menu_run_icon_selection_loop();
}

/* Run BUNIT, release its high-overlay memory, then restore the unit view. */
void menu_run_bunit_return_thread(void) {
    s32 i;
    s32 unused[2]; /* keeps the original 0x28 frame */

    g_menu_transition_state = 4;
    do {
        thread_yield();
    } while (thread_is_running_asm_variant(THREAD_SLOT_MENU_GRANDCHILD) != 0);

    g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_BUNIT;
    g_menu_help_open = 0;
    do {
        thread_wait_frames(1);
    } while (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);

    menu_free_high_overlay();
    for (i = 2; i < 15; i++) {
        thread_set_parameters(i, 0, 0, 1);
    }
    for (i = 0; i < 0x100; i += 8) {
        g_gfx_screen_fade = 0xFF - i;
        thread_yield();
    }
    menu_clear_modal_open();
    g_menu_overlay_state = 0;
    g_gfx_screen_fade = 0;
    g_menu_transition_state = 0;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_HOVERED_UNIT, g_menu_selected_unit.battle_id, 0xFF);
    thread_exit_current();
}

/* Fade out and load BUNIT, then start its return thread. */
void menu_run_bunit_transition_thread(void) {
    s32 level;

    for (level = 0; level < 0x100; level += 0x10) {
        g_gfx_screen_fade = level;
        thread_yield();
    }
    g_gfx_screen_fade = 0xFF;
    g_menu_overlay_state = 1;
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_UNIT_SUMMARY_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_RIGHT_UNIT_STATS, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_HOVERED_UNIT_STATS, 0, 0, 1);
    thread_wait_for_unit_view_panels();
    menu_request_open_companion_executable(COMPANION_OVERLAY_BUNIT);
    menu_reserve_high_overlay(0x20000);
    menu_request_open_companion_executable(COMPANION_OVERLAY_BUNIT_SECOND_HALF);
    thread_wait_frames(2);
    g_menu_help_open = 1;
    thread_start(THREAD_BORROWED_SLOT_FADE_OUT, menu_run_bunit_return_thread);
    thread_exit_current();
}

/* Start the unit-list transition in a child slot and wait for it to finish. */
void menu_open_bunit(void) {
    menu_set_modal_open();
    thread_start(g_thread_current_id - 1, menu_run_bunit_transition_thread);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_exit_current();
}

/* Reserve and load the help overlay, then run its battle-help screen. */
void menu_run_help_menu_thread(void) {
    if (thread_find_running_by_task(THREAD_TASK_UNIT_EDITOR_PANEL) != 0
        && thread_find_running_by_task(THREAD_TASK_MENU_WINDOW_BUILD) == 0) {
        sound_queue_sfx_confirm();
    }
    menu_reserve_high_overlay(0x20000);
    menu_request_open_companion_executable(COMPANION_OVERLAY_HELPMENU);
    menu_run_battle_help();
}

/* Run the job-status overlay while its state is nonzero. */
void menu_run_jobstts_thread(void) {
    if (thread_is_running_asm_variant(THREAD_SLOT_MENU_BUILDING) != 0) {
        thread_exit_current();
    }
    if (thread_is_running_asm_variant(THREAD_SLOT_EVENT) != 0) {
        thread_exit_current();
    }
    menu_set_modal_open();
    sound_queue_sfx_confirm();
    menu_reserve_high_overlay(0x20000);
    menu_request_open_companion_executable(COMPANION_OVERLAY_JOBSTTS);
    thread_wait_frames(2);
    g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_JOBSTTS;
    do {
        g_script_event_input = 0;
        thread_yield();
    } while (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    g_script_event_input = 0;
    menu_free_high_overlay();
    thread_wait_frames(2);
    menu_clear_modal_open();
    thread_exit_current();
}

/* Run the EQUIP overlays, then restore the menu threads and icon-list cursors. */
void menu_run_equip_thread(void) {
    s32 saved;
    s32 i;
    s32 j;

    menu_set_modal_open();
    saved = g_menu_unit_view_mode;
    menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
    g_menu_overlay_state = 1;
    thread_wait_for_unit_view_panels();
    menu_request_open_companion_executable(COMPANION_OVERLAY_EQUIP);
    menu_reserve_high_overlay(0x20000);
    menu_request_open_companion_executable(COMPANION_OVERLAY_EQUIP_SECOND_HALF);
    thread_suspend(THREAD_SLOT_MENU);
    thread_suspend(THREAD_SLOT_MENU_CHILD);
    g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_EQUIP;
    do {
        g_script_event_input = 0;
        thread_yield();
    } while (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    g_menu_thread_menu_data = g_menu_idle_action_entries;
    g_menu_action_slot_row_actions[0] = -3;
    g_sound_suppressed = 1;
    g_script_event_input |= PSX_PAD_CIRCLE;
    if (g_menu_companion_overlay_primitives == 0) {
        thread_resume(THREAD_SLOT_MENU);
        thread_resume(THREAD_SLOT_MENU_CHILD);
    }
    for (i = 9; i < 15; i++) {
        thread_set_parameters(i, 0, 0, 1);
    }
    thread_wait_frames(4);
    if (g_menu_companion_overlay_primitives != 0) {
        for (j = 0; j < 2; j++) {
            g_menu_pending_selection[j]
                = ((menu_entry_t*)g_thread_contexts[8 - j].function_parameter_1)->selected_index;
        }
    }
    menu_free_high_overlay();
    g_menu_overlay_state = 0;
    menu_start_unit_view_thread(saved, 0xFF, 0xFF);
    g_sound_suppressed = 0;
    g_script_event_input = 0;
    menu_clear_modal_open();
    thread_exit_current();
}

/* Open the unit editor for a rider or mount, then restore its menu view. */
void menu_open_debugchr_unit_editor(void) {
    s16* unit_index = &g_menu_selected_unit.battle_id;

    if (battle_unit_get_stats_from_battle_id(*unit_index)->mount_info != 0) {
        menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
        menu_set_modal_open();
        g_menu_overlay_state = 1;
        menu_request_open_companion_executable(COMPANION_OVERLAY_DEBUGCHR);
        thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
        thread_wait_until_inactive(THREAD_SLOT_MENU);
        thread_wait_for_unit_view_panels();
        menu_record_selection_for_selected_unit();
        battle_menu_enter_status_screen_selection();
        debugchr_editor_run_unit_thread(*unit_index);
        g_menu_overlay_state = 0;
        g_sound_suppressed = 0;
        g_script_event_input = 0;
        menu_build_idle_action_menu_variant(g_menu_current_id);
        menu_start_unit_view_thread(UNIT_VIEW_MODE_UNIT_SUMMARY, *unit_index, *unit_index);
        menu_clear_modal_open();
        battle_menu_leave_status_screen_selection();
        thread_exit_current();
    }
}

/* Close the panels, run the requested debug session, then restore menu state. */
void menu_open_debugchr(void) {
    menu_start_unit_view_thread(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
    menu_set_modal_open();
    g_menu_overlay_state = 1;
    menu_request_open_companion_executable(COMPANION_OVERLAY_DEBUGCHR);
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    thread_wait_for_unit_view_panels();
    thread_yield();
    thread_yield();
    debugchr_editor_run_session();
    g_menu_overlay_state = 0;
    g_sound_suppressed = 0;
    g_script_event_input = 0;
    menu_clear_modal_open();
    thread_exit_current();
}

void script_show_graphic(void) {
    thread_set_current_task_id(THREAD_TASK_SHOW_GRAPHIC);
    menu_request_open_companion_executable(COMPANION_OVERLAY_ETC);
    etc_graphic_show_async();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_show_graphic and
 * menu_load_portrait_image. Nothing on the disc calls it or points at it; the
 * name is a neutral placeholder until a reference turns up. */
void script_noop_3(void) {
}

/* Load portrait sector 0x164b + 4 * portrait_id straight into VRAM. */
void menu_load_portrait_image(s32 portrait_id) {
    menu_portrait_image_file_t* buffer;

    buffer = menu_alloc_memory(sizeof(*buffer));
    g_thread_call_target = (void (*)(void))main_file_load_checked_to_address;
    thread_call_on_main_stack(portrait_id * 4 + DISC_SECTOR_EVENT_EVTFACE_BIN, sizeof(*buffer), buffer);
    LoadImage(&g_menu_portrait_pixel_rect, buffer->pixels[0][0]);
    LoadImage(&g_menu_portrait_palette_rect, (u32*)buffer->palettes);
    menu_free_memory(buffer);
}

/*
 * Loads one 0x7800-byte block of EVTCHR.BIN into a fresh allocation on the main
 * stack, publishing it through g_battle_event_pending_loaded_evtchr_slot / g_battle_event_loaded_evtchr_buffer before
 * releasing it. Parameter 2 selects the block; each occupies 15 sectors from 0x1d4c.
 */
void gfx_load_evtchr_thread(void) {
    s32 parameter;
    s32 block;
    evtchr_file_record_t* buffer;

    parameter = thread_get_current_parameter_1_variant();
    block = thread_get_current_parameter_2_variant();
    thread_set_current_task_id(THREAD_TASK_LOAD_EVTCHR);
    buffer = main_heap_alloc(0x7800);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (thread_call_on_main_stack((block * 15) + DISC_SECTOR_EVENT_EVTCHR_BIN, 0x7800, buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);
    g_script_pending_loaded_evtchr_slot = parameter;
    g_script_loaded_evtchr_buffer = buffer;
    thread_yield();
    main_heap_free(buffer);
    thread_exit_current();
}

/* Uploads eight 32x48 portraits and their palettes from one EVTFACE.BIN block. */
void script_load_portrait_colors_event_instruction(s32 portrait_id) {
    RECT rect;
    menu_portrait_image_file_t* buffer;
    u32(*pixels)[48][4];
    s32 i;
    palette_t* palette;

    buffer = menu_alloc_memory(sizeof(*buffer));
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (thread_call_on_main_stack(portrait_id * 4 + DISC_SECTOR_EVENT_EVTFACE_BIN, sizeof(*buffer), buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = main_file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);

    palette = buffer->palettes;
    mem_copy_bytes_nonempty(&rect, &g_menu_portrait_pixel_rect, sizeof(rect));
    i = 0;
    pixels = buffer->pixels;
    rect.w = 8;
    rect.h = 0x30;
    do {
        LoadImage(&rect, pixels[0][0]);
        rect.x += 8;
        pixels += 1;
        i += 1;
    } while (i < 8);
    LoadImage(&g_menu_portrait_palette_rect, (u32*)palette);
    menu_free_memory(buffer);
    thread_yield();
}

/* Read event block `id` (EVENT_BLOCK_SECTORS sectors from the event archive) into
 * the script event block, yielding until the load finishes, then turn the block's
 * text offset into the event-message section pointer.
 *
 * The 32-bit offset is then overwritten with EVENT_TEXT_OFFSET_PROCESSED: the
 * interpreter starts at byte zero of the block and reads those four bytes as
 * one-byte NOP opcodes. A block whose first 32-bit value already holds that marker leaves
 * the section pointer alone. Both the read request and the load poll run on the
 * main stack. */
void script_load_event(s32 id) {
    event_file_block_t* block;
    u32 off;

    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
    } while (thread_call_on_main_stack(
                 id * EVENT_BLOCK_SECTORS + DISC_SECTOR_EVENT_TEST_EVT, EVENT_BLOCK_BYTES, g_main_script_event_block)
        != 0);

    do {
        thread_yield();
        g_thread_call_target = main_file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);

    block = g_script_event_block;
    off = block->text_offset_or_marker;
    if (off != EVENT_TEXT_OFFSET_PROCESSED) {
        g_text_section_pointers[TEXT_SECTION_EVENT_MESSAGES] = (u8*)(off + (u32)block);
        block->text_offset_or_marker = EVENT_TEXT_OFFSET_PROCESSED;
    }
}

void menu_set_palette_colors(const menu_palette_pair_t* source) {
    rgba5551_t* destination;

    destination = g_menu_palette_colors.colors;
    mem_copy_bytes_nonempty(destination, source->colors, sizeof(palette_t));
    mem_copy_bytes_nonempty(destination + PALETTE_COLOR_COUNT, source->colors + PALETTE_COLOR_COUNT, sizeof(palette_t));
}

void menu_init_palette_and_text(s32 unused_0) {
    menu_set_palette_colors(&g_menu_default_palette_colors);
    text_init_section_pointers_variant();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_init_palette_and_text and menu_open_companion_data_file. Nothing on the
 * disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void menu_noop_4(void) {
}

/* Non-blocking two-step file load driven by g_thread_call_target:
 * issue the read for the pending request, then poll until it completes and
 * clear the request. */
void menu_open_companion_data_file(void) {
    if ((g_menu_companion_data_file_request_index != 0) && (g_menu_companion_data_file_loading == 0)) {
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (thread_call_on_main_stack(g_menu_companion_data_file_lbas[g_menu_companion_data_file_request_index],
                g_menu_companion_data_file_sizes[g_menu_companion_data_file_request_index],
                g_menu_companion_data_file_buffer)
            == 0) {
            g_menu_companion_data_file_loading = 1;
        }
    } else if ((g_menu_companion_data_file_request_index != 0) && (g_menu_companion_data_file_loading != 0)) {
        g_thread_call_target = main_file_is_still_loading;
        if (thread_call_on_main_stack() == 0) {
            g_menu_companion_data_file_loading = 0;
            g_menu_companion_data_file_request_index = 0;
        }
    }
}

/* Per-frame driver of menu_request_open_companion_executable: start the
 * disc read of the requested overlay on the main stack, at its offset from the
 * companion overlay load address, then poll until the read finishes and clear the
 * request. */
void menu_open_companion_executable(void) {
    s32 idx;

    idx = g_menu_companion_executable_request_index;
    if ((idx != 0) && (g_menu_companion_executable_loading == 0)) {
        g_thread_call_target = (void (*)(void))main_file_call_build_header;
        if (thread_call_on_main_stack(g_menu_companion_executable_lbas[idx], g_menu_companion_executable_sizes[idx],
                g_menu_companion_executable_offsets[idx] + (s32)g_main_heap_companion_overlay_load_address)
            == 0) {
            g_menu_companion_executable_loading = 1;
        }
    } else if ((g_menu_companion_executable_request_index != 0) && (g_menu_companion_executable_loading != 0)) {
        g_thread_call_target = main_file_is_still_loading;
        if (thread_call_on_main_stack() == 0) {
            g_menu_companion_executable_loading = 0;
            g_menu_companion_executable_request_index = 0;
        }
    }
}

void menu_reset_unit_records_wrapper(void) {
    menu_reset_unit_records();
}

/* Empty: the overlay entry of COMPANION_OVERLAY_STATE_STUB. */
void menu_run_companion_overlay_stub(void) {
}

void menu_start_entry_thread(void) {
    s32 menu_id = thread_get_current_parameter_1_variant();
    s32 count;

    thread_wait_until_inactive(THREAD_SLOT_MENU);
    if (g_menu_overlay_state != 0) {
        thread_exit_current();
    }
    camera_wait_until_idle();
    while (thread_find_running_by_task(THREAD_TASK_ATTACK_TARGET_STATS) != 0) {
        thread_yield();
    }
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        if (script_is_tutorial_event_slot_variant() != 0) {
            g_menu_input_mode = MENU_INPUT_TUTORIAL;
            script_set_variable(
                EVENT_SCRIPT_VAR_CURRENT_EVENT, script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) + 1);
            script_start_current_event_thread();
            thread_wait_frames(0x1E);
        }
        if (g_menu_input_mode != MENU_INPUT_PLAYER) {
            thread_wait_frames(2);
        }
    } else {
        thread_wait_frames(2);
    }
    script_pulse_tutorial_wait_value(0xFF);
    if (menu_id == BATTLE_MENU_ID_OTHER_UNIT || menu_id == BATTLE_MENU_ID_OTHER_UNIT_INSPECT) {
        if (menu_get_selected_unit_stats()->unit_flags & UNIT_FLAG_EGG) {
            menu_id = BATTLE_MENU_ID_EGG;
        }
    }
    thread_start(THREAD_SLOT_MENU, g_menu_thread_menu_data[menu_id].thread_entry);
    thread_set_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[menu_id], 0, 0);
    count = g_menu_restore_depth;
    g_script_event_input = 0;
    if (g_menu_restore_pending != 0 && g_menu_restore_unit_id == g_menu_selected_unit.battle_id) {
        g_menu_help_open = 1;
        thread_wait_frames(2);
        if (count > 0) {
            thread_start(THREAD_SLOT_MENU_CHILD, menu_run_skillset_thread);
            thread_set_parameters(
                THREAD_SLOT_MENU_CHILD, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_SKILLSET_LIST], 0, 0);
        }
        thread_wait_frames(2);
        if (count >= 2) {
            thread_start(THREAD_SLOT_MENU_GRANDCHILD, menu_select_unit_action_slots_thread);
            thread_set_parameters(
                THREAD_SLOT_MENU_GRANDCHILD, (s32)&g_menu_thread_menu_data[BATTLE_MENU_ID_ABILITY_LIST], 0, 0);
        }
        thread_wait_frames(2);
        g_menu_restore_pending = 0;
        g_menu_help_open = 0;
    } else {
        g_menu_restore_depth = 0;
        g_menu_restore_unit_id = g_menu_selected_unit.battle_id;
    }
    thread_exit_current();
}
