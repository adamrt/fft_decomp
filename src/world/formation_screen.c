/* The formation screen, including the menu input filter, unit selection slides, the screen run, sound dispatch, menu
 * state, frame processing and unit selection staging. */

#include "fft/equipment.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/formation_item.h"
#include "fft/formation_job.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/menu.h"
#include "fft/menu_gfx.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_window.h"
#include "fft/music.h"
#include "fft/party.h"
#include "fft/screen.h"
#include "fft/shop.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/tutorial.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Read the menu input (tutorial or pad) and filter it: nothing while the
 * item category change, the status banner or the lockout timer is busy, and
 * no Select or Start (mask 0xF0FF) while a help text is requested or
 * showing, or while the dismiss menu's message runs. */
s32 screen_input_filter(void) {
    s32 input = screen_input_read(0);

    if (g_formation_item_category_input_lock != 0 || g_formation_banner_active != 0
        || screen_input_get_lockout() != 0) {
        input = 0;
    } else if (thread_is_running(THREAD_SCREEN_SLOT_TEXT) != 0) {
        if (g_formation_current_menu == FORMATION_MENU_DISMISS_UNIT) {
            input &= 0xF0FF;
        }
    } else if (g_help_message_text_id != 0 || g_screen_grid_id != MENU_GRID_HELP_NONE) {
        input &= 0xF0FF;
    }
    return input;
}

/* Steps the formation unit-selection slide; once its frame counter reaches 9 it
 * ends the slide, sets the status display redraw_request to 1 and clears flags.
 *
 * Compiled at -O1 like its neighbors: the final arm's two field stores share
 * one `la` of redraw_request, with flags at -4 from it. At -O2, cse2 folds that
 * -4 store back into an absolute address and the scheduler moves the first
 * arm's store into the frame load's delay slot.
 */
static void formation_step_selection_slide(void) {
    if (g_formation_unit_slide_frame < 9) {
        g_formation_unit_slide_frame = g_formation_unit_slide_frame + 1;
        g_unit_panel_status_display_thread_params.redraw_request = 0;
    } else {
        g_formation_unit_slide_frame = 0;
        g_formation_unit_slide_pending = 0;
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        g_unit_panel_status_display_thread_params.flags = 0;
    }
}

/*
 * Run the WORLD formation screen until g_formation_screen_running clears or the tutorial ends.
 *
 * Binds both packet buffers onto a stack-resident pool block, starts the
 * formation music unless a tutorial (1-10) is running, then steps the current
 * formation menu handler once per frame. On exit it waits for menu threads 1-16
 * to finish and saves the edited records back to party data.
 *
 * Compiled with the -O1 profile: the target leaves the packet-pool argument
 * stores unscheduled and reloads g_formation_unit_cycle_mode where -O2's cse-follow-jumps would
 * reuse it. i is one local reused for the button disabled flag, the otag
 * index and the thread loop counter, which keeps the (s16) extension.
 */
