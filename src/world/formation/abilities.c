/* Formation sets, removes and learns abilities. Its job list supplies the selected job, JP and mastery values to the
 * separate ability-category list. */

#include "fft/ability.h"
#include "fft/ability_list.h"
#include "fft/formation.h"
#include "fft/formation_ability.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/help.h"
#include "fft/item.h"
#include "fft/item_menu.h"
#include "fft/job.h"
#include "fft/job_list.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_list.h"
#include "fft/menu_script.h"
#include "fft/menu_selection.h"
#include "fft/menu_window.h"
#include "fft/message_box.h"
#include "fft/option.h"
#include "fft/screen.h"
#include "fft/sound.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_panel.h"
#include "psx/libc.h"
#include "psx/pad.h"
#include "psx/types.h"

/* ability-menu state */
extern menu_entry_t g_formation_ability_menu;
extern u8 g_formation_ability_menu_unit_index;
extern u8 g_formation_ability_menu_slots_changed;
extern u8 g_formation_ability_menu_saved_browse_enabled;
extern u8 g_formation_ability_submenu_running;
extern s8 g_formation_ability_learn_menu_step; /* menu_job_list_step_e */

extern u8 g_formation_ability_set_unit_index;
extern s8 g_formation_ability_set_slot; /* -1 before the first cursor update, then ability slot 1..4 */

extern u8 g_formation_ability_set_cursor;
extern u16 g_formation_ability_set_cursor_anim;
extern point16_t g_formation_ability_set_cursor_point;
extern s8 g_formation_ability_set_list_open; /* ability list open */

extern s8 g_formation_ability_set_entry_count; /* ability list entry count */

extern s8 g_formation_ability_set_list_close_pending; /* close the ability list this frame */

extern s8 g_formation_ability_set_show_stat_preview; /* show the stat preview window */

extern s8 g_formation_ability_set_unread_flag; /* cleared when Set Ability opens; never read */

extern s8 g_formation_ability_remove_unit_index;
extern s8 g_formation_ability_remove_cursor;
extern u16 g_formation_ability_remove_cursor_anim;
extern point16_t g_formation_ability_remove_cursor_point;
extern s8 g_formation_ability_remove_all_prompt_active;
extern volatile u16 g_formation_ability_remove_all_blink_counter;
extern point16_t g_formation_ability_remove_all_cursor_point;

/* Byte-aligned display streams; each extent includes the terminating opcode. */
extern u8 g_formation_ability_set_window_script[67];

/* Byte-aligned display streams; each extent includes the terminating opcode. */
extern u8 g_formation_ability_learn_job_list_script[217];

static void idle_thread_run(void);
static s32 removal_menu_update(void);
static s32 removal_prompt_update(void);
static s32 slot_menu_update(void);
static s32 learn_menu_update(void);
static s32 equipment_support_change_check(s16 unit_id, s16 ability_id);
static s32 job_jp_get(s32 index);

static s16 g_formation_ability_remove_slots[5] = { 0 };

/* A thread that only waits, frame after frame; it occupies the upper menu's child slot
 * while the Set or Remove ability mode runs. */
static void idle_thread_run(void) {
    for (;;) {
        thread_frames_wait(1);
    }
}

/*
 * Dispatch formation ability-menu transitions and its set/remove/learn menus.
 *
 * Refresh the ordered unit list when a submenu closes, preserving selection
 * by roster slot. Mimes may enter the learn submenu but cannot set abilities.
 */
