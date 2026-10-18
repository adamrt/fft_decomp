/* Companion overlay loading and menu launchers, event image and script loading, and menu palette setup. */

#include "fft/after_battle.h"
#include "fft/at_list.h"
#include "fft/battle_map.h"
#include "fft/camera.h"
#include "fft/command.h"
#include "fft/crystal.h"
#include "fft/deploy.h"
#include "fft/equipment.h"
#include "fft/file.h"
#include "fft/fullscreen_graphic.h"
#include "fft/gfx.h"
#include "fft/gfx_image.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/message.h"
#include "fft/option.h"
#include "fft/overlay.h"
#include "fft/portrait.h"
#include "fft/script.h"
#include "fft/script_instruction.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/status.h"
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

/* Overlay load bookkeeping and disc tables. */
extern s32 g_overlay_companion_executable_loading;
extern s32 g_overlay_companion_executable_lbas[];
extern s32 g_overlay_companion_executable_sizes[];
extern s32 g_overlay_companion_executable_offsets[];
extern s32 g_overlay_companion_data_file_request_index;
extern s32 g_overlay_companion_data_file_loading;
extern s32 g_overlay_companion_data_file_lbas[];
extern s32 g_overlay_companion_data_file_sizes[];
extern u8* g_overlay_companion_data_file_buffer;

/* Menu threads and panel rendering. */
extern char g_menu_wait_for_file_allocation_message[];
extern s32 g_menu_building_thread_started;
extern u8 g_menu_idle_action_window_x[];
extern u8 g_command_restore_unit_id;

/* One EVTFACE.BIN block: pixels followed by eight 16-color palettes. */
typedef struct portrait_image_file {
    u32 pixels[8][48][4];  /* 0x0000; eight 32x48 4bpp portraits */
    palette_t palettes[8]; /* 0x1800; four palettes per uploaded VRAM row */
    u8 _unused_1900[0x700];
} portrait_image_file_t;
typedef char portrait_image_file_size_must_be_8192[sizeof(portrait_image_file_t) == 8192 ? 1 : -1];

/* The two programs reserve different menu image columns for EVTFACE. */
#ifdef FFT_BATTLE
static RECT g_menu_portrait_pixel_rect = { 448, 206, 64, 48 };
static RECT g_menu_portrait_palette_rect = { 448, 254, 64, 2 };
#else
static RECT g_menu_portrait_pixel_rect = { 320, 206, 64, 48 };
static RECT g_menu_portrait_palette_rect = { 320, 254, 64, 2 };
#endif

/* What every caller reserves at the high overlay address: room for the second half of BUNIT or
 * EQUIP, or for HELPMENU or JOBSTTS. */
enum { MENU_HIGH_OVERLAY_SIZE = 0x20000 };

/* Reserve size bytes of the main heap at the high overlay load address, yielding until that block
 * is free (with a wait message when the debug font is on). Every caller reserves
 * MENU_HIGH_OVERLAY_SIZE. */
static void overlay_reserve_high(u32 size) {
    while (1) {
        if (heap_reserve_at(g_heap_high_overlay_load_address, size) == g_heap_high_overlay_load_address) {
            break;
        }
        thread_yield();
        if (g_debug_font_print_enabled != 0) {
            FntPrint(g_menu_wait_for_file_allocation_message);
        }
    }
}

/* Free the block overlay_reserve_high reserved. */
static void overlay_free_high(void) {
    heap_free(g_heap_high_overlay_load_address);
}

/* Load companion overlay request_index and wait for it: wait until no executable or data-file
 * request is pending, publish this one, and yield until the per-frame driver
 * (overlay_poll_companion) clears it. The calling thread exits instead of returning
 * when the frame result override is 1. */
void overlay_request_companion(s32 request_index) {
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
        thread_exit_current();
    }
}

/* The data-file counterpart of overlay_request_companion, driven by
 * overlay_poll_companion_file into g_overlay_companion_data_file_buffer. */