void formation_run_screen(s32 tutorial, s32 unused) {
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

    g_screen_ot_length = 0x40;
    g_screen_textured_quad_capacity = 0x1F4;
    g_screen_gradient_quad_capacity = 0x5A;
    g_screen_textured_gradient_quad_capacity = 0x3C;
    g_screen_flat_line_capacity = 0xA;
    g_screen_gradient_line_capacity = 0x82;
    g_gfx_tile_capacity = 0x1E;
    g_screen_draw_move_capacity = 0x1E;
    g_screen_draw_area_capacity = 0x1E;
    g_gfx_draw_mode_capacity = 0x1E;

    screen_init_packet_buffers(buffers, otags, 0, 0, 0, textured_quads, 0, 0, gradient_quads, textured_gradient_quads,
        flat_lines, 0, 0, gradient_lines, 0, 0, tiles, 0, 0, 0, 0, 0, 0, draw_moves, draw_areas, draw_modes);
    g_screen_context_id = SCREEN_CONTEXT_FORMATION;
    formation_init_menu_state();

    /* Values above 10 are not tutorials and become 0, the plain screen. */
    tutorial = -(tutorial < 11) & tutorial;
    tutorial_start(tutorial);
    if (tutorial == 0) {
        g_screen_music_slot = music_load_into_free_slot(MUSIC_FORMATION);
        while (music_is_loading_stub() != 0) { }
        music_switch_track(g_screen_music_slot, 0x7F, 0);
    }

    if (g_formation_screen_running != 0) {
        do {
            screen_present_frame(0, 0);
            if (tutorial_step() == 0) {
                break;
            }
            menu_input_update_with_message_state();
            if (g_formation_banner_active != 0) {
                formation_banner_countdown(0, 0);
                menu_input_clear_state();
            }
            screen_input_update_lockout();
            screen_fade_update_in();
            if (g_menu_input_primary_repeat & PSX_PAD_SELECT) {
                screen_start_help_text(DIALOG_POSITION_CENTER);
            }
            if (g_screen_grid_id != MENU_GRID_HELP_NONE) {
                screen_update_grid_cursor(g_formation_unit_pointers[g_unit_selected_index]->uses_monster_skillset);
            }
            screen_set_palette_mode(g_menu_input_lock_mode);

            g_formation_menu_handlers[g_formation_current_menu]();
            if (g_formation_current_menu == FORMATION_MENU_UNIT_GRID
                || g_formation_current_menu == FORMATION_MENU_DISMISS_UNIT
                || g_formation_current_menu == FORMATION_MENU_ORDER_UNIT) {
                formation_update_grid(1, g_formation_current_menu == FORMATION_MENU_UNIT_GRID,
                    g_formation_current_menu == FORMATION_MENU_UNIT_GRID,
                    g_formation_triangle_menu_open + g_screen_grid_id != 0 ? 0
                                                                           : g_screen_input_secondary_repeat_latched,
                    screen_is_busy(), 0, 0);
            } else {
                formation_draw_background_tiles(g_formation_scroll_position + g_formation_scroll_velocity, 0);
            }

            /* L1/R1 cycle units; i is the buttons' disabled flag here. */
            i = 0;
            if (g_formation_unit_cycle_mode != 0 || g_formation_unit_browse_enabled != 0) {
                if (g_menu_input_lock_mode != 0 || g_screen_grid_id != MENU_GRID_HELP_NONE
                    || g_formation_unit_cycle_mode == 1 || g_formation_triangle_menu_running != 0) {
                    i = 1;
                }
                buttons = 0;
                if (g_formation_current_menu == FORMATION_MENU_DISMISS_UNIT) {
                    i = 1;
                }
                if (i == 0) {
                    buttons = g_menu_input_secondary_repeat;
                }
                formation_cycle_selected_unit(buttons);
                menu_draw_pressable_button(
                    MENU_PRESSABLE_BUTTON_L1, 0xC, 0xE, buttons & PSX_PAD_L1, i, SOUND_SFX_PAGE_SWITCH);
                menu_draw_pressable_button(
                    MENU_PRESSABLE_BUTTON_R1, 0xE0, 0xE, buttons & PSX_PAD_R1, i, SOUND_SFX_PAGE_SWITCH);
            }

            if (g_formation_current_menu != FORMATION_MENU_ITEM_REMOVE) {
                formation_update_selection();
            }
            if (g_formation_unit_slide_pending != 0) {
                formation_step_selection_slide();
            }

            /* The event layer goes to OT 0x3E over the item list, the job wheel and the transitions (14 and up). */
            g_gfx_current_otag_entry = g_menu_gfx_context->otag + 63;
            menu_draw_active_window_frames();
            if (g_formation_current_menu == FORMATION_MENU_ITEM_LIST
                || g_formation_current_menu >= FORMATION_MENU_JOB_WHEEL_ENTER
                || g_formation_current_menu == FORMATION_MENU_JOB_WHEEL) {
                i = 0x3E;
            } else {
                i = 9;
            }
            if (g_screen_grid_id != MENU_GRID_HELP_NONE) {
                i = 9;
            }
            if (g_formation_cursor_ot_override != 0) {
                i = g_formation_cursor_ot_override;
            }
            menu_input_update_event_state(
                g_menu_gfx_context->otag + i, screen_input_filter(), g_screen_draw_buffer_clip_y == 0 ? 0xF0 : 0);

            /* Entering a new menu resets the per-menu state. */
            if (g_formation_current_menu != last_menu) {
                g_formation_cursor_ot_override = 0;
                g_formation_unit_cycle_mode = 0;
                menu_set_vsync_mode_and_event_speed(0);
                g_help_message_text_id = 0;
                screen_reset_selection_results();
                last_menu = g_formation_current_menu;
            }
            screen_sound_dispatch_effect();
            music_update_tunes();
        } while (g_formation_screen_running != 0);
    }

    /* Stop every menu thread and let the tutorial text finish; buttons flags one still running. */
    do {
        buttons = 0;
        for (i = 1; i < THREAD_ID_COUNT; i++) {
            if (thread_is_running(i) != 0) {
                buttons = 1;
                menu_request_thread_stop(i);
            }
        }
        if (thread_is_running(SCREEN_THREAD_SLOT_TUTORIAL_TEXT) != 0) {
            buttons = 1;
            screen_text_resume_printing(SCREEN_THREAD_SLOT_TUTORIAL_TEXT);
        }
        menu_input_update_event_state(g_menu_gfx_context->otag, 0, 0);
    } while (buttons != 0);

    if (tutorial == 0) {
        music_unload_scenario_slot(g_screen_music_slot);
    }
    formation_save_records();
    formation_stop_panels();
    menu_set_vsync_mode_and_event_speed(0);
    screen_reset_context();
}