void formation_ability_menu_update(void) {
    s32 state;
    s8 running;
    u8 saved;
    /* One local holds both the lock result and the saved roster slot; the
     * target keeps it in $s0 across both roles. */
    s32 value;
    s32 allowed;
    s16* selection;

    state = g_formation_current_menu;
    if (state == FORMATION_MENU_ABILITY_ENTER || state == FORMATION_MENU_ABILITY_ENTER_SPRITE_SLIDE) {
        if (g_formation_ability_menu_initialized == 0) {
            saved = g_formation_unit_browse_enabled;
            g_formation_unit_banner_at_bottom = 0;
            g_formation_unit_browse_enabled = 0;
            g_formation_ability_menu_saved_browse_enabled = saved;
            menu_request_thread_stop(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
            g_formation_ability_menu_initialized = 1;
            g_formation_ability_menu_slots_changed = 0;
        }
        if (g_formation_current_menu == FORMATION_MENU_ABILITY_ENTER) {
            g_formation_ability_menu_initialized = formation_grid_transition_update(1);
        } else {
            g_formation_ability_menu_initialized = formation_sprite_slide_update();
        }
        if (g_formation_ability_menu_initialized == 0) {
            g_formation_current_menu = FORMATION_MENU_ABILITY;
        }
        return;
    }
    if (state == FORMATION_MENU_ABILITY_LEAVE) {
        running = formation_grid_return_update(1);
        g_formation_ability_menu_initialized = running;
        if (running == 0) {
            g_formation_current_menu = FORMATION_MENU_UNIT_GRID;
        }
        return;
    }
    if (g_formation_ability_menu_initialized == 0) {
        menu_selection_records_clear();
        g_formation_ability_menu.selected_index = 0;
        g_formation_ability_menu_unit_index = 0xff;
    }
    if (g_formation_current_menu < FORMATION_MENU_ABILITY_LEARN) {
        formation_sprite_at_rest_draw(g_unit_selected_index);
    }
    if (g_formation_ability_menu_unit_index != g_unit_selected_index) {
        value = formation_unit_lock_get(g_unit_selected_index);
        g_formation_ability_menu_unit_index = g_unit_selected_index;
        if (value == FORMATION_UNIT_LOCK_MIME) {
            g_formation_ability_menu.text_id = FORMATION_ABILITY_MENU_TEXT_MIME;
        } else {
            if (value == FORMATION_UNIT_UNLOCKED) {
                g_formation_ability_menu.text_id = FORMATION_ABILITY_MENU_TEXT_UNLOCKED;
            } else {
                g_formation_ability_menu.text_id = FORMATION_ABILITY_MENU_TEXT_LOCKED;
            }
        }
        g_screen_text_redraw_request = 1;
    }
    if (g_formation_current_menu == FORMATION_MENU_ABILITY) {
        running = menu_managed_thread_start(SCREEN_THREAD_SLOT_UPPER_MENU, &g_formation_ability_menu);
        g_formation_ability_menu_initialized = running;
        g_formation_unit_cycle_mode = 2;
        if (running == 0) {
            formation_toggle_ability_panel(0);
            g_formation_current_menu = FORMATION_MENU_ABILITY_LEAVE;
            g_formation_unit_browse_enabled = g_formation_ability_menu_saved_browse_enabled;
            g_formation_unit_banner_enabled = g_formation_saved_banner_enabled;
        }
        selection = &g_screen_selection_results[8];
        if (*selection != -1) {
            value = formation_unit_lock_get(g_unit_selected_index);
            allowed = 0;
            if (value == FORMATION_UNIT_LOCK_MIME) {
                allowed = *selection == 2;
            }
            if (value == FORMATION_UNIT_UNLOCKED) {
                allowed = 1;
            }
            if (allowed != 0) {
                g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
                g_formation_current_menu = *selection + FORMATION_MENU_ABILITY_SET;
                if (g_formation_current_menu != FORMATION_MENU_ABILITY_LEARN) {
                    thread_start(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD, idle_thread_run);
                    menu_selection_value_init(SCREEN_CURSOR_SET_ABILITY_SLOT, 0);
                    menu_selection_value_init(SCREEN_CURSOR_REMOVE_ABILITY_SLOT, 0);
                } else {
                    menu_managed_thread_stop(SCREEN_THREAD_SLOT_UPPER_MENU);
                }
            } else {
                g_sound_queued_effect_id = SOUND_SFX_INVALID;
            }
            screen_selection_results_reset();
        }
    } else {
        if (g_formation_ability_submenu_running == 0) {
            g_formation_current_menu = FORMATION_MENU_ABILITY;
            thread_suspend(SCREEN_THREAD_SLOT_UPPER_MENU_CHILD);
            value = g_formation_unit_pointers[g_unit_selected_index]->identity.roster_slot;
            formation_record_list_build(0, g_formation_unit_pointers, FORMATION_RECORD_LIST_ALL);
            g_unit_selected_index = formation_roster_slot_find(value);
            g_formation_ability_submenu_running = 1;
        }
        if (g_formation_current_menu == FORMATION_MENU_ABILITY_SET) {
            g_formation_ability_submenu_running = slot_menu_update();
        }
        if (g_formation_current_menu == FORMATION_MENU_ABILITY_REMOVE) {
            g_formation_ability_submenu_running = removal_menu_update();
        }
        if (g_formation_current_menu == FORMATION_MENU_ABILITY_LEARN) {
            g_formation_ability_submenu_running = learn_menu_update();
        }
    }
}

/*
 * Run the ability-removal menu for the selected formation unit.
 *
 * Return 0 when canceled and 1 while active. The cursor runs over slots 1-4
 * (secondary skillset, reaction, support, movement); Circle empties the
 * slot, and Left opens the remove-all prompt. Highlighting a Move+/Jump+
 * movement ability previews losing it.
 */
static s32 removal_menu_update(void) {
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

    if (g_formation_ability_remove_unit_index != unit_index) {
        g_formation_ability_remove_initialized = 0;
        g_formation_ability_remove_unit_index = g_unit_selected_index;
    }
    if (g_formation_ability_remove_initialized == 0) {
        bcopy(g_formation_unit_pointers[unit_index]->detail.abilities, g_formation_ability_remove_slots,
            sizeof(g_formation_ability_remove_slots));
        g_formation_ability_remove_cursor = -1;
        g_formation_unit_cycle_mode = 2;
        g_formation_ability_remove_all_prompt_active = 0;
        g_formation_ability_remove_initialized = 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        formation_toggle_preview_stats_window(0);
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        g_formation_ability_remove_initialized = 0;
        g_formation_ability_remove_unit_index = -1;
        return 0;
    }
    lock_state = formation_unit_lock_get(g_unit_selected_index);
    if (lock_state == FORMATION_UNIT_UNLOCKED) {
        lock_state = 0;
    }
    if (lock_state != 0) {
        g_help_message_text_id = 0;
        formation_toggle_preview_stats_window(0);
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        }
        return 1;
    }
    if (g_formation_ability_remove_all_prompt_active != 0) {
        g_formation_ability_remove_all_prompt_active = removal_prompt_update();
        if (g_formation_ability_remove_all_prompt_active == 0) {
            g_formation_ability_remove_cursor = -1;
            bcopy(g_formation_unit_pointers[g_unit_selected_index]->detail.abilities, g_formation_ability_remove_slots,
                sizeof(g_formation_ability_remove_slots));
            return 1;
        }
        return 1;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_LEFT) {
        formation_ability_diff_movement_stats(&g_item_preview_stat_detail, g_formation_ability_remove_slots[4], 0);
        formation_toggle_preview_stats_window(1);
        g_formation_ability_remove_all_prompt_active = 1;
        g_unit_panel_preview_stats_thread_params.redraw_request = 1;
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
    }
    slot = menu_selection_vertical_sound_step(
               4, SCREEN_CURSOR_REMOVE_ABILITY_SLOT, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH)
        + 1;
    cursor_y = &g_formation_ability_remove_cursor_point.y;
    slot_index = slot;
    *cursor_y = slot_index * 16 + 0x90;
    gfx_set_otag_index(0x28);
    screen_sliding_cursor_draw(
        &g_formation_ability_remove_cursor_point, &g_formation_ability_remove_cursor_anim, g_menu_input_lock_mode);
    /* Slot 1 holds a skillset id, the others ability ids; each kind has its own help text block. */
    ability = g_formation_unit_pointers[g_unit_selected_index]->detail.abilities[slot_index];
    g_help_message_text_id = ability;
    if (ability == 0) {
        g_help_message_text_id = -1;
    } else if (slot_index >= 2) {
        g_help_message_text_id = ability + HELP_TEXT_ID_ABILITY_BASE;
    } else {
        g_help_message_text_id = ability + HELP_TEXT_ID_SKILLSET_BASE;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        saved_slots = g_formation_ability_remove_slots;
        saved_slot = saved_slots + slot;
        sound = *saved_slot;
        if (sound == 0) {
            sound = SOUND_SFX_INVALID;
        } else {
            formation_ability_slot_set(g_unit_selected_index, slot, 0,
                equipment_support_change_check(
                    g_unit_selected_index, g_formation_unit_pointers[g_unit_selected_index]->detail.abilities[slot]));
            g_formation_ability_menu_slots_changed = 1;
            *saved_slot = 0;
            g_formation_ability_remove_cursor = -1;
            formation_stage_selected_unit();
            sound = SOUND_SFX_UNEQUIP;
        }
        g_sound_queued_effect_id = sound;
    }
    if (g_formation_ability_remove_cursor != slot) {
        g_formation_ability_remove_cursor = slot;
        preview = 0;
        if (slot == 4) {
            movement_ability = g_formation_ability_remove_slots[4];
            if (movement_ability >= ABILITY_ID_MOVEMENT_MOVE_PLUS_1
                && movement_ability <= ABILITY_ID_MOVEMENT_JUMP_PLUS_3) {
                formation_ability_diff_movement_stats(&g_item_preview_stat_detail, movement_ability, 0);
                g_unit_panel_preview_stats_thread_params.redraw_request = 1;
                preview = 1;
            }
        }
        formation_toggle_preview_stats_window(preview);
    }
    return 1;
}

