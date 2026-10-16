/* Message durations, entry sizing, timed icon threads, panel fades, overlay reservation, companion file requests,
 * display conditions, the dark screen and spell quote threads, and the openers for the option, BUNIT, help, JOBSTTS,
 * EQUIP and DEBUGCHR menus. */

#include "fft/battle_camera.h"
#include "fft/battle_map.h"
#include "fft/battle_menu.h"
#include "fft/battle_unit.h"
#include "fft/camera.h"
#include "fft/event_debugchr.h"
#include "fft/event_etc.h"
#include "fft/event_option.h"
#include "fft/event_require.h"
#include "fft/event_small.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/map.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/menu_panel.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Set the frame budget of the next timed message (menu_icon_timed_thread); adding 0x100 also holds
 * off cancel until it runs out. */
void text_set_message_duration_frames(u16 frames) {
    g_text_message_duration_frames = frames;
}

/* Size a message window to its text and center it on x = 0x102. */
static void menu_size_entry_to_text(menu_entry_t* entry, s16* width, s16* height, s32* out_pad, s32 pad_short) {
    const u8* text = text_find_entry(entry->text_id);
    u16 padded;
    u16 w;
    u16 h;

    *out_pad = 0;
    text_measure_pixels(width, height, text);
    if (pad_short != 0) {
        if (*width < 8 && *height == 1) {
            *out_pad = (8 - *width) * 5;
            *width = 8;
        }
    }
    padded = *width + 0x18;
    *width = padded + (padded & 3);
    *height = *height * 16 + 16;
    entry->window.x = 0x102 - (*width >> 1);
    w = *width;
    entry->text_image.width = w;
    entry->inner_width = w;
    entry->window.w = w;
    h = *height;
    entry->text_image.height = h;
    entry->inner_height = h;
    entry->window.h = h;
}

/* Timed menu icon thread: shows one text entry with alternating sprite
 * records until input arrives or the g_text_message_duration_frames frame budget (scaled by the
 * event speed) runs out. Bit 8 of the budget suppresses cancel handling
 * until the budget expires. The thread-id check reads an unset local in the
 * original. */
void menu_icon_timed_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    s32 hold;
    menu_icon_record_t* record;
    s16 width;
    s16 height;
    s32 pad;
    s32 unset;

    hold = 0;
    if (g_text_message_duration_frames >= 0x100) {
        hold = 1;
        g_text_message_duration_frames &= 0xFF;
    }
    i = 0;
    param = (menu_entry_t*)thread_get_current_parameter_1();
    menu_size_entry_to_text(param, &width, &height, &pad, 1);
    menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], 0x7C);
    buffer = menu_build_and_upload_window_frame_image(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = width;
    menu_set_text_origin(pad + 8, 9);
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, (u32*)buffer);
    for (;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        if (g_text_message_duration_frames < i * g_script_event_speed) {
            hold = 0;
            g_script_event_input = PSX_PAD_CIRCLE;
            g_sound_suppressed = 1;
        }
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        if (unset >= 0) {
            g_thread_current_id--;
            menu_select_icon_cluts(&records[i & 1].base);
            g_thread_current_id++;
        } else {
            menu_select_icon_cluts(&records[i & 1].base);
        }
        record = &records[i & 1];
        menu_update_icon_cursor_sprites(param, &record->base, i, -1);
        if (hold == 0) {
            menu_handle_action(param, 0);
            menu_handle_cancel_input(param);
        }
        menu_submit_icon_primitives(&record->base);
    }
    g_menu_panel_fade_intensity = 0;
    g_sound_suppressed = 0;
    thread_yield();
    if (thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}

/* Menu icon thread that also opens a linked entry: when row_actions[0]
 * names a menu entry, that entry is centered on this text width, placed below
 * this entry, and started in the previous thread slot. The icon records then
 * alternate until input completes, selecting CLUTs in the linked slot. */