static void overlay_request_companion_file(s32 request_index) {
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
        thread_exit_current();
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * overlay_request_companion_file and
 * script_execute_display_conditions_instruction. Nothing on the disc calls it
 * or points at it; the name is a neutral placeholder until a reference turns
 * up. */
void script_noop_1(void) {
}

/* Thread body of DisplayConditions: draw numeric values again and run REQUIRE's
 * condition dispatcher, which takes the display id and its operand from the
 * thread parameters. */
void script_execute_display_conditions_instruction(void) {
    g_unit_panel_hide_numeric_values = 0;
    require_conditions_entrypoint();
}

/* The thread EVENT_OPCODE_DARK_SCREEN starts: it takes the task id, and in
 * BATTLE loads REQUIRE.OUT and runs its bonus image burst (WORLD has no
 * REQUIRE.OUT to run). */
void script_dark_screen_thread(void) {
    thread_set_current_task_id(THREAD_TASK_DARK_SCREEN);
#ifdef FFT_BATTLE
    overlay_request_companion(COMPANION_OVERLAY_REQUIRE);
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

    g_menu_system_function_thread_busy = 1;
    thread_set_current_task_id(THREAD_TASK_UNIT_MESSAGE);
    script_set_event_speed(2);
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    thread_wait_frames(3);
    window_mode = 0x11;
    id = thread_get_current_parameter_1();
    g_text_substitution_values[0] = id;
    g_text_substitution_values[1] = id;
    g_text_substitution_values[2] = id;
    unit = unit_get_stats(id);
    unit_project_to_screen(unit_get_runtime_id_by_battle_id(id), &screen_coords);
    id = unit->unit_id;
    if (screen_coords.vy < 0x80) {
        window_mode = 0x12;
    }
    message_id = thread_get_current_parameter_2() | 0xA800;
    g_overlay_companion_data_file_buffer = heap_alloc(0x4000);
    overlay_request_companion_file(COMPANION_FILE_SPELL_MES);
    g_text_section_pointers[TEXT_SECTION_UNIT_MESSAGE] = g_overlay_companion_data_file_buffer;
    thread_start(THREAD_SLOT_MENU, message_dialog_thread);
    thread_set_parameters(THREAD_SLOT_MENU, window_mode, message_id, id);
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    heap_free(g_overlay_companion_data_file_buffer);
    g_command_pending_selection[0] = 7;
    script_set_event_speed(1);
    g_menu_system_function_thread_busy = 0;
    thread_exit_current();
#endif
}

/* Thread entry: load OPTION.OUT and open its game options menu. */
void option_open(void) {
    menu_set_modal_open();
    overlay_request_companion(COMPANION_OVERLAY_OPTION);
    option_entrypoint(MENU_OPTION_TYPE_GAME_OPTIONS);
}

/* Thread entry of menu entry 0x1d: load OPTION.OUT and open its AT list. */
void at_list_open(void) {
    menu_set_modal_open();
    overlay_request_companion(COMPANION_OVERLAY_OPTION);
    option_entrypoint(MENU_OPTION_TYPE_AT_LIST);
}

/* Thread started for system function 0x1f (crystal or treasure): load OPTION.OUT
 * and open its crystal pickup panel. */
void crystal_open_pickup_panel(void) {
    menu_set_modal_open();
    overlay_request_companion(COMPANION_OVERLAY_OPTION);
    option_entrypoint(MENU_OPTION_TYPE_CRYSTAL_PICKUP);
}

/* Wait for the camera and menu transition, then run Triangle-menu selection. */
void mini_menu_select(void) {
    thread_set_current_task_id(THREAD_TASK_MINI_MENU);
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
        thread_wait_frames(1);
    } while (g_overlay_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);

    overlay_free_high();
    /* Parameter 3 = 1 asks the threads in slots 2-14 to close; then fade the screen back in. */
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
    unit_view_start(UNIT_VIEW_MODE_HOVERED_UNIT, g_unit_panel_selected_unit.battle_id, 0xFF);
    thread_exit_current();
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
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_TILE_INFO_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_UNIT_SUMMARY_PANEL, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_RIGHT_UNIT_STATS, 0, 0, 1);
    thread_set_parameters(THREAD_SLOT_HOVERED_UNIT_STATS, 0, 0, 1);
    unit_view_wait_for_panels();
    overlay_request_companion(COMPANION_OVERLAY_BUNIT);
    overlay_reserve_high(MENU_HIGH_OVERLAY_SIZE);
    overlay_request_companion(COMPANION_OVERLAY_BUNIT_SECOND_HALF);
    thread_wait_frames(2);
    g_help_open = 1;
    thread_start(THREAD_BORROWED_SLOT_FADE_OUT, unit_list_return_thread);
    thread_exit_current();
}