/* Per-frame WORLD sound effect dispatch. A queued text glyph or page sound
 * plays at once on a free channel pair, as does an error message sound in
 * the menu queue. Then the id in g_sound_pending_sfx_id (dropped
 * when negative or while the menu sounds are muted), or failing that the one
 * in g_sound_queued_effect_id, plays on the sound_play_sfx channels
 * and both queues are cleared. Cancel and Cursor Move are copied to the menu
 * queue before the mute test, so they still play while muted. */
void screen_sound_dispatch_effect(void) {
    s32 sound_id;

    if (g_screen_sound_release_held_loop != 0) {
        g_sound_queued_effect_id = SOUND_SFX_NONE;
    }
    if (g_sound_pending_sfx_id == SOUND_SFX_TEXT_GLYPH || g_sound_pending_sfx_id == SOUND_SFX_TEXT_PAGE) {
        sound_play_sfx_on_free_channel(g_sound_pending_sfx_id);
        g_sound_pending_sfx_id = SOUND_SFX_NONE;
    } else if (g_sound_queued_effect_id == SOUND_SFX_ERROR_MESSAGE) {
        sound_play_sfx_on_free_channel(SOUND_SFX_ERROR_MESSAGE);
        g_sound_queued_effect_id = SOUND_SFX_NONE;
    }
    if (g_sound_pending_sfx_id == SOUND_SFX_CANCEL || g_sound_pending_sfx_id == SOUND_SFX_CURSOR_MOVE) {
        g_sound_queued_effect_id = g_sound_pending_sfx_id;
    }
    if (g_sound_pending_sfx_id < 0 || g_sound_suppressed != 0) {
        g_sound_pending_sfx_id = SOUND_SFX_NONE;
    }
    sound_id = g_sound_pending_sfx_id;
    if (sound_id == SOUND_SFX_NONE) {
        sound_id = g_sound_queued_effect_id;
    }
    g_sound_queued_effect_id = SOUND_SFX_NONE;
    g_sound_pending_sfx_id = SOUND_SFX_NONE;
    if (sound_id != SOUND_SFX_NONE) {
        sound_play_sfx(sound_id);
    }
}

/* Reset the formation screen to its initial state: every submenu's
 * "initialized" latch, the panel thread styles, the selected unit and the
 * banner and browse settings, then the menu display and the unit list. */
void formation_init_menu_state(void) {
    g_formation_main_menu_initialized = 0;
    g_formation_item_menu_initialized = 0;
    g_formation_item_equip_menu_initialized = 0;
    g_formation_item_best_remove_initialized = 0;
    g_formation_item_submenu_initialized = 0;
    g_formation_item_category_menu_initialized = 0;
    g_item_rearrange_initialized = 0;
    g_formation_item_action_menu_initialized = 0;
    g_formation_item_sort_order_initialized = 0;
    g_formation_item_equipping_units_initialized = 0;
    g_formation_ability_menu_initialized = 0;
    g_formation_ability_remove_initialized = 0;
    g_formation_ability_set_initialized = 0;
    g_formation_ability_learn_menu_initialized = 0;
    g_formation_job_wheel_initialized = 0;
    g_formation_ability_learn_job_list_initialized = 0;
    g_formation_ability_learn_ability_list_open = 0;
    g_formation_ability_view_initialized = 0;
    g_formation_ability_view_list_initialized = 0;
    g_formation_dismiss_prompt_running = 0;
    g_formation_order_open = 0;
    /* Through the records rather than the member scalars (offset_y, flags,
     * highlight): the target addresses these stores from one base each. */
    g_unit_panel_status_display_thread_params.position.y = 0x90;
    g_unit_panel_status_display_thread_params.flags = 0;
    g_unit_panel_status_display_thread_params.style = 0;
    g_unit_panel_comparison_display_thread_params.style = 0;
    g_formation_change_banner_thread_params.style = 0;
    g_formation_stat_preview_panel_thread_params.style = 0;
    g_formation_ability_panel_thread_params.style = 0;
    g_formation_equipment_panel_thread_params.style = 0;
    g_unit_panel_numeric_panel_params.style = 0;
    g_unit_panel_preview_stats_thread_params.style = 0;
    g_unit_panel_preview_stats_thread_params.position.y = 2;
    g_formation_triangle_menu.selected_index = 0;
    g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
    g_unit_selected_index = 0;
    g_formation_selected_unit_index_latch = 0;
    g_formation_unit_cycle_mode = 1;
    g_formation_unit_banner_enabled = 1;
    g_formation_unit_banner_at_bottom_latch = 1;
    g_formation_unit_banner_at_bottom = 1;
    g_formation_unit_browse_enabled = 0;
    g_formation_cursor_ot_override = 0;
    g_formation_triangle_menu_open = 0;
    formation_init_display();
    formation_rebuild_unit_list();
}