void menu_icon_linked_entry_thread(void) {
    RECT rect;
    menu_icon_record_t records[2];
    menu_entry_t* param;
    u8* buffer;
    s32 i;
    s32 parent;
    menu_icon_record_t* record;
    s16 width;
    s16 height;
    s32 pad;

    param = (menu_entry_t*)thread_get_current_parameter_1();
    menu_size_entry_to_text(param, &width, &height, &pad, 0);
    menu_build_window_sprites(&rect, param, &records[0]);
    mem_copy_bytes_nonempty(&records[1], &records[0], 0x7C);
    parent = param->row_actions[0];
    if (parent >= 0) {
        /* Unsigned narrowing keeps the retail 16-bit centering expression. */
        g_menu_thread_menu_data[parent].window.x
            = (u16)((width >> 1) - (g_menu_thread_menu_data[parent].inner_width - 0x102));
        g_menu_thread_menu_data[parent].window.y = param->window.y + param->window.h;
        thread_start(g_thread_current_id - 1, g_menu_thread_menu_data[parent].thread_entry);
        thread_set_parameters(g_thread_current_id - 1, (s32)&g_menu_thread_menu_data[parent], 0, 0);
    }
    buffer = menu_build_and_upload_window_frame_image(width, height, &rect, MENU_WINDOW_FRAME_BUILD);
    g_menu_text_state.pen.stride = width;
    menu_set_text_origin(8, 9);
    menu_display_text_entry(param->text_id, buffer, &g_menu_text_state.pen);
    LoadImage(&rect, (u32*)buffer);
    for (i = 0;; i++) {
        thread_yield();
        if (i == 0) {
            menu_free_memory(buffer);
        }
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        if (parent >= 0) {
            g_thread_current_id--;
            menu_select_icon_cluts(&records[i & 1].base);
            g_thread_current_id++;
        } else {
            menu_select_icon_cluts(&records[i & 1].base);
        }
        record = &records[i & 1];
        menu_update_icon_cursor_sprites(param, &record->base, i, -1);
        menu_handle_action(param, 0);
        menu_handle_cancel_input(param);
        menu_submit_icon_primitives(&record->base);
    }
    g_sound_suppressed = 0;
    thread_yield();
    if (thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}

/* Menu thread with nothing to draw: with menu sounds muted, handle Circle on row 0 and cancel each
 * frame until the menu should close. */
void menu_input_thread(void) {
    menu_entry_t* menu = (menu_entry_t*)thread_get_current_parameter_1();

    g_sound_suppressed = 1;
    while (1) {
        thread_yield();
        if (menu_should_close_thread(&g_script_event_input) != 0) {
            break;
        }
        menu_handle_action(menu, 0);
        menu_handle_cancel_input(menu);
    }
    thread_yield();
    g_sound_suppressed = 0;
    if (thread_get_current_parameter_2() == 0) {
        thread_exit_current();
    }
}

/* Read a script variable from any of the 32-bit variable, flag and nibble ranges by adding
 * it into 32-bit script variable 0 with the variable commands. Variable 0 (the comparison
 * result) is restored before returning.
 *
 * Reading the combined weather/time variable first rebuilds it: bit 15 from bit 0
 * of the time of day and bits 12-14 from the low three bits of the weather. GNS
 * mesh selection combines those upper bits with the layout's low 12 bits. */
s32 script_get_variable(s32 variable_id) {
    s32 saved_current_variable = *g_script_variables;

    if (variable_id == EVENT_SCRIPT_VAR_WEATHER_AND_TIME) {
        s32 time_of_day;
        s32 weather;

        time_of_day = script_get_variable(EVENT_SCRIPT_VAR_TIME_OF_DAY);
        time_of_day &= 1;
        weather = script_get_variable(EVENT_SCRIPT_VAR_WEATHER);
        weather &= 7;
        time_of_day <<= 15;
        weather <<= 12;
        script_set_variable(EVENT_SCRIPT_VAR_WEATHER_AND_TIME, time_of_day | weather);
    }
    script_run_variable_command(EVENT_OPCODE_ZERO_VARIABLE, EVENT_SCRIPT_VAR_COMPARISON_RESULT, 0, 0);
    script_run_variable_command(EVENT_OPCODE_ADD_VARIABLE, EVENT_SCRIPT_VAR_COMPARISON_RESULT, variable_id, 0);
    {
        s32 value = *g_script_variables;

        *g_script_variables = saved_current_variable;
        return value;
    }
}

/* Store a value in a script variable, whichever of the 32-bit variable, flag and nibble
 * ranges it belongs to, by running the ZeroVariable and AddImmediate commands on
 * it. 32-bit script variable 0 (the comparison result) is preserved around them, and war
 * funds are capped at 99,999,999. */
void script_set_variable(s32 variable_id, s32 value) {
    s32 saved_current_variable = *g_script_variables;

    if (variable_id == EVENT_SCRIPT_VAR_WAR_FUNDS && value > 0x05f5e0ff) {
        value = 0x05f5e0ff;
    }
    script_run_variable_command(EVENT_OPCODE_ZERO_VARIABLE, variable_id, 0, 0);
    script_run_variable_command(EVENT_OPCODE_ADD_IMMEDIATE, variable_id, value, 0);
    *g_script_variables = saved_current_variable;
}

/* Build both buffers' panel fade primitives: a black semi-transparent 256 x 50 center tile at
 * (128, 172) with nine one-line tiles above and below it for the graded edges that
 * menu_update_panel_fade animates. */
void menu_init_panel_fade_tiles(void) {
    menu_gradient_panel_prims_t* frame = &g_menu_panel_fade_frames[0];
    s32 i;

    SetDrawMode(&frame->draw_mode, 0, 0, GetTPage(0, 2, 0x3c0, 0x100), &g_gfx_texture_window);

    SetTile(&frame->center);
    setRGB0(&frame->center, 0, 0, 0);
    SetSemiTrans(&frame->center, 1);
    frame->center.x0 = 0x80;
    frame->center.y0 = 0xac;
    frame->center.w = 0x100;
    frame->center.h = 0x32;

    for (i = 0; i < 9; i++) {
        SetTile(&frame->top[i]);
        setRGB0(&frame->top[i], 0, 0, 0);
        SetSemiTrans(&frame->top[i], 1);
        frame->top[i].w = 0x100;
        frame->top[i].h = 1;
        frame->top[i].x0 = 0x80;
        frame->top[i].y0 = i + 0xa3;

        SetTile(&frame->bottom[i]);
        setRGB0(&frame->bottom[i], 0, 0, 0);
        SetSemiTrans(&frame->bottom[i], 1);
        frame->bottom[i].w = 0x100;
        frame->bottom[i].h = 1;
        frame->bottom[i].x0 = 0x80;
        frame->bottom[i].y0 = i + 0xde;
    }

    mem_copy_bytes_nonempty(
        &g_menu_panel_fade_frames[1], &g_menu_panel_fade_frames[0], sizeof(menu_gradient_panel_prims_t));
}

/* Update panel brightness and submit the center and edge tiles. */
void menu_update_panel_fade(void) {
    menu_gradient_panel_prims_t* frame;
    s32 i;
    s32 intensity;

    if (g_menu_panel_fade_mode != MENU_PANEL_FADE_NONE) {
        frame = &g_menu_panel_fade_frames[g_menu_packet_buffer_index];
        if (g_menu_panel_fade_mode == MENU_PANEL_FADE_IN && g_menu_panel_fade_intensity < 80) {
            g_menu_panel_fade_intensity += 8;
        }
        if (g_menu_panel_fade_mode == MENU_PANEL_FADE_OUT) {
            if (g_menu_panel_fade_intensity != 0) {
                g_menu_panel_fade_intensity -= 8;
            } else {
                g_menu_panel_fade_mode = MENU_PANEL_FADE_NONE;
            }
        }
        setRGB0(&frame->center, g_menu_panel_fade_intensity, g_menu_panel_fade_intensity, g_menu_panel_fade_intensity);
        intensity = g_menu_panel_fade_intensity;
        for (i = 0; i < 9; i++) {
            if (intensity != 0) {
                intensity -= 8;
            }
            /* The pointer walk preserves the target's operand order for the biased index. */
            (frame->top + (8 - i))->r0 = intensity;
            (frame->top + (8 - i))->g0 = intensity;
            (frame->top + (8 - i))->b0 = intensity;
            setRGB0(&frame->bottom[i], intensity, intensity, intensity);
            gfx_draw_or_append_gpu_primitive(&frame->top[8 - i]);
            gfx_draw_or_append_gpu_primitive(&frame->bottom[i]);
        }
        gfx_draw_or_append_gpu_primitive(&g_menu_panel_fade_frames[g_menu_packet_buffer_index].center);
        gfx_draw_or_append_gpu_primitive(&g_menu_panel_fade_frames[g_menu_packet_buffer_index].draw_mode);
    }
}

/* Draws a full-screen semi-transparent tile of gray level |level|: positive
 * values subtract (abr 2), negative values add (abr 1), and 0 draws nothing.
 *
 * The draw mode is submitted
 * before the tile while an ETC full-screen graphic draws primitives immediately,
 * and after it when both are linked into the ordering table. A non-zero level
 * also clears g_script_event_input, so menus see no button while the
 * screen is faded. */
void gfx_submit_screen_fade_overlay(fade_overlay_t* overlay, s32 level) {
    s32 blend;

    if (level != 0) {
        SetTile(&overlay->tile);
        SetSemiTrans(&overlay->tile, 1);
        blend = 2;
        overlay->tile.w = 256;
        overlay->tile.h = 240;
        setRGB0(&overlay->tile, 0, 0, 0);
        overlay->tile.x0 = 128;
        overlay->tile.y0 = 0;
        if (level < 0) {
            blend = 1;
            level = -level;
        }
        SetDrawMode(&overlay->mode, 0, 0, GetTPage(0, blend, 0x3c0, 0x100), &g_gfx_texture_window);
        setRGB0(&overlay->tile, level, level, level);
        if (g_gfx_draw_primitives_immediately != 0) {
            gfx_draw_or_append_gpu_primitive(&overlay->mode);
            gfx_draw_or_append_gpu_primitive(&overlay->tile);
        } else {
            gfx_draw_or_append_gpu_primitive(&overlay->tile);
            gfx_draw_or_append_gpu_primitive(&overlay->mode);
        }
        g_script_event_input = 0;
    }
}

/* Reserve size bytes of the main heap at the high overlay load address, yielding until that block
 * is free (with a wait message when the debug font is on). */
static void menu_reserve_high_overlay(u32 size) {
    while (1) {
        if (main_heap_reserve_at(g_main_heap_high_overlay_load_address, size)
            == g_main_heap_high_overlay_load_address) {
            break;
        }
        thread_yield();
        if (g_main_font_print_enabled != 0) {
            FntPrint(g_menu_wait_for_file_allocation_message);
        }
    }
}

/* Free the block menu_reserve_high_overlay reserved. */
static void menu_free_high_overlay(void) {
    main_heap_free(g_main_heap_high_overlay_load_address);
}

/* Publish an executable request, wait for it to clear, then honor the frame exit request. */
void menu_request_open_companion_executable(s32 request_index) {
    while (g_menu_companion_executable_request_index != 0 || g_menu_companion_data_file_request_index != 0) {
        thread_yield();
    }
    g_menu_companion_executable_request_index = request_index;
    if (request_index != 0) {
        do {
            thread_yield();
        } while (g_menu_companion_executable_request_index != 0);
    }
    if (g_script_frame_result_override == 1) {
        thread_exit_current();
    }
}

/* Publish a data-file request, wait for it to clear, then honor the frame exit request. */
static void menu_request_open_companion_data_file(s32 request_index) {
    while (g_menu_companion_executable_request_index != 0 || g_menu_companion_data_file_request_index != 0) {
        thread_yield();
    }
    g_menu_companion_data_file_request_index = request_index;
    if (request_index != 0) {
        do {
            thread_yield();
        } while (g_menu_companion_data_file_request_index != 0);
    }
    if (g_script_frame_result_override == 1) {
        thread_exit_current();
    }
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * menu_request_open_companion_data_file and
 * script_execute_display_conditions_instruction. Nothing on the disc calls it
 * or points at it; the name is a neutral placeholder until a reference turns
 * up. */
void script_noop_1(void) {
}

/* Thread body of DisplayConditions: draw numeric values again and run REQUIRE's
 * condition dispatcher, which takes the display id and its operand from the
 * thread parameters. */
void script_execute_display_conditions_instruction(void) {
    g_menu_hide_numeric_values = 0;
    require_condition_dispatch();
}

/* The thread EVENT_OPCODE_DARK_SCREEN starts: it takes the task id, and in
 * BATTLE loads REQUIRE.OUT and runs its bonus image burst (WORLD has no
 * REQUIRE.OUT to run). */
void script_dark_screen_thread(void) {
    thread_set_current_task_id(THREAD_TASK_DARK_SCREEN);
#ifdef FFT_BATTLE
    menu_request_open_companion_executable(COMPANION_OVERLAY_REQUIRE);
    require_reward_animate_bonus_image_burst();
#endif
}

/* Empty in BATTLE and WORLD; in BATTLE it sits between
 * script_dark_screen_thread and text_run_spell_quote_thread. Nothing on the
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
void text_run_spell_quote_thread(void) {
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
    unit = battle_unit_get_stats_from_battle_id(id);
    unit_project_to_screen(battle_unit_get_runtime_id_by_battle_id(id), &screen_coords);
    id = unit->unit_id;
    if (screen_coords.vy < 0x80) {
        window_mode = 0x12;
    }
    message_id = thread_get_current_parameter_2() | 0xA800;
    g_menu_companion_data_file_buffer = main_heap_alloc(0x4000);
    menu_request_open_companion_data_file(BATTLE_COMPANION_FILE_SPELL_MES);
    g_text_section_pointers[TEXT_SECTION_UNIT_MESSAGE] = g_menu_companion_data_file_buffer;
    thread_start(THREAD_SLOT_MENU, text_run_dialog_thread_variant);
    thread_set_parameters(THREAD_SLOT_MENU, window_mode, message_id, id);
    thread_wait_until_inactive(THREAD_SLOT_MENU);
    main_heap_free(g_menu_companion_data_file_buffer);
    g_menu_pending_selection[0] = 7;
    script_set_event_speed(1);
    g_menu_system_function_thread_busy = 0;
    thread_exit_current();
#endif
}

/* Thread entry: load OPTION.OUT and open its game options menu. */
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
    while (battle_camera_is_active() != 0 || g_menu_transition_state != 0) {
        thread_yield();
    }
    script_pulse_tutorial_wait_value(0xFE);
    g_menu_hide_numeric_values = 0;
    menu_run_icon_selection_loop();
}

/* Run BUNIT, release its high-overlay memory, then restore the unit view. */
static void menu_run_bunit_return_thread(void) {
    s32 i;
    s32 unused[2]; /* keeps the original 0x28 frame */

    g_menu_transition_state = 4;
    do {
        thread_yield();
    } while (thread_is_running_asm(THREAD_SLOT_MENU_GRANDCHILD) != 0);

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
static void menu_run_bunit_transition_thread(void) {
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
static void menu_run_help_menu_thread(void) {
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
    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        thread_exit_current();
    }
    if (thread_is_running_asm(THREAD_SLOT_EVENT) != 0) {
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
        menu_build_idle_action_menu(g_menu_current_id);
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

/* Thread body of the ShowGraphic event instruction: load the ETC overlay and show its graphic. */
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
 * stack, publishing it through g_script_pending_loaded_evtchr_slot / g_script_loaded_evtchr_buffer before
 * releasing it. Parameter 2 selects the block; each occupies 15 sectors from 0x1d4c.
 */
void gfx_load_evtchr_thread(void) {
    s32 parameter = thread_get_current_parameter_1();
    s32 block = thread_get_current_parameter_2();
    evtchr_file_record_t* buffer;

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
void script_opcode_portrait_col(s32 portrait_id) {
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

/* Copy a normal and a dimmed menu palette (16 colors each) into g_menu_palette_colors, which the
 * panel frame lines take their colors from. */
void menu_set_palette_colors(const menu_palette_pair_t* source) {
    rgba5551_t* destination = g_menu_palette_colors.colors;

    mem_copy_bytes_nonempty(destination, source->colors, sizeof(palette_t));
    mem_copy_bytes_nonempty(destination + PALETTE_COLOR_COUNT, source->colors + PALETTE_COLOR_COUNT, sizeof(palette_t));
}

/* Load the default menu palettes and rebuild the text section pointers. */
void menu_init_palette_and_text(s32 unused_0) {
    menu_set_palette_colors(&g_menu_default_palette_colors);
    text_init_section_pointers();
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
    s32 idx = g_menu_companion_executable_request_index;

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

/* Call menu_reset_unit_records; nothing in BATTLE or WORLD calls this wrapper. */
void menu_reset_unit_records_wrapper(void) {
    menu_reset_unit_records();
}

/* Empty: the overlay entry of COMPANION_OVERLAY_STATE_STUB. */
void menu_run_companion_overlay_stub(void) {
}

static void menu_start_entry_thread(void) {
    s32 menu_id = thread_get_current_parameter_1();
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
        if (script_is_tutorial_event_slot() != 0) {
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

enum {
    IDLE_ACTION_MENU_ENTRY_COUNT = 0x34,
};

/* Open an idle action menu (battle_menu_id_e) for the active-turn unit: place the
 * entry windows beside the unit, restore the row the unit last chose and start
 * the menu-building thread on menu_start_entry_thread.
 *
 * Nothing happens while menu input is blocked. The menu is also skipped while the
 * menus are suspended, a help window is open,
 * g_menu_building_thread_started is set (it is cleared here either way) or
 * menu_id is BATTLE_MENU_ID_NONE.
 *
 * g_battle_menu_idle_action_entry_labels holds two window x bytes per entry, each
 * 0x80 less than the screen x; the second is used when the unit's projected x is
 * 0x101 or more, and a zero byte leaves the entry's x unchanged. WORLD only
 * clears the menu-building-started flag and does not read the menu id. */
void menu_build_idle_action_menu(s32 menu_id) {
#ifdef FFT_BATTLE
    SVECTOR screen_position;
    s32 i;
    s32 column;
    u8* label_table;
    menu_entry_t* entry;
    s16* selected_index;

    if (g_menu_input_mode != MENU_INPUT_BLOCKED) {
        if ((g_menu_overlay_state == 0) && (g_menu_help_open == 0) && (g_menu_building_thread_started == 0)
            && (menu_id != BATTLE_MENU_ID_NONE)) {
            g_menu_current_id = menu_id;
            g_menu_thread_menu_data = g_menu_idle_action_entries;
            unit_project_to_screen(
                battle_unit_get_runtime_id_by_battle_id(g_menu_selected_unit.battle_id), &screen_position);
            i = 0;
            entry = g_menu_thread_menu_data;
            label_table = g_battle_menu_idle_action_entry_labels;
            /* Two window x bytes per entry; a projected x of 0x101 or more uses the second. */
            column = screen_position.vx >= 0x101;
            do {
                /* The offset-plus-base integer sum is byte-required (addu operand order). */
                u8* label = (u8*)(column + (u32)label_table);

                column += 2;
                if (*label != 0) {
                    entry->window.x = *label + 0x80;
                }
                i += 1;
                entry += 1;
            } while (i < IDLE_ACTION_MENU_ENTRY_COUNT);
            menu_restore_selection_for_selected_unit(menu_id);
            selected_index = &g_menu_thread_menu_data[8].selected_index;
            *selected_index = 0;
            g_menu_status_list_layout.row_offset = 0;
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
void menu_start_help_menu_thread(void) {
    if (thread_is_running_asm(THREAD_SLOT_MENU) != 0 || thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        if (g_menu_current_id != BATTLE_MENU_ID_MINI_MENU) {
            g_menu_building_thread_started = 1;
        }
    }
    if (g_script_input_suppression_frames != 0) {
        return;
    }
    if (g_menu_modal_open != 0) {
        return;
    }
    if (battle_camera_is_active() != 0) {
        return;
    }
    if (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE) {
        return;
    }
    if (thread_is_running_asm(THREAD_SLOT_MENU_BUILDING) != 0) {
        return;
    }
    g_menu_help_open = 1;
    thread_start(THREAD_SLOT_MENU_BUILDING, menu_run_help_menu_thread);
    thread_set_parameters(THREAD_SLOT_MENU_BUILDING, 0, 0, 0);
}

/* Request SMALL.OUT as the companion executable without waiting; menu_poll_small_load drives the
 * load during map initialization. */
void menu_request_open_small(void) {
    g_menu_companion_executable_request_index = COMPANION_OVERLAY_SMALL;
}

/* Drive a pending companion overlay load during map initialization. Returns 1
 * while the request is still pending; once it completes the SMALL.OUT text
 * pointers are bound and it returns 0. With no request it returns 0 at once. */
s32 menu_poll_small_load(void) {
    if (g_menu_companion_executable_request_index == 0) {
        return 0;
    }
    menu_open_companion_executable();
    if (g_menu_companion_executable_request_index != 0) {
        return 1;
    }
    small_text_init_battle_pointers();
    return 0;
}

/* Returns 0 in BATTLE and WORLD; in BATTLE it sits between
 * menu_poll_small_load and map_data_load_complete_thread. Nothing on the disc
 * calls it or points at it; the name is a neutral placeholder until a
 * reference turns up. */
s32 map_return_zero_1(void) {
    return 0;
}

/* Mark map-data loading complete after one yield, then exit after another. */
void map_data_load_complete_thread(void) {
    thread_yield();
    battle_deployment_mark_finished();
    thread_yield();
    thread_exit_current();
}
