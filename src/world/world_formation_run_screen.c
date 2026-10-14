#include "fft/gfx.h"
#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Run the WORLD formation screen until g_world_formation_screen_running clears or the tutorial ends.
 *
 * Binds both packet buffers onto a stack-resident pool block, starts the
 * formation music unless a tutorial (1-10) is running, then steps the current
 * formation menu handler once per frame. On exit it waits for menu threads 1-16
 * to finish and saves the edited records back to party data.
 *
 * Compiled with the -O1 profile: the target leaves the packet-pool argument
 * stores unscheduled and reloads g_world_formation_unit_cycle_mode where -O2's cse-follow-jumps would
 * reuse it. i is one local reused for the button disabled flag, the otag
 * index and the thread loop counter, which keeps the (s16) extension.
 */
void world_formation_run_screen(s32 tutorial, s32 unused) {
    POLY_FT4 textured_quads[2 * 500];
    POLY_G4 gradient_quads[2 * 90];
    LINE_G2 gradient_lines[2 * 130];
    LINE_F2 flat_lines[2 * 10];
    POLY_GT4 textured_gradient_quads[2 * 60];
    TILE tiles[2 * 30];
    DR_MOVE draw_moves[2 * 30];
    DR_MODE draw_modes[2 * 30];
    DR_AREA draw_areas[2 * 30];
    u32 otags[2 * 64];
    menu_gfx_context_t buffers[2];
    u8 unused_packet_gap[0x18]; /* Retains the original unused stack space. */
    s32 buttons;
    s32 i;
    u8 last_menu;

    g_world_ot_length = 0x40;
    g_world_gfx_textured_quad_capacity = 0x1F4;
    g_world_gfx_gradient_quad_capacity = 0x5A;
    g_world_gfx_textured_gradient_quad_capacity = 0x3C;
    g_world_gfx_flat_line_capacity = 0xA;
    g_world_gfx_gradient_line_capacity = 0x82;
    g_world_gfx_tile_capacity = 0x1E;
    g_world_gfx_draw_move_capacity = 0x1E;
    g_world_gfx_draw_area_capacity = 0x1E;
    g_world_gfx_draw_mode_capacity = 0x1E;
    world_gfx_init_double_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, gradient_quads,
        textured_gradient_quads, flat_lines, 0, 0, gradient_lines, 0, 0, tiles, 0, 0, 0, 0, 0, 0, draw_moves,
        draw_areas, draw_modes);
    g_world_menu_context_id = WORLD_MENU_CONTEXT_FORMATION;
    world_formation_init_menu_state();
    tutorial = -(tutorial < 11) & tutorial;
    world_script_start_tutorial(tutorial);
    if (tutorial == 0) {
        g_world_menu_music_slot = main_sound_open_music_into_free_slot(MAIN_MUSIC_FORMATION);
        while (main_sound_is_music_loading_stub() != 0) { }
        main_sound_switch_music_track(g_world_menu_music_slot, 0x7F, 0);
    }
    if (g_world_formation_screen_running != 0) {
        do {
            world_gfx_present_frame_and_swap_packet_buffer(0, 0);
            if (world_script_handle_tutorial_command() == 0) {
                break;
            }
            world_input_update_with_message_state();
            if (g_world_unit_status_banner_active != 0) {
                world_menu_run_unit_status_banner_countdown(0, 0);
                world_input_clear_state();
            }
            world_menu_update_thread_7_idle_countdown();
            world_gfx_update_fade_in_tile();
            if (g_world_input_primary_repeat & PSX_PAD_SELECT) {
                world_menu_start_description_text_thread(DIALOG_POSITION_CENTER);
            }
            if (g_world_grid_menu_id != MENU_GRID_HELP_NONE) {
                world_menu_update_grid_cursor(
                    g_world_unit_pointers[g_world_formation_selected_unit_index]->uses_monster_skillset);
            }
            world_menu_set_palette_mode(g_menu_input_lock_mode);
            g_world_formation_menu_handlers[g_world_formation_current_menu]();
            if (g_world_formation_current_menu == WORLD_FORMATION_MENU_UNIT_GRID || g_world_formation_current_menu == 4
                || g_world_formation_current_menu == 5) {
                world_formation_update_and_draw_unit_grid(1,
                    g_world_formation_current_menu == WORLD_FORMATION_MENU_UNIT_GRID,
                    g_world_formation_current_menu == WORLD_FORMATION_MENU_UNIT_GRID,
                    g_world_formation_triangle_menu_open + g_world_grid_menu_id != 0
                        ? 0
                        : g_world_input_secondary_repeat_latched,
                    world_map_is_busy(), 0, 0);
            } else {
                world_formation_draw_background_tiles(
                    g_world_formation_scroll_position + g_world_formation_scroll_velocity, 0);
            }
            i = 0;
            if (g_world_formation_unit_cycle_mode != 0 || g_world_formation_unit_browse_enabled != 0) {
                if (g_menu_input_lock_mode != 0 || g_world_grid_menu_id != MENU_GRID_HELP_NONE
                    || g_world_formation_unit_cycle_mode == 1 || g_world_formation_triangle_menu_running != 0) {
                    i = 1;
                }
                buttons = 0;
                if (g_world_formation_current_menu == WORLD_FORMATION_MENU_DISMISS_UNIT) {
                    i = 1;
                }
                if (i == 0) {
                    buttons = g_world_input_secondary_repeat;
                }
                world_formation_cycle_selected_unit(buttons);
                world_menu_draw_pressable_button(
                    MENU_PRESSABLE_BUTTON_L1, 0xC, 0xE, buttons & PSX_PAD_L1, i, MAIN_SFX_PAGE_SWITCH);
                world_menu_draw_pressable_button(
                    MENU_PRESSABLE_BUTTON_R1, 0xE0, 0xE, buttons & PSX_PAD_R1, i, MAIN_SFX_PAGE_SWITCH);
            }
            if (g_world_formation_current_menu != WORLD_FORMATION_MENU_ITEM_REMOVE) {
                world_formation_update_unit_selection();
            }
            if (g_world_formation_unit_slide_pending != 0) {
                world_formation_step_unit_selection_slide();
            }
            g_gfx_current_otag_entry = g_menu_gfx_context->otag + 63;
            menu_draw_active_window_frames();
            if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ITEM_LIST || g_world_formation_current_menu >= 14
                || g_world_formation_current_menu == WORLD_FORMATION_MENU_JOB_WHEEL) {
                i = 0x3E;
            } else {
                i = 9;
            }
            if (g_world_grid_menu_id != MENU_GRID_HELP_NONE) {
                i = 9;
            }
            if (g_world_formation_cursor_ot_override != 0) {
                i = g_world_formation_cursor_ot_override;
            }
            world_input_update_event_state(g_menu_gfx_context->otag + i, world_input_filter_menu(),
                g_world_gfx_draw_buffer_clip_y == 0 ? 0xF0 : 0);
            if (g_world_formation_current_menu != last_menu) {
                g_world_formation_cursor_ot_override = 0;
                g_world_formation_unit_cycle_mode = 0;
                world_script_set_vsync_mode_and_event_speed(0);
                g_world_menu_description_text_id = 0;
                world_menu_reset_selection_results();
                last_menu = g_world_formation_current_menu;
            }
            world_sound_dispatch_effect();
            main_sound_update_tunes();
        } while (g_world_formation_screen_running != 0);
    }
    do {
        buttons = 0;
        for (i = 1; i < 16; i++) {
            if (thread_is_running(i) != 0) {
                buttons = 1;
                world_menu_stop_unit_status_banner_thread(i);
            }
        }
        if (thread_is_running(WORLD_THREAD_SLOT_TUTORIAL_TEXT) != 0) {
            buttons = 1;
            world_text_resume_printing(WORLD_THREAD_SLOT_TUTORIAL_TEXT);
        }
        world_input_update_event_state(g_menu_gfx_context->otag, 0, 0);
    } while (buttons != 0);
    if (tutorial == 0) {
        main_sound_unload_scenario_mus(g_world_menu_music_slot);
    }
    world_formation_save_records_to_party();
    world_formation_stop_menu_threads();
    world_script_set_vsync_mode_and_event_speed(0);
    world_formation_reset_menu_context();
}
