/* Post-battle party changes, including congratulations text, brave and faith updates, low brave and high faith
 * departures, join candidates, the bugged ready condition, selection waits and the JOBSTTS overlay open. */

#include "fft/battle_unit.h"
#include "fft/event_require.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/script.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

/*
 * Thread body of the "Congratulations!" battle-complete display.
 *
 * Swaps the scenario music for the victory track, shows banner 1 and, half a
 * second later, banner 7, holds them, then fades both out and waits for
 * their threads. During event 0x145 a different track plays, the waits
 * cannot be skipped and the hold is 300 frames in place of 180.
 */
void require_text_show_battle_congratulations(void) {
    s32 thread_a;
    s32 thread_b;

    g_thread_call_target = main_sound_unload_scenario_music_and_tunes;
    thread_call_on_main_stack();
    g_thread_call_target = (void (*)(void))main_sound_open_scenario_smd_files;
    thread_call_on_main_stack(script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON
            ? MAIN_MUSIC_CONGRATULATIONS
            : MAIN_MUSIC_CONGRATULATIONS_FINAL_BATTLE);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))main_sound_poll_scenario_smd_load;
    } while (thread_call_on_main_stack() != 0);
    g_thread_call_target = (void (*)(void))main_sound_switch_music_track;
    thread_call_on_main_stack(1, 0x7f, 0);

    thread_a = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_a, require_condition_run_special_thread);
    thread_set_parameters(thread_a, 1, 0, 0);
    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON)
        require_input_wait_frames_or_skip(0x1e);
    else
        thread_wait_frames(0x1e);

    thread_b = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_b, require_condition_run_special_thread);
    thread_set_parameters(thread_b, 7, 1, 0);
    if (script_get_variable(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON)
        require_input_wait_frames_or_skip(0xb4);
    else
        thread_wait_frames(0x12c);

    thread_set_parameters(thread_a, 0, 0, 2);
    thread_set_parameters(thread_b, 0, 0, 2);
    thread_wait_until_inactive(thread_a);
    thread_wait_until_inactive(thread_b);
    thread_exit_current();
}

/* Apply permanent Brave/Faith changes and collect party-leaving warnings.
 *
 * Each unit keeps a quarter of the battle's change (delta * 250 / 100 / 10,
 * each division truncating). A unit then at Brave 6..15 or Faith 85..94 gets a
 * warning; guests, Ramza and unit ids 1..3 are exempt.
 *
 * The staged multiplication and divisions preserve the target's two signed
 * constant-division sequences. */
