/* Post-battle party changes, including congratulations text, Brave and Faith updates, low Brave and high Faith
 * departures, join candidates, the bugged ready condition, selection waits and the JOBSTTS overlay open. */

#include "fft/after_battle.h"
#include "fft/gfx.h"
#include "fft/gfx_primitive.h"
#include "fft/input.h"
#include "fft/memory.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/message.h"
#include "fft/music.h"
#include "fft/overlay.h"
#include "fft/party.h"
#include "fft/script.h"
#include "fft/script_variable.h"
#include "fft/status.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/unit_identity.h"
#include "fft/unit_panel.h"
#include "fft/unit_party.h"
#include "fft/unit_runtime.h"
#include "fft/unit_view.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern POLY_FT4 g_unit_panel_transition_quads[2][30];

static s32 g_party_affected_unit_message_args[50] = { 0 };
static s32 g_party_affected_unit_message_ids[50] = { 0 };
s32 g_party_affected_battle_unit_indices[50] = { 0 };

/*
 * Thread body of the "Congratulations!" battle-complete display.
 *
 * Swaps the scenario music for the victory track, shows Congratulations and, half a
 * second later, the battle-complete banner, holds them, then fades both out and waits for
 * their threads. During event 0x145 a different track plays, the waits
 * cannot be skipped and the hold is 300 frames in place of 180.
 */
void victory_congratulations_thread(void) {
    s32 thread_a;
    s32 thread_b;

    g_thread_call_target = music_all_unload;
    thread_call_on_main_stack();
    g_thread_call_target = (void (*)(void))music_free_slot_load_start;
    thread_call_on_main_stack(script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON
            ? MUSIC_CONGRATULATIONS
            : MUSIC_CONGRATULATIONS_FINAL_BATTLE);
    do {
        thread_yield();
        g_thread_call_target = (void (*)(void))music_scenario_load_poll;
    } while (thread_call_on_main_stack() != 0);
    g_thread_call_target = (void (*)(void))music_track_switch;
    thread_call_on_main_stack(1, 0x7f, 0);

    thread_a = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_a, conditions_special_thread);
    thread_parameters_set(thread_a, CONDITIONS_BANNER_CONGRATULATIONS, 0, 0);
    if (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON)
        input_wait_frames_or_skip(0x1e);
    else
        thread_frames_wait(0x1e);

    thread_b = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
    thread_start(thread_b, conditions_special_thread);
    thread_parameters_set(thread_b, CONDITIONS_BANNER_BATTLE_COMPLETE, 1, 0);
    if (script_variable_get(EVENT_SCRIPT_VAR_CURRENT_EVENT) != EVENT_ID_FINAL_BATTLE_WON)
        input_wait_frames_or_skip(0xb4);
    else
        thread_frames_wait(0x12c);

    thread_parameters_set(thread_a, 0, 0, 2);
    thread_parameters_set(thread_b, 0, 0, 2);
    thread_inactive_wait(thread_a);
    thread_inactive_wait(thread_b);
    thread_current_exit();
}

/* Apply permanent Brave/Faith changes and collect party-leaving warnings.
 *
 * Each unit keeps a quarter of the battle's change (delta * 250 / 100 / 10,
 * each division truncating). A unit then at Brave 6..15 or Faith 85..94 gets a
 * warning; guests, Ramza and unit ids 1..3 are exempt. The warning lines are in
 * AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_WARNINGS (text id 0x800 on): a story character's
 * by its sprite group (Brave from entry 0x18, Faith from 0x62), a generic unit's four
 * per job, picked by battle slot (Brave from 0, Faith from 0xc). A story character's
 * Brave line takes its identity as the argument where every other line takes its unit id.
 *
 * The staged multiplication and divisions preserve the target's two signed
 * constant-division sequences. */
