#include "fft/event_require.h"
#include "psx/pad.h"
#include "psx/types.h"

void battle_thread_start(s32 thread_id, void (*function)(void));

/* Applies the post-battle party changes for each affected battle unit.
 *
 * Shows each unit's status panel, lets the player choose whether to recruit
 * it (removing a party member first when the roster is full), then saves it
 * to the party or updates its bio variable. Restores the saved thread block
 * and menu descriptor on exit.
 */
void require_apply_post_battle_unit_changes(void) {
    s16 text_width_px;
    s16 text_height_px;
    world_menu_entry_t* saved_menu_descriptor;
    s32 affected_index;
    s32 transition;
    s32* initial_unit_index_ptr;
    s32* unit_index_ptr;
    s32* selected_unit_index_ptr;
    u8* menu_base;
    u8* menu_fields;
    u8* unit_display_data;
    s32 one;
    battle_stats_t* unit;
    s32 input;
    s32 thread_id;
    s32 text_width;
    s16 menu_x;
    s32 half_menu_x;
    u8* text_data;
    s32* transition_c_ptr;
    s32 screen_fade_max;

    if (require_party_find_join_candidates() == 0) {
        battle_thread_exit_current();
    }
    g_battle_post_battle_unit_changes_active = 1;
    battle_thread_suspend(1);
    battle_copy_bytes(g_require_saved_thread_block, &g_battle_threads[1], 0x400);

    text_data = g_require_text_data;
    g_battle_text_section_pointers[1] = text_data + g_require_text_section_offsets[1];
    g_battle_text_section_pointers[2] = text_data + g_require_text_section_offsets[2];
    saved_menu_descriptor = g_battle_menu_thread_menu_data;
    menu_base = g_require_join_menu_descriptor;
    g_battle_menu_thread_menu_data = (world_menu_entry_t*)menu_base;
    g_battle_text_section_pointers[3] = text_data + g_require_text_section_offsets[3];
    g_battle_text_section_pointers[6] = text_data + g_require_text_section_offsets[4];

    affected_index = 0;
    require_text_build_battle_nicknames();
    initial_unit_index_ptr = g_require_party_affected_battle_unit_indices;
    selected_unit_index_ptr = (s32*)g_require_active_unit_data;
    battle_unit_get_stats_from_battle_id(*initial_unit_index_ptr);
    *(u16*)(g_require_panel_active_unit_banner + 0x52) = *(u16*)initial_unit_index_ptr;
    *(u16*)selected_unit_index_ptr = *(u16*)initial_unit_index_ptr;
    require_editor_load_selected_unit();
    *(u16*)(g_require_panel_active_unit_banner + 0xa) = *(u16*)initial_unit_index_ptr;

    /* `affected_index < count` (not `count > 0`): combine folds the known
     * zero into a `blez` and leaves the eliminated compare pseudo with a
     * flow reference count, which gives one of the target's two unreferenced
     * 8-byte frame slots. */
    if (affected_index < g_require_party_affected_unit_count) {
        transition_c_ptr = &g_require_character_status_frame_config;
        unit_display_data = (u8*)selected_unit_index_ptr;
        one = 1;
        menu_fields = menu_base + 4;
        /* Hide the constants from local-alloc, or the s3-s5 assignment rotates. */
        __asm__("" : "=r"(one) : "0"(one));
        __asm__("" : "=r"(menu_fields) : "0"(menu_fields));
        unit_index_ptr = g_require_party_affected_battle_unit_indices;
        do {
            require_panel_set_transition_value(0);
            g_require_status_display_thread_params = -0x100;
            g_require_numeric_editor_thread_params = -0x100;
            *transition_c_ptr = -0x100;
            battle_action_copy_active_turn_data_from(
                g_require_panel_active_unit_banner, g_require_active_unit_data, g_require_panel_billboard_data);

            /* This zero feeds the else-arm fade and fills the `bnez` delay
             * slot; the separate zero before the second fade is copied into
             * the if-arm's `j` slot by reorg (after jump2 could have deleted
             * an arm-local duplicate) and moved into the else arm's first
             * call slot on reorg's second pass. */
            transition = 0;
            if (affected_index == 0) {
                battle_thread_start(8, require_render_unit_status_panel_thread);
                battle_thread_set_parameters(8, (s32)&g_require_status_display_thread_params, 0, 0);
                battle_thread_start(12, require_editor_run_numeric_thread);
                battle_thread_set_parameters(12, (s32)&g_require_numeric_editor_thread_params, 0, 0);
                battle_thread_start(9, require_panel_run_character_status_thread);
                battle_thread_set_parameters(9, (s32)transition_c_ptr, 0, 0);
            } else {
                u16 index_value;

                do {
                    g_require_status_display_thread_params = transition;
                    g_require_numeric_editor_thread_params = transition;
                    g_require_character_status_frame_config = transition;
                    transition += 0x20;
                    battle_thread_yield();
                } while (transition < 0x101);

                battle_unit_get_stats_from_battle_id(*unit_index_ptr);
                index_value = *(u16*)unit_index_ptr;
                *(u16*)(unit_display_data + 0x0a) = index_value;
                *(u16*)(unit_display_data + 0) = index_value;
                require_editor_load_selected_unit();
                battle_action_copy_active_turn_data_from(
                    g_require_panel_active_unit_banner, unit_display_data, g_require_panel_billboard_data);
                index_value = *(u16*)unit_index_ptr;
                g_require_status_display_redraw_request = one;
                g_require_numeric_editor_redraw_request = one;
                g_require_character_status_redraw_request = one;
                *(u16*)(g_require_panel_active_unit_banner + 0xa) = index_value;
                battle_thread_yield();
            }

            transition = 0;
            do {
                g_require_status_display_thread_params = transition - 0x100;
                g_require_numeric_editor_thread_params = transition - 0x100;
                g_require_character_status_frame_config = transition - 0x100;
                transition += 0x20;
                battle_thread_yield();
            } while (transition < 0x101);

            /* The removal retry re-enters the selection loop and the
             * not-joining retry re-enters the removal loop from outside, so
             * loop.c treats both loops as having multiple entries and hoists
             * nothing out of them (the g_require_panel_active_unit_banner + 0xa address stays in the
             * remove path), while flow still weights their references by
             * depth. The not-joining retry repeats the selected-unit
             * assignment; jump2 cross-jumps it into this one, so the
             * `bne v0,s4` at +0x6a4 re-enters at +0x2d0, one instruction
             * before the two retry_selection entries at +0x2d4. */
            selected_unit_index_ptr = unit_index_ptr;
            do {
            retry_selection:
                while (1) {
                    require_panel_set_transition_value(0);
                    do {
                        battle_thread_yield();
                        g_require_input_controller = battle_script_get_controller_input_pointer(0);
                        input = *g_require_input_controller;
                        if (input & PSX_PAD_CIRCLE) {
                            break;
                        }
                        if (input & PSX_PAD_SELECT) {
                            thread_id = battle_thread_resolve_id(16);
                            battle_thread_start(thread_id, require_overlay_run_helpmenu);
                            battle_thread_wait_until_inactive(thread_id);
                        }
                    } while (1);

                    require_panel_set_transition_value(1);
                    g_battle_text_substitution_values[0] = *selected_unit_index_ptr;
                    battle_text_measure_pixels(&text_width_px, &text_height_px, battle_text_init_entry(0x5006));
                    /* The s16 sum reads the halfword directly (a second copy of the
                     * loaded width) while the modulo uses the widened s32 value; the
                     * narrowing also leaves the other unreferenced frame slot. */
                    text_width = text_width_px;
                    half_menu_x = text_width % 4 + 0x18;
                    menu_x = text_width_px + half_menu_x;
                    half_menu_x = menu_x / 2;
                    *(s16*)(menu_fields + 0x10) = menu_x;
                    *(s16*)(menu_fields + 0x08) = menu_x;
                    *(s16*)(menu_fields + 0x00) = menu_x;
                    *(s16*)(menu_fields + 0x04) = 0x100 - half_menu_x + menu_x % 2;
                    if (require_party_get_free_slot_status(*selected_unit_index_ptr) == 2) {
                        g_require_message_menu_result = 0;
                    } else {
                        g_require_menu_selection_result = 0xff;
                        battle_thread_start(7, require_menu_run_simple_selection_thread);
                        battle_thread_set_parameters(7, (s32)(menu_fields - 4), 0, 0);
                        if (g_require_editor_unit_fields.state.generic_monster != 0) {
                            battle_thread_start(5, battle_menu_run_icon_selection_loop);
                            battle_thread_set_parameters(5, (s32)(menu_fields + 0x1dc), 0, 0);
                        } else {
                            battle_thread_start(5, battle_menu_run_icon_selection_loop);
                            battle_thread_set_parameters(5, (s32)(menu_fields + 0x38), 0, 0);
                        }
                    }

                    require_menu_wait_selection_threads();
                    if (g_require_menu_selection_result != -1) {
                        break;
                    }
                    require_panel_set_transition_value(0);
                }
                if (g_require_menu_selection_result != 0) {
                    goto not_joining;
                }
                if (require_party_get_free_slot_status(*selected_unit_index_ptr) != 0) {
                    break;
                }
                {
                    u16 index_value;
                    u16 display_index;
                    s32 removal_result;
                    s32 removal_unit_index;

                    g_battle_screen_fade = 0;
                    do {
                        battle_thread_yield();
                        g_battle_screen_fade += 0x10;
                    } while (g_battle_screen_fade < 0x100);
                    screen_fade_max = 0xff;
                    g_battle_screen_fade = screen_fade_max;
                    g_battle_post_battle_unit_changes_active = 0;
                    removal_result = require_select_party_unit_to_remove();
                    /* The target loads *selected_unit_index_ptr at 0x514,
                     * above both global stores. GCC 2.6.3 emits a
                     * straight-line block in strict source order, so the
                     * load has to be written above them. */
                    removal_unit_index = *selected_unit_index_ptr;
                    g_require_party_removal_result = removal_result;
                    g_battle_post_battle_unit_changes_active = one;

                    battle_unit_get_stats_from_battle_id(removal_unit_index);
                    /* The target reads the index into v0 here and reloads it
                     * into v1 after the call; one shared local is one allocno
                     * and takes v1 at both sites. */
                    display_index = *(u16*)selected_unit_index_ptr;
                    *(u16*)(unit_display_data + 0x0a) = display_index;
                    *(u16*)(unit_display_data + 0) = display_index;
                    require_editor_load_selected_unit();
                    index_value = *(u16*)selected_unit_index_ptr;
                    g_require_status_display_redraw_request = one;
                    g_require_numeric_editor_redraw_request = one;
                    g_require_character_status_redraw_request = one;
                    *(u16*)(g_require_panel_active_unit_banner + 0xa) = index_value;
                    if (g_require_party_removal_result == 0) {
                        require_panel_set_transition_value(1);
                    }
                    battle_action_copy_active_turn_data_from(
                        g_require_panel_active_unit_banner, unit_display_data, g_require_panel_billboard_data);
                    g_battle_screen_fade = screen_fade_max;
                    do {
                        battle_thread_yield();
                        g_battle_screen_fade -= 0x10;
                    } while (g_battle_screen_fade > 0);
                    g_battle_screen_fade = 0;
                }
            } while (g_require_party_removal_result != 0);
            if (require_party_sell_equipment_over_limit(*selected_unit_index_ptr) != 0) {
                battle_thread_start(5, battle_menu_icon_linked_entry_thread);
                battle_thread_set_parameters(5, (s32)&g_require_party_removal_menus[5], 0, 0);
                battle_thread_wait_until_inactive(5);
            }
            battle_thread_wait_frames(20);
            battle_thread_start(5, require_render_display_condition_special_cases_thread);
            battle_thread_set_parameters(5, 10, 0, 0);
            require_input_wait_frames_or_skip(60);
            battle_thread_set_parameters(5, 0, 0, 2);
            battle_thread_wait_until_inactive(5);
            require_panel_set_transition_value(0);

            unit = battle_unit_get_stats_from_battle_id(*selected_unit_index_ptr);
            unit_index_ptr++;
            main_party_save_unit(unit, unit->unit_flags & 1);
            battle_thread_wait_frames(20);
            goto next_unit;
        not_joining:
            if (g_require_menu_selection_result != one) {
                selected_unit_index_ptr = unit_index_ptr;
                goto retry_selection;
            }
            require_panel_set_transition_value(0);
            battle_thread_wait_frames(10);
            unit = battle_unit_get_stats_from_battle_id(*selected_unit_index_ptr);
            unit_index_ptr++;
            require_party_update_bio_variable_for_unit_class(unit->character_identity, 12);
        next_unit:
            affected_index++;
        } while (affected_index < g_require_party_affected_unit_count);
    }

    transition = 0;
    do {
        g_require_status_display_thread_params = transition;
        g_require_numeric_editor_thread_params = transition;
        g_require_character_status_frame_config = transition;
        transition += 0x20;
        battle_thread_yield();
    } while (transition < 0x101);
    battle_thread_set_parameters(8, 0, 0, 1);
    battle_thread_set_parameters(12, 0, 0, 1);
    battle_thread_set_parameters(9, 0, 0, 1);
    battle_thread_wait_until_inactive(8);
    battle_thread_wait_until_inactive(12);
    battle_thread_wait_until_inactive(9);
    g_battle_post_battle_unit_changes_active = 0;
    battle_copy_bytes(&g_battle_threads[1], g_require_saved_thread_block, 0x400);
    g_battle_menu_thread_menu_data = saved_menu_descriptor;
    battle_thread_resume(1);
    battle_thread_yield();
    battle_thread_exit_current();
}