static s32 require_party_update_brave_faith_and_collect_warnings(void) {
    s32 battle_index;
    s32 warning_count = 0;
    unit_stats_t* battle_unit;
    unit_stats_t* unit;
    s32 found_battle_index;
    s32 brave;
    s32 faith;
    s32 base;
    s32 raw;
    s32 delta1;
    s32 delta2;
    s32 value1;
    s32 value2;
    s32 value3;
    s32 value4;

    g_require_party_affected_unit_count = 0;
    for (battle_index = 0; battle_index < BATTLE_UNIT_SLOT_COUNT; battle_index++) {
        battle_unit = battle_unit_get_stats_from_battle_id(battle_index);
        unit = battle_unit_find_stats_by_entd_unit_id(battle_unit->unit_id, &found_battle_index);
        if (found_battle_index < 0 || battle_index != found_battle_index
            || unit->formation_index == BATTLE_FORMATION_INDEX_NONE || unit->unit_id == 0) {
            continue;
        }

        base = unit->original_brave;
        raw = unit->brave;
        delta1 = raw - base;
        delta2 = delta1 * 250;
        raw = delta2 / 100;
        raw = raw / 10;
        brave = base + raw;
        unit->original_brave = brave;
        unit->brave = brave;
        base = unit->original_faith;
        raw = unit->faith;
        delta1 = raw - base;
        delta2 = delta1 * 250;
        raw = delta2 / 100;
        raw = raw / 10;
        faith = base + raw;
        unit->original_faith = faith;
        unit->faith = faith;

        if (unit->formation_index >= PARTY_GUEST_SLOT_FIRST
            || (unit->character_identity != CHARACTER_IDENTITY_ENTD_NONE
                && unit->character_identity <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)
            || (unit->unit_id != 0 && unit->unit_id < 4)) {
            continue;
        }

        if (brave >= 6 && brave <= 15) {
            if (unit->character_identity < CHARACTER_IDENTITY_SELECTOR_FIRST) {
                s32 column;

                for (brave = 0; brave < 0x49; brave++) {
                    for (column = 0; column < 3; column++) {
                        if (unit->character_identity == g_require_gfx_formation_sprite_groups[brave][column]) {
                            break;
                        }
                    }
                    if (column != 3) {
                        break;
                    }
                }
                if (brave == 0x49) {
                    brave = 0;
                }
                if (brave == 0x47) {
                    brave = 0x48;
                }
                value1 = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[value1] = unit->character_identity;
                g_require_party_affected_unit_message_ids[value1] = brave + 0x818;
                g_require_party_affected_battle_unit_indices[value1] = battle_index;
                g_require_party_affected_unit_count = value1 + 1;
            } else {
                value2 = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[value2] = unit->unit_id;
                g_require_party_affected_unit_message_ids[value2]
                    = (unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) * 4 + (battle_index & 3) + 0x800;
                g_require_party_affected_battle_unit_indices[value2] = battle_index;
                g_require_party_affected_unit_count = value2 + 1;
            }
            warning_count++;
        }

        if (faith >= 0x55 && faith <= 0x5e) {
            if (unit->character_identity < CHARACTER_IDENTITY_SELECTOR_FIRST) {
                s32 group_index;
                s32 column;

                for (group_index = 0; group_index < 0x49; group_index++) {
                    for (column = 0; column < 3; column++) {
                        if (unit->character_identity == g_require_gfx_formation_sprite_groups[group_index][column]) {
                            break;
                        }
                    }
                    if (column != 3) {
                        break;
                    }
                }
                if (group_index == 0x49) {
                    group_index = 0;
                }
                if (group_index == 0x47) {
                    group_index = 0x48;
                }
                value3 = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[value3] = unit->unit_id;
                g_require_party_affected_unit_message_ids[value3] = group_index + 0x862;
                g_require_party_affected_battle_unit_indices[value3] = battle_index;
                g_require_party_affected_unit_count = value3 + 1;
            } else {
                value4 = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[value4] = unit->unit_id;
                g_require_party_affected_unit_message_ids[value4]
                    = (unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) * 4 + (battle_index & 3) + 0x80c;
                g_require_party_affected_battle_unit_indices[value4] = battle_index;
                g_require_party_affected_unit_count = value4 + 1;
            }
            warning_count++;
        }
    }
    return warning_count;
}

/* Set the three panel dim flags and both status animation flags to `value`. */
void require_panel_set_transition_value(s32 value) {
    g_require_panel_dim_a = value;
    g_require_panel_dim_b = value;
    g_require_panel_dim_c = value;
    g_menu_panel_status_animation[0] = value;
    g_menu_panel_status_animation[1] = value;
}

/* Empty. Called for each unit that leaves over a Brave below 6, with its battle index, unit id and
 * character identity. */
static void require_party_report_brave_departure_stub(
    s32 unused_battle_index, s32 unused_unit_id, s32 unused_character_identity) {
}

/* Collect the party members that leave after the battle: Brave below 6 or
 * Faith of 95 and above. Story characters are named by their sprite group's
 * message; generic units by job and battle slot. Returns how many leave. */