static s32 party_update_brave_faith(void) {
    s32 battle_index;
    s32 warning_count = 0;
    unit_stats_t* battle_unit;
    unit_stats_t* unit;
    s32 found_battle_index;
    s32 brave;
    s32 faith;
    s32 base;
    s32 raw;
    s32 change;
    s32 scaled_change;

    g_party_affected_unit_count = 0;
    for (battle_index = 0; battle_index < UNIT_SLOT_COUNT; battle_index++) {
        battle_unit = unit_stats_get(battle_index);
        unit = unit_entd_find_stats(battle_unit->unit_id, &found_battle_index);
        if (found_battle_index < 0 || battle_index != found_battle_index
            || unit->formation_index == UNIT_FORMATION_INDEX_NONE || unit->unit_id == 0) {
            continue;
        }

        base = unit->original_brave;
        raw = unit->brave;
        change = raw - base;
        scaled_change = change * 250;
        raw = scaled_change / 100;
        raw = raw / 10;
        brave = base + raw;
        unit->original_brave = brave;
        unit->brave = brave;
        base = unit->original_faith;
        raw = unit->faith;
        change = raw - base;
        scaled_change = change * 250;
        raw = scaled_change / 100;
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
                s32 warning_index;

                /* Find the identity's message group (three identities each); an identity in
                 * none uses group 0, and group 71 shares group 72's line. */
                for (brave = 0; brave < PARTY_MESSAGE_IDENTITY_GROUP_COUNT; brave++) {
                    for (column = 0; column < PARTY_MESSAGE_IDENTITIES_PER_GROUP; column++) {
                        if (unit->character_identity == g_party_message_identity_groups[brave][column]) {
                            break;
                        }
                    }
                    if (column != PARTY_MESSAGE_IDENTITIES_PER_GROUP) {
                        break;
                    }
                }
                if (brave == PARTY_MESSAGE_IDENTITY_GROUP_COUNT) {
                    brave = 0;
                }
                if (brave == 71) {
                    brave = 72;
                }
                warning_index = g_party_affected_unit_count;
                g_party_affected_unit_message_args[warning_index] = unit->character_identity;
                g_party_affected_unit_message_ids[warning_index] = brave + 0x818;
                g_party_affected_battle_unit_indices[warning_index] = battle_index;
                g_party_affected_unit_count = warning_index + 1;
            } else {
                s32 warning_index;

                warning_index = g_party_affected_unit_count;
                g_party_affected_unit_message_args[warning_index] = unit->unit_id;
                g_party_affected_unit_message_ids[warning_index]
                    = (unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) * 4 + (battle_index & 3) + 0x800;
                g_party_affected_battle_unit_indices[warning_index] = battle_index;
                g_party_affected_unit_count = warning_index + 1;
            }
            warning_count++;
        }

        if (faith >= 85 && faith <= 94) {
            if (unit->character_identity < CHARACTER_IDENTITY_SELECTOR_FIRST) {
                s32 group_index;
                s32 column;
                s32 warning_index;

                for (group_index = 0; group_index < PARTY_MESSAGE_IDENTITY_GROUP_COUNT; group_index++) {
                    for (column = 0; column < PARTY_MESSAGE_IDENTITIES_PER_GROUP; column++) {
                        if (unit->character_identity == g_party_message_identity_groups[group_index][column]) {
                            break;
                        }
                    }
                    if (column != PARTY_MESSAGE_IDENTITIES_PER_GROUP) {
                        break;
                    }
                }
                if (group_index == PARTY_MESSAGE_IDENTITY_GROUP_COUNT) {
                    group_index = 0;
                }
                if (group_index == 71) {
                    group_index = 72;
                }
                warning_index = g_party_affected_unit_count;
                g_party_affected_unit_message_args[warning_index] = unit->unit_id;
                g_party_affected_unit_message_ids[warning_index] = group_index + 0x862;
                g_party_affected_battle_unit_indices[warning_index] = battle_index;
                g_party_affected_unit_count = warning_index + 1;
            } else {
                s32 warning_index;

                warning_index = g_party_affected_unit_count;
                g_party_affected_unit_message_args[warning_index] = unit->unit_id;
                g_party_affected_unit_message_ids[warning_index]
                    = (unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) * 4 + (battle_index & 3) + 0x80c;
                g_party_affected_battle_unit_indices[warning_index] = battle_index;
                g_party_affected_unit_count = warning_index + 1;
            }
            warning_count++;
        }
    }
    return warning_count;
}

/* Set the three panel dim flags and both status animation flags to `value`. */
void unit_panel_set_transition(s32 value) {
    g_unit_panel_status_display_thread_params.style = value;
    g_unit_panel_numbers_thread_params.style = value;
    g_unit_panel_character_status_frame_config.style = value;
    g_unit_panel_status_animation[0] = value;
    g_unit_panel_status_animation[1] = value;
}

/* Empty. Called for each unit that leaves over a Brave below 6, with its battle index, unit id and
 * character identity. */
static void party_report_departure_stub(s32 unused_battle_index, s32 unused_unit_id, s32 unused_character_identity) {
}

