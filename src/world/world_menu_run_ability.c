#include "fft/data.h"
#include "fft/main_sound.h"
#include "fft/world.h"

/*
 * Dispatch formation ability-menu transitions and its set/remove/learn menus.
 *
 * Refresh the ordered unit list when a submenu closes, preserving selection
 * by roster slot. Mimes may enter the learn submenu but cannot set abilities.
 */
void world_menu_run_ability(void) {
    s32 state;
    s8 running;
    u8 saved;
    /* One local holds both the lock result and the saved roster slot; the
     * target keeps it in $s0 across both roles. */
    s32 value;
    s32 allowed;
    s16* selection;

    state = g_world_formation_current_menu;
    if (state == 0x12 || state == 0x16) {
        if (g_world_ability_menu_initialized == 0) {
            saved = g_world_formation_unit_browse_enabled;
            g_world_formation_scroll_enabled = 0;
            g_world_formation_unit_browse_enabled = 0;
            g_world_ability_menu_saved_browse_enabled = saved;
            world_menu_stop_unit_status_banner_thread(9);
            g_world_ability_menu_initialized = 1;
            g_world_ability_menu_slots_changed = 0;
        }
        if (g_world_formation_current_menu == 0x12) {
            g_world_ability_menu_initialized = world_formation_step_grid_transition(1);
        } else {
            g_world_ability_menu_initialized = world_formation_step_unit_portrait_slide();
        }
        if (g_world_ability_menu_initialized == 0) {
            g_world_formation_current_menu = 2;
        }
        return;
    }
    if (state == 0x13) {
        running = world_formation_step_grid_return_transition(1);
        g_world_ability_menu_initialized = running;
        if (running == 0) {
            g_world_formation_current_menu = 0;
        }
        return;
    }
    if (g_world_ability_menu_initialized == 0) {
        world_gfx_clear_sprite_slots();
        g_world_ability_menu.selected_index = 0;
        g_world_ability_menu_unit_index = 0xff;
    }
    if (g_world_formation_current_menu < 12) {
        world_formation_place_unit_portrait_at_rest(g_world_formation_selected_unit_index);
    }
    if (g_world_ability_menu_unit_index != g_world_formation_selected_unit_index) {
        value = world_formation_lock_equipment_and_abilities(g_world_formation_selected_unit_index);
        g_world_ability_menu_unit_index
            = ((world_menu_halfword_view_t*)&g_world_formation_selected_unit_index)->bytes[0];
        if (value == -6) {
            g_world_ability_menu.text_id = 0xd80d;
        } else {
            if (value == 1) {
                g_world_ability_menu.text_id = 0xd80b;
            } else {
                g_world_ability_menu.text_id = 0xd80c;
            }
        }
        g_world_menu_text_redraw_request = 1;
    }
    if (g_world_formation_current_menu == 2) {
        running = world_menu_run_thread(15, &g_world_ability_menu);
        g_world_ability_menu_initialized = running;
        g_world_formation_unit_cycle_mode = 2;
        if (running == 0) {
            world_menu_toggle_ability_panel_thread(0);
            g_world_formation_current_menu = 0x13;
            g_world_formation_unit_browse_enabled = g_world_ability_menu_saved_browse_enabled;
            g_world_formation_unit_banner_enabled = g_world_formation_saved_banner_enabled;
        }
        selection = &g_world_menu_selection_results[8];
        if (*selection != -1) {
            value = world_formation_lock_equipment_and_abilities(g_world_formation_selected_unit_index);
            allowed = 0;
            if (value == -6) {
                allowed = *selection == 2;
            }
            if (value == 1) {
                allowed = 1;
            }
            if (allowed != 0) {
                g_world_menu_sound_effect_id = MAIN_SFX_CONFIRM;
                g_world_formation_current_menu = *selection + 10;
                if (g_world_formation_current_menu != 12) {
                    world_thread_start(14, world_thread_idle_wait_forever_b);
                    world_menu_set_cursor_position(4, 0);
                    world_menu_set_cursor_position(5, 0);
                } else {
                    world_menu_stop_thread(15);
                }
            } else {
                g_world_menu_sound_effect_id = MAIN_SFX_INVALID;
            }
            world_menu_reset_selection_results();
        }
    } else {
        if (g_world_ability_submenu_running == 0) {
            g_world_formation_current_menu = 2;
            world_thread_suspend(14);
            value = g_world_formation_unit_pointers[g_world_formation_selected_unit_index]->roster_slot;
            world_formation_build_record_list(0, g_world_formation_unit_pointers, 0);
            g_world_formation_selected_unit_index = world_formation_find_index_by_roster_slot(value);
            g_world_ability_submenu_running = 1;
        }
        if (g_world_formation_current_menu == 10) {
            g_world_ability_submenu_running = world_formation_run_set_ability_menu();
        }
        if (g_world_formation_current_menu == 11) {
            g_world_ability_submenu_running = world_menu_run_remove_ability();
        }
        if (g_world_formation_current_menu == 12) {
            g_world_ability_submenu_running = world_menu_run_learn();
        }
    }
}