static s32 require_party_find_low_brave_high_faith_departures(void) {
    s32 battle_index;
    s32 departure_count = 0;
    unit_stats_t* battle_unit;
    unit_stats_t* unit;
    s32 found_battle_index;
    s32 brave;
    s32 faith;
    s32 group;
    s32 column;

    g_require_party_affected_unit_count = 0;
    for (battle_index = 0; battle_index < BATTLE_UNIT_SLOT_COUNT; battle_index++) {
        battle_unit = battle_unit_get_stats_from_battle_id(battle_index);
        unit = battle_unit_find_stats_by_entd_unit_id(battle_unit->unit_id, &found_battle_index);
        if (found_battle_index < 0 || battle_index != found_battle_index
            || unit->formation_index == BATTLE_FORMATION_INDEX_NONE || unit->unit_id == 0) {
            continue;
        }
        if (unit->formation_index >= PARTY_GUEST_SLOT_FIRST
            || (unit->character_identity != CHARACTER_IDENTITY_ENTD_NONE
                && unit->character_identity <= CHARACTER_IDENTITY_RAMZA_CHAPTER_4)
            || (unit->unit_id != 0 && unit->unit_id < 4)) {
            continue;
        }

        brave = unit->original_brave;
        faith = unit->original_faith;
        if (brave < 6) {
            if (unit->character_identity < CHARACTER_IDENTITY_SELECTOR_FIRST) {
                s32 count;

                for (group = 0; group < 0x49; group++) {
                    for (column = 0; column < 3; column++) {
                        if (unit->character_identity == g_require_gfx_formation_sprite_groups[group][column]) {
                            break;
                        }
                    }
                    if (column != 3) {
                        break;
                    }
                }
                if (group == 0x49) {
                    group = 0;
                }
                if (group == 0x47) {
                    group = 0x48;
                }
                count = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[count] = unit->character_identity;
                g_require_party_affected_unit_message_ids[count] = group + 0x1018;
                g_require_party_affected_battle_unit_indices[count] = battle_index;
                g_require_party_affected_unit_count = count + 1;
                require_party_update_bio_variable_for_unit_class(unit->character_identity, 0xd);
            } else {
                s32 count;

                count = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[count] = unit->unit_id;
                g_require_party_affected_unit_message_ids[count]
                    = (unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) * 4 + (battle_index & 3) + 0x1000;
                g_require_party_affected_battle_unit_indices[count] = battle_index;
                g_require_party_affected_unit_count = count + 1;
            }
            /* The target passes three arguments the callee does not take. */
            require_party_report_brave_departure_stub(battle_index, unit->unit_id, unit->character_identity);
            departure_count++;
        } else if (faith >= 0x5f) {
            if (unit->character_identity < CHARACTER_IDENTITY_SELECTOR_FIRST) {
                s32 count;

                for (group = 0; group < 0x49; group++) {
                    for (column = 0; column < 3; column++) {
                        if (unit->character_identity == g_require_gfx_formation_sprite_groups[group][column]) {
                            break;
                        }
                    }
                    if (column != 3) {
                        break;
                    }
                }
                if (group == 0x49) {
                    group = 0;
                }
                if (group == 0x47) {
                    group = 0x48;
                }
                count = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[count] = unit->unit_id;
                g_require_party_affected_unit_message_ids[count] = group + 0x1062;
                g_require_party_affected_battle_unit_indices[count] = battle_index;
                g_require_party_affected_unit_count = count + 1;
                require_party_update_bio_variable_for_unit_class(unit->character_identity, 0xe);
            } else {
                s32 count;

                count = g_require_party_affected_unit_count;
                g_require_party_affected_unit_message_args[count] = unit->unit_id;
                g_require_party_affected_unit_message_ids[count]
                    = (unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) * 4 + (battle_index & 3) + 0x100c;
                g_require_party_affected_battle_unit_indices[count] = battle_index;
                g_require_party_affected_unit_count = count + 1;
            }
            departure_count++;
        }
    }
    return departure_count;
}

/*
 * End-of-battle party update. Returns the number of units that still need a
 * "join the party?" prompt.
 *
 * First pass, every occupied roster slot that fielded a battle unit: a unit
 * that is gone (disabled, absent or not found) and is not one of ENTD units
 * 1..3 is removed from the roster, its equipment going back to the inventory
 * unless it became a treasure; Ramza's identities are always saved; a guest
 * without the save-formation flag is dropped; anyone else is saved.
 *
 * Second pass, every present battle unit with no roster slot that is not a
 * Ramza identity, has unit id 4 or more and either carries the
 * join-after-event flag or is invited: one with the save-formation flag joins
 * at once (equipment over the inventory limit is sold); the others are queued
 * for the prompt with message 0x1800 + (identity - 0x80) * 8 + a variant from
 * the random script variable.
 */
