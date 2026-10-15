#include "fft/thread.h"
#include "fft/world.h"
#include "psx/pad.h"

/*
 * Run one frame of the formation screen.
 */
void world_formation_process_frame(void) {
    u32 input;

    input = g_menu_input_primary_repeat;
    if (g_world_formation_current_menu == WORLD_FORMATION_MENU_UNIT_GRID) {
        if (g_world_formation_main_menu_initialized == 0) {
            g_world_formation_unit_banner_enabled = g_world_formation_saved_banner_enabled;
            g_world_formation_dismiss_anim_timer = 0;
            g_world_formation_unit_count
                = world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
            if (g_unit_selected_index > g_world_formation_unit_count - 1) {
                g_world_formation_selected_unit_index_latch = g_unit_selected_index = g_world_formation_unit_count - 1;
            }
            world_menu_reset_selection_results();
            g_world_formation_unit_cycle_mode = 0;
            world_formation_init_selection_cursor(g_unit_selected_index);
            if (g_world_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                g_world_formation_unit_browse_enabled = 0;
                g_menu_numeric_panel_params.style = 0;
                g_world_equipment_ability_panel_thread_params.style = 0;
                menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
                menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
                g_world_formation_triangle_menu_open = 0;
            }
            g_world_formation_main_menu_initialized = 1;
        }
        if (g_world_script_tutorial_id == 0 && world_gfx_update_fade_out_tile() == 0) {
            g_world_formation_main_menu_initialized = 0;
            thread_suspend(THREAD_SCREEN_SLOT_MENU);
            g_world_formation_screen_running = 0;
            return;
        }
        if (g_world_formation_triangle_menu_running != 0) {
            g_world_menu_description_text_id = 0;
            if (g_world_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                if ((g_world_formation_triangle_menu_running
                        = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_formation_egg_triangle_menu))
                    == 0) {
                    if (g_world_menu_selection_results[0] == 0) {
                        world_formation_dismiss_unit(g_unit_selected_index);
                        g_world_formation_main_menu_initialized = 0;
                        g_sound_queued_effect_id = MAIN_SFX_DISMISS;
                    }
                    g_world_formation_egg_submenu.selected_index = 0;
                    world_menu_reset_selection_results();
                    g_world_equipment_ability_panel_thread_params.style = 0;
                    g_menu_numeric_panel_params.style = 0;
                    g_world_formation_triangle_menu_open = 0;
                }
            } else {
                if ((g_world_formation_triangle_menu_running
                        = menu_start_managed_thread(THREAD_SCREEN_SLOT_MENU, &g_world_formation_triangle_menu))
                    == 0) {
                    if (g_world_menu_selection_results[0] == 0) {
                        g_world_formation_current_menu = WORLD_FORMATION_MENU_ITEM_ENTER;
                        world_formation_item_menu();
                    } else if (g_world_menu_selection_results[0] == 1) {
                        g_world_formation_current_menu = WORLD_FORMATION_MENU_ABILITY_ENTER;
                        world_menu_run_ability();
                    } else if (g_world_menu_selection_results[0] == 2) {
                        g_world_formation_current_menu = WORLD_FORMATION_MENU_JOB_WHEEL_ENTER;
                        world_formation_run_job_wheel_menu();
                    } else if (g_world_menu_selection_results[0] == 3) {
                        g_world_formation_current_menu = WORLD_FORMATION_MENU_DISMISS_UNIT;
                    } else if (g_world_menu_selection_results[0] == 4) {
                        g_world_formation_current_menu = WORLD_FORMATION_MENU_ORDER_UNIT;
                    }
                    g_world_equipment_ability_panel_thread_params.style = 0;
                    g_menu_numeric_panel_params.style = 0;
                    if (g_world_menu_selection_results[0] != -1) {
                        g_world_formation_saved_banner_enabled = g_world_formation_unit_banner_enabled;
                        if (g_world_formation_current_menu != WORLD_FORMATION_MENU_DISMISS_UNIT) {
                            menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
                            menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
                        } else {
                            g_world_equipment_ability_panel_thread_params.style = 1;
                            g_menu_numeric_panel_params.style = 1;
                        }
                        g_world_formation_selection_cursor_ready = 0;
                        g_world_formation_main_menu_initialized = 0;
                        return;
                    }
                    g_world_formation_triangle_menu_open = 0;
                }
            }
        } else {
            g_world_menu_description_text_id = -1;
            if (g_world_unit_pointers[g_unit_selected_index]->gender_flags & UNIT_FLAG_EGG) {
                g_world_menu_description_text_id = 0x101C;
            } else if (g_world_formation_triangle_menu_open == 0) {
                if (g_world_formation_unit_browse_enabled != 0) {
                    g_world_menu_description_text_id = MENU_GRID_HELP_UNIT_BROWSE;
                } else if (g_world_formation_unit_banner_enabled != 0) {
                    g_world_menu_description_text_id = g_world_formation_unit_banner_at_bottom != 0
                        ? MENU_GRID_HELP_BANNER_BOTTOM
                        : MENU_GRID_HELP_BANNER_TOP;
                }
            }
            if (world_input_get_lockout_timer() == 0 && world_formation_is_unit_list_scroll_needed() == 0) {
                if ((input & PSX_PAD_TRIANGLE) || (input & PSX_PAD_START)
                    || g_world_formation_triangle_menu_open != 0) {
                    if (g_world_formation_unit_banner_at_bottom != 0) {
                        g_world_formation_triangle_menu.window.y = 0x20;
                    } else {
                        g_world_formation_triangle_menu.window.y = 0x78;
                    }
                    g_world_formation_triangle_menu.window.x
                        = ((g_unit_selected_index % 4) < 2 || g_world_formation_unit_browse_enabled != 0) ? 0x12C
                                                                                                          : 0x8A;
                    g_world_equipment_ability_panel_thread_params.style = 1;
                    g_menu_numeric_panel_params.style = 1;
                    g_world_formation_triangle_menu_running = 1;
                    g_world_formation_triangle_menu_open = 1;
                    if ((input & PSX_PAD_TRIANGLE) || (input & PSX_PAD_START)) {
                        g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                    }
                } else if (input & PSX_PAD_CIRCLE) {
                    if (g_world_formation_unit_browse_enabled != 0) {
                        if (g_world_unit_pointers[g_unit_selected_index]->uses_monster_skillset != 0) {
                            g_sound_queued_effect_id = MAIN_SFX_INVALID;
                        } else {
                            g_world_formation_main_menu_initialized = 0;
                            g_world_formation_current_menu = WORLD_FORMATION_MENU_VIEW_ABILITIES;
                        }
                    }
                }
                if (g_world_formation_unit_browse_enabled == 0 && (input & PSX_PAD_CROSS)
                    && g_world_script_tutorial_id == 0) {
                    world_gfx_start_increasing_fade();
                    main_sound_set_current_music_target(0, 0xF0);
                    g_sound_queued_effect_id = MAIN_SFX_CANCEL;
                }
            }
        }
    } else if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ORDER_UNIT) {
        world_menu_run_order_unit();
    } else if (g_world_formation_current_menu == WORLD_FORMATION_MENU_DISMISS_UNIT) {
        world_formation_run_dismiss_unit_menu_step();
    }
}