/* One frame of the remove-all-abilities prompt: blink the cursors on the four ability
 * slots; Circle empties them all and Right backs out, both returning 0, else 1. */
static s32 removal_prompt_update(void) {
    formation_unit_t* unit;
    u16 value;
    s32 unused[16];
    s32 i;
    s32 changed;

    g_help_message_text_id = -1;
    gfx_set_otag_index(0x28);
    g_formation_ability_remove_all_blink_counter = g_formation_ability_remove_all_blink_counter + 1;
    if (g_formation_ability_remove_all_blink_counter & 1) {
        for (i = 1; i < 5; i++) {
            g_formation_ability_remove_all_cursor_point.y = i * 0x10 + 0x90;
            value = 0;
            screen_sliding_cursor_draw(&g_formation_ability_remove_all_cursor_point, &value, g_menu_input_lock_mode);
        }
    }
    if (g_menu_input_primary_repeat & PSX_PAD_RIGHT) {
        g_sound_queued_effect_id = SOUND_SFX_PAGE_SWITCH;
        return 0;
    }
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        changed = 0;
        for (i = 1; i < 5; i++) {
            unit = g_formation_unit_pointers[g_unit_selected_index];
            if (unit->detail.abilities[i] != 0) {
                changed = 1;
                formation_ability_slot_set(g_unit_selected_index, i, 0,
                    equipment_support_change_check(g_unit_selected_index, unit->detail.abilities[i]));
            }
        }
        formation_stage_selected_unit();
        if (changed == 0) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
        } else {
            g_sound_queued_effect_id = SOUND_SFX_UNEQUIP;
        }
        return 0;
    }
    return 1;
}