/* Collect the party members that leave after the battle: Brave below 6 or
 * Faith of 95 and above. Story characters are named by their sprite group's
 * message; generic units by job and battle slot. Returns how many leave.
 * The farewell lines are in AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES (text id
 * 0x1000 on), laid out like the warnings; a departing story character's bio variable
 * is set to 0xd (low Brave) or 0xe (high Faith). */
static s32 party_find_departures(void) {
    s32 battle_index;
    s32 departure_count = 0;
    unit_stats_t* battle_unit;
    unit_stats_t* unit;
    s32 found_battle_index;
    s32 brave;
    s32 faith;
    s32 group;
    s32 column;

    g_party_affected_unit_count = 0;
    for (battle_index = 0; battle_index < UNIT_SLOT_COUNT; battle_index++) {
        battle_unit = unit_stats_get(battle_index);
        unit = unit_entd_find_stats(battle_unit->unit_id, &found_battle_index);
        if (found_battle_index < 0 || battle_index != found_battle_index
            || unit->formation_index == UNIT_FORMATION_INDEX_NONE || unit->unit_id == 0) {
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

                for (group = 0; group < PARTY_MESSAGE_IDENTITY_GROUP_COUNT; group++) {
                    for (column = 0; column < PARTY_MESSAGE_IDENTITIES_PER_GROUP; column++) {
                        if (unit->character_identity == g_party_message_identity_groups[group][column]) {
                            break;
                        }
                    }
                    if (column != PARTY_MESSAGE_IDENTITIES_PER_GROUP) {
                        break;
                    }
                }
                if (group == PARTY_MESSAGE_IDENTITY_GROUP_COUNT) {
                    group = 0;
                }
                if (group == 71) {
                    group = 72;
                }
                count = g_party_affected_unit_count;
                g_party_affected_unit_message_args[count] = unit->character_identity;
                g_party_affected_unit_message_ids[count] = group + 0x1018;
                g_party_affected_battle_unit_indices[count] = battle_index;
                g_party_affected_unit_count = count + 1;
                party_set_biography_state(unit->character_identity, PARTY_BIO_STATE_LEFT_LOW_BRAVE);
            } else {
                s32 count;

                count = g_party_affected_unit_count;
                g_party_affected_unit_message_args[count] = unit->unit_id;
                g_party_affected_unit_message_ids[count]
                    = (unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) * 4 + (battle_index & 3) + 0x1000;
                g_party_affected_battle_unit_indices[count] = battle_index;
                g_party_affected_unit_count = count + 1;
            }
            /* Retail calls this empty reporting stub for low-Brave departures only. */
            party_report_departure_stub(battle_index, unit->unit_id, unit->character_identity);
            departure_count++;
        } else if (faith >= 95) {
            if (unit->character_identity < CHARACTER_IDENTITY_SELECTOR_FIRST) {
                s32 count;

                for (group = 0; group < PARTY_MESSAGE_IDENTITY_GROUP_COUNT; group++) {
                    for (column = 0; column < PARTY_MESSAGE_IDENTITIES_PER_GROUP; column++) {
                        if (unit->character_identity == g_party_message_identity_groups[group][column]) {
                            break;
                        }
                    }
                    if (column != PARTY_MESSAGE_IDENTITIES_PER_GROUP) {
                        break;
                    }
                }
                if (group == PARTY_MESSAGE_IDENTITY_GROUP_COUNT) {
                    group = 0;
                }
                if (group == 71) {
                    group = 72;
                }
                count = g_party_affected_unit_count;
                g_party_affected_unit_message_args[count] = unit->unit_id;
                g_party_affected_unit_message_ids[count] = group + 0x1062;
                g_party_affected_battle_unit_indices[count] = battle_index;
                g_party_affected_unit_count = count + 1;
                party_set_biography_state(unit->character_identity, PARTY_BIO_STATE_LEFT_HIGH_FAITH);
            } else {
                s32 count;

                count = g_party_affected_unit_count;
                g_party_affected_unit_message_args[count] = unit->unit_id;
                g_party_affected_unit_message_ids[count]
                    = (unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) * 4 + (battle_index & 3) + 0x100c;
                g_party_affected_battle_unit_indices[count] = battle_index;
                g_party_affected_unit_count = count + 1;
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
s32 party_update_after_battle(void) {
    s32 prompt_count = 0;
    s32 party_index;
    s32 found_index;
    s32 i;

    g_party_affected_unit_count = 0;
    for (party_index = 0; party_index < PARTY_ROSTER_SLOT_COUNT; party_index++) {
        party_unit_t* party_unit = party_unit_get(party_index);

        if (party_unit->party_id != PARTY_ID_NONE) {
            unit_stats_t* unit;

            for (i = 0; i < UNIT_SLOT_COUNT; i++) {
                unit = unit_stats_get(i);
                if (unit->formation_index == party_index && unit->existence != UNIT_EXISTENCE_NONE
                    && unit->unit_id != 0) {
                    break;
                }
            }

            if (i != UNIT_ID_NONE) {
                unit = unit_stats_get(i);
                unit = unit_entd_find_stats(unit->unit_id, &found_index);
                if (unit->existence == UNIT_EXISTENCE_DISABLED || unit->existence == UNIT_EXISTENCE_NONE
                    || found_index == -3) {
                    if (unit->unit_id == 0 || unit->unit_id >= 4) {
                        if (!(unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_TREASURE)]
                                & STATUS_BYTE_MASK(STATUS_ID_TREASURE)))
                            party_store_or_sell_equipment(i);
                        party_unit_remove(party_index);
                        party_set_biography_state(unit->character_identity, PARTY_BIO_STATE_LOST_IN_BATTLE);
                    }
                } else if (unit->character_identity != CHARACTER_IDENTITY_ENTD_NONE
                    && unit->character_identity < CHARACTER_IDENTITY_RAMZA_END) {
                    party_unit_save(unit, 0);
                } else if (!(unit->unit_flags & UNIT_FLAG_SAVE_FORMATION)
                    && unit->formation_index >= PARTY_GUEST_SLOT_FIRST) {
                    party_unit_remove(unit->formation_index);
                    unit->formation_index = UNIT_FORMATION_INDEX_NONE;
                } else {
                    party_unit_save(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
                }
            }
        }
    }

    for (party_index = 0; party_index < UNIT_SLOT_COUNT; party_index++) {
        unit_stats_t* unit;
        unit_stats_t* battle_unit;

        battle_unit = unit_stats_get(party_index);
        unit = unit_entd_find_stats(battle_unit->unit_id, &found_index);
        if (found_index >= 0 && party_index == found_index && unit->formation_index == UNIT_FORMATION_INDEX_NONE
            && unit->unit_id != 0) {
            u32 identity = unit->character_identity;
            if ((identity == CHARACTER_IDENTITY_ENTD_NONE || identity >= CHARACTER_IDENTITY_RAMZA_END)
                && unit->unit_id >= 4
                && ((unit->unit_flags & UNIT_FLAG_JOIN_AFTER_EVENT)
                    || (unit->status_sets.current[STATUS_BYTE_INDEX(STATUS_ID_INVITE)]
                        & STATUS_BYTE_MASK(STATUS_ID_INVITE)))) {
                if (unit->unit_flags & UNIT_FLAG_SAVE_FORMATION) {
                    s32 column;

                    /* Retail looks up the unit's message identity group and
                     * then does not use it. */
                    for (i = 0; i < PARTY_MESSAGE_IDENTITY_GROUP_COUNT; i++) {
                        for (column = 0; column < PARTY_MESSAGE_IDENTITIES_PER_GROUP; column++) {
                            if (unit->character_identity == g_party_message_identity_groups[i][column]) {
                                break;
                            }
                        }
                        if (column != PARTY_MESSAGE_IDENTITIES_PER_GROUP) {
                            break;
                        }
                    }
                    if (i == PARTY_MESSAGE_IDENTITY_GROUP_COUNT) {
                        i = 0;
                    }
                    if (i == 71) {
                        i = 72;
                    }
                    party_sell_equipment_over_limit(party_index);
                    party_unit_save(unit, unit->unit_flags & UNIT_FLAG_SAVE_FORMATION);
                } else {
                    s32 count;
                    s32 script_variant;
                    unit->unit_flags &= ~(UNIT_FLAG_SAVE_FORMATION | UNIT_FLAG_LOAD_FORMATION);
                    g_party_affected_unit_message_args[g_party_affected_unit_count] = unit->unit_id;
                    script_variant = script_variable_get(EVENT_SCRIPT_VAR_RANDOM_VALUE) & 7;
                    prompt_count++;
                    count = g_party_affected_unit_count;
                    g_party_affected_unit_message_ids[count]
                        = ((unit->character_identity - CHARACTER_IDENTITY_SELECTOR_FIRST) << 3) + script_variant
                        + 0x1800;
                    g_party_affected_battle_unit_indices[count] = party_index;
                    g_party_affected_unit_count = count + 1;
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
 * party_update_brave_faith reports
 * nobody. Otherwise it binds the Brave/Faith text sections, shows Warning
 * and plays each affected unit's warning line in turn before fading the
 * banner out.
 */
void party_brave_faith_report_thread(void) {
    s32 i;
    s32 main_thread;

    if (script_variable_get(EVENT_SCRIPT_VAR_WORLD_DEBUG_BATTLE_STYLE) != 0)
        thread_current_exit();
    if (party_update_brave_faith() == 0)
        thread_current_exit();
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_WARNINGS]
        = g_text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_WARNINGS];
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES]
        = g_text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES];
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_DISMISS_UNIT_REPLIES]
        = g_text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_DISMISS_UNIT_REPLIES];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES]
        = g_text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_JOB_NAMES];
    main_thread = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
    thread_start(main_thread, conditions_special_thread);
    thread_parameters_set(main_thread, CONDITIONS_BANNER_WARNING, 0, 0);
    gfx_fade_rgb_31_frames(0xd, 0xd, 2);
    for (i = 0; i < g_party_affected_unit_count; i++) {
        s32 unit_thread = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
        g_text_substitution_values[0] = g_party_affected_battle_unit_indices[i];
        thread_start(unit_thread, message_dialog_thread);
        thread_parameters_set(
            unit_thread, 0x1b, g_party_affected_unit_message_ids[i], g_party_affected_unit_message_args[i]);
        thread_inactive_wait(unit_thread);
    }
    thread_parameters_set(main_thread, 0, 0, 2);
    thread_inactive_wait(main_thread);
    gfx_fade_rgb_31_frames(6, 5, 2);
    thread_current_exit();
}