/* Start the unit-list transition in a child slot and wait for it to finish. */
void unit_list_open(void) {
    menu_set_modal_open();
    thread_start(g_thread_current_id - 1, unit_list_transition_thread);
    thread_wait_until_inactive(g_thread_current_id - 1);
    thread_exit_current();
}

/* Reserve and load the help overlay, then run its battle-help screen. */
static void help_menu_thread(void) {
    if (thread_find_running_by_task(THREAD_TASK_STATUS_SCREEN) != 0
        && thread_find_running_by_task(THREAD_TASK_MENU_WINDOW_BUILD) == 0) {
        sound_queue_sfx_confirm();
    }
    overlay_reserve_high(MENU_HIGH_OVERLAY_SIZE);
    overlay_request_companion(COMPANION_OVERLAY_HELPMENU);
    helpmenu_battle_entrypoint();
}

/* Run the job-status overlay while its state is nonzero. */
void status_screen_jobs_thread(void) {
    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        thread_exit_current();
    }
    if (thread_is_running_asm(THREAD_SLOT_EVENT) != 0) {
        thread_exit_current();
    }
    menu_set_modal_open();
    sound_queue_sfx_confirm();
    overlay_reserve_high(MENU_HIGH_OVERLAY_SIZE);
    overlay_request_companion(COMPANION_OVERLAY_JOBSTTS);
    thread_wait_frames(2);
    g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_JOBSTTS;
    do {
        g_script_event_input = 0;
        thread_yield();
    } while (g_overlay_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    g_script_event_input = 0;
    overlay_free_high();
    thread_wait_frames(2);
    menu_clear_modal_open();
    thread_exit_current();
}

/* Run the EQUIP overlays with the menu threads suspended, then reopen the idle action menu
 * with Circle pressed for the player (sounds muted) and close the panels in slots 9-14. When
 * EQUIP left its primitives up, the menu and skillset list stay suspended and their selected
 * rows are kept as the pending selection instead. */
void equipment_thread(void) {
    s32 saved_view_mode;
    s32 i;
    s32 j;

    menu_set_modal_open();
    saved_view_mode = g_unit_view_mode;
    unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
    g_menu_overlay_state = 1;
    unit_view_wait_for_panels();
    overlay_request_companion(COMPANION_OVERLAY_EQUIP);
    overlay_reserve_high(MENU_HIGH_OVERLAY_SIZE);
    overlay_request_companion(COMPANION_OVERLAY_EQUIP_SECOND_HALF);
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
        thread_set_parameters(i, 0, 0, 1);
    }
    thread_wait_frames(4);
    if (g_overlay_companion_overlay_primitives != 0) {
        for (j = 0; j < 2; j++) {
            g_command_pending_selection[j]
                = ((menu_entry_t*)g_thread_contexts[THREAD_SLOT_MENU - j].function_parameter_1)->selected_index;
        }
    }
    overlay_free_high();
    g_menu_overlay_state = 0;
    unit_view_start(saved_view_mode, 0xFF, 0xFF);
    g_sound_suppressed = 0;
    g_script_event_input = 0;
    menu_clear_modal_open();
    thread_exit_current();
}

/* Show DEBUGCHR's read-only status screen for the selected unit when it is a rider or a mount, then rebuild
 * the idle action menu and the unit summary for it. */
