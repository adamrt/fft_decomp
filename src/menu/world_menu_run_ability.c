#include "fft/thread.h"
#include "fft/world.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

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
    if (state == WORLD_FORMATION_MENU_ABILITY_ENTER || state == WORLD_FORMATION_MENU_ABILITY_ENTER_PORTRAIT_SLIDE) {
        if (g_world_ability_menu_initialized == 0) {
            saved = g_world_formation_unit_browse_enabled;
            g_world_formation_unit_banner_at_bottom = 0;
            g_world_formation_unit_browse_enabled = 0;
            g_world_ability_menu_saved_browse_enabled = saved;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
            g_world_ability_menu_initialized = 1;
            g_world_ability_menu_slots_changed = 0;
        }
        if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ABILITY_ENTER) {
            g_world_ability_menu_initialized = world_formation_step_grid_transition(1);
        } else {
            g_world_ability_menu_initialized = world_formation_step_unit_portrait_slide();
        }
        if (g_world_ability_menu_initialized == 0) {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_ABILITY;
        }
        return;
    }
    if (state == WORLD_FORMATION_MENU_ABILITY_LEAVE) {
        running = world_formation_step_grid_return_transition(1);
        g_world_ability_menu_initialized = running;
        if (running == 0) {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_UNIT_GRID;
        }
        return;
    }
    if (g_world_ability_menu_initialized == 0) {
        menu_clear_selection_records();
        g_world_ability_menu.selected_index = 0;
        g_world_ability_menu_unit_index = 0xff;
    }
    if (g_world_formation_current_menu < 12) {
        world_formation_place_unit_portrait_at_rest(g_unit_selected_index);
    }
    if (g_world_ability_menu_unit_index != g_unit_selected_index) {
        value = world_formation_get_unit_lock(g_unit_selected_index);
        g_world_ability_menu_unit_index = (u8)g_unit_selected_index;
        if (value == WORLD_FORMATION_UNIT_LOCK_MIME) {
            g_world_ability_menu.text_id = 0xd80d;
        } else {
            if (value == WORLD_FORMATION_UNIT_UNLOCKED) {
                g_world_ability_menu.text_id = 0xd80b;
            } else {
                g_world_ability_menu.text_id = 0xd80c;
            }
        }
        g_world_menu_text_redraw_request = 1;
    }
    if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ABILITY) {
        running = menu_start_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU, &g_world_ability_menu);
        g_world_ability_menu_initialized = running;
        g_world_formation_unit_cycle_mode = 2;
        if (running == 0) {
            world_menu_toggle_ability_panel_thread(0);
            g_world_formation_current_menu = WORLD_FORMATION_MENU_ABILITY_LEAVE;
            g_world_formation_unit_browse_enabled = g_world_ability_menu_saved_browse_enabled;
            g_world_formation_unit_banner_enabled = g_world_formation_saved_banner_enabled;
        }
        selection = &g_world_menu_selection_results[8];
        if (*selection != -1) {
            value = world_formation_get_unit_lock(g_unit_selected_index);
            allowed = 0;
            if (value == WORLD_FORMATION_UNIT_LOCK_MIME) {
                allowed = *selection == 2;
            }
            if (value == WORLD_FORMATION_UNIT_UNLOCKED) {
                allowed = 1;
            }
            if (allowed != 0) {
                g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
                g_world_formation_current_menu = *selection + WORLD_FORMATION_MENU_ABILITY_SET;
                if (g_world_formation_current_menu != WORLD_FORMATION_MENU_ABILITY_LEARN) {
                    thread_start(WORLD_THREAD_SLOT_UPPER_MENU_CHILD, world_formation_ability_menu_idle_thread);
                    menu_init_selection_value(WORLD_MENU_CURSOR_SET_ABILITY_SLOT, 0);
                    menu_init_selection_value(WORLD_MENU_CURSOR_REMOVE_ABILITY_SLOT, 0);
                } else {
                    menu_stop_managed_thread(WORLD_THREAD_SLOT_UPPER_MENU);
                }
            } else {
                g_sound_queued_effect_id = MAIN_SFX_INVALID;
            }
            world_menu_reset_selection_results();
        }
    } else {
        if (g_world_ability_submenu_running == 0) {
            g_world_formation_current_menu = WORLD_FORMATION_MENU_ABILITY;
            thread_suspend(WORLD_THREAD_SLOT_UPPER_MENU_CHILD);
            value = g_world_unit_pointers[g_unit_selected_index]->identity.roster_slot;
            world_formation_build_record_list(0, g_world_unit_pointers, WORLD_FORMATION_RECORD_LIST_ALL);
            g_unit_selected_index = world_formation_find_index_by_roster_slot(value);
            g_world_ability_submenu_running = 1;
        }
        if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ABILITY_SET) {
            g_world_ability_submenu_running = world_formation_run_set_ability_menu();
        }
        if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ABILITY_REMOVE) {
            g_world_ability_submenu_running = world_menu_run_remove_ability();
        }
        if (g_world_formation_current_menu == WORLD_FORMATION_MENU_ABILITY_LEARN) {
            g_world_ability_submenu_running = world_menu_run_learn();
        }
    }
}