s32 require_party_save_battle_units_and_find_join_candidates(void) {
    s32 prompt_count = 0;
    s32 party_index;
    s32 found_index;
    s32 i;

    g_require_party_affected_unit_count = 0;
    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_unit_t* party_unit = main_party_get_unit(party_index);

        if (party_unit->party_id != PARTY_ID_NONE) {
            unit_stats_t* unit;

            for (i = 0; i < BATTLE_UNIT_SLOT_COUNT; i++) {
                unit = battle_unit_get_stats_from_battle_id(i);
                if (unit->formation_index == party_index && unit->existence != BATTLE_UNIT_EXISTENCE_NONE
                    && unit->unit_id != 0) {
                    break;
                }
            }

            if (i != BATTLE_UNIT_ID_NONE) {
                unit = battle_unit_get_stats_from_battle_id(i);
                unit = battle_unit_find_stats_by_entd_unit_id(unit->unit_id, &found_index);
                if (unit->existence == BATTLE_UNIT_EXISTENCE_DISABLED || unit->existence == BATTLE_UNIT_EXISTENCE_NONE
                    || found_index == -3) {
                    if (unit->unit_id == 0 || unit->unit_id >= 4) {
                        if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                                & STATUS_BYTE_MASK(STATUS_ID_TREASURE)))
                            require_party_store_equipment_or_sell_overflow(i);
                        main_party_remove_unit(party_index);
                        require_party_update_bio_variable_for_unit_class(unit->character_identity, 0xf);
                    }
                } else if (unit->character_identity != CHARACTER_IDENTITY_ENTD_NONE
                    && unit->character_identity < CHARACTER_IDENTITY_RAMZA_END) {
                    main_party_save_unit(unit, 0);
                } else if (!(unit->unit_flags & UNIT_FLAG_SAVE_FORMATION)
                    && unit->formation_index >= PARTY_GUEST_SLOT_FIRST) {
                    main_party_remove_unit(unit->formation_index);
                    unit->formation_index = BATTLE_FORMATION_INDEX_NONE;
                } else {
                    main_party_save_unit(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
                }
            }
        }
    }

    for (party_index = 0; party_index < BATTLE_UNIT_SLOT_COUNT; party_index++) {
        unit_stats_t* unit;
        unit_stats_t* battle_unit;

        battle_unit = battle_unit_get_stats_from_battle_id(party_index);
        unit = battle_unit_find_stats_by_entd_unit_id(battle_unit->unit_id, &found_index);
        if (found_index >= 0 && party_index == found_index && unit->formation_index == BATTLE_FORMATION_INDEX_NONE
            && unit->unit_id != 0) {
            u32 base_class = unit->character_identity;
            if ((base_class == CHARACTER_IDENTITY_ENTD_NONE || base_class >= CHARACTER_IDENTITY_RAMZA_END)
                && unit->unit_id >= 4
                && ((unit->unit_flags & UNIT_FLAG_JOIN_AFTER_EVENT)
                    || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INVITE)]
                        & STATUS_BYTE_MASK(STATUS_ID_INVITE)))) {
                if (unit->unit_flags & UNIT_FLAG_SAVE_FORMATION) {
                    s32 column;

                    /* Retail looks up the unit's formation sprite group and
                     * then does not use it. */
                    for (i = 0; i < 0x49; i++) {
                        for (column = 0; column < 3; column++) {
                            if (unit->character_identity == g_require_gfx_formation_sprite_groups[i][column]) {
                                break;
                            }
                        }
                        if (column != 3) {
                            break;
                        }
                    }
                    if (i == 0x49) {
                        i = 0;
                    }
                    if (i == 0x47) {
                        i = 0x48;
                    }
                    require_party_sell_equipment_over_limit(party_index);
                    main_party_save_unit(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
                } else {
                    s32 count;
                    s32 script_index = EVENT_SCRIPT_VAR_RANDOM_VALUE;
                    s32 script_variant;
                    unit->unit_flags &= ~(UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION);
                    g_require_party_affected_unit_message_args[g_require_party_affected_unit_count] = unit->unit_id;
                    script_variant = script_get_variable(script_index) & 7;
                    prompt_count++;
                    count = g_require_party_affected_unit_count;
                    g_require_party_affected_unit_message_ids[count]
                        = ((unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) << 3) + script_variant
                        + 0x1800;
                    g_require_party_affected_battle_unit_indices[count] = party_index;
                    g_require_party_affected_unit_count = count + 1;
                }
            }
        }
    }
    return prompt_count;
}

/*
 * Thread body of the post-battle Brave/Faith report.
 *
 * Exits at once in the world debug battle style, or when
 * require_party_update_brave_faith_and_collect_warnings reports
 * nobody. Otherwise it binds the Brave/Faith text sections, shows banner 5
 * and plays each affected unit's warning line in turn before fading the
 * banner out.
 */