/* Clear the 16 menu selection results to -1 (nothing chosen).
 *
 * Built at -O1: -O1 does no strength reduction, which is why the target
 * recomputes the index inside the loop. Same profile as the adjacent
 * formation_init_menu_state. */
void screen_reset_selection_results(void) {
    s32 i;

    for (i = 0; i < 16; i++) {
        g_screen_selection_results[i] = -1;
    }
}

/*
 * Run one frame of the formation screen's unit grid, order and dismiss menus.
 *
 * On the unit grid, the first call rebuilds the unit list and clamps the
 * selection. Triangle or Start opens the unit's menu (Item, Ability, Job,
 * Dismiss, Order; an egg's menu only offers dismissal) and the choice moves
 * to that menu's state; Circle while browsing opens View Abilities; Cross
 * fades out, and the screen ends once the fade completes. The order and
 * dismiss menus run their own steps.
 */
void formation_grid_menu(void) {
    u32 input = g_menu_input_primary_repeat;

    if (g_formation_current_menu == FORMATION_MENU_UNIT_GRID) {
        if (g_formation_main_menu_initialized == 0) {
            g_formation_unit_banner_enabled = g_formation_saved_banner_enabled;
            g_formation_dismiss_anim_timer = 0;
            g_formation_unit_count
                = formation_build_record_list(0, g_formation_unit_pointers, FORMATION_RECORD_LIST_ALL);
            if (g_unit_selected_index > g_formation_unit_count - 1) {
                g_formation_selected_unit_index_latch = g_unit_selected_index = g_formation_unit_count - 1;
            }
            screen_reset_selection_results();
            g_formation_unit_cycle_mode = 0;
            formation_init_selection_cursor(g_unit_selected_index);
            if (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                g_formation_unit_browse_enabled = 0;
                g_unit_panel_numeric_panel_params.style = 0;
                g_formation_equipment_panel_thread_params.style = 0;
                menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
                menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
                g_formation_triangle_menu_open = 0;
            }
            g_formation_main_menu_initialized = 1;
        }

        /* Leave once the fade-out that Cross starts below completes. */
        if (g_tutorial_id == 0 && screen_fade_update_out() == 0) {
            g_formation_main_menu_initialized = 0;
            thread_suspend(THREAD_SCREEN_SLOT_MENU);
            g_formation_screen_running = 0;
            return;
        }

        if (g_formation_triangle_menu_running != 0) {
            g_help_message_text_id = 0;
            /* An egg gets its own triangle menu, whose first option dismisses it. */
            if (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                if ((g_formation_triangle_menu_running
                        = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_formation_egg_triangle_menu))
                    == 0) {
                    if (g_screen_selection_results[0] == 0) {
                        formation_dismiss(g_unit_selected_index);
                        g_formation_main_menu_initialized = 0;
                        g_sound_queued_effect_id = SOUND_SFX_DISMISS;
                    }
                    g_formation_egg_submenu.selected_index = 0;
                    screen_reset_selection_results();
                    g_formation_equipment_panel_thread_params.style = 0;
                    g_unit_panel_numeric_panel_params.style = 0;
                    g_formation_triangle_menu_open = 0;
                }
            } else {
                if ((g_formation_triangle_menu_running
                        = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_formation_triangle_menu))
                    == 0) {
                    if (g_screen_selection_results[0] == 0) {
                        g_formation_current_menu = FORMATION_MENU_ITEM_ENTER;
                        formation_item_menu();
                    } else if (g_screen_selection_results[0] == 1) {
                        g_formation_current_menu = FORMATION_MENU_ABILITY_ENTER;
                        formation_ability_menu();
                    } else if (g_screen_selection_results[0] == 2) {
                        g_formation_current_menu = FORMATION_MENU_JOB_WHEEL_ENTER;
                        formation_job_wheel_menu();
                    } else if (g_screen_selection_results[0] == 3) {
                        g_formation_current_menu = FORMATION_MENU_DISMISS_UNIT;
                    } else if (g_screen_selection_results[0] == 4) {
                        g_formation_current_menu = FORMATION_MENU_ORDER_UNIT;
                    }
                    g_formation_equipment_panel_thread_params.style = 0;
                    g_unit_panel_numeric_panel_params.style = 0;

                    if (g_screen_selection_results[0] != -1) {
                        g_formation_saved_banner_enabled = g_formation_unit_banner_enabled;
                        if (g_formation_current_menu != FORMATION_MENU_DISMISS_UNIT) {
                            menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
                            menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
                        } else {
                            g_formation_equipment_panel_thread_params.style = 1;
                            g_unit_panel_numeric_panel_params.style = 1;
                        }
                        g_formation_selection_cursor_ready = 0;
                        g_formation_main_menu_initialized = 0;
                        return;
                    }
                    g_formation_triangle_menu_open = 0;
                }
            }
        } else {
            g_help_message_text_id = -1;
            if (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                g_help_message_text_id = 0x101C;
            } else if (g_formation_triangle_menu_open == 0) {
                if (g_formation_unit_browse_enabled != 0) {
                    g_help_message_text_id = MENU_GRID_HELP_UNIT_BROWSE;
                } else if (g_formation_unit_banner_enabled != 0) {
                    g_help_message_text_id = g_formation_unit_banner_at_bottom != 0 ? MENU_GRID_HELP_BANNER_BOTTOM
                                                                                    : MENU_GRID_HELP_BANNER_TOP;
                }
            }

            if (screen_input_get_lockout() == 0 && formation_is_grid_scroll_needed() == 0) {
                if ((input & PSX_PAD_TRIANGLE) || (input & PSX_PAD_START) || g_formation_triangle_menu_open != 0) {
                    /* Keep the menu clear of the banner and of the cursor's half of the grid. */
                    if (g_formation_unit_banner_at_bottom != 0) {
                        g_formation_triangle_menu.window.y = 0x20;
                    } else {
                        g_formation_triangle_menu.window.y = 0x78;
                    }
                    g_formation_triangle_menu.window.x
                        = ((g_unit_selected_index % 4) < 2 || g_formation_unit_browse_enabled != 0) ? 0x12C : 0x8A;
                    g_formation_equipment_panel_thread_params.style = 1;
                    g_unit_panel_numeric_panel_params.style = 1;
                    g_formation_triangle_menu_running = 1;
                    g_formation_triangle_menu_open = 1;
                    if ((input & PSX_PAD_TRIANGLE) || (input & PSX_PAD_START)) {
                        g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                    }
                } else if (input & PSX_PAD_CIRCLE) {
                    if (g_formation_unit_browse_enabled != 0) {
                        if (g_formation_unit_pointers[g_unit_selected_index]->uses_monster_skillset != 0) {
                            g_sound_queued_effect_id = SOUND_SFX_INVALID;
                        } else {
                            g_formation_main_menu_initialized = 0;
                            g_formation_current_menu = FORMATION_MENU_VIEW_ABILITIES;
                        }
                    }
                }

                if (g_formation_unit_browse_enabled == 0 && (input & PSX_PAD_CROSS) && g_tutorial_id == 0) {
                    screen_fade_start_increasing();
                    music_set_target(0, 0xF0);
                    g_sound_queued_effect_id = SOUND_SFX_CANCEL;
                }
            }
        }
    } else if (g_formation_current_menu == FORMATION_MENU_ORDER_UNIT) {
        formation_order_menu();
    } else if (g_formation_current_menu == FORMATION_MENU_DISMISS_UNIT) {
        formation_dismiss_menu();
    }
}

