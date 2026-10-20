/* Load companion overlays and launch their native menus, unit views and spell quotes. */

#include "fft/after_battle.h"
#include "fft/at_list.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/crystal.h"
#include "fft/equipment.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/message.h"
#include "fft/option.h"
#include "fft/overlay.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/status_screen.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_editor.h"
#include "fft/unit_event.h"
#include "fft/unit_list.h"
#include "fft/unit_panel.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

extern s32 g_overlay_companion_data_file_request_index;
extern u8* g_overlay_companion_data_file_buffer;

/* What every caller reserves at the high overlay address: room for the second half of BUNIT or
 * EQUIP, or for HELPMENU or JOBSTTS. */
enum { MENU_HIGH_OVERLAY_SIZE = 0x20000 };

/* Reserve size bytes of the main heap at the high overlay load address, yielding until that block
 * is free (with a wait message when the debug font is on). Every caller reserves
 * MENU_HIGH_OVERLAY_SIZE. */
static void high_overlay_reserve(u32 size) {
    while (1) {
        if (heap_reserve_at(g_heap_high_overlay_load_address, size) == g_heap_high_overlay_load_address) {
            break;
        }
        thread_yield();
        if (g_debug_font_print_enabled != 0) {
            FntPrint("WAIT FOR FILEALLOCATION\n");
        }
    }
}

/* Free the block high_overlay_reserve reserved. */
static void high_overlay_free(void) {
    heap_free(g_heap_high_overlay_load_address);
}

/* Load companion overlay request_index and wait for it: wait until no executable or data-file
 * request is pending, publish this one, and yield until the per-frame driver
 * (overlay_companion_poll) clears it. The calling thread exits instead of returning
 * when the frame result override is 1. */
void overlay_companion_request(s32 request_index) {
    while (g_overlay_companion_executable_request_index != 0 || g_overlay_companion_data_file_request_index != 0) {
        thread_yield();
    }
    g_overlay_companion_executable_request_index = request_index;
    if (request_index != 0) {
        do {
            thread_yield();
        } while (g_overlay_companion_executable_request_index != 0);
    }
    if (g_script_frame_result_override == 1) {
        thread_current_exit();
    }
}

/* The data-file counterpart of overlay_companion_request, driven by
 * overlay_companion_file_poll into g_overlay_companion_data_file_buffer. */