void status_screen_open_mounted_unit(void) {
    s16* unit_index = &g_unit_panel_selected_unit.battle_id;

    if (unit_get_stats(*unit_index)->mount_info != 0) {
        unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
        menu_set_modal_open();
        g_menu_overlay_state = 1;
        overlay_request_companion(COMPANION_OVERLAY_DEBUGCHR);
        thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
        thread_wait_until_inactive(THREAD_SLOT_MENU);
        unit_view_wait_for_panels();
        command_record_unit_selection();
        menu_enter_status_screen_selection();
        debugchr_mounted_unit_entrypoint(*unit_index);
        g_menu_overlay_state = 0;
        g_sound_suppressed = 0;
        g_script_event_input = 0;
        command_open_menu(g_command_current_id);
        unit_view_start(UNIT_VIEW_MODE_UNIT_SUMMARY, *unit_index, *unit_index);
        menu_clear_modal_open();
        menu_leave_status_screen_selection();
        thread_exit_current();
    }
}

/* Close the panels, run the requested debug session, then restore menu state. */
void debug_unit_editor_open(void) {
    unit_view_start(UNIT_VIEW_MODE_NONE, 0xFF, 0xFF);
    menu_set_modal_open();
    g_menu_overlay_state = 1;
    overlay_request_companion(COMPANION_OVERLAY_DEBUGCHR);
    thread_set_parameters(THREAD_SLOT_MENU, 0, 0, 1);
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    unit_view_wait_for_panels();
    thread_yield();
    thread_yield();
    debugchr_editor_entrypoint();
    g_menu_overlay_state = 0;
    g_sound_suppressed = 0;
    g_script_event_input = 0;
    menu_clear_modal_open();
    thread_exit_current();
}

/* Thread body of the ShowGraphic event instruction: load the ETC overlay and show its graphic. */
void script_show_graphic(void) {
    thread_set_current_task_id(THREAD_TASK_SHOW_GRAPHIC);
    overlay_request_companion(COMPANION_OVERLAY_ETC);
    etc_entrypoint();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between script_show_graphic and
 * portrait_load_image. Nothing on the disc calls it or points at it; the
 * name is a neutral placeholder until a reference turns up. */
void script_noop_3(void) {
}

/* Upload one EVTFACE.BIN block as a single image, followed by its palette rows. */
void portrait_load_image(s32 portrait_id) {
    portrait_image_file_t* buffer;

    buffer = menu_alloc_memory(sizeof(*buffer));
    g_thread_call_target = (void (*)(void))file_load_checked_to_address;
    thread_call_on_main_stack(portrait_id * 4 + DISC_SECTOR_EVENT_EVTFACE_BIN, sizeof(*buffer), buffer);
    LoadImage(&g_menu_portrait_pixel_rect, buffer->pixels[0][0]);
    LoadImage(&g_menu_portrait_palette_rect, (u32*)buffer->palettes);
    menu_free_memory(buffer);
}

/*
 * Loads one 0x7800-byte block of EVTCHR.BIN into a fresh heap allocation, with the read
 * and its polling run on the main stack. Parameter 1 is the EVTCHR slot and parameter 2
 * the block; each block occupies 15 sectors from 0x1d4c. The block is published through
 * g_script_pending_loaded_evtchr_slot and g_script_loaded_evtchr_buffer for one frame, during
 * which the per-frame event update unpacks it into the slot, and then freed.
 */
void gfx_load_evtchr_thread(void) {
    s32 parameter = thread_get_current_parameter_1();
    s32 block = thread_get_current_parameter_2();
    evtchr_file_record_t* buffer;

    thread_set_current_task_id(THREAD_TASK_LOAD_EVTCHR);
    buffer = heap_alloc(0x7800);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))file_request_read_bytes;
    } while (thread_call_on_main_stack((block * 15) + DISC_SECTOR_EVENT_EVTCHR_BIN, 0x7800, buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);
    g_script_pending_loaded_evtchr_slot = parameter;
    g_script_loaded_evtchr_buffer = buffer;
    thread_yield();
    heap_free(buffer);
    thread_exit_current();
}

/* Uploads eight 32x48 portraits and their palettes from one EVTFACE.BIN block, yielding while
 * the file loads. Each portrait is 8 VRAM columns (32 4-bit pixels) wide, placed side by side
 * from g_menu_portrait_pixel_rect. */