/*
 * Refresh formation-screen state after the selected unit may have changed.
 *
 * On a new selection this snapshots the unit's display data, starts the
 * status-banner slide (g_formation_banner_slide_step), and records the scroll direction in
 * g_unit_panel_status_display_thread_params.redraw_request; it then toggles the per-menu windows and advances the
 * slide. Compiled at O1 like its formation_stage_selected_unit neighbor.
 */
void formation_update_selection(void) {
    s32 saved;
    s32 previous;
    s32* flags;

    if (g_unit_selected_index != g_formation_selected_unit_index_latch) {
        if (g_formation_unit_banner_enabled != 0) {
            saved = g_unit_panel_status_display_thread_params.flags;
            if (g_formation_unit_banner_at_bottom_latch != g_formation_unit_banner_at_bottom) {
                bcopy(&g_formation_unit_pointers[g_formation_selected_unit_index_latch]->status,
                    &g_formation_comparison_unit_stat_summary, sizeof(unit_panel_summary_t));
                bcopy(&g_formation_unit_pointers[g_formation_selected_unit_index_latch]->identity,
                    &g_formation_comparison_unit_identity, sizeof(menu_unit_status_identity_t));
                unit_panel_toggle_comparison_thread(1);
                g_unit_panel_comparison_display_thread_params.flags = saved;
                g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom;
                if (g_formation_banner_slide_step < 0) {
                    g_formation_banner_slide_step += 0x10;
                } else if (g_formation_banner_slide_step > 0) {
                    g_formation_banner_slide_step -= 0x10;
                } else if (g_formation_unit_banner_at_bottom != 0) {
                    g_formation_banner_slide_step = -1;
                } else {
                    g_formation_banner_slide_step = 1;
                }
            }
        } else {
            if ((g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom) != 0) {
                g_unit_panel_status_display_thread_params.position.y = 0x90;
            } else {
                g_unit_panel_status_display_thread_params.position.y = 0;
            }
        }
        previous = g_formation_selected_unit_index_latch;
        formation_stage_selected_unit();
        if ((g_menu_input_primary_repeat & PSX_PAD_RIGHT) || (g_menu_input_secondary_repeat & PSX_PAD_R1)) {
            g_unit_panel_status_display_thread_params.redraw_request = 1;
        } else if ((g_menu_input_primary_repeat & PSX_PAD_LEFT) || (g_menu_input_secondary_repeat & PSX_PAD_L1)) {
            g_unit_panel_status_display_thread_params.redraw_request = 2;
        } else {
            g_unit_panel_status_display_thread_params.redraw_request = previous < g_unit_selected_index ? 1 : 2;
        }
    } else if (g_formation_unit_banner_at_bottom_latch != g_formation_unit_banner_at_bottom
        && g_formation_banner_slide_step == 0) {
        g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom;
        if (g_formation_unit_banner_enabled != 0) {
            if (g_formation_unit_banner_at_bottom != 0) {
                flags = &g_unit_panel_status_display_thread_params.flags;
                *flags |= MENU_STATUS_PANEL_SLIDE_DOWN;
            } else {
                flags = &g_unit_panel_status_display_thread_params.flags;
                *flags |= MENU_STATUS_PANEL_SLIDE_UP;
            }
        } else if (g_formation_unit_banner_at_bottom != 0) {
            g_unit_panel_status_display_thread_params.position.y = 0x90;
        } else {
            g_unit_panel_status_display_thread_params.position.y = 0;
        }
    }
    if (g_formation_current_menu != FORMATION_MENU_ABILITY_LEAVE
        && g_formation_current_menu != FORMATION_MENU_ITEM_LEAVE
        && g_formation_current_menu != FORMATION_MENU_JOB_WHEEL_LEAVE) {
        formation_open_equipment_panel(g_formation_unit_browse_enabled);
    }
    if (g_formation_current_menu == FORMATION_MENU_ITEM || g_formation_current_menu == FORMATION_MENU_ITEM_EQUIP
        || g_formation_current_menu == FORMATION_MENU_ITEM_BEST_FIT) {
        formation_toggle_change_panel(1);
    } else if (g_formation_current_menu == FORMATION_MENU_ABILITY) {
        formation_toggle_ability_panel(1);
    }
    unit_panel_toggle_banner_thread(g_formation_unit_banner_enabled != 0 || g_formation_unit_browse_enabled != 0);
    if (g_formation_banner_slide_step < 0) {
        g_unit_panel_status_display_thread_params.position.y = g_formation_banner_slide_step * 8 + 0x108;
        g_unit_panel_comparison_display_thread_params.position.y = g_formation_banner_slide_step * 8;
        g_formation_banner_slide_step--;
        if (g_formation_banner_slide_step < -0xf) {
            g_formation_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    } else if (g_formation_banner_slide_step > 0) {
        g_unit_panel_status_display_thread_params.position.y = g_formation_banner_slide_step * 8 - 0x78;
        g_unit_panel_comparison_display_thread_params.position.y = g_formation_banner_slide_step * 8 + 0x90;
        g_formation_banner_slide_step++;
        if (g_formation_banner_slide_step >= 0x10) {
            g_formation_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    }
}

/*
 * Snapshot the selected formation unit after restoring its HP and MP.
 *
 * The three display-data groups are copied separately before the full unit
 * snapshot. O1 preserves the target's load/store order and direct array
 * reference in the final branch; O2 emits 364 bytes instead of 388.
 */
void formation_stage_selected_unit(void) {
    /* Preserve the target's unused 128-byte stack area. */
    u8 unused[128];
    formation_unit_t** units;
    s32 index;

    g_formation_selected_unit_index_latch = g_unit_selected_index;
    index = g_unit_selected_index;
    units = g_formation_unit_pointers;
    units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_HP].value
        = units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_HP].max;
    units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_MP].value
        = units[index]->status.gauges[MENU_UNIT_STATUS_GAUGE_MP].max;
    bcopy(&units[index]->status, &g_unit_panel_selected_unit_stat_summary, sizeof(unit_panel_summary_t));
    bcopy(&units[g_unit_selected_index]->identity, &g_formation_selected_unit_identity,
        sizeof(menu_unit_status_identity_t));
    bcopy(&units[g_unit_selected_index]->move, &g_formation_selected_unit_stat_detail, 0x40);
    formation_copy_unit_to_temp(g_unit_selected_index, FORMATION_PREVIEW_UNIT_INDEX);
    if (g_unit_panel_status_display_thread_params.flags == MENU_STATUS_PANEL_HIDDEN) {
        g_formation_unit_slide_pending = !(units[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG);
    } else {
        g_unit_panel_status_display_thread_params.flags
            = (g_formation_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG)
            ? MENU_STATUS_PANEL_HIDDEN
            : 0;
    }
    formation_redraw_panels();
}

