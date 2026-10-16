/* The formation ability menu, including the idle thread, set, remove and clear-all abilities, learning, equip support
 * changes, and job level, learn JP and total JP calculations. */

#include "fft/data_ability.h"
#include "fft/gfx.h"
#include "fft/item.h"
#include "fft/job.h"
#include "fft/main.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_overlay.h"
#include "fft/menu_panel.h"
#include "fft/menu_script.h"
#include "fft/menu_text.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "fft/world_text.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

static s32 world_ability_is_equip_support_change(s16 unit_id, s16 ability_id);
static s32 world_formation_run_set_ability_menu(void);
static s32 world_menu_run_learn(void);
static s32 world_menu_run_remove_ability(void);
static s32 world_menu_update_clear_all_abilities_prompt(void);

/* A thread that only waits, frame after frame; it occupies the upper menu's child slot
 * while the Set or Remove ability mode runs. */
static void world_formation_ability_menu_idle_thread(void) {
    for (;;) {
        thread_wait_frames(1);
    }
}

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
        g_world_ability_menu_unit_index = g_unit_selected_index;
        if (value == WORLD_FORMATION_UNIT_LOCK_MIME) {
            g_world_ability_menu.text_id = WORLD_FORMATION_ABILITY_MENU_TEXT_MIME;
        } else {
            if (value == WORLD_FORMATION_UNIT_UNLOCKED) {
                g_world_ability_menu.text_id = WORLD_FORMATION_ABILITY_MENU_TEXT_UNLOCKED;
            } else {
                g_world_ability_menu.text_id = WORLD_FORMATION_ABILITY_MENU_TEXT_LOCKED;
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

/*
 * Run the ability-removal menu for the selected formation unit.
 *
 * Return 0 when canceled and 1 while active. The remove-all command opens its
 * confirmation; individual changes refresh the ability and stat previews.
 */
static s32 world_menu_run_remove_ability(void) {
    s16 unit_index = g_unit_selected_index;
    u8 lock_state;
    u8 slot;
    u32 slot_index;
    u16 ability;
    s32 sound;
    s16* saved_slot;
    s16* saved_slots;
    s16* cursor_y;
    s16 movement_ability;
    s32 preview;

    if (g_world_remove_ability_unit_index != unit_index) {
        g_world_remove_ability_initialized = 0;
        g_world_remove_ability_unit_index = g_unit_selected_index;
    }
    if (g_world_remove_ability_initialized == 0) {
        bcopy(g_world_unit_pointers[unit_index]->abilities.slots, g_world_remove_ability_slots,
            sizeof(g_world_remove_ability_slots));
        g_world_remove_ability_cursor = -1;
        g_world_formation_unit_cycle_mode = 2;
        g_world_clear_abilities_prompt_active = 0;
        g_world_remove_ability_initialized = 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        world_menu_toggle_preview_stats_window(0);
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        g_world_remove_ability_initialized = 0;
        g_world_remove_ability_unit_index = -1;
        return 0;
    }
    lock_state = world_formation_get_unit_lock(g_unit_selected_index);
    if (lock_state == WORLD_FORMATION_UNIT_UNLOCKED) {
        lock_state = 0;
    }
    if (lock_state != 0) {
        g_menu_help_message_text_id = 0;
        world_menu_toggle_preview_stats_window(0);
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        }
        return 1;
    }
    if (g_world_clear_abilities_prompt_active != 0) {
        g_world_clear_abilities_prompt_active = world_menu_update_clear_all_abilities_prompt();
        if (g_world_clear_abilities_prompt_active == 0) {
            g_world_remove_ability_cursor = -1;
            bcopy(g_world_unit_pointers[g_unit_selected_index]->abilities.slots, g_world_remove_ability_slots,
                sizeof(g_world_remove_ability_slots));
            return 1;
        }
        return 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_LEFT) {
        world_ability_find_movement_stat_changes(&g_item_preview_stat_detail, g_world_remove_ability_slots[4], 0);
        world_menu_toggle_preview_stats_window(1);
        g_world_clear_abilities_prompt_active = 1;
        g_menu_preview_stats_thread_params.redraw_request = 1;
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
    }
    slot = menu_update_vertical_selection_and_mark_change(
               4, WORLD_MENU_CURSOR_REMOVE_ABILITY_SLOT, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH)
        + 1;
    cursor_y = &g_world_remove_ability_cursor_point.y;
    slot_index = slot;
    *cursor_y = slot_index * 16 + 0x90;
    gfx_set_otag_index(0x28);
    menu_update_and_draw_sliding_cursor_variant(
        &g_world_remove_ability_cursor_point, &g_world_remove_ability_cursor_anim, g_menu_input_lock_mode);
    ability = g_world_unit_pointers[g_unit_selected_index]->abilities.slots[slot_index];
    g_menu_help_message_text_id = ability;
    if (ability == 0) {
        g_menu_help_message_text_id = -1;
    } else if (slot_index >= 2) {
        g_menu_help_message_text_id = ability + 0x7800;
    } else {
        g_menu_help_message_text_id = ability + 0x9800;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        saved_slots = g_world_remove_ability_slots;
        saved_slot = saved_slots + slot;
        sound = *saved_slot;
        if (sound == 0) {
            sound = MAIN_SFX_INVALID;
        } else {
            world_formation_set_unit_ability_slot(g_unit_selected_index, slot, 0,
                world_ability_is_equip_support_change(
                    g_unit_selected_index, g_world_unit_pointers[g_unit_selected_index]->abilities.slots[slot]));
            g_world_ability_menu_slots_changed = 1;
            *saved_slot = 0;
            g_world_remove_ability_cursor = -1;
            world_formation_stage_selected_unit();
            sound = MAIN_SFX_UNEQUIP;
        }
        g_sound_queued_effect_id = sound;
    }
    if (g_world_remove_ability_cursor != slot) {
        g_world_remove_ability_cursor = slot;
        preview = 0;
        if (slot == 4) {
            movement_ability = g_world_remove_ability_slots[4];
            if (movement_ability >= ABILITY_ID_MOVEMENT_MOVE_PLUS_1
                && movement_ability <= ABILITY_ID_MOVEMENT_JUMP_PLUS_3) {
                world_ability_find_movement_stat_changes(&g_item_preview_stat_detail, movement_ability, 0);
                g_menu_preview_stats_thread_params.redraw_request = 1;
                preview = 1;
            }
        }
        world_menu_toggle_preview_stats_window(preview);
    }
    return 1;
}

/* One frame of the remove-all-abilities prompt: blink the cursors on the four ability
 * slots; Circle empties them all and Right backs out, both returning 0, else 1. */
static s32 world_menu_update_clear_all_abilities_prompt(void) {
    world_unit_t* unit;
    u16 value;
    s32 unused[16];
    s32 i;
    s32 changed;

    g_menu_help_message_text_id = -1;
    gfx_set_otag_index(0x28);
    g_world_clear_abilities_blink_counter = g_world_clear_abilities_blink_counter + 1;
    if (g_world_clear_abilities_blink_counter & 1) {
        for (i = 1; i < 5; i++) {
            g_world_clear_abilities_cursor_point.y = i * 0x10 + 0x90;
            value = 0;
            menu_update_and_draw_sliding_cursor_variant(
                &g_world_clear_abilities_cursor_point, &value, g_menu_input_lock_mode);
        }
    }
    if (g_menu_input_primary_repeat & PSX_PAD_RIGHT) {
        g_sound_queued_effect_id = MAIN_SFX_PAGE_SWITCH;
        return 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        changed = 0;
        for (i = 1; i < 5; i++) {
            unit = g_world_unit_pointers[g_unit_selected_index];
            if (unit->abilities.slots[i] != 0) {
                changed = 1;
                world_formation_set_unit_ability_slot(g_unit_selected_index, i, 0,
                    world_ability_is_equip_support_change(g_unit_selected_index, unit->abilities.slots[i]));
            }
        }
        world_formation_stage_selected_unit();
        if (changed == 0) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = MAIN_SFX_UNEQUIP;
        }
        return 0;
    }
    return 1;
}