void require_party_report_brave_faith_changes(void) {
    s32 i;
    s32 main_thread;

    if (script_get_variable(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0)
        thread_exit_current();
    if (require_party_update_brave_faith_and_collect_warnings() == 0)
        thread_exit_current();
    g_text_section_pointers[REQUIRE_TEXT_SECTION_BRAVE_FAITH_WARNINGS] = g_text_data + g_text_section_offsets[1];
    g_text_section_pointers[REQUIRE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES] = g_text_data + g_text_section_offsets[2];
    g_text_section_pointers[REQUIRE_TEXT_SECTION_DISMISS_UNIT_REPLIES] = g_text_data + g_text_section_offsets[3];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = g_text_data + g_text_section_offsets[4];
    main_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(main_thread, require_condition_run_special_thread);
    thread_set_parameters(main_thread, 5, 0, 0);
    require_gfx_fade_rgb_31_frames(0xd, 0xd, 2);
    for (i = 0; i < g_require_party_affected_unit_count; i++) {
        s32 unit_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
        g_text_substitution_values[0] = g_require_party_affected_battle_unit_indices[i];
        thread_start(unit_thread, text_run_dialog_thread_variant);
        thread_set_parameters(unit_thread, 0x1b, g_require_party_affected_unit_message_ids[i],
            g_require_party_affected_unit_message_args[i]);
        thread_wait_until_inactive(unit_thread);
    }
    thread_set_parameters(main_thread, 0, 0, 2);
    thread_wait_until_inactive(main_thread);
    require_gfx_fade_rgb_31_frames(6, 5, 2);
    thread_exit_current();
}

/*
 * Thread body of the post-battle desertions.
 *
 * Exits at once unless require_party_find_low_brave_high_faith_departures
 * found someone. Otherwise it binds the Brave/Faith text sections, shows the
 * banner and, for each departing unit in turn, plays its farewell line beside
 * the "left the party" banner for its gender, waits half a second and
 * removes the unit from the roster.
 */
void require_party_remove_low_brave_high_faith_units(void) {
    s32 i;
    s32 main_thread;

    if (require_party_find_low_brave_high_faith_departures() == 0)
        thread_exit_current();
    g_text_section_pointers[REQUIRE_TEXT_SECTION_BRAVE_FAITH_WARNINGS] = g_text_data + g_text_section_offsets[1];
    g_text_section_pointers[REQUIRE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES] = g_text_data + g_text_section_offsets[2];
    g_text_section_pointers[REQUIRE_TEXT_SECTION_DISMISS_UNIT_REPLIES] = g_text_data + g_text_section_offsets[3];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES] = g_text_data + g_text_section_offsets[4];
    thread_yield();
    main_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
    thread_start(main_thread, require_condition_run_special_thread);
    thread_set_parameters(main_thread, 6, 0, 0);
    require_gfx_fade_rgb_31_frames(0xd, 5, 2);
    thread_yield();
    for (i = 0; i < g_require_party_affected_unit_count; i++) {
        unit_stats_t* unit;
        s32 unit_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
        s32 message_thread;
        g_text_substitution_values[0] = g_require_party_affected_battle_unit_indices[i];
        thread_start(unit_thread, text_run_dialog_thread_variant);
        thread_set_parameters(unit_thread, 0x1b, g_require_party_affected_unit_message_ids[i],
            g_require_party_affected_unit_message_args[i]);
        unit = battle_unit_get_stats_from_battle_id(g_require_party_affected_battle_unit_indices[i]);
        message_thread = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);
        thread_start(message_thread, require_condition_run_special_thread);
        if (unit->unit_flags & UNIT_FLAG_FEMALE) {
            thread_set_parameters(message_thread, 9, 1, 0);
        } else {
            thread_set_parameters(message_thread, 8, 1, 0);
        }
        thread_wait_until_inactive(unit_thread);
        thread_set_parameters(message_thread, 0, 0, 2);
        thread_wait_until_inactive(message_thread);
        thread_wait_frames(0x1e);
        main_party_remove_unit(unit->formation_index);
    }
    thread_set_parameters(main_thread, 6, 0, 2);
    thread_wait_until_inactive(main_thread);
    require_gfx_fade_rgb_31_frames(6, 5, 2);
    thread_exit_current();
}

/* Thread body of display condition "ready": runs the hovered-unit stats
 * display thread for 60 frames, or until Circle, then stops it. */
void require_condition_show_ready_bugged(void) {
    s32 thread_id = thread_resolve_id(THREAD_SLOT_FIRST_IDLE);

    thread_start(thread_id, menu_refresh_hovered_unit_stats_display);
    thread_set_parameters(thread_id, 0, 0, 0);
    require_input_wait_frames_or_skip(0x3c);
    thread_set_parameters(thread_id, 0, 0, 1);
    thread_wait_until_inactive(thread_id);
    thread_exit_current();
}