/*
 * Restage the formation unit when the selection changes and step the unit
 * banner slide.
 *
 * On a change, the previous unit's display snapshot is kept for the slide
 * (g_formation_comparison_unit_stat_summary/g_formation_comparison_unit_identity, the second slot after
 * formation_stage_selected_unit's copies) and g_unit_panel_status_display_thread_params.redraw_request records the
 * direction: 1 for Right or R1 or a forward move, 2 otherwise. g_formation_banner_slide_step counts the
 * 16-frame banner slide in either direction, driving the banner y positions in
 * g_unit_panel_status_display_thread_params.position.y/g_unit_panel_comparison_display_thread_params.position.y.
 * Compiled at -O1; the flag update goes through a pointer so each branch materializes the address like the target.
 */
void formation_restage_on_selection_change(void) {
    s32 previous;
    s32* flags;

    if (g_unit_selected_index != g_formation_selected_unit_index_latch) {
        if (g_formation_unit_banner_enabled != 0) {
            if (g_formation_unit_banner_at_bottom_latch != g_formation_unit_banner_at_bottom) {
                bcopy(g_formation_unit_pointers[g_formation_selected_unit_index_latch],
                    &g_formation_comparison_unit_stat_summary, 0x22);
                bcopy(&g_formation_unit_pointers[g_formation_selected_unit_index_latch]->identity,
                    &g_formation_comparison_unit_identity, sizeof(menu_unit_status_identity_t));
                unit_panel_toggle_comparison_thread(1);
                g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom;
                if (g_formation_banner_slide_step < 0) {
                    g_formation_banner_slide_step += 16;
                } else if (g_formation_banner_slide_step > 0) {
                    g_formation_banner_slide_step -= 16;
                } else if (g_formation_unit_banner_at_bottom != 0) {
                    g_formation_banner_slide_step = -1;
                } else {
                    g_formation_banner_slide_step = 1;
                }
            }
        } else {
            if ((g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom) != 0) {
                g_unit_panel_status_display_thread_params.position.y = 0x90;
            } else {
                g_unit_panel_status_display_thread_params.position.y = 0;
            }
        }
        previous = g_formation_selected_unit_index_latch;
        /* The fitting room shows stats for the equipment being tried on. */
        if (g_shop_menu_step >= SHOP_STEP_FITTING_ROOM && g_shop_menu_step <= SHOP_STEP_FITTING_ROOM_CHECKOUT) {
            formation_recalculate_selected_stats();
        } else {
            formation_stage_selected_unit();
        }
        if ((g_menu_input_primary_repeat & PSX_PAD_RIGHT) || (g_menu_input_secondary_repeat & PSX_PAD_R1)) {
            g_unit_panel_status_display_thread_params.redraw_request = 1;
        } else if ((g_menu_input_primary_repeat & PSX_PAD_LEFT) || (g_menu_input_secondary_repeat & PSX_PAD_L1)) {
            g_unit_panel_status_display_thread_params.redraw_request = 2;
        } else {
            g_unit_panel_status_display_thread_params.redraw_request = previous < g_unit_selected_index ? 1 : 2;
        }
    } else {
        if (g_formation_unit_banner_at_bottom_latch != g_formation_unit_banner_at_bottom
            && g_formation_banner_slide_step == 0) {
            g_formation_unit_banner_at_bottom_latch = g_formation_unit_banner_at_bottom;
            if (g_formation_unit_banner_enabled != 0) {
                if (g_formation_unit_banner_at_bottom != 0) {
                    flags = &g_unit_panel_status_display_thread_params.flags;
                    *flags |= MENU_STATUS_PANEL_SLIDE_DOWN;
                } else {
                    flags = &g_unit_panel_status_display_thread_params.flags;
                    *flags |= MENU_STATUS_PANEL_SLIDE_UP;
                }
            } else if (g_formation_unit_banner_at_bottom != 0) {
                g_unit_panel_status_display_thread_params.position.y = 0x90;
            } else {
                g_unit_panel_status_display_thread_params.position.y = 0;
            }
        }
    }
    if (g_shop_menu_step >= SHOP_STEP_SOLDIER_OFFICE_MENU) {
        formation_open_equipment_panel(g_formation_unit_browse_enabled);
    } else {
        formation_toggle_change_panel(g_formation_unit_browse_enabled);
    }
    unit_panel_toggle_banner_thread(g_formation_unit_banner_enabled != 0 || g_formation_unit_browse_enabled != 0);
    if (g_formation_banner_slide_step < 0) {
        g_unit_panel_status_display_thread_params.position.y = g_formation_banner_slide_step * 8 + 0x108;
        g_unit_panel_comparison_display_thread_params.position.y = g_formation_banner_slide_step * 8;
        g_formation_banner_slide_step--;
        if (g_formation_banner_slide_step < -15) {
            g_formation_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    } else if (g_formation_banner_slide_step > 0) {
        g_unit_panel_status_display_thread_params.position.y = g_formation_banner_slide_step * 8 - 0x78;
        g_unit_panel_comparison_display_thread_params.position.y = g_formation_banner_slide_step * 8 + 0x90;
        g_formation_banner_slide_step++;
        if (g_formation_banner_slide_step >= 16) {
            g_formation_banner_slide_step = 0;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_COMPARISON_STATUS);
        }
    }
}