/*
 * Run the formation Set Ability menu for the selected unit.
 *
 * Return 0 when canceled and 1 while active. With the list closed the cursor
 * picks an ability slot and opens its candidate list (ability_list_build for the
 * secondary skillset, formation_ability_rsm_list_build otherwise);
 * with it open, the chosen ability is written to the slot and the Move+N
 * stat preview is refreshed.
 *
 * `cursor` is only assigned while the list has entries, as in the target,
 * which compares its stale register value when the list is empty. The inner
 * `(u16)` casts keep the Move+N range checks in unsigned-short arithmetic
 * (`li 0xfe1a` / `addu` / `andi 0xffff`).
 */
static s32 slot_menu_update(void) {
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

    if (g_formation_ability_set_unit_index != g_unit_selected_index) {
        g_formation_ability_set_initialized = 0;
        g_formation_ability_set_unit_index = g_unit_selected_index;
    }
    if (g_formation_ability_set_initialized == 0) {
        menu_selection_records_clear();
        g_formation_ability_set_slot = -1;
        g_formation_ability_set_initialized = 1;
        g_formation_ability_set_cursor = 0xFF;
        g_formation_ability_set_list_close_pending = 0;
        g_formation_ability_set_unread_flag = 0;
        g_formation_ability_set_list_open = 0;
        g_formation_unit_cycle_mode = 2;
        g_formation_ability_set_show_stat_preview = 0;
    }

    /* Cross leaves the menu from the slot cursor, or closes the open list below. */
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        if (g_formation_ability_set_list_open == 0) {
            thread_resume(SCREEN_THREAD_SLOT_UPPER_MENU);
            g_formation_ability_set_initialized = 0;
            screen_selection_results_reset();
            return 0;
        }
        g_formation_ability_set_list_close_pending = 1;
    }

    /* UNLOCKED is 1, so fold it to 0: any other nonzero result is a locked unit. */
    lock_state = formation_unit_lock_get(g_unit_selected_index);
    if (lock_state == FORMATION_UNIT_UNLOCKED) {
        lock_state = 0;
    }
    if (lock_state != 0) {
        formation_toggle_preview_stats_window(0);
        g_help_message_text_id = 0;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = SOUND_SFX_INVALID;
            return 1;
        }
        return 1;
    }

    if (g_formation_ability_set_list_open == 0) {
        /* Retail reads slot -1 on the first pass, reaching the preceding accessory item ID. */
        slot = g_formation_ability_set_slot;
        current = g_formation_unit_pointers[g_unit_selected_index]->detail.abilities[slot];
        g_formation_unit_cycle_mode = 2;
        /* Slot 1 holds a skillset id, the others ability ids; each kind has its own help text block. */
        g_help_message_text_id = current;
        if (current == 0) {
            g_help_message_text_id = -1;
        } else if (slot >= 2) {
            g_help_message_text_id = current + HELP_TEXT_ID_ABILITY_BASE;
        } else {
            g_help_message_text_id = current + HELP_TEXT_ID_SKILLSET_BASE;
        }

        slot = menu_selection_vertical_sound_step(
                   4, SCREEN_CURSOR_SET_ABILITY_SLOT, g_menu_input_secondary_repeat, SOUND_SFX_PAGE_SWITCH)
            + 1;
        g_formation_ability_set_slot = slot;
        if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            if (slot == 1) {
                g_formation_ability_set_entry_count = ability_list_build(g_unit_selected_index,
                    g_formation_unit_pointers[g_unit_selected_index]->identity.job_id, MENU_ABILITY_CATEGORY_ACTION,
                    g_item_list_entries, slot);
            } else {
                g_formation_ability_set_entry_count
                    = formation_ability_rsm_list_build(g_unit_selected_index, slot - 1, g_item_list_entries);
            }

            menu_set_vsync_mode_and_event_speed(2);
            g_formation_unit_cycle_mode = 1;
            g_formation_ability_set_list_open = 1;
            thread_suspend(SCREEN_THREAD_SLOT_UPPER_MENU);
            g_formation_ability_panel_thread_params.style = 1;
            g_formation_ability_set_cursor = 0xFF;
            menu_selection_record_restore(g_formation_ability_set_slot + 4, &x, &y, g_item_list_entries);
            value = TEXT_SECTION_ABILITY_NAMES;
            if (g_formation_ability_set_slot == 1) {
                value = TEXT_SECTION_SKILLSET_NAMES;
            }
            menu_init_scrollable_list(g_item_list_entries, x, y, g_text_section_pointers[value]);
        }
    } else {
        if (g_menu_list_entry_count != 0) {
            cursor = g_menu_list_selected_index;
            value = HELP_TEXT_ID_ABILITY_BASE;
            if (g_formation_ability_set_slot == 1) {
                value = HELP_TEXT_ID_SKILLSET_BASE;
            }
            g_help_message_text_id = value + (g_item_list_entries[cursor] & ABILITY_LIST_ENTRY_ID_MASK);
            if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
                selected = g_item_list_entries[cursor];
                if (selected & ABILITY_LIST_ENTRY_DISABLED) {
                    message_box_show_with_sound(0xD81D, SOUND_SFX_ERROR_MESSAGE);
                } else {
                    /* Revalidate equipment when either the new or the replaced ability is an equip support. */
                    value = equipment_support_change_check(g_unit_selected_index, selected);
                    current = g_formation_unit_pointers[g_unit_selected_index]
                                  ->detail.abilities[g_formation_ability_set_slot];
                    formation_ability_slot_set(g_unit_selected_index, g_formation_ability_set_slot, selected,
                        value | equipment_support_change_check(g_unit_selected_index, current));
                    g_sound_queued_effect_id = SOUND_SFX_EQUIP;
                    g_formation_ability_menu_slots_changed = 1;
                    g_formation_ability_set_list_close_pending = 1;
                    formation_stage_selected_unit();
                }
            }
        } else {
            g_help_message_text_id = -1;
            if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
                if (g_formation_ability_set_slot != 1) {
                    message_box_show_with_sound(0xC006, SOUND_SFX_ERROR_MESSAGE);
                } else {
                    g_sound_queued_effect_id = SOUND_SFX_INVALID;
                }
            }
        }

        /* Preview Move and Jump when the highlighted or the slot's current ability is a Move+/Jump+ one. */
        if (g_formation_ability_set_cursor != cursor) {
            g_formation_ability_set_cursor = cursor;
            if ((g_item_list_entries[cursor] >= ABILITY_ID_MOVEMENT_MOVE_PLUS_1
                    && g_item_list_entries[cursor] <= ABILITY_ID_MOVEMENT_JUMP_PLUS_3)
                || (u16)(g_formation_unit_pointers[g_unit_selected_index]
                             ->detail.abilities[g_formation_ability_set_slot]
                       - ABILITY_ID_MOVEMENT_MOVE_PLUS_1)
                    < 6) {
                formation_ability_diff_movement_stats(&g_item_preview_stat_detail,
                    g_formation_unit_pointers[g_unit_selected_index]->detail.abilities[g_formation_ability_set_slot],
                    g_item_list_entries[g_menu_list_selected_index]);
                g_unit_panel_preview_stats_thread_params.redraw_request = 1;
                g_formation_ability_set_show_stat_preview = 1;
            } else {
                g_formation_ability_set_show_stat_preview = 0;
            }
        }
        menu_run_script_with_palette_mode(
            g_formation_ability_set_window_script, g_menu_input_primary_repeat, menu_input_lock_mode_get(0));
    }

    if (g_formation_ability_set_list_close_pending != 0) {
        menu_set_window_scale_step(0);
        g_formation_ability_panel_thread_params.style = 0;
        g_formation_ability_set_list_open = 0;
        g_formation_unit_cycle_mode = 2;
        menu_set_vsync_mode_and_event_speed(0);
        g_formation_ability_set_show_stat_preview = 0;
        g_unit_panel_status_display_thread_params.redraw_request = 1;
        menu_selection_record_set(g_formation_ability_set_slot + 4, g_menu_list_selected_index,
            g_menu_scroll_base_index, g_item_list_entries);
        thread_resume(SCREEN_THREAD_SLOT_UPPER_MENU);
        g_formation_ability_set_list_close_pending = 0;
    }

    busy = 0;
    formation_toggle_preview_stats_window(g_formation_ability_set_show_stat_preview);
    g_formation_ability_set_cursor_point.y = g_formation_ability_set_slot * 16 + 0x90;
    gfx_set_otag_index(0x28);
    if (menu_input_lock_mode_get(0) != 0 || g_formation_ability_set_list_open != 0) {
        busy = 1;
    }
    screen_sliding_cursor_draw(&g_formation_ability_set_cursor_point, &g_formation_ability_set_cursor_anim, busy);
    return 1;
}