/* Wait until the menu thread ends or a selection result (-1, 0 or 1) is in,
 * then ask the message thread to close and wait for both threads. */
void require_menu_wait_selection_threads(void) {
    while (g_threads[REQUIRE_THREAD_SLOT_MENU].is_running != 0) {
        s32 selection = g_require_menu_selection_result;

        if (selection == -1) {
            break;
        }
        if (selection == 1) {
            break;
        }
        if (selection == 0) {
            break;
        }
        thread_yield();
    }
    thread_yield();
    g_threads[REQUIRE_THREAD_SLOT_MESSAGE].task_id = THREAD_TASK_STOP_REQUEST;
    thread_set_parameters(REQUIRE_THREAD_SLOT_MESSAGE, 0, 0, 2);
    thread_wait_until_inactive(REQUIRE_THREAD_SLOT_MESSAGE);
    thread_wait_until_inactive(REQUIRE_THREAD_SLOT_MENU);
}

/* Yield until Circle is newly pressed, at least one frame. */
void require_input_wait_skip(void) {
    u32* input;

    do {
        thread_yield();
        input = script_get_controller_input_pointer(0);
        g_menu_input_controller = input;
    } while ((*input & PSX_PAD_CIRCLE) == 0);
}

/*
 * Thread body that runs the JOBSTTS overlay from the REQUIRE screen.
 *
 * Copies the selected unit's billboard in, waits until 0x1f000 bytes can be
 * reserved at the high overlay address, requests the overlay and hands the
 * frame loop the JOBSTTS state. Until that state returns to idle the
 * 32-bit controller value is zeroed every frame so this screen sees no input. Then
 * the reservation is freed and the option-menu flag cleared.
 */
static void require_overlay_open_jobstts(void) {
    void* allocation;

    menu_copy_billboard_data_in(&g_menu_selected_unit_status, &g_menu_selected_unit, &g_menu_selected_unit_fields);
    while (1) {
        allocation = main_heap_reserve_at(g_main_heap_high_overlay_load_address, 0x1f000);
        if (allocation == g_main_heap_high_overlay_load_address) {
            break;
        }
        thread_yield();
        if (g_main_font_print_enabled != 0) {
            FntPrint(g_require_text_wait_for_file_allocation_message);
        }
    }
    menu_request_open_companion_executable(COMPANION_OVERLAY_JOBSTTS);
    thread_wait_frames(2);
    g_menu_companion_overlay_state = COMPANION_OVERLAY_STATE_JOBSTTS;
    do {
        g_menu_input_controller = script_get_controller_input_pointer(0);
        *g_menu_input_controller = 0;
        thread_yield();
    } while (g_menu_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    *g_menu_input_controller = 0;
    main_heap_free(allocation);
    thread_wait_frames(2);
    g_menu_modal_open = 0;
    thread_exit_current();
}

void require_overlay_open_jobstts_thread(void) {
    thread_start(6, require_overlay_open_jobstts);
    thread_wait_until_inactive(6);
    thread_exit_current();
}

/*
 * Thread body: suspend thread g_require_thread_suspended_id and draw the
 * gradient grid and status group primitives every frame in its place until
 * the third thread parameter is set.
 *
 * The suspended thread's record is copied aside first and copied back before
 * the thread is resumed.
 */
void require_gfx_run_suspended_thread_transition(void) {
    s32 frame = 0;
    u8* saved;

    thread_suspend(g_require_thread_suspended_id);
    saved = menu_alloc_memory(THREAD_STRIDE);
    mem_copy_bytes_nonempty(saved, &g_threads[g_require_thread_suspended_id], THREAD_STRIDE);
    g_menu_panel_status_group_count = 2;
    while (1) {
        s32 buffer = frame & 1;
        gfx_build_gradient_grid_primitives_variant(g_require_gfx_poly_gt4_buffers[buffer]);
        menu_build_status_group_primitives_variant(g_gfx_poly_ft4_buffers[buffer]);
        thread_yield();
        if (thread_get_current_parameter_3() != 0) {
            break;
        }
        frame++;
    }
    mem_copy_bytes_nonempty(&g_threads[g_require_thread_suspended_id], saved, THREAD_STRIDE);
    menu_free_memory(saved);
    thread_resume(g_require_thread_suspended_id);
    thread_yield();
    thread_exit_current();
}