/*
 * Rebuild the selected formation unit's derived stats from its job and
 * equipment, then refresh the three display snapshots.
 *
 * Shields in either hand go to the battle record's shield slots. Like
 * formation_stage_selected_unit, this needs O1 for the
 * target's store order and unreduced loop addressing.
 */
void formation_recalculate_selected_stats(void) {
    /* Preserve the target's unused 128-byte stack area. */
    u8 unused[128];
    formation_unit_t record;
    unit_stats_t unit;
    s32 roster_slot;
    s32 i;
    s32 item;

    g_formation_selected_unit_index_latch = g_unit_selected_index;
    roster_slot = g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot;
    unit_init_from_party_slot(&unit, roster_slot, 1);
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        unit.equipment[i] = ITEM_ID_NONE;
    }
    item = g_formation_unit_pointers[g_unit_selected_index]->equipment[MENU_EQUIPMENT_SLOT_RIGHT_HAND]
        & ITEM_ENTRY_ID_MASK;
    if (item != ITEM_ID_NOTHING) {
        if (g_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
            unit.equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_SHIELD] = item;
        } else {
            unit.equipment[UNIT_EQUIPMENT_SLOT_RIGHT_HAND_WEAPON] = item;
        }
    }
    item = g_formation_unit_pointers[g_unit_selected_index]->equipment[MENU_EQUIPMENT_SLOT_LEFT_HAND]
        & ITEM_ENTRY_ID_MASK;
    if (item != ITEM_ID_NOTHING) {
        if (g_item_primary_data[item].type == ITEM_TYPE_SHIELD) {
            unit.equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_SHIELD] = item;
        } else {
            unit.equipment[UNIT_EQUIPMENT_SLOT_LEFT_HAND_WEAPON] = item;
        }
    }
    unit.equipment[UNIT_EQUIPMENT_SLOT_HEAD]
        = g_formation_unit_pointers[g_unit_selected_index]->equipment[MENU_EQUIPMENT_SLOT_HEAD];
    unit.equipment[UNIT_EQUIPMENT_SLOT_BODY]
        = g_formation_unit_pointers[g_unit_selected_index]->equipment[MENU_EQUIPMENT_SLOT_BODY];
    unit.equipment[UNIT_EQUIPMENT_SLOT_ACCESSORY]
        = g_formation_unit_pointers[g_unit_selected_index]->equipment[MENU_EQUIPMENT_SLOT_ACCESSORY];
    for (i = 0; i < UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        item = unit.equipment[i];
        if (item == ITEM_ID_NOTHING || item >= ITEM_ID_END) {
            unit.equipment[i] = ITEM_ID_NONE;
        }
    }
    unit_update_formation_stats(&unit);
    formation_build_unit_record(&unit, &record, party_get_unit(roster_slot));
    bcopy(g_formation_unit_pointers[g_unit_selected_index]->equipment, record.equipment, sizeof(record.equipment));
    record.two_handed = unit_is_two_handing_weapon(
        (s16*)record.equipment, record.support_abilities[2] & UNIT_SUPPORT_SET_3_TWO_HANDS);
    bcopy(&record.status, &g_unit_panel_selected_unit_stat_summary, sizeof(unit_panel_summary_t));
    bcopy(&record.identity, &g_formation_selected_unit_identity, sizeof(menu_unit_status_identity_t));
    bcopy(&record.move, &g_formation_selected_unit_stat_detail, 0x40);
    equipment_sum_weapon_and_evade_bonuses(
        &g_formation_selected_unit_stat_detail, g_formation_unit_pointers[g_unit_selected_index]->equipment);
    formation_copy_unit_to_temp(g_unit_selected_index, FORMATION_PREVIEW_UNIT_INDEX);
    formation_redraw_panels();
}