void script_opcode_50_portrait_col(s32 portrait_id) {
    RECT rect;
    portrait_image_file_t* buffer;
    u32(*pixels)[48][4];
    s32 i;
    palette_t* palette;

    buffer = menu_alloc_memory(sizeof(*buffer));
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))file_request_read_bytes;
    } while (thread_call_on_main_stack(portrait_id * 4 + DISC_SECTOR_EVENT_EVTFACE_BIN, sizeof(*buffer), buffer) != 0);
    do {
        thread_yield();
        g_thread_call_target = file_is_still_loading;
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
        g_thread_call_target = (void (*)(void))file_request_read_bytes;
    } while (thread_call_on_main_stack(
                 id * EVENT_BLOCK_SECTORS + DISC_SECTOR_EVENT_TEST_EVT, EVENT_BLOCK_BYTES, &g_script_event_block_buffer)
        != 0);

    do {
        thread_yield();
        g_thread_call_target = file_is_still_loading;
    } while (thread_call_on_main_stack() != 0);

    block = g_script_event_block;
    off = block->text_offset_or_marker;
    if (off != EVENT_TEXT_OFFSET_PROCESSED) {
        /* Integer addition retains the native addu operand order. */
        g_text_section_pointers[TEXT_SECTION_EVENT_MESSAGES] = (u8*)(off + (u32)block);
        block->text_offset_or_marker = EVENT_TEXT_OFFSET_PROCESSED;
    }
}

/* Copy a normal and a dimmed menu palette (16 colors each) into g_menu_palette_colors, which the
 * panel frame lines take their colors from. */
void menu_set_palette_colors(const menu_palette_pair_t* source) {
    rgba5551_t* destination = g_menu_palette_colors.colors;

    mem_copy_bytes_nonempty(destination, source->colors, sizeof(palette_t));
    mem_copy_bytes_nonempty(destination + PALETTE_COLOR_COUNT, source->colors + PALETTE_COLOR_COUNT, sizeof(palette_t));
}