/*
 * Thread body of the post-battle desertions.
 *
 * Exits at once unless party_find_departures
 * found someone. Otherwise it binds the Brave/Faith text sections, shows the
 * banner and, for each departing unit in turn, plays its farewell line beside
 * the "left the party" banner for its gender, waits half a second and
 * removes the unit from the roster.
 */
void party_departures_thread(void) {
    s32 i;
    s32 main_thread;

    if (party_find_departures() == 0)
        thread_current_exit();
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_WARNINGS]
        = g_text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_WARNINGS];
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES]
        = g_text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_BRAVE_FAITH_DEPARTURES];
    g_text_section_pointers[AFTER_BATTLE_TEXT_SECTION_DISMISS_UNIT_REPLIES]
        = g_text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_DISMISS_UNIT_REPLIES];
    g_text_section_pointers[TEXT_SECTION_JOB_NAMES]
        = g_text_data + g_text_section_offsets[AFTER_BATTLE_TEXT_SECTION_JOB_NAMES];
    thread_yield();
    main_thread = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
    thread_start(main_thread, conditions_special_thread);
    thread_parameters_set(main_thread, CONDITIONS_BANNER_PARTING_SHOT, 0, 0);
    gfx_fade_rgb_31_frames(0xd, 5, 2);
    thread_yield();
    for (i = 0; i < g_party_affected_unit_count; i++) {
        unit_stats_t* unit;
        s32 unit_thread = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
        s32 message_thread;
        g_text_substitution_values[0] = g_party_affected_battle_unit_indices[i];
        thread_start(unit_thread, message_dialog_thread);
        thread_parameters_set(
            unit_thread, 0x1b, g_party_affected_unit_message_ids[i], g_party_affected_unit_message_args[i]);
        unit = unit_stats_get(g_party_affected_battle_unit_indices[i]);
        message_thread = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);
        thread_start(message_thread, conditions_special_thread);
        if (unit->unit_flags & UNIT_FLAG_FEMALE) {
            thread_parameters_set(message_thread, CONDITIONS_BANNER_SHE_LEFT_COMPANY, 1, 0);
        } else {
            thread_parameters_set(message_thread, CONDITIONS_BANNER_HE_LEFT_COMPANY, 1, 0);
        }
        thread_inactive_wait(unit_thread);
        thread_parameters_set(message_thread, 0, 0, 2);
        thread_inactive_wait(message_thread);
        thread_frames_wait(0x1e);
        party_unit_remove(unit->formation_index);
    }
    thread_parameters_set(main_thread, CONDITIONS_BANNER_PARTING_SHOT, 0, 2);
    thread_inactive_wait(main_thread);
    gfx_fade_rgb_31_frames(6, 5, 2);
    thread_current_exit();
}

