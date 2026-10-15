#include "fft/battle_gfx.h"
#include "fft/battle_menu.h"
#include "fft/battle_script.h"
#include "fft/event_helpmenu.h"
#include "fft/event_require.h"
#include "fft/gfx.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/sound.h"
#include "fft/thread.h"
#include "psx/libgpu.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

/* Applies the post-battle party changes for each affected battle unit.
 *
 * Shows each unit's status panel, lets the player choose whether to recruit
 * it (removing a party member first when the roster is full), then saves it
 * to the party or updates its bio variable. Restores the saved thread block
 * and menu descriptor on exit.
 */
void require_party_apply_post_battle_changes(void) {
    s16 text_width_px;
    s16 text_height_px;
    menu_entry_t* saved_menu_descriptor;
    s32 affected_index;
    s32 transition;
    s32* initial_unit_index_ptr;
    s32* unit_index_ptr;
    menu_active_unit_t* initial_unit_data;
    s32* selected_unit_index_ptr;
    menu_entry_t* menu_base;
    u8* menu_fields;
    menu_active_unit_t* unit_display_data;
    s32 one;
    unit_stats_t* unit;
    s32 input;
    s32 thread_id;
    s32 text_width;
    s16 menu_x;
    s32 half_menu_x;
    u8* text_data;
    s32* transition_c_ptr;
    s32 screen_fade_max;

    if (require_party_save_battle_units_and_find_join_candidates() == 0) {
        thread_exit_current();
    }
    g_menu_post_battle_unit_changes_active = 1;
    thread_suspend(THREAD_SLOT_EVENT);
    mem_copy_bytes_nonempty(g_require_saved_thread_block, &g_threads[THREAD_SLOT_EVENT], 0x400);

    text_data = g_text_data;
    g_text_section_pointers[REQUIRE_TEXT_SECTION_BRAVE_FAITH_WARNINGS] = text_data + g_text_section_offsets[1];
    g_text_section_pointers[REQUIRE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES] = text_data + g_text_section_offsets[2];
    saved_menu_descriptor = g_menu_thread_menu_data;
    menu_base = g_require_join_menu_descriptor;
    g_menu_thread_menu_data = menu_base;
    g_text_section_pointers[REQUIRE_TEXT_SECTION_DISMISS_UNIT_REPLIES] = text_data + g_text_section_offsets[3];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = text_data + g_text_section_offsets[4];

    affected_index = 0;
    require_text_build_battle_nicknames();
    initial_unit_index_ptr = g_require_party_affected_battle_unit_indices;
    initial_unit_data = &g_menu_selected_unit;
    battle_unit_get_stats_from_battle_id(*initial_unit_index_ptr);
    g_menu_selected_unit.battle_id = *initial_unit_index_ptr;
    initial_unit_data->name_id = *initial_unit_index_ptr;
    menu_load_selected_unit_variant();
    g_menu_selected_unit_status.unit_index = *initial_unit_index_ptr;

    /* `affected_index < count` (not `count > 0`): combine folds the known
     * zero into a `blez` and leaves the eliminated compare pseudo with a
     * flow reference count, which gives one of the target's two unreferenced
     * 8-byte frame slots. */
    if (affected_index < g_require_party_affected_unit_count) {
        transition_c_ptr = &g_require_character_status_origin_x;
        unit_display_data = initial_unit_data;
        one = 1;
        menu_fields = (u8*)&menu_base->inner_width;
        /* Hide the constants from local-alloc, or the s3-s5 assignment rotates. */
        __asm__("" : "=r"(one) : "0"(one));
        __asm__("" : "=r"(menu_fields) : "0"(menu_fields));
        unit_index_ptr = g_require_party_affected_battle_unit_indices;
        do {
            require_panel_set_transition_value(0);
            g_require_status_display_origin_x = -0x100;
            g_require_numeric_editor_origin_x = -0x100;
            *transition_c_ptr = -0x100;
            menu_copy_billboard_data_in(
                &g_menu_selected_unit_status, &g_menu_selected_unit, &g_menu_selected_unit_fields);

            /* This zero feeds the else-arm fade and fills the `bnez` delay
             * slot; the separate zero before the second fade is copied into
             * the if-arm's `j` slot by reorg (after jump2 could have deleted
             * an arm-local duplicate) and moved into the else arm's first
             * call slot on reorg's second pass. */
            transition = 0;
            if (affected_index == 0) {
                thread_start(THREAD_SCREEN_SLOT_UNIT_STATUS, menu_run_unit_status_thread_variant);
                thread_set_parameters(
                    THREAD_SCREEN_SLOT_UNIT_STATUS, (s32)&g_require_status_display_thread_params, 0, 0);
                thread_start(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, menu_run_numeric_editor_thread);
                thread_set_parameters(
                    THREAD_SCREEN_SLOT_NUMERIC_EDITOR, (s32)&g_require_numeric_editor_thread_params, 0, 0);
                thread_start(THREAD_SCREEN_SLOT_CHARACTER_STATUS, menu_run_status_panel_thread);
                thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, (s32)transition_c_ptr, 0, 0);
            } else {
                u16 index_value;

                do {
                    g_require_status_display_origin_x = transition;
                    g_require_numeric_editor_origin_x = transition;
                    g_require_character_status_origin_x = transition;
                    transition += 0x20;
                    thread_yield();
                } while (transition < 0x101);

                battle_unit_get_stats_from_battle_id(*unit_index_ptr);
                index_value = *unit_index_ptr;
                unit_display_data->battle_id = index_value;
                unit_display_data->name_id = index_value;
                menu_load_selected_unit_variant();
                menu_copy_billboard_data_in(
                    &g_menu_selected_unit_status, unit_display_data, &g_menu_selected_unit_fields);
                index_value = *unit_index_ptr;
                g_status_display_redraw_request = one;
                g_numeric_editor_redraw_request = one;
                g_character_status_redraw_request = one;
                g_menu_selected_unit_status.unit_index = index_value;
                thread_yield();
            }

            transition = 0;
            do {
                g_require_status_display_origin_x = transition - 0x100;
                g_require_numeric_editor_origin_x = transition - 0x100;
                g_require_character_status_origin_x = transition - 0x100;
                transition += 0x20;
                thread_yield();
            } while (transition < 0x101);

            /* The removal retry re-enters the selection loop and the
             * not-joining retry re-enters the removal loop from outside, so
             * loop.c treats both loops as having multiple entries and hoists
             * nothing out of them (the g_menu_selected_unit_status.unit_index address stays in the
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
                        thread_yield();
                        g_menu_input_controller = script_get_controller_input_pointer(0);
                        input = *g_menu_input_controller;
                        if (input & PSX_PAD_CIRCLE) {
                            break;
                        }
                        if (input & PSX_PAD_SELECT) {
                            thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
                            thread_start(thread_id, require_overlay_run_helpmenu);
                            thread_wait_until_inactive(thread_id);
                        }
                    } while (1);

                    require_panel_set_transition_value(1);
                    g_text_substitution_values[0] = *selected_unit_index_ptr;
                    text_measure_pixels(&text_width_px, &text_height_px, text_find_entry(0x5006));
                    /* The s16 sum reads the 16-bit value directly (a second copy of the
                     * loaded width) while the modulo uses the widened s32 value; the
                     * narrowing also leaves the other unreferenced frame slot. */
                    text_width = text_width_px;
                    half_menu_x = text_width % 4 + 0x18;
                    menu_x = text_width_px + half_menu_x;
                    half_menu_x = menu_x / 2;
                    /* A recovered struct pointer hoists the owner address across the call (two instructions differ).
                     * Derive each byte displacement from the named field instead. */
#define MENU_FIELD(field)                                                                                              \
    (*(s16*)(menu_fields + (unsigned long)&((menu_entry_t*)0)->field - (unsigned long)&((menu_entry_t*)0)->inner_width))
                    MENU_FIELD(text_image.width) = menu_x;
                    MENU_FIELD(window.w) = menu_x;
                    MENU_FIELD(inner_width) = menu_x;
                    MENU_FIELD(window.x) = 0x100 - half_menu_x + menu_x % 2;
#undef MENU_FIELD
                    if (require_party_get_free_slot_status(*selected_unit_index_ptr) == 2) {
                        g_require_message_menu_result = 0;
                    } else {
                        g_require_menu_selection_result = 0xff;
                        thread_start(REQUIRE_THREAD_SLOT_MESSAGE, menu_icon_strip_thread);
                        thread_set_parameters(REQUIRE_THREAD_SLOT_MESSAGE,
                            (s32)(menu_fields - (unsigned long)&((menu_entry_t*)0)->inner_width), 0, 0);
                        if (g_menu_selected_unit_fields.state.generic_monster != 0) {
                            thread_start(REQUIRE_THREAD_SLOT_MENU, menu_run_icon_selection_loop);
                            thread_set_parameters(REQUIRE_THREAD_SLOT_MENU,
                                (s32)(menu_fields + 8 * sizeof(*menu_base)
                                    - (unsigned long)&((menu_entry_t*)0)->inner_width),
                                0, 0);
                        } else {
                            thread_start(REQUIRE_THREAD_SLOT_MENU, menu_run_icon_selection_loop);
                            thread_set_parameters(REQUIRE_THREAD_SLOT_MENU,
                                (s32)(menu_fields + sizeof(*menu_base)
                                    - (unsigned long)&((menu_entry_t*)0)->inner_width),
                                0, 0);
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

                    g_gfx_screen_fade = 0;
                    do {
                        thread_yield();
                        g_gfx_screen_fade += 0x10;
                    } while (g_gfx_screen_fade < 0x100);
                    screen_fade_max = 0xff;
                    g_gfx_screen_fade = screen_fade_max;
                    g_menu_post_battle_unit_changes_active = 0;
                    removal_result = require_party_select_unit_to_remove();
                    /* The target loads *selected_unit_index_ptr at 0x514,
                     * above both global stores. GCC 2.6.3 emits a
                     * straight-line block in strict source order, so the
                     * load has to be written above them. */
                    removal_unit_index = *selected_unit_index_ptr;
                    g_require_party_removal_result = removal_result;
                    g_menu_post_battle_unit_changes_active = one;

                    battle_unit_get_stats_from_battle_id(removal_unit_index);
                    /* The target reads the index into v0 here and reloads it
                     * into v1 after the call; one shared local is one allocno
                     * and takes v1 at both sites. */
                    display_index = *selected_unit_index_ptr;
                    unit_display_data->battle_id = display_index;
                    unit_display_data->name_id = display_index;
                    menu_load_selected_unit_variant();
                    index_value = *selected_unit_index_ptr;
                    g_status_display_redraw_request = one;
                    g_numeric_editor_redraw_request = one;
                    g_character_status_redraw_request = one;
                    g_menu_selected_unit_status.unit_index = index_value;
                    if (g_require_party_removal_result == 0) {
                        require_panel_set_transition_value(1);
                    }
                    menu_copy_billboard_data_in(
                        &g_menu_selected_unit_status, unit_display_data, &g_menu_selected_unit_fields);
                    g_gfx_screen_fade = screen_fade_max;
                    do {
                        thread_yield();
                        g_gfx_screen_fade -= 0x10;
                    } while (g_gfx_screen_fade > 0);
                    g_gfx_screen_fade = 0;
                }
            } while (g_require_party_removal_result != 0);
            if (require_party_sell_equipment_over_limit(*selected_unit_index_ptr) != 0) {
                thread_start(REQUIRE_THREAD_SLOT_MENU, menu_icon_linked_entry_thread);
                thread_set_parameters(REQUIRE_THREAD_SLOT_MENU, (s32)&g_require_party_removal_menus[5], 0, 0);
                thread_wait_until_inactive(REQUIRE_THREAD_SLOT_MENU);
            }
            thread_wait_frames(20);
            thread_start(REQUIRE_THREAD_SLOT_MENU, require_condition_run_special_thread);
            thread_set_parameters(REQUIRE_THREAD_SLOT_MENU, 10, 0, 0);
            require_input_wait_frames_or_skip(60);
            thread_set_parameters(REQUIRE_THREAD_SLOT_MENU, 0, 0, 2);
            thread_wait_until_inactive(REQUIRE_THREAD_SLOT_MENU);
            require_panel_set_transition_value(0);

            unit = battle_unit_get_stats_from_battle_id(*selected_unit_index_ptr);
            unit_index_ptr++;
            main_party_save_unit(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
            thread_wait_frames(20);
            goto next_unit;
        not_joining:
            if (g_require_menu_selection_result != one) {
                selected_unit_index_ptr = unit_index_ptr;
                goto retry_selection;
            }
            require_panel_set_transition_value(0);
            thread_wait_frames(10);
            unit = battle_unit_get_stats_from_battle_id(*selected_unit_index_ptr);
            unit_index_ptr++;
            require_party_update_bio_variable_for_unit_class(unit->character_identity, 12);
        next_unit:
            affected_index++;
        } while (affected_index < g_require_party_affected_unit_count);
    }

    transition = 0;
    do {
        g_require_status_display_origin_x = transition;
        g_require_numeric_editor_origin_x = transition;
        g_require_character_status_origin_x = transition;
        transition += 0x20;
        thread_yield();
    } while (transition < 0x101);
    thread_set_parameters(THREAD_SCREEN_SLOT_UNIT_STATUS, 0, 0, 1);
    thread_set_parameters(THREAD_SCREEN_SLOT_NUMERIC_EDITOR, 0, 0, 1);
    thread_set_parameters(THREAD_SCREEN_SLOT_CHARACTER_STATUS, 0, 0, 1);
    thread_wait_until_inactive(THREAD_SCREEN_SLOT_UNIT_STATUS);
    thread_wait_until_inactive(THREAD_SCREEN_SLOT_NUMERIC_EDITOR);
    thread_wait_until_inactive(THREAD_SCREEN_SLOT_CHARACTER_STATUS);
    g_menu_post_battle_unit_changes_active = 0;
    mem_copy_bytes_nonempty(&g_threads[THREAD_SLOT_EVENT], g_require_saved_thread_block, 0x400);
    g_menu_thread_menu_data = saved_menu_descriptor;
    thread_resume(THREAD_SLOT_EVENT);
    thread_yield();
    thread_exit_current();
}