/*
 * Run the formation Set Ability menu for the selected unit.
 *
 * Return 0 when canceled and 1 while active. With the list closed the cursor
 * picks an ability slot and opens its candidate list (ability_build_list_variant for the
 * secondary skillset, world_ability_build_equippable_rsm_list otherwise);
 * with it open, the chosen ability is written to the slot and the Move+N
 * stat preview is refreshed.
 *
 * `cursor` is only assigned while the list has entries, as in the target,
 * which compares its stale register value when the list is empty. The inner
 * `(u16)` casts keep the Move+N range checks in unsigned-short arithmetic
 * (`li 0xfe1a` / `addu` / `andi 0xffff`).
 */
static s32 world_formation_run_set_ability_menu(void) {
    s32 unused[2]; /* never referenced; reproduces the 0x40 frame */
    s16 x;
    s16 y;
    s16 lock_state;
    s8 slot;
    s32 current;
    s32 cursor;
    s32 value; /* description base, list text section, then the first support-change flag */
    s16 selected;
    s32 busy;

    if (g_world_set_ability_unit_index != g_unit_selected_index) {
        g_world_set_ability_initialized = 0;
        g_world_set_ability_unit_index = g_unit_selected_index;
    }
    if (g_world_set_ability_initialized == 0) {
        menu_clear_selection_records();
        g_world_set_ability_slot = -1;
        g_world_set_ability_initialized = 1;
        g_world_set_ability_cursor = 0xFF;
        g_world_set_ability_list_close_pending = 0;
        g_world_unknown_8018d18a = 0;
        g_world_set_ability_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        g_world_set_ability_show_stat_preview = 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = MAIN_SFX_CANCEL;
        if (g_world_set_ability_list_open == 0) {
            thread_resume(WORLD_THREAD_SLOT_UPPER_MENU);
            g_world_set_ability_initialized = 0;
            world_menu_reset_selection_results();
            return 0;
        }
        g_world_set_ability_list_close_pending = 1;
    }
    lock_state = world_formation_get_unit_lock(g_unit_selected_index);
    if (lock_state == WORLD_FORMATION_UNIT_UNLOCKED) {
        lock_state = 0;
    }
    if (lock_state != 0) {
        world_menu_toggle_preview_stats_window(0);
        g_menu_help_message_text_id = 0;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = MAIN_SFX_INVALID;
            return 1;
        }
        return 1;
    }
    if (g_world_set_ability_list_open == 0) {
        slot = g_world_set_ability_slot;
        current = g_world_unit_pointers[g_unit_selected_index]->abilities.slots[slot];
        g_world_formation_unit_cycle_mode = 2;
        g_menu_help_message_text_id = current;
        if (current == 0) {
            g_menu_help_message_text_id = -1;
        } else if (slot >= 2) {
            g_menu_help_message_text_id = current + 0x7800;
        } else {
            g_menu_help_message_text_id = current + 0x9800;
        }
        slot = menu_update_vertical_selection_and_mark_change(
                   4, WORLD_MENU_CURSOR_SET_ABILITY_SLOT, g_menu_input_secondary_repeat, MAIN_SFX_PAGE_SWITCH)
            + 1;
        g_world_set_ability_slot = slot;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = MAIN_SFX_CONFIRM;
            if (slot == 1) {
                g_world_set_ability_entry_count = ability_build_list_variant(g_unit_selected_index,
                    g_world_unit_pointers[g_unit_selected_index]->identity.job_id, MENU_ABILITY_CATEGORY_ACTION,
                    g_item_list_entries, slot);
            } else {
                g_world_set_ability_entry_count
                    = world_ability_build_equippable_rsm_list(g_unit_selected_index, slot - 1, g_item_list_entries);
            }
            menu_set_vsync_mode_and_event_speed(2);
            g_world_formation_unit_cycle_mode = 1;
            g_world_set_ability_list_open = 1;
            thread_suspend(WORLD_THREAD_SLOT_UPPER_MENU);
            g_world_ability_panel_thread_params.style = 1;
            g_world_set_ability_cursor = 0xFF;
            menu_get_selection_record_variant(g_world_set_ability_slot + 4, &x, &y, g_item_list_entries);
            value = TEXT_SECTION_ABILITY_NAMES;
            if (g_world_set_ability_slot == 1) {
                value = TEXT_SECTION_SKILLSET_NAMES;
            }
            menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[value]);
        }
    } else {
        if (g_menu_list_entry_count != 0) {
            cursor = g_menu_list_selected_index;
            value = 0x7800;
            if (g_world_set_ability_slot == 1) {
                value = 0x9800;
            }
            g_menu_help_message_text_id = value + (g_item_list_entries[cursor] & ABILITY_LIST_ENTRY_ID_MASK);
            if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
                selected = g_item_list_entries[cursor];
                if (selected & ABILITY_LIST_ENTRY_DISABLED) {
                    world_text_show_message_and_play_sound(0xD81D, MAIN_SFX_ERROR_MESSAGE);
                } else {
                    value = world_ability_is_equip_support_change(g_unit_selected_index, selected);
                    world_formation_set_unit_ability_slot(g_unit_selected_index, g_world_set_ability_slot, selected,
                        value
                            | world_ability_is_equip_support_change(g_unit_selected_index,
                                g_world_unit_pointers[g_unit_selected_index]
                                    ->abilities.slots[g_world_set_ability_slot]));
                    g_sound_queued_effect_id = MAIN_SFX_EQUIP;
                    g_world_ability_menu_slots_changed = 1;
                    g_world_set_ability_list_close_pending = 1;
                    world_formation_stage_selected_unit();
                }
            }
        } else {
            g_menu_help_message_text_id = -1;
            if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
                if (g_world_set_ability_slot != 1) {
                    world_text_show_message_and_play_sound(0xC006, MAIN_SFX_ERROR_MESSAGE);
                } else {
                    g_sound_queued_effect_id = MAIN_SFX_INVALID;
                }
            }
        }
        if (g_world_set_ability_cursor != cursor) {
            g_world_set_ability_cursor = cursor;
            if ((g_item_list_entries[cursor] >= ABILITY_ID_MOVEMENT_MOVE_PLUS_1
                    && g_item_list_entries[cursor] <= ABILITY_ID_MOVEMENT_JUMP_PLUS_3)
                || (u16)(g_world_unit_pointers[g_unit_selected_index]->abilities.slots[g_world_set_ability_slot]
                       - ABILITY_ID_MOVEMENT_MOVE_PLUS_1)
                    < 6) {
                world_ability_find_movement_stat_changes(&g_item_preview_stat_detail,
                    g_world_unit_pointers[g_unit_selected_index]->abilities.slots[g_world_set_ability_slot],
                    g_item_list_entries[g_menu_list_selected_index]);
                g_menu_preview_stats_thread_params.redraw_request = 1;
                g_world_set_ability_show_stat_preview = 1;
            } else {
                g_world_set_ability_show_stat_preview = 0;
            }
        }
        menu_run_script_with_palette_mode_variant(
            &g_world_set_ability_window_script, g_menu_input_primary_repeat, world_thread_is_task_active(0));
    }
    if (g_world_set_ability_list_close_pending != 0) {
        menu_set_window_scale_step(0);
        g_world_ability_panel_thread_params.style = 0;
        g_world_set_ability_list_open = 0;
        g_world_formation_unit_cycle_mode = 2;
        menu_set_vsync_mode_and_event_speed(0);
        g_world_set_ability_show_stat_preview = 0;
        g_menu_status_display_thread_params.redraw_request = 1;
        menu_set_selection_record(
            g_world_set_ability_slot + 4, g_menu_list_selected_index, g_menu_scroll_base_index, g_item_list_entries);
        thread_resume(WORLD_THREAD_SLOT_UPPER_MENU);
        g_world_set_ability_list_close_pending = 0;
    }
    busy = 0;
    world_menu_toggle_preview_stats_window(g_world_set_ability_show_stat_preview);
    g_world_set_ability_cursor_point.y = g_world_set_ability_slot * 16 + 0x90;
    gfx_set_otag_index(0x28);
    if (world_thread_is_task_active(0) != 0 || g_world_set_ability_list_open != 0) {
        busy = 1;
    }
    menu_update_and_draw_sliding_cursor_variant(
        &g_world_set_ability_cursor_point, &g_world_set_ability_cursor_anim, busy);
    return 1;
}