/* Learn menu processing: opens the menu on first entry, then runs either the job list
 * (MENU_JOB_LIST_STEP_JOBS) or the chosen job's ability list (MENU_JOB_LIST_STEP_ABILITIES).
 * Returns 0 once the menu has been closed, saving the records to the party. */
static s32 learn_menu_update(void) {
    s32 result;

    if (g_formation_ability_learn_menu_initialized == 0) {
        g_formation_ability_learn_menu_step = MENU_JOB_LIST_STEP_JOBS;
        g_formation_unit_banner_enabled = 0;
        menu_request_thread_stop(THREAD_SCREEN_SLOT_NUMERIC_PANEL);
        menu_request_thread_stop(SCREEN_THREAD_SLOT_EQUIP_ABILITY_PANEL);
        g_formation_unit_cycle_mode = 0;
        screen_cursor_position_init(SCREEN_CURSOR_ABILITY_TAB, 0);
        menu_selection_record_clear(9);
        g_formation_ability_learn_menu_initialized = 1;
    }
    if (g_formation_ability_learn_menu_step == MENU_JOB_LIST_STEP_JOBS) {
        result = formation_ability_job_list_update();
        if (result == MENU_JOB_LIST_STEP_ABILITIES) {
            menu_scrollable_list_core_init(0, 0, 0);
            g_formation_ability_learn_menu_step = MENU_JOB_LIST_STEP_ABILITIES;
        } else if (result == MENU_JOB_LIST_STEP_CLOSE) {
            g_formation_ability_learn_menu_initialized = 0;
            g_formation_unit_banner_enabled = 1;
            formation_save_records();
            return 0;
        }
    } else {
        g_formation_ability_learn_menu_step = formation_ability_learn_list_update();
    }
    return 1;
}