/* Thread body of display condition "ready": runs the hovered-unit stats
 * display thread for 60 frames, or until Circle, then stops it. */
void conditions_show_ready_bugged(void) {
    s32 thread_id = thread_slot_resolve(THREAD_SLOT_FIRST_IDLE);

    thread_start(thread_id, unit_view_refresh_hovered_stats);
    thread_parameters_set(thread_id, 0, 0, 0);
    input_wait_frames_or_skip(0x3c);
    thread_parameters_set(thread_id, 0, 0, 1);
    thread_inactive_wait(thread_id);
    thread_current_exit();
}

/* Wait until the menu thread ends or a selection result (-1, 0 or 1) is in,
 * then ask the message thread to close and wait for both threads. */
void menu_wait_selection_threads(void) {
    while (g_threads[AFTER_BATTLE_THREAD_SLOT_MENU].is_running != 0) {
        s32 selection = g_after_battle_menu_selection_result;

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
    g_threads[AFTER_BATTLE_THREAD_SLOT_MESSAGE].task_id = THREAD_TASK_STOP_REQUEST;
    thread_parameters_set(AFTER_BATTLE_THREAD_SLOT_MESSAGE, 0, 0, 2);
    thread_inactive_wait(AFTER_BATTLE_THREAD_SLOT_MESSAGE);
    thread_inactive_wait(AFTER_BATTLE_THREAD_SLOT_MENU);
}

/* Yield until Circle is newly pressed, at least one frame. */
void input_wait_skip(void) {
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
static void status_screen_open_after_battle(void) {
    void* allocation;

    unit_panel_copy_billboard_in(
        &g_unit_panel_selected_unit_status, &g_unit_panel_selected_unit, &g_unit_panel_selected_unit_fields);
    while (1) {
        allocation = heap_reserve_at(g_heap_high_overlay_load_address, 0x1f000);
        if (allocation == g_heap_high_overlay_load_address) {
            break;
        }
        thread_yield();
        if (g_debug_font_print_enabled != 0) {
            FntPrint("WAIT FOR FILEALLOCATION\n");
        }
    }
    overlay_companion_request(COMPANION_OVERLAY_JOBSTTS);
    thread_frames_wait(2);
    g_overlay_companion_overlay_state = COMPANION_OVERLAY_STATE_JOBSTTS;
    do {
        g_menu_input_controller = script_get_controller_input_pointer(0);
        *g_menu_input_controller = 0;
        thread_yield();
    } while (g_overlay_companion_overlay_state != COMPANION_OVERLAY_STATE_IDLE);
    *g_menu_input_controller = 0;
    heap_free(allocation);
    thread_frames_wait(2);
    g_menu_modal_open = 0;
    thread_current_exit();
}

/* The recruitment menu's Status choice: run status_screen_open_after_battle in thread slot 6 and wait for it. */
void status_screen_open_after_battle_thread(void) {
    thread_start(6, status_screen_open_after_battle);
    thread_inactive_wait(6);
    thread_current_exit();
}

/*
 * Thread body: suspend thread g_after_battle_thread_suspended_id and draw the
 * gradient grid and status group primitives every frame in its place until
 * the third thread parameter is set.
 *
 * The suspended thread's record is copied aside first and copied back before
 * the thread is resumed.
 */
void unit_panel_transition_thread(void) {
    s32 frame = 0;
    thread_t* saved;

    thread_suspend(g_after_battle_thread_suspended_id);
    saved = menu_alloc_memory(sizeof(*saved));
    mem_copy_bytes_nonempty(saved, &g_threads[g_after_battle_thread_suspended_id], sizeof(*saved));
    g_unit_panel_status_group_count = 2;
    while (1) {
        s32 buffer = frame & 1;
        gfx_build_gradient_grid_primitives(g_gfx_poly_gt4_buffers[buffer]);
        unit_panel_build_status_groups(g_unit_panel_transition_quads[buffer]);
        thread_yield();
        if (thread_current_parameter_3_get() != 0) {
            break;
        }
        frame++;
    }
    mem_copy_bytes_nonempty(&g_threads[g_after_battle_thread_suspended_id], saved, sizeof(*saved));
    menu_free_memory(saved);
    thread_resume(g_after_battle_thread_suspended_id);
    thread_yield();
    thread_current_exit();
}