static void companion_file_request(s32 request_index) {
    while (g_overlay_companion_executable_request_index != 0 || g_overlay_companion_data_file_request_index != 0) {
        thread_yield();
    }
    g_overlay_companion_data_file_request_index = request_index;
    if (request_index != 0) {
        do {
            thread_yield();
        } while (g_overlay_companion_data_file_request_index != 0);
    }
    if (g_script_frame_result_override == 1) {
        thread_current_exit();
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * companion_file_request and
 * script_conditions_display. Nothing on the disc calls it
 * or points at it; the name is a neutral placeholder until a reference turns
 * up. */
void script_noop_1(void) {
}

/* Thread body of DisplayConditions: draw numeric values again and run REQUIRE's
 * condition dispatcher, which takes the display id and its operand from the
 * thread parameters. */
void script_conditions_display(void) {
    g_unit_panel_hide_numeric_values = 0;
    require_conditions_entrypoint();
}

/* The thread EVENT_OPCODE_DARK_SCREEN starts: it takes the task id, and in
 * BATTLE loads REQUIRE.OUT and runs its bonus image burst (WORLD has no
 * REQUIRE.OUT to run). */
void script_dark_screen_thread(void) {
    thread_current_task_set(THREAD_TASK_DARK_SCREEN);
#ifdef FFT_BATTLE
    overlay_companion_request(COMPANION_OVERLAY_REQUIRE);
    require_dark_screen_entrypoint();
#endif
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_dark_screen_thread and announcement_spell_quote_thread. Nothing on the
 * disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void script_noop_2(void) {
}

/* Task 0x46: show message 0xa800 | parameter 2 about the unit whose battle id
 * is parameter 1.
 *
 * The id also fills the three text substitution values. The text thread runs
 * with window mode 0x11, or 0x12 when the unit is on the upper half of the
 * screen, from SPELL.MES (companion data file 4) loaded into a 16 KiB
 * buffer. WORLD has no unit messages: its copy ends at once. */
void announcement_spell_quote_thread(void) {
#ifdef FFT_BATTLE
    SVECTOR screen_coords;
    unit_stats_t* unit;
    s32 id; /* Battle id, then the unit's unit_id: one variable in the target. */
    s32 window_mode;
    s32 message_id;

    g_menu_request_busy = 1;
    thread_current_task_set(THREAD_TASK_UNIT_MESSAGE);
    script_event_speed_set(2);
    thread_parameters_set(THREAD_SLOT_MENU, 0, 0, 1);
    thread_inactive_wait(THREAD_SLOT_MENU);
    thread_frames_wait(3);
    window_mode = 0x11;
    id = thread_current_parameter_1_get();
    g_text_substitution_values[0] = id;
    g_text_substitution_values[1] = id;
    g_text_substitution_values[2] = id;
    unit = unit_stats_get(id);
    unit_screen_position_project(unit_battle_runtime_id_get(id), &screen_coords);
    id = unit->unit_id;
    if (screen_coords.vy < 0x80) {
        window_mode = 0x12;
    }
    message_id = thread_current_parameter_2_get() | 0xA800;
    g_overlay_companion_data_file_buffer = heap_alloc(0x4000);
    companion_file_request(COMPANION_FILE_SPELL_MES);
    g_text_section_pointers[TEXT_SECTION_UNIT_MESSAGE] = g_overlay_companion_data_file_buffer;
    thread_start(THREAD_SLOT_MENU, message_dialog_thread);
    thread_parameters_set(THREAD_SLOT_MENU, window_mode, message_id, id);
    thread_inactive_wait(THREAD_SLOT_MENU);
    heap_free(g_overlay_companion_data_file_buffer);
    g_command_pending_selection[0] = 7;
    script_event_speed_set(1);
    g_menu_request_busy = 0;
    thread_current_exit();
#endif
}

/* Thread entry: load OPTION.OUT and open its game options menu. */
void option_open(void) {
    menu_modal_open();
    overlay_companion_request(COMPANION_OVERLAY_OPTION);
    option_entrypoint(MENU_OPTION_TYPE_GAME_OPTIONS);
}

/* Thread entry of menu entry 0x1d: load OPTION.OUT and open its AT list. */
void at_list_open(void) {
    menu_modal_open();
    overlay_companion_request(COMPANION_OVERLAY_OPTION);
    option_entrypoint(MENU_OPTION_TYPE_AT_LIST);
}

/* Thread started for system function 0x1f (crystal or treasure): load OPTION.OUT
 * and open its crystal pickup panel. */
void crystal_pickup_panel_open(void) {
    menu_modal_open();
    overlay_companion_request(COMPANION_OVERLAY_OPTION);
    option_entrypoint(MENU_OPTION_TYPE_CRYSTAL_PICKUP);
}

/* Wait for the camera and menu transition, then run Triangle-menu selection. */
void mini_menu_select(void) {
    thread_current_task_set(THREAD_TASK_MINI_MENU);
    while (camera_is_active() != 0 || g_menu_transition_state != 0) {
        thread_yield();
    }
    tutorial_pulse_wait_value(SCRIPT_TUTORIAL_WAIT_MINI_MENU);
    g_unit_panel_hide_numeric_values = 0;
    menu_run_icon_selection_loop();
}

/* Run BUNIT, release its high-overlay memory, then restore the unit view. */
static void unit_list_return_thread(void) {
    s32 i;
    s32 unused[2]; /* keeps the original 0x28 frame */

    g_menu_transition_state = 4;
    do {
        thread_yield();
    } while (thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0);

    g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_BUNIT;
    g_help_open = 0;
    do {
        thread_frames_wait(1);
    } while (g_overlay_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);

    high_overlay_free();
    /* Parameter 3 = 1 asks the threads in slots 2-14 to close; then fade the screen back in. */
    for (i = 2; i < 15; i++) {
        thread_parameters_set(i, 0, 0, 1);
    }
    for (i = 0; i < 0x100; i += 8) {
        g_gfx_screen_fade = 0xFF - i;
        thread_yield();
    }
    menu_modal_close();
    g_menu_overlay_state = 0;
    g_gfx_screen_fade = 0;
    g_menu_transition_state = 0;
    unit_view_start(UNIT_VIEW_MODE_HOVERED_UNIT, g_unit_panel_selected_unit.battle_id, 0xFF);
    thread_current_exit();
}

/* Fade the screen out, close the menu and unit view panels, load both halves of BUNIT, then
 * start unit_list_return_thread, which runs the unit list and comes back. */
static void unit_list_transition_thread(void) {
    s32 level;

    for (level = 0; level < 0x100; level += 0x10) {
        g_gfx_screen_fade = level;
        thread_yield();
    }
    g_gfx_screen_fade = 0xFF;
    g_menu_overlay_state = 1;
    thread_parameters_set(THREAD_SLOT_MENU, 0, 0, 1);
    thread_parameters_set(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 1);
    thread_parameters_set(THREAD_SLOT_UNIT_SUMMARY_PANEL, 0, 0, 1);
    thread_parameters_set(THREAD_SLOT_RIGHT_UNIT_STATS, 0, 0, 1);
    thread_parameters_set(THREAD_SLOT_HOVERED_UNIT_STATS, 0, 0, 1);
    unit_view_panels_wait();
    overlay_companion_request(COMPANION_OVERLAY_BUNIT);
    high_overlay_reserve(MENU_HIGH_OVERLAY_SIZE);
    overlay_companion_request(COMPANION_OVERLAY_BUNIT_SECOND_HALF);
    thread_frames_wait(2);
    g_help_open = 1;
    thread_start(THREAD_BORROWED_SLOT_FADE_OUT, unit_list_return_thread);
    thread_current_exit();
}

/* Start the unit-list transition in a child slot and wait for it to finish. */
void unit_list_open(void) {
    menu_modal_open();
    thread_start(g_thread_current_id - 1, unit_list_transition_thread);
    thread_inactive_wait(g_thread_current_id - 1);
    thread_current_exit();
}

/* Reserve and load the help overlay, then run its battle-help screen. */
void help_menu_thread(void) {
    if (thread_task_find(THREAD_TASK_STATUS_SCREEN) != 0 && thread_task_find(THREAD_TASK_MENU_WINDOW_BUILD) == 0) {
        sound_confirm_queue();
    }
    high_overlay_reserve(MENU_HIGH_OVERLAY_SIZE);
    overlay_companion_request(COMPANION_OVERLAY_HELPMENU);
    helpmenu_battle_entrypoint();
}

/* Run the job-status overlay while its state is nonzero. */
void status_screen_jobs_thread(void) {
    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        thread_current_exit();
    }
    if (thread_is_running_asm(THREAD_SLOT_EVENT) != 0) {
        thread_current_exit();
    }
    menu_modal_open();
    sound_confirm_queue();
    high_overlay_reserve(MENU_HIGH_OVERLAY_SIZE);
    overlay_companion_request(COMPANION_OVERLAY_JOBSTTS);
    thread_frames_wait(2);
    g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_JOBSTTS;
    do {
        g_script_event_input = 0;
        thread_yield();
    } while (g_overlay_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    g_script_event_input = 0;
    high_overlay_free();
    thread_frames_wait(2);
    menu_modal_close();
    thread_current_exit();
}

/* Run the EQUIP overlays with the menu threads suspended, then reopen the idle action menu
 * with Circle pressed for the player (sounds muted) and close the panels in slots 9-14. When
 * EQUIP left its primitives up, the menu and skillset list stay suspended and their selected
 * rows are kept as the pending selection instead. */
void equipment_thread(void) {
    s32 saved_view_mode;
    s32 i;
    s32 j;

    menu_modal_open();
    saved_view_mode = g_unit_view_mode;
    unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
    g_menu_overlay_state = 1;
    unit_view_panels_wait();
    overlay_companion_request(COMPANION_OVERLAY_EQUIP);
    high_overlay_reserve(MENU_HIGH_OVERLAY_SIZE);
    overlay_companion_request(COMPANION_OVERLAY_EQUIP_SECOND_HALF);
    thread_suspend(THREAD_SLOT_MENU);
    thread_suspend(THREAD_SLOT_MENU_CHILD);
    g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_EQUIP;
    do {
        g_script_event_input = 0;
        thread_yield();
    } while (g_overlay_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    g_menu_thread_menu_data = g_command_idle_action_entries;
    g_command_action_slot_row_actions[0] = -3;
    g_sound_suppressed = 1;
    g_script_event_input |= PSX_PAD_CIRCLE;
    if (g_overlay_companion_overlay_primitives == 0) {
        thread_resume(THREAD_SLOT_MENU);
        thread_resume(THREAD_SLOT_MENU_CHILD);
    }
    for (i = 9; i < 15; i++) {
        thread_parameters_set(i, 0, 0, 1);
    }
    thread_frames_wait(4);
    if (g_overlay_companion_overlay_primitives != 0) {
        for (j = 0; j < 2; j++) {
            g_command_pending_selection[j]
                = ((menu_entry_t*)g_thread_contexts[THREAD_SLOT_MENU - j].function_parameter_1)->selected_index;
        }
    }
    high_overlay_free();
    g_menu_overlay_state = 0;
    unit_view_start(saved_view_mode, 0xFF, 0xFF);
    g_sound_suppressed = 0;
    g_script_event_input = 0;
    menu_modal_close();
    thread_current_exit();
}

/* Show DEBUGCHR's read-only status screen for the selected unit when it is a rider or a mount, then rebuild
 * the idle action menu and the unit summary for it. */
void status_screen_mounted_unit_open(void) {
    s16* unit_index = &g_unit_panel_selected_unit.battle_id;

    if (unit_stats_get(*unit_index)->mount_info != 0) {
        unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
        menu_modal_open();
        g_menu_overlay_state = 1;
        overlay_companion_request(COMPANION_OVERLAY_DEBUGCHR);
        thread_parameters_set(THREAD_SLOT_MENU, 0, 0, 1);
        thread_inactive_wait(THREAD_SLOT_MENU);
        unit_view_panels_wait();
        command_record_unit_selection();
        menu_enter_status_screen_selection();
        debugchr_mounted_unit_entrypoint(*unit_index);
        g_menu_overlay_state = 0;
        g_sound_suppressed = 0;
        g_script_event_input = 0;
        command_menu_open(g_command_current_id);
        unit_view_start(UNIT_VIEW_MODE_UNIT_SUMMARY, *unit_index, *unit_index);
        menu_modal_close();
        menu_leave_status_screen_selection();
        thread_current_exit();
    }
}

/* Close the panels, run the requested debug session, then restore menu state. */
void debug_unit_editor_open(void) {
    unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
    menu_modal_open();
    g_menu_overlay_state = 1;
    overlay_companion_request(COMPANION_OVERLAY_DEBUGCHR);
    thread_parameters_set(THREAD_SLOT_MENU, 0, 0, 1);
    thread_inactive_wait(THREAD_SLOT_MENU);
    unit_view_panels_wait();
    thread_yield();
    thread_yield();
    debugchr_editor_entrypoint();
    g_menu_overlay_state = 0;
    g_sound_suppressed = 0;
    g_script_event_input = 0;
    menu_modal_close();
    thread_current_exit();
}