/* Return whether a support change requires equipment revalidation.
 *
 * The preview record is excluded. With Max Equip at Job Change enabled, the
 * eight equip-type supports, Martial Arts, Two Hands and Two Swords can
 * invalidate the current loadout. */
static s32 equipment_support_change_check(s16 unit_id, s16 ability_id) {
    if (unit_id == FORMATION_PREVIEW_UNIT_INDEX || g_game_options.fields.max_equip_at_job_change != GAME_OPTION_ON)
        return 0;
    if (ability_id >= ABILITY_ID_SUPPORT_EQUIP_ARMOR && ability_id < ABILITY_ID_SUPPORT_EQUIP_ARMOR + 8)
        return 1;
    if (ability_id == ABILITY_ID_SUPPORT_MARTIAL_ARTS || ability_id == ABILITY_ID_SUPPORT_TWO_HANDS)
        return 1;
    return ability_id == ABILITY_ID_SUPPORT_TWO_SWORDS;
}

/* Return the menu input lock mode; nonzero while another window holds the input. The argument is
 * ignored. */
s32 menu_input_lock_mode_get(s32 unused_0) {
    return g_menu_input_lock_mode;
}

/* Job level of the selected unit in the Learn Abilities menu's `index`th
 * unlocked job; levels are packed two per byte (odd jobs in the low nibble). */