/* Load the default menu palettes and rebuild the text section pointers. */
void menu_init_palette_and_text(s32 unused_0) {
    menu_set_palette_colors(&g_menu_glyph_image_data.palette);
    text_init_section_pointers();
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_init_palette_and_text and overlay_poll_companion_file. Nothing on the
 * disc calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
void menu_noop_4(void) {
}

/* Non-blocking two-step file load driven by g_thread_call_target:
 * issue the read for the pending request, then poll until it completes and
 * clear the request. */
void overlay_poll_companion_file(void) {
    if ((g_overlay_companion_data_file_request_index != 0) && (g_overlay_companion_data_file_loading == 0)) {
        g_thread_call_target = (void (*)(void))file_request_read_bytes;
        if (thread_call_on_main_stack(g_overlay_companion_data_file_lbas[g_overlay_companion_data_file_request_index],
                g_overlay_companion_data_file_sizes[g_overlay_companion_data_file_request_index],
                g_overlay_companion_data_file_buffer)
            == 0) {
            g_overlay_companion_data_file_loading = 1;
        }
    } else if ((g_overlay_companion_data_file_request_index != 0) && (g_overlay_companion_data_file_loading != 0)) {
        g_thread_call_target = file_is_still_loading;
        if (thread_call_on_main_stack() == 0) {
            g_overlay_companion_data_file_loading = 0;
            g_overlay_companion_data_file_request_index = 0;
        }
    }
}

/* Per-frame driver of overlay_request_companion: start the
 * disc read of the requested overlay on the main stack, at its offset from the
 * companion overlay load address, then poll until the read finishes and clear the
 * request. */
void overlay_poll_companion(void) {
    s32 idx = g_overlay_companion_executable_request_index;

    if ((idx != 0) && (g_overlay_companion_executable_loading == 0)) {
        g_thread_call_target = (void (*)(void))file_request_read_bytes;
        if (thread_call_on_main_stack(g_overlay_companion_executable_lbas[idx],
                g_overlay_companion_executable_sizes[idx],
                g_overlay_companion_executable_offsets[idx] + (s32)g_heap_companion_overlay_load_address)
            == 0) {
            g_overlay_companion_executable_loading = 1;
        }
    } else if ((g_overlay_companion_executable_request_index != 0) && (g_overlay_companion_executable_loading != 0)) {
        g_thread_call_target = file_is_still_loading;
        if (thread_call_on_main_stack() == 0) {
            g_overlay_companion_executable_loading = 0;
            g_overlay_companion_executable_request_index = 0;
        }
    }
}

/* Call command_reset_unit_records; nothing in BATTLE or WORLD calls this wrapper. */
void command_reset_unit_records_wrapper(void) {
    command_reset_unit_records();
}

/* Empty: the overlay entry of COMPANION_OVERLAY_STATE_STUB. */
void overlay_companion_stub(void) {
}

/* Menu-building thread: once the previous menu, the camera and the target panel are done
 * (and a tutorial slot has started its next event), open menu parameter 1, the egg menu for
 * an egg, and reopen the skillset and ability lists when restoring the same unit's menu. */
static void menu_start_entry_thread(void) {
    s32 menu_id = thread_get_current_parameter_1();
    s32 restore_depth;

    thread_wait_until_inactive(THREAD_SLOT_MENU);
    if (g_menu_overlay_state != 0) {
        thread_exit_current();
    }
    camera_wait_until_idle();
    while (thread_find_running_by_task(THREAD_TASK_ATTACK_TARGET_STATS) != 0) {
        thread_yield();
    }
    if (g_menu_input_mode == MENU_INPUT_PLAYER) {
        if (tutorial_is_current_event() != 0) {
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
    tutorial_pulse_wait_value(SCRIPT_TUTORIAL_WAIT_MENU_ENTRY);
    if (menu_id == MENU_COMMAND_ID_OTHER_UNIT || menu_id == MENU_COMMAND_ID_OTHER_UNIT_INSPECT) {
        if (menu_get_selected_unit_stats()->unit_flags & UNIT_FLAG_EGG) {
            menu_id = MENU_COMMAND_ID_EGG;
        }
    }
    thread_start(THREAD_SLOT_MENU, g_menu_thread_menu_data[menu_id].thread_entry);
    thread_set_parameters(THREAD_SLOT_MENU, (s32)&g_menu_thread_menu_data[menu_id], 0, 0);
    restore_depth = g_menu_restore_depth;
    g_script_event_input = 0;
    if (g_menu_restore_pending != 0 && g_command_restore_unit_id == g_unit_panel_selected_unit.battle_id) {
        g_help_open = 1;
        thread_wait_frames(2);
        if (restore_depth > 0) {
            thread_start(THREAD_SLOT_MENU_CHILD, command_act_menu_thread);
            thread_set_parameters(
                THREAD_SLOT_MENU_CHILD, (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_SKILLSET_LIST], 0, 0);
        }
        thread_wait_frames(2);
        if (restore_depth >= 2) {
            thread_start(THREAD_SLOT_MENU_GRANDCHILD, command_ability_list_thread);
            thread_set_parameters(
                THREAD_SLOT_MENU_GRANDCHILD, (s32)&g_menu_thread_menu_data[MENU_COMMAND_ID_ABILITY_LIST], 0, 0);
        }
        thread_wait_frames(2);
        g_menu_restore_pending = 0;
        g_help_open = 0;
    } else {
        g_menu_restore_depth = 0;
        g_command_restore_unit_id = g_unit_panel_selected_unit.battle_id;
    }
    thread_exit_current();
}

enum { IDLE_ACTION_MENU_ENTRY_COUNT = 0x34 };

/* Open an idle action menu (command_menu_e) for the active-turn unit: place the
 * entry windows beside the unit, restore the row the unit last chose and start
 * the menu-building thread on menu_start_entry_thread.
 *
 * Nothing happens while menu input is blocked. The menu is also skipped while the
 * menus are suspended, a help window is open,
 * g_menu_building_thread_started is set (it is cleared here either way) or
 * menu_id is MENU_COMMAND_ID_NONE.
 *
 * g_menu_idle_action_window_x holds two window x bytes per entry, each
 * 0x80 less than the screen x; the second is used when the unit's projected x is
 * 0x101 or more, and a zero byte leaves the entry's x unchanged. WORLD only
 * clears the menu-building-started flag and does not read the menu id. */
void command_open_menu(s32 menu_id) {
#ifdef FFT_BATTLE
    SVECTOR screen_position;
    s32 i;
    s32 column;
    u8* window_x_table;
    menu_entry_t* entry;
    s16* selected_index;

    if (g_menu_input_mode != MENU_INPUT_BLOCKED) {
        if ((g_menu_overlay_state == 0) && (g_help_open == 0) && (g_menu_building_thread_started == 0)
            && (menu_id != MENU_COMMAND_ID_NONE)) {
            g_command_current_id = menu_id;
            g_menu_thread_menu_data = g_command_idle_action_entries;
            unit_project_to_screen(
                unit_get_runtime_id_by_battle_id(g_unit_panel_selected_unit.battle_id), &screen_position);
            i = 0;
            entry = g_menu_thread_menu_data;
            window_x_table = g_menu_idle_action_window_x;
            /* Two window x bytes per entry; a projected x of 0x101 or more uses the second. */
            column = screen_position.vx >= 0x101;
            do {
                /* The offset-plus-base integer sum is byte-required (addu operand order). */
                u8* window_x = (u8*)(column + (u32)window_x_table);

                column += 2;
                if (*window_x != 0) {
                    entry->window.x = *window_x + 0x80;
                }
                i += 1;
                entry += 1;
            } while (i < IDLE_ACTION_MENU_ENTRY_COUNT);
            command_restore_unit_selection(menu_id);
            selected_index = &g_menu_thread_menu_data[MENU_COMMAND_ID_STATUS_LIST].selected_index;
            *selected_index = 0;
            g_status_screen_status_list_layout.row_offset = 0;
            menu_clear_entry_selections();
            thread_start(THREAD_SLOT_MENU_BUILDING, menu_start_entry_thread);
            thread_set_parameters(THREAD_SLOT_MENU_BUILDING, menu_id, 0, 0);
        }
        g_menu_building_thread_started = 0;
    }
#else
    g_menu_building_thread_started = 0;
#endif
}

/* Open the help menu on the menu-building thread slot unless input is held off, a modal overlay is
 * open, the camera moves, a companion overlay is busy or that slot runs. While a menu other than
 * the mini menu is up it also sets g_menu_building_thread_started, so the next idle action menu is
 * skipped. */
void help_start_menu_thread(void) {
    if (thread_is_running_asm(THREAD_SLOT_MENU) != 0 || thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        if (g_command_current_id != MENU_COMMAND_ID_MINI_MENU) {
            g_menu_building_thread_started = 1;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        return;
    }
    if (g_menu_modal_open != 0) {
        return;
    }
    if (camera_is_active() != 0) {
        return;
    }
    if (g_overlay_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE) {
        return;
    }
    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        return;
    }
    g_help_open = 1;
    thread_start(THREAD_SLOT_MENU_BUILDING, help_menu_thread);
    thread_set_parameters(THREAD_SLOT_MENU_BUILDING, 0, 0, 0);
}

/* Request SMALL.OUT as the companion executable without waiting; overlay_poll_small drives the
 * load during map initialization. */
void overlay_request_small(void) {
    g_overlay_companion_executable_request_index = COMPANION_OVERLAY_SMALL;
}

/* Drive a pending companion overlay load during map initialization. Returns 1
 * while the request is still pending; once it completes the SMALL.OUT text
 * pointers are bound and it returns 0. With no request it returns 0 at once. */
s32 overlay_poll_small(void) {
    if (g_overlay_companion_executable_request_index == 0) {
        return 0;
    }
    overlay_poll_companion();
    if (g_overlay_companion_executable_request_index != 0) {
        return 1;
    }
    small_entrypoint();
    return 0;
}

/* Returns 0 in BATTLE and WORLD; in BATTLE it sits between
 * overlay_poll_small and battle_map_skip_deployment_thread. Nothing on the disc
 * calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
s32 battle_map_return_zero_1(void) {
    return 0;
}

/* Mark map-data loading complete after one yield, then exit after another. */
void battle_map_skip_deployment_thread(void) {
    thread_yield();
    deploy_mark_finished();
    thread_yield();
    thread_exit_current();
}