/* Learn menu processing: opens the menu on first entry, then runs either
 * the skillset list (state 0) or the ability list (state 1). Returns 0
 * once the menu has been closed. */
static s32 world_menu_run_learn(void) {
    s32 result;

    if (g_world_learn_menu_initialized == 0) {
        g_world_learn_menu_step = MENU_JOB_LIST_STEP_JOBS;
        g_world_formation_unit_banner_enabled = 0;
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
        menu_request_thread_stop(WORLD_THREAD_SLOT_EQUIP_ABILITY_PANEL);
        g_world_formation_unit_cycle_mode = 0;
        world_menu_init_cursor_position(WORLD_MENU_CURSOR_ABILITY_TAB, 0);
        menu_clear_selection_record(9);
        g_world_learn_menu_initialized = 1;
    }
    if (g_world_learn_menu_step == MENU_JOB_LIST_STEP_JOBS) {
        result = world_menu_run_learn_job_list();
        if (result == MENU_JOB_LIST_STEP_ABILITIES) {
            menu_init_scrollable_list_core_variant(0, 0, 0);
            g_world_learn_menu_step = MENU_JOB_LIST_STEP_ABILITIES;
        } else if (result == MENU_JOB_LIST_STEP_CLOSE) {
            g_world_learn_menu_initialized = 0;
            g_world_formation_unit_banner_enabled = 1;
            world_formation_save_records_to_party();
            return 0;
        }
    } else {
        g_world_learn_menu_step = world_ability_run_learn_list();
    }
    return 1;
}