s32 job_list_level_calculate(s32 index) {
    s32 job_index = job_generic_index_get(g_job_list_job_ids[index]);

    g_job_list_job_current_level = g_formation_unit_pointers[g_unit_selected_index]->job_levels[job_index >> 1];
    if (job_index & 1) {
        g_job_list_job_current_level = g_job_list_job_current_level & 0xF;
    } else {
        g_job_list_job_current_level = g_job_list_job_current_level >> 4;
    }
    return g_job_list_job_current_level;
}

/* JP the selected unit has in the Learn Abilities menu's `index`th unlocked job. */
static s32 job_jp_get(s32 index) {
    s32 job_index = job_generic_index_get(g_job_list_job_ids[index]);

    return g_formation_unit_pointers[g_unit_selected_index]->job_points.current.values[job_index];
}

/* Caches and returns the selected unit's total JP for the Learn Abilities
 * menu's `index`th unlocked job; the job list's menu script reads it as a
 * number callback. */
s32 job_list_total_jp_calculate(s32 index) {
    s32 generic_job = job_generic_index_get(g_job_list_job_ids[index]);

    return g_formation_ability_learn_job_total_jp
        = g_formation_unit_pointers[g_unit_selected_index]->job_points.total.values[generic_job];
}

/* JP needed for the next level of the job whose level
 * the current-level callback last cached: the table entry for levels
 * below 8, otherwise the previous value with the dashes flag set. The
 * `index`th job is looked up but its result is not used. */
s32 job_list_next_level_jp_calculate(s32 index) {
    s32 level;

    job_generic_index_get(g_job_list_job_ids[index]);
    level = g_job_list_job_current_level & 0xf;
    if (level >= UNIT_JOB_LEVEL_MAX) {
        g_job_list_job_next_level_jp_requirement |= MENU_NUMBER_VALUE_DASHES;
    } else {
        g_job_list_job_next_level_jp_requirement = g_job_jp_requirements_by_level[level + 1];
    }
    return g_job_list_job_next_level_jp_requirement;
}

/* Caches and returns whether the `index`th job's skillset has nothing left
 * for the selected unit to learn (no unlearned ability counted). */
s32 job_list_mastered_calculate(s32 index) {
    g_job_current_mastered = ability_list_build(g_unit_selected_index, g_job_list_job_ids[index],
                                 MENU_ABILITY_CATEGORY_ALL, 0, ABILITY_LIST_MODE_COUNT_UNLEARNED)
        == 0;
    return g_job_current_mastered;
}

/* Runs the learn menu's job list for the selected formation unit.
 *
 * The first call builds the unlocked-job list and installs the job
 * level/JP menu-script callbacks. Returns -1 when canceled, 1 when a job
 * with learnable abilities is confirmed (caching its id and generic job
 * index), and 0 while the list is still open. */
s32 formation_ability_job_list_update(void) {
    s16 x;
    s16 y;

    if (g_formation_ability_learn_job_list_initialized == 0) {
        menu_set_vsync_mode_and_event_speed(2);
        job_list_build(g_unit_selected_index, g_job_list_job_ids, 0);
        g_menu_row_callbacks[0].value = job_list_level_calculate;
        g_menu_row_callbacks[2].value = job_jp_get;
        g_menu_row_callbacks[5].value = job_list_next_level_jp_calculate;
        g_menu_row_callbacks[6].value = job_list_total_jp_calculate;
        g_menu_row_callbacks[13].value = job_list_mastered_calculate;
        menu_selection_record_restore(9, &x, &y, g_job_list_job_ids);
        menu_init_scrollable_list(g_job_list_job_ids, x, y, g_text_section_pointers[TEXT_SECTION_JOB_NAMES]);
        g_formation_ability_learn_job_list_initialized = 1;
    }
    g_help_message_text_id = g_job_list_job_ids[g_menu_list_selected_index] + 0x6000;
    if (g_menu_input_primary_repeat & PSX_PAD_CROSS) {
        menu_set_vsync_mode_and_event_speed(0);
        g_sound_queued_effect_id = SOUND_SFX_CANCEL;
        g_formation_ability_learn_job_list_initialized = 0;
        return MENU_JOB_LIST_STEP_CLOSE;
    }
    /* A mastered job has nothing left to learn, so only View Abilities may open it. */
    if (g_menu_input_primary_repeat & PSX_PAD_CIRCLE) {
        if (job_list_mastered_calculate(g_menu_list_selected_index) == 0
            || g_formation_current_menu == FORMATION_MENU_VIEW_ABILITIES) {
            g_formation_ability_learn_selected_job = g_job_list_job_ids[g_menu_list_selected_index];
            g_formation_ability_learn_selected_job_index
                = job_generic_index_get(g_formation_ability_learn_selected_job);
            job_list_level_calculate(g_menu_list_selected_index);
            job_jp_get(g_menu_list_selected_index);
            job_list_total_jp_calculate(g_menu_list_selected_index);
            job_list_next_level_jp_calculate(g_menu_list_selected_index);
            menu_selection_record_set(9, g_menu_list_selected_index, g_menu_scroll_base_index, g_job_list_job_ids);
            g_formation_ability_learn_job_list_initialized = 0;
            g_sound_queued_effect_id = SOUND_SFX_CONFIRM;
            return MENU_JOB_LIST_STEP_ABILITIES;
        }
        message_box_show_with_sound(0xC007, SOUND_SFX_ERROR_MESSAGE);
    }
    menu_run_script_with_palette_mode(
        g_formation_ability_learn_job_list_script, g_menu_input_primary_repeat, g_menu_input_lock_mode);
    return MENU_JOB_LIST_STEP_JOBS;
}

/* Store the selected ability-name text ID, then build the prompt. */
void formation_ability_learn_prompt_build(void) {
    g_text_substitution_values[0]
        = g_ability_list_ability_entries[g_menu_list_selected_index] + TEXT_ID_ABILITY_NAME_BASE;
    menu_icon_linked_entry_thread();
}

/* Menu callback: the selected formation unit's JP in the job chosen in the
 * learn menu. The row argument is not read. */
s32 formation_ability_chosen_job_jp_get(s32 unused_row) {
    return g_formation_unit_pointers[g_unit_selected_index]
        ->job_points.current.values[g_formation_ability_learn_selected_job_index];
}

/* Row callback: return the cached level of the job chosen in the learn menu; the row is ignored. */
s32 formation_ability_chosen_job_level_get(s32 unused_row) {
    return g_job_list_job_current_level;
}

/* Row callback: return the cached JP the chosen job needs for its next level; the row is ignored. */
s32 formation_ability_chosen_job_next_jp_get(s32 unused_row) {
    return g_job_list_job_next_level_jp_requirement;
}

/* Row callback: return the cached total JP earned in the chosen job; the row is ignored. */
s32 formation_ability_chosen_job_total_jp_get(s32 unused_row) {
    return g_formation_ability_learn_job_total_jp;
}

/* Row callback: return whether the chosen job is mastered (nothing left to learn); the row is
 * ignored. */
s32 formation_ability_is_chosen_job_mastered(s32 unused_row) {
    return g_job_current_mastered;
}