/* Return whether a support change requires equipment revalidation.
 *
 * The preview record at index 20 is excluded. With Max Equip at Job Change
 * enabled, equipment-type supports, Martial Arts, Two Hands and Two Swords
 * can invalidate the current loadout. */
static s32 world_ability_is_equip_support_change(s16 unit_id, s16 ability_id) {
    if (unit_id == 20 || g_main_game_options.fields.max_equip_at_job_change != GAME_OPTION_ON)
        return 0;
    if (ability_id >= ABILITY_ID_SUPPORT_EQUIP_ARMOR && ability_id < ABILITY_ID_SUPPORT_EQUIP_ARMOR + 8)
        return 1;
    if (ability_id == ABILITY_ID_SUPPORT_MARTIAL_ARTS || ability_id == ABILITY_ID_SUPPORT_TWO_HANDS)
        return 1;
    return ability_id == ABILITY_ID_SUPPORT_TWO_SWORDS;
}

/* Return the menu input lock mode; nonzero while another window holds the input. The argument is
 * ignored. */
s32 world_thread_is_task_active(s32 unused_0) {
    return g_menu_input_lock_mode;
}

/* Job level of the selected unit in the Learn Abilities menu's `index`th
 * unlocked job; levels are packed two per byte (odd jobs in the low nibble). */
s32 job_calculate_current_level_variant(s32 index) {
    s32 job_index = job_get_generic_index(g_menu_job_ids[index]);

    g_menu_job_current_level = g_world_unit_pointers[g_unit_selected_index]->job_levels[job_index >> 1];
    if (job_index & 1) {
        g_menu_job_current_level = g_menu_job_current_level & 0xF;
    } else {
        g_menu_job_current_level = g_menu_job_current_level >> 4;
    }
    return g_menu_job_current_level;
}

/* JP the selected unit has in the Learn Abilities menu's `index`th unlocked job. */
s32 world_menu_get_learn_job_jp(s32 index) {
    s32 job_index = job_get_generic_index(g_menu_job_ids[index]);

    return g_world_unit_pointers[g_unit_selected_index]->job_points.current.values[job_index];
}

/* Caches and returns the selected unit's total JP for the Learn Abilities
 * menu's `index`th unlocked job; the job list's menu script reads it as a
 * number callback. */
s32 job_calculate_current_total_jp_variant(s32 index) {
    s32 generic_job = job_get_generic_index(g_menu_job_ids[index]);

    return g_world_job_cached_total_jp
        = g_world_unit_pointers[g_unit_selected_index]->job_points.total.values[generic_job];
}
