/* Proposition participant messages, success and rewards, countdown and sequence messages, discovery announcements,
 * active propositions and cancel confirmation, sound novel levels, unexplored lands and treasures. */

#include "fft/data.h"
#include "fft/file.h"
#include "fft/gfx.h"
#include "fft/main.h"
#include "fft/main_file.h"
#include "fft/main_sound.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_text.h"
#include "fft/script.h"
#include "fft/thread.h"
#include "fft/unit.h"
#include "fft/window.h"
#include "fft/wldcore.h"
#include "fft/wldcore_map.h"
#include "fft/wldcore_menu.h"
#include "fft/wldcore_proposition.h"
#include "fft/wldcore_script.h"
#include "fft/wldcore_window.h"
#include "fft/world.h"
#include "fft/world_formation.h"
#include "fft/world_menu.h"
#include "fft/world_text.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/suzuki.h"
#include "psx/types.h"

static void wldcore_gfx_prepare_play_time_display(void);
static s32 wldcore_gfx_step_dissolve_image_upload(GsIMAGE* image, s32 step);
static void wldcore_menu_push_sound_novel_history_level(void);
static void wldcore_menu_push_sound_novel_quit_level(void);
static void wldcore_menu_push_sound_novel_resume_level(s32 mode);
static void wldcore_menu_push_unexplored_land_detail_level(s32 value);
static void wldcore_menu_show_next_sequence_message(wldcore_menu_countdown_t* countdown);
static void wldcore_proposition_push_cancel_confirm_level(s32 proposition);
static void wldcore_window_draw_indexed_text(wldcore_menu_sound_novel_history_level_t* level, s32 index);

/* The switch jump table lives at .rodata 0x800671dc (0x18 bytes). */

/*
 * Run the proposition speaker-selection and message sequence.
 *
 * With speaker == -1 the speaker is drawn at random from the selected
 * proposition's participants other than the excluded one; a proposition with a
 * single participant has nobody else to speak and returns 1 at once. Returns 1
 * once the message has been acknowledged and the phase reset, 0 while running.
 */
s32 wldcore_proposition_step_participant_message(wldcore_proposition_report_level_t* state, s32 message, s32 speaker) {
    switch (state->phase) {
    case 0:
        if (speaker == -1) {
            s32 candidates[5];
            s32 count;
            s32 i;

            if (g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_count == 1)
                return 1;
            i = 0;
            count = 0;
            if (g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_count != 0)
                do {
                    s32 participant = g_main_active_propositions[g_wldcore_proposition_report.proposition_index]
                                          .participant_indices[i];
                    if (participant != g_wldcore_proposition_report.excluded_party_index)
                        candidates[count++] = participant;
                    i++;
                } while (
                    i < g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_count);
            g_wldcore_proposition_report.speaker_party_index = candidates[(rand() * count) >> 15];
        } else {
            g_wldcore_proposition_report.speaker_party_index = speaker;
        }
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        state->phase++;
        break;
    case 1:
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE))
            return 0;
        state->phase++;
        break;
    case 2:
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
        g_text_substitution_values[0] = g_wldcore_proposition_report.speaker_party_index + TEXT_ID_UNIT_NAME_BASE;
        script_set_variable(EVENT_SCRIPT_VAR_DIALOG_PORTRAIT,
            world_get_party_unit_formation_sprite(g_wldcore_proposition_report.speaker_party_index));
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19,
            wldcore_proposition_adjust_message_index(message, g_wldcore_proposition_report.speaker_party_index)
                + TEXT_ID_WORLD_MESSAGE_BASE,
            0);
        state->phase++;
        break;
    case 3:
        if (!(g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE))
            return 0;
        if (!world_menu_get_event_state_flag())
            return 0;
        state->phase++;
        break;
    case 4:
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
        state->phase++;
        break;
    case 5:
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE))
            return 0;
        state->phase = 0;
        return 1;
    }
    return 0;
}

/* Proposition job category of a job: 0..19 for Squire..Mime, 22 for the
 * special jobs below Squire and 21 for anything above Mime (monsters). */
static s32 wldcore_map_job_id_to_category_index(s32 job_id) {
    if (job_id < JOB_ID_SQUIRE) {
        return 22;
    }
    if (job_id <= JOB_ID_MIME) {
        return job_id - JOB_ID_SQUIRE;
    }
    return 21;
}

/* Scores the current proposition's success chance.
 *
 * A recalled proposition (progress flag 0x02) clears the JP and gil awards and
 * reports result 2. Otherwise each participant's score accumulates
 * four proposition data tables (records 3..6) keyed on the proposition's
 * fields, the participant's job category and its bravery, faith and level
 * tiers; the total maps onto result 0 (>= 0x63), 1 (>= 0x3b) or 2. A named
 * preferred job then gives a 61 in 100 roll to promote the result back to 0. The
 * preferred-job scan reads the party record once, with the scoring loop's
 * leftover index, as the target does. The target builds the row walk pointers
 * from the proposition_index address, which needs the arrays and that 32-bit value to be
 * members of one object (g_wldcore_proposition_report). */
void wldcore_proposition_determine_success(void) {
    const u8* job_table_a;
    const u8* job_table_b;
    const u8* tier_table_a;
    const u8* tier_table_b;
    party_unit_t* unit;
    s32 participant_count;
    s32 total;
    s32 score;
    s32 tier;
    s32 value;
    s32 i;

    participant_count = g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_count;
    job_table_a = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_CATEGORY_JOB_SCORES);
    job_table_b = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_JOB_LIST_JOB_SCORES);
    tier_table_a = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_BRAVERY_LEVEL_SCORES);
    total = 0;
    tier_table_b = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_FAITH_SCORES);

    if (g_main_active_propositions[g_wldcore_proposition_report.proposition_index].flags
        & WLDCORE_PROPOSITION_PROGRESS_RECALLED) {
        for (i = 0; i < participant_count; i++) {
            g_wldcore_proposition_report.participant_gil[i] = 0;
            g_wldcore_proposition_report.participant_jp[i] = 0;
        }
        g_wldcore_proposition_report.recalled = 1;
        g_wldcore_proposition_report.result = WLDCORE_PROPOSITION_RESULT_BAD_FAILURE;
        g_wldcore_proposition_report.reward_type = WLDCORE_PROPOSITION_REWARD_NONE;
        return;
    }

    g_wldcore_proposition_report.recalled = 0;
    for (i = 0; i < participant_count; i++) {
        unit = wldcore_get_party_unit(
            g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_indices[i]);

        g_wldcore_proposition_report.participant_scores[i]
            = job_table_a[(g_wldcore_selected_proposition_row[0].category_index - 1) * 22
                + wldcore_map_job_id_to_category_index(unit->job_id)];
        g_wldcore_proposition_report.participant_scores[i]
            += job_table_b[(g_wldcore_selected_proposition_row[0].job_list_index - 1) * 22
                + wldcore_map_job_id_to_category_index(unit->job_id)];

        value = unit->bravery;
        if (value != 0) {
            tier = (value - 1) / 20;
        } else {
            tier = 0;
        }
        g_wldcore_proposition_report.participant_scores[i]
            += tier_table_a[(g_wldcore_selected_proposition_row[0].job_list_index - 1) * 5 + tier];

        value = unit->faith;
        if (value != 0) {
            tier = (value - 1) / 20;
        } else {
            tier = 0;
        }
        g_wldcore_proposition_report.participant_scores[i]
            += tier_table_b[(g_wldcore_selected_proposition_row[0].job_list_index - 1) * 5 + tier];

        value = unit->level;
        if (value != 0) {
            tier = (value - 1) / 10;
        } else {
            tier = 0;
        }
        score = g_wldcore_proposition_report.participant_scores[i]
            + tier_table_a[(g_wldcore_selected_proposition_row[0].job_list_index - 1) * 10 + tier];
        total += score;
        g_wldcore_proposition_report.participant_scores[i] = score;
    }

    g_wldcore_proposition_report.result = WLDCORE_PROPOSITION_RESULT_SUCCESS;
    if (total < 0x63) {
        g_wldcore_proposition_report.result = WLDCORE_PROPOSITION_RESULT_FAILURE;
    }
    if (total < 0x3B) {
        g_wldcore_proposition_report.result = WLDCORE_PROPOSITION_RESULT_BAD_FAILURE;
    }
    if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_SUCCESS) {
        return;
    }
    if (g_wldcore_selected_proposition_row[0].preferred_job == 0) {
        return;
    }

    unit = wldcore_get_party_unit(
        g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_indices[i]);
    for (i = 0; i < participant_count; i++) {
        if (g_wldcore_selected_proposition_row[0].preferred_job - 1
            == wldcore_map_job_id_to_category_index(unit->job_id)) {
            if (((rand() * 100) >> 15) < 0x3D) {
                g_wldcore_proposition_report.result = WLDCORE_PROPOSITION_RESULT_SUCCESS;
                return;
            }
        }
    }
}

/* JP and gil of each participant: the proposition's JP and gil
 * amounts, on a failure JP / 10 and gil / 5, on a bad failure both / 20; each
 * then gains the participant's score. No awards are calculated
 * when the proposition was recalled. */
void wldcore_proposition_calculate_base_jp_and_gil(void) {
    const u16* jp_rewards;
    const u16* gil_rewards;
    s32 count;
    s32 i;
    s32 value;

    if (g_wldcore_proposition_report.recalled == 0) {
        jp_rewards = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_JP_AMOUNTS);
        gil_rewards = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_GIL_AMOUNTS);
        count = g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_count;
        value = 0;

        for (i = 0; i < count; i++) {
            value = jp_rewards[g_wldcore_selected_proposition_row[0].jp_reward - 1];
            if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_FAILURE) {
                value = value / 10;
            }
            if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_BAD_FAILURE) {
                value = value / 20;
            }
            g_wldcore_proposition_report.participant_jp[i] = value;
            g_wldcore_proposition_report.participant_jp[i] += g_wldcore_proposition_report.participant_scores[i];
        }

        for (i = 0; i < count; i++) {
            value = gil_rewards[g_wldcore_selected_proposition_row[0].gil_reward - 1];
            if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_FAILURE) {
                value = value / 5;
            }
            if (g_wldcore_proposition_report.result == WLDCORE_PROPOSITION_RESULT_BAD_FAILURE) {
                value = value / 20;
            }
            g_wldcore_proposition_report.participant_gil[i] = value;
            g_wldcore_proposition_report.participant_gil[i] += g_wldcore_proposition_report.participant_scores[i];
        }
    }
}

/* Rolls the reward of a successful proposition.
 *
 * Reward kind 1 or 2 (proposition field 10) picks a random script variable
 * still clear in 0x321..0x33f or 0x350..0x35f, falling back to any index;
 * otherwise field 11 == 1 rolls a three-tier random reward (60/30/10%).
 * The chosen pair from proposition data records 10/11 then adds its bonus
 * to every participant's JP award and sets the reward value. The target places
 * the random-reward branch last, hence the goto. */
void wldcore_proposition_roll_bonus_reward(void) {
    s32 candidates[32];
    u16* bonus_table;
    u16* value_table;
    /* Discovery-row indexing adds two instructions; retain its byte index. */
    u8* location_pairs;
    wldcore_proposition_reward_pair_t* random_pairs;
    s32 count;
    s32 i;
    s32 index;
    s32 participant_count;
    u16 discovery_kind;
    s32 bonus;

    bonus_table = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_JP_AMOUNTS);
    value_table = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_GIL_AMOUNTS);
    location_pairs = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_DISCOVERY_REWARDS);
    random_pairs = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_RANDOM_REWARDS);
    g_wldcore_proposition_report.reward_type = WLDCORE_PROPOSITION_REWARD_NONE;
    g_wldcore_proposition_report.reward_value = 0;
    if (g_wldcore_proposition_report.result != WLDCORE_PROPOSITION_RESULT_SUCCESS) {
        return;
    }
    discovery_kind = g_wldcore_selected_proposition_row[0].discovery_kind;
    if (discovery_kind == WLDCORE_PROPOSITION_REWARD_TREASURE) {
        g_wldcore_proposition_report.reward_type = WLDCORE_PROPOSITION_REWARD_TREASURE;
        count = 0;
        for (i = 0; i < 31; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) == 0) {
                candidates[count] = i;
                count++;
            }
        }
    } else if (discovery_kind == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
        count = 0;
        g_wldcore_proposition_report.reward_type = discovery_kind;
        for (i = 0; i < 16; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) == 0) {
                candidates[count] = i;
                count++;
            }
        }
    } else {
        goto random_reward;
    }
    if (count != 0) {
        index = (rand() * count) >> 15;
        g_wldcore_proposition_report.reward_index = candidates[index];
    } else if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_TREASURE) {
        g_wldcore_proposition_report.reward_index = (rand() * 31) >> 15;
    } else {
        g_wldcore_proposition_report.reward_index = (rand() * 16) >> 15;
    }
    participant_count = g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_count;
    index = (g_wldcore_selected_proposition_row[0].category_index - 1) * 2;
    for (i = 0; i < participant_count; i++) {
        bonus = bonus_table[location_pairs[index + 1] - 1];
        g_wldcore_proposition_report.participant_jp[i] += bonus;
    }
    g_wldcore_proposition_report.reward_value = value_table[location_pairs[index] - 1];
    return;

random_reward:
    if (g_wldcore_selected_proposition_row[0].random_bonus != 1) {
        return;
    }
    g_wldcore_proposition_report.reward_type = WLDCORE_PROPOSITION_REWARD_RANDOM_BONUS;
    index = (rand() * 100) >> 15;
    if (index < 40) {
        if (index < 10) {
            index = 2;
        } else {
            index = 1;
        }
    } else {
        index = 0;
    }
    index += (g_wldcore_selected_proposition_row[0].category_index - 1) * 3;
    g_wldcore_proposition_report.reward_index = index;
    participant_count = g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_count;
    for (i = 0; i < participant_count; i++) {
        bonus = bonus_table[random_pairs[index].jp_index - 1];
        g_wldcore_proposition_report.participant_jp[i] += bonus;
    }
    g_wldcore_proposition_report.reward_value = value_table[random_pairs[index].gil_index - 1];
}

/* Records 0xC and 0xD of the proposition data table are 16-bit percentage
 * ramps indexed by the number of days the proposition has been running. */
void wldcore_proposition_apply_extra_days_multiplier(void) {
    s32 record;
    const u16* reward_scale;
    const u16* bonus_scale;
    s32 day;
    s32 count;
    s32 percent;
    s32 i;

    if (g_wldcore_proposition_report.recalled != 0) {
        return;
    }
    reward_scale = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_EXTRA_DAY_JP_SCALES);
    bonus_scale = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_EXTRA_DAY_GIL_SCALES);
    record = g_wldcore_proposition_report.proposition_index;
    day = g_main_active_propositions[record].elapsed_days - g_wldcore_selected_proposition_row[0].min_days;
    count = g_main_active_propositions[record].participant_count;
    if (day < 0) {
        return;
    }
    if (day >= 10) {
        day = 9;
    }
    percent = reward_scale[day];
    for (i = 0; i < count; i++) {
        g_wldcore_proposition_report.participant_jp[i] = g_wldcore_proposition_report.participant_jp[i] * percent / 100;
    }
    percent = bonus_scale[day];
    for (i = 0; i < count; i++) {
        g_wldcore_proposition_report.participant_gil[i]
            = g_wldcore_proposition_report.participant_gil[i] * percent / 100;
    }
}

/* Return 1 + the party index of the participant the job report praises, or 0
 * for none. A participant whose job falls in the proposition's preferred job
 * category wins, preferring one other than excluded_party_index. Otherwise,
 * among the other participants whose job is in the proposition's nine-job
 * list, the one whose job comes earliest wins, but only when one or two
 * match; three matches name nobody.
 *
 * The target keeps &g_wldcore_proposition_report.proposition_index in s5 and
 * reaches excluded_party_index, 12 bytes below, off the same base, which is
 * why it saves one more callee-saved register than a per-field access would.
 *
 * `base` stays an explicit local so the job-list offset is computed once in
 * the preheader and the inner add uses two registers. The slot test is
 * written `slots[0] > slots[1]`; the reversed spelling swaps the slots[] load
 * pair and costs instructions.
 */
s32 wldcore_proposition_find_preferred_job_unit(void) {
    s32 candidates[4];
    s32 slots[4];
    s32 count;
    s32 i;
    s32 k;
    s32 participant_count;
    s32 party_index;
    s32 base;
    const u8* jobs;
    party_unit_t* unit;

    participant_count = g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_count;
    jobs = wldcore_proposition_get_data_pointer(WLDCORE_PROPOSITION_DATA_JOB_LISTS);
    count = 0;

    if (g_wldcore_selected_proposition_row[0].preferred_job != 0) {
        for (i = 0; i < participant_count; i++) {
            party_index
                = g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_indices[i];
            unit = wldcore_get_party_unit(party_index);
            if (g_wldcore_selected_proposition_row[0].preferred_job - 1
                == wldcore_map_job_id_to_category_index(unit->job_id)) {
                candidates[count] = party_index;
                count++;
            }
        }
    }

    if (count != 0) {
        for (i = 0; i < count; i++) {
            if (candidates[i] != g_wldcore_proposition_report.excluded_party_index) {
                return candidates[i] + 1;
            }
        }
        return candidates[0] + 1;
    }

    base = (g_wldcore_selected_proposition_row[0].job_list_index - 1) * 9;
    for (i = 0; i < participant_count; i++) {
        party_index = g_main_active_propositions[g_wldcore_proposition_report.proposition_index].participant_indices[i];
        if (party_index != g_wldcore_proposition_report.excluded_party_index) {
            unit = wldcore_get_party_unit(party_index);
            for (k = 0; k < 9; k++) {
                if (jobs[base + k] == unit->job_id) {
                    candidates[count] = party_index;
                    slots[count] = k;
                    count++;
                    break;
                }
            }
        }
    }

    if (count == 1) {
        return candidates[0] + 1;
    }
    if (count == 2) {
        if (slots[0] <= slots[1]) {
            return candidates[0] + 1;
        }
        return candidates[1] + 1;
    }
    return 0;
}

/* Push the discovery announcement for a treasure or unexplored-land milestone: the first milestone
 * shows four messages from the kind's first one, each later milestone a single message (first + 2
 * + rank). */
void wldcore_menu_push_countdown_level(void) {
    s32 count = wldcore_get_completion_milestone_rank();

    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_TREASURE) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.current
            = WLDCORE_TEXT_ID_TREASURE_FOUND_FIRST;
    } else {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.current
            = WLDCORE_TEXT_ID_LAND_FOUND_FIRST;
    }
    if (count == 1) {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.remaining = 4;
    } else {
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.remaining = 1;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown.current += 2 + count;
    }
    if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
    }
    wldcore_menu_show_next_sequence_message(&g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].countdown);
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_COUNTDOWN;
    g_wldcore_menu_stack_depth++;
}

/* Show the countdown's next message and count it off; the last one also
 * fades the music out and moves the countdown to
 * WLDCORE_COUNTDOWN_PHASE_LAST_MESSAGE with the timer cleared. */
static void wldcore_menu_show_next_sequence_message(wldcore_menu_countdown_t* countdown) {
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, countdown->current, 0);
    countdown->current++;
    countdown->remaining--;
    if (countdown->remaining == 0) {
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 4);
        countdown->phase = WLDCORE_COUNTDOWN_PHASE_LAST_MESSAGE;
        countdown->timer = 0;
    } else {
        countdown->phase = WLDCORE_COUNTDOWN_PHASE_MESSAGES;
    }
}

/* Return the one-based treasure or unexplored-land collection milestone
 * reached by a successful proposition, or zero when the count is not a milestone. */
s32 wldcore_get_completion_milestone_rank(void) {
    s32 count;
    s32 i;
    s32 index;

    if (g_wldcore_proposition_report.result != WLDCORE_PROPOSITION_RESULT_SUCCESS) {
        return 0;
    }
    if (g_wldcore_proposition_report.reward_type != WLDCORE_PROPOSITION_REWARD_TREASURE
        && g_wldcore_proposition_report.reward_type != WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
        return 0;
    }
    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_TREASURE) {
        count = 0;
        for (i = 0; i < 31; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
                count++;
            }
        }
        for (i = 0; i < 8; i++) {
            if (g_wldcore_treasure_count_milestones[i] == count) {
                break;
            }
        }
        if (i == 8) {
            return 0;
        }
        index = i;
    }
    if (g_wldcore_proposition_report.reward_type == WLDCORE_PROPOSITION_REWARD_UNEXPLORED_LAND) {
        count = 0;
        for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) != 0) {
                count++;
            }
        }
        for (i = 0; i < 8; i++) {
            if (g_wldcore_unexplored_land_count_milestones[i] == count) {
                break;
            }
        }
        if (i == 8) {
            return 0;
        }
        index = i;
    }
    return index + 1;
}

/* Phase driver for a countdown object: phase 1 waits for the menu event flag
 * and plays the milestone music, phase 2 waits 180 frames and returns to the bar music, phase 3
 * pops the menu level through 0x80077174 once both 32-bit busy fields clear; any
 * other phase steps the countdown on a confirm press (0x20). The 32-bit busy fields
 * are read as g_wldcore_audio_queue members, not scalars, which keeps the timer store
 * ahead of their loads as in the target. */
void wldcore_map_step_discovery_announcement(wldcore_menu_countdown_t* countdown) {
    if (countdown->phase == WLDCORE_COUNTDOWN_PHASE_CLOSE) {
        if (g_wldcore_audio_queue.count == 0 && g_wldcore_audio_queue.current_command == WLDCORE_AUDIO_COMMAND_NONE) {
            g_wldcore_menu_stack_depth--;
            wldcore_list_open_completed_propositions();
        }
    } else if (countdown->phase == WLDCORE_COUNTDOWN_PHASE_JINGLE) {
        countdown->timer++;
        if (g_wldcore_audio_queue.count == 0 && g_wldcore_audio_queue.current_command == WLDCORE_AUDIO_COMMAND_NONE
            && countdown->timer >= 180) {
            countdown->phase = WLDCORE_COUNTDOWN_PHASE_CLOSE;
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
            wldcore_sound_enqueue_audio_command(
                WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_BAR));
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
        }
    } else if (countdown->phase == WLDCORE_COUNTDOWN_PHASE_LAST_MESSAGE) {
        if (world_menu_get_event_state_flag() != 0
            && g_wldcore_audio_queue.current_command == WLDCORE_AUDIO_COMMAND_NONE
            && g_wldcore_audio_queue.count == 0) {
            wldcore_sound_enqueue_audio_command(
                WLDCORE_AUDIO_COMMAND_PLAY_MUSIC, WLDCORE_AUDIO_MUSIC(2, MAIN_MUSIC_MILESTONE));
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_IN, 0x10);
            countdown->phase = WLDCORE_COUNTDOWN_PHASE_JINGLE;
        }
    } else if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        wldcore_menu_show_next_sequence_message(countdown);
    }
}

/* Open the bar's in-progress list: every accepted job that has not returned, recalled ones marked,
 * under 'These jobs're in progress.'; up to six rows. */
void wldcore_list_open_propositions(void) {
    point32_t origin;
    wldcore_window_panel_layout_t layout;
    point32_t unused_point;
    s32 count;
    s32 i;
    s32 loaded_count;
    s32 previous;
    s32 total;
    s32 entry_value_base;
    s16* row_flags;
    s32 active_row_flag;

    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_IN_PROGRESS, 0);
    count = 0;
    loaded_count = g_main_save_proposition_count;
    if ((i = 0, loaded_count) > 0) {
        entry_value_base = TEXT_ID_PROPOSITION_NAME_BASE;
        row_flags = g_wldcore_list_row_flags;
        active_row_flag = 8;
        total = loaded_count;
        do {
            if ((g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RETURNED) == 0) {
                g_wldcore_list_entry_values[count] = g_main_active_propositions[i].proposition_id + entry_value_base;
                if ((g_main_active_propositions[i].flags & WLDCORE_PROPOSITION_PROGRESS_RECALLED) != 0) {
                    row_flags[count] = active_row_flag;
                } else {
                    row_flags[count] = 0;
                }
                g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entries[count] = i;
                count++;
            }
            i++;
        } while (i < total);
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 6) {
        i = count;
    } else {
        i = 6;
    }
    layout.text_width = 0x96;
    origin.x = 0x4C;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_PROPOSITIONS, g_main_save_proposition_count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    previous = g_wldcore_menu_stack_depth - 1;
    g_wldcore_menu_ordering_table_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    if ((g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
                .flags
            & WORLD_RENDER_RECORD_HIDDEN)
        != 0) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.first_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.second_window].flags
            &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[previous].window_pair_render.render_index]
            .flags |= WORLD_RENDER_RECORD_POP_IN;
    }
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].window_pair_render.first_window = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITIONS;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

/* Resume handler of the active propositions list: block Circle, Cross and
 * Select for the levels underneath and show prompt 0xB81B again. */
void wldcore_bar_active_propositions_restore_prompt(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_JOBS_IN_PROGRESS, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level runs. */
/* The caller hands in its menu level; the target reads no argument. */
void wldcore_bar_active_propositions_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* List-menu input step: cancel (0x40) and confirm (0x20) close the level
 * through WORLD thread 12, and 0x100 opens help text 0x1062. Once the thread
 * finishes, a confirmed level dispatches the selected row's value through
 * 0x8007af9c.
 *
 * The target keeps each `level->panel.result` store ahead of the `li $a0,5`
 * argument of the following cursor-state call, which lands in the jal delay
 * slot; without the empty barriers GCC schedules the li first.
 */
void wldcore_bar_handle_active_propositions_input(wldcore_menu_stack_record_t* level) {
    u32 buttons;
    s32 unused[2]; /* 8-byte frame slot the target reserves */

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        if (level->panel.result != 0) {
            wldcore_proposition_push_cancel_confirm_level(
                level->panel.entries[g_wldcore_window_panel_render_state.selected_index]);
        } else {
            wldcore_menu_dispatch_resume_handler();
        }
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->panel.result = 0;
        /* Scheduling barrier: keeps the store ahead of the cursor argument. */
        __asm__("");
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_PROPOSITIONS);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    } else if (buttons & PSX_PAD_SELECT) {
        wldcore_bar_active_propositions_block_all_buttons(&level->panel);
        wldcore_menu_push_message_level(0x1062, 1);
    } else if (buttons & PSX_PAD_CIRCLE) {
        if (g_wldcore_list_row_flags[g_wldcore_window_panel_render_state.selected_index] != 0) {
            wldcore_sound_play_effect(MAIN_SFX_INVALID);
            thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_RETURN_TOMORROW, 0);
        } else {
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            level->panel.result = 1;
            /* Scheduling barrier: keeps the store ahead of the cursor argument. */
            __asm__("");
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_PROPOSITIONS);
            g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        }
    }
}

/* Ask whether to call back the party sent on active proposition `proposition`: hide the
 * list level's windows, show the call-back prompt, an OK/No choice with its cursor and
 * frame and the proposition's detail panel, then push the confirm level. */
static void wldcore_proposition_push_cancel_confirm_level(s32 proposition) {
    RECT bounds;
    u8 unused[0x30];
    s32 index;
    s32 depth;
    point32_t* base;

    wldcore_unpack_proposition_row(
        g_wldcore_selected_proposition_row, g_main_active_propositions[proposition].proposition_id);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.proposition = proposition;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_pair_render.first_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_pair_render.second_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .window_pair_render.render_index]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CALL_BACK_PROMPT, 0);

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.frame_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CHOICE_PANEL_HEADER;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.choice_render = index;
    bounds.x = 0x80;
    bounds.w = 0x28;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record_variant(index, bounds, 0, WLDCORE_TEXT_ID_OK_NO, g_wldcore_window_image_buffer);
    base = &g_wldcore_window_render_records[index].base;
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_window_render_records[index].priority = 8;
    base->x = 0x48;
    base->y = 0x20;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_cancel_confirm.cursor_window]
        .position.x = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_cancel_confirm.cursor_window]
        .position.y = g_wldcore_window_render_records[index].base.y + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_cancel_confirm.frame_window]
        .position.x = g_wldcore_window_render_records[index].base.x + 3;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].proposition_cancel_confirm.frame_window]
        .position.y = g_wldcore_window_render_records[index].base.y - 2;
    wldcore_list_build_proposition_detail_panel_image(&g_wldcore_menu_stack_records_next[depth], proposition, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.choice = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].proposition_cancel_confirm.confirmed = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_PROPOSITION_CANCEL_CONFIRM;
    g_wldcore_menu_stack_depth++;
}

/* Build the image of the proposition detail panel for g_main_active_propositions[proposition]:
 * the elapsed/assigned day counts and the location go into text substitution
 * 32-bit values 0-2, and one 16-pixel row is drawn per participant.
 *
 * Both menu variants share the detail render index and row count.
 *
 * The panel's row count is the measured row count of text g_wldcore_selected_proposition_row[0].name_text +
 * 0xf7ff plus two, kept in the level record at +0x0c.
 *
 * The target stores base_x/base_y through one pointer to the pair, and reads
 * every progress field through the array form g_main_active_propositions[proposition], which
 * is what produces its four-instruction ASPSX $at indexed loads.
 *
 * `panel_text` keeps the text-id source's address in a saved register across
 * both reads, as the target does. Assigning bounds.x before
 * position.y is what puts the two by-value halves in the target's registers. */
void wldcore_list_build_proposition_detail_panel_image(
    wldcore_menu_stack_record_t* level, s32 proposition, s32 coordinate_mode) {
    RECT bounds;
    point32_t point;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 rows;
    s32 i;
    u16* panel_text;
    point32_t* base;

    panel_text = &g_wldcore_selected_proposition_row[0].name_text;
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    level->proposition_detail_panel.render_index = index;
    text_measure_pixels(
        &text_width, &text_rows, text_find_entry(*panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1)));
    rows = text_rows + 2;
    level->proposition_detail_panel.panel_rows = rows;
    g_text_substitution_values[0] = g_main_active_propositions[proposition].elapsed_days;
    g_text_substitution_values[1] = g_main_active_propositions[proposition].assigned_days;
    g_text_substitution_values[2] = g_main_active_propositions[proposition].location | TEXT_ID_LOCATION_NAME_BASE;
    bounds.x = 0;
    bounds.y = 0x60;
    bounds.w = 0xF4;
    bounds.h = (g_main_active_propositions[proposition].participant_count + rows) * 0x10 + 0x10;
    wldcore_window_build_render_record_image(index, bounds, coordinate_mode, g_wldcore_proposition_detail_image_buffer);
    point.x = 8;
    point.y = text_rows * 0x10 + 8;
    window_display_text_entry_variant(
        index, WLDCORE_TEXT_ID_JOB_CONDITION_LABELS, point, g_wldcore_proposition_detail_image_buffer);
    point.x = 8;
    point.y = 8;
    window_display_text_entry_variant(
        index, WLDCORE_TEXT_ID_JOB_CONTENTS_LABEL, point, g_wldcore_proposition_detail_image_buffer);
    point.x = 0x44;
    point.y = 8;
    window_display_text_entry_variant(index, *panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1), point,
        g_wldcore_proposition_detail_image_buffer);
    for (i = 0; i < g_main_active_propositions[proposition].participant_count; i++) {
        s32 row = i + 2;

        point.x = 0x3C;
        point.y = (text_rows + row) * 0x10 + 8;
        window_display_text_entry_variant(index,
            g_main_active_propositions[proposition].participant_indices[i] | TEXT_ID_UNIT_NAME_BASE, point,
            g_wldcore_proposition_detail_image_buffer);
    }
    wldcore_window_load_image_record_to_vram(index, g_wldcore_proposition_detail_image_buffer);
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_POP_IN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x7A;
    base->y = -0x28;
}

/* Input step for the call-back question on an active proposition: up/down
 * toggle the highlighted row and move the cursor window. Circle on the first
 * row recalls the party (WLDCORE_PROPOSITION_PROGRESS_RECALLED) and says "Ok,
 * I'll have them return by tomorrow."; Circle on the second row, or Cross,
 * closes the level and reopens the propositions list. */
void wldcore_proposition_handle_cancel_confirm_input(wldcore_menu_stack_record_t* level) {
    s32 choice;

    if (g_wldcore_window_render_records[level->proposition_cancel_confirm.choice_render].flags
        & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (level->proposition_cancel_confirm.confirmed != 0) {
        if (g_wldcore_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            goto close;
        }
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        /* Shared tail: the target keeps this copy and the other close paths
         * jump into it; duplicated copies cross-jump into the last one. */
    close:
        g_wldcore_window_record_count -= 2;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth--;
        wldcore_list_open_propositions();
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0 || wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        choice = level->proposition_cancel_confirm.choice ^ 1;
        level->proposition_cancel_confirm.choice = choice;
        g_wldcore_window_records[level->proposition_cancel_confirm.cursor_window].position.y
            = g_wldcore_window_render_records[level->proposition_cancel_confirm.choice_render].base.y
            - -(choice * 0x10 + 0xE);
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (!(g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE)) {
        return;
    }
    if (level->proposition_cancel_confirm.confirmed != 0) {
        return;
    }
    wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
    if (level->proposition_cancel_confirm.choice == 1) {
        goto close;
    }
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_CALL_BACK_ACCEPTED, 0);
    level->proposition_cancel_confirm.confirmed = 1;
    g_main_active_propositions[level->proposition_cancel_confirm.proposition].flags
        |= WLDCORE_PROPOSITION_PROGRESS_RECALLED;
    g_wldcore_window_records[level->proposition_cancel_confirm.cursor_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[level->proposition_cancel_confirm.frame_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[level->proposition_cancel_confirm.choice_render].flags
        |= WORLD_RENDER_RECORD_HIDDEN;
}

/*
 * Pushes the sound-novel menu level (WLDCORE_MENU_LEVEL_SOUND_NOVEL).
 * `argument` is kept in the level and names the treasure detail reopened
 * when the novel ends; the novel read is the one in
 * g_wldcore_saved_record_index.
 *
 * Hides the context value display, clears VRAM and the scratch buffer, and
 * appends the level's display objects, all hidden: the two button prompts,
 * the VRAM strip record and the overlay. It then appends the novel's sound
 * effect data, restores any saved reading state into the first render record
 * (wldcore_sound_novel_restore_saved_state), blocks Cross, Square and Select
 * for the menu and starts the screen fade.
 */
static void wldcore_menu_push_sound_novel_level(s32 argument) {
    RECT bounds;
    point32_t origin;
    world_animated_render_record_t* window1;
    world_animated_render_record_t* window2;
    world_animated_render_record_t* window4;
    s32 bar_render;
    s32 index;
    point32_t* pos1;
    point32_t* pos2;
    point32_t* pos3;
    point32_t* pos4;
    point32_t* render_pos;

    /* Held from entry: with every saved register taken, reload rematerializes
     * the constant beside its one use, which is the target's la placement. */
    render_pos = &g_wldcore_window_render_records[0].base;
    g_wldcore_saved_context_value_display_position = g_wldcore_context_value_display_position;
    g_main_system_flags |= MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    wldcore_gfx_clear_vram_and_scratch(1);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.message = argument;

    bar_render
        = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.left_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_START_BUTTON;
    window1 = &g_wldcore_window_records[index];
    window1->priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    pos1 = &g_wldcore_window_records[index].position;
    pos1->x = -0x7A;
    pos1->y = 0x60;
    g_wldcore_window_records[index].palette = 0xA;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.right_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_SQUARE_BUTTON;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    window2 = &g_wldcore_window_records[index];
    window2->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    pos2 = &g_wldcore_window_records[index].position;
    pos2->x = 0x42;
    pos2->y = 0x60;
    g_wldcore_window_records[index].palette = 0xA;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.render_index = index;
    bounds.x = 0x1E4;
    bounds.y = 0x1E8;
    bounds.w = 0x20;
    bounds.h = 8;
    origin.x = 0x20;
    origin.y = 0x1E8;
    wldcore_window_init_vram_render_record(index, bounds, origin, 0);
    pos3 = (point32_t*)((s32)render_pos + index * sizeof(wldcore_window_render_record_t));
    pos3->x = 0x34;
    pos3->y = 0x55;
    g_wldcore_window_render_records[index].priority = 8;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_UNTINTED;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.overlay_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_OVERLAY;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_UNTINTED;
    window4 = &g_wldcore_window_records[index];
    window4->anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    pos4 = &g_wldcore_window_records[index].position;
    pos4->x = 0x54;
    pos4->y = 0x58;
    g_wldcore_window_records[index].palette = 0;

    suzuki_sound_append_resource(g_wldcore_sound_novel_sound_resource);
    wldcore_sound_novel_restore_saved_state(bar_render);
    g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
    g_wldcore_menu_ordering_table_offset = 9;
    g_wldcore_screen_fade_state.priorities[0] = 8;
    g_wldcore_screen_fade_state.flags[1] |= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel._unknown_20 = 1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.countdown = 0x270F;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.fade_timer = 0;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel.phase = WLDCORE_SOUND_NOVEL_PHASE_RUN;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL;
    g_wldcore_menu_stack_depth++;
    wldcore_fade_start_screen(0, 0x10);
}

/* Resume handler of the sound novel level: reload the saved background set, show the picture again
 * (and the side windows when the script left them visible) and resume the message thread. */
void wldcore_menu_resume_sound_novel_level(wldcore_menu_stack_record_t* level) {
    s16 set = g_wldcore_active_saved_record.background_set;

    if (set != -1) {
        wldcore_gfx_load_background_set(set);
        file_wait_for_pending_variant();
    }
    g_wldcore_window_render_records[g_wldcore_active_saved_record.picture_render_index].flags
        &= ~WORLD_RENDER_RECORD_BLANK;
    if (g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE) {
        g_wldcore_window_records[level->sound_novel.left_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[level->sound_novel.right_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    }
    thread_resume(WLDCORE_THREAD_SLOT_MESSAGE);
}

/* The g_wldcore_menu_publish_handlers entry of WLDCORE_MENU_LEVEL_SOUND_NOVEL: nothing to publish. */
void wldcore_menu_publish_sound_novel_level(void) {
}

/*
 * Per-frame handler for a WLDCORE brightness/fade menu level.
 *
 * The unused array reproduces the target's 0x40-byte frame. The 0x140 state-flag test is a
 * 32-bit load (`lw`) of the u16 g_wldcore_active_saved_record.state_flags in the target, so it
 * reads the flags through an s32 cast; the u16 declaration gives `lhu`.
 */
void wldcore_menu_step_sound_novel_level(wldcore_menu_sound_novel_level_t* level) {
    s32 unused[2];
    u16* state_flags;
    wldcore_sortbox_state_t* fade;
    s16* brightness;
    u32 buttons;
    s32 width;
    s32 window;
    u16 shade;

    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && !(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
        switch (level->phase) {
        case WLDCORE_SOUND_NOVEL_PHASE_OPEN_HISTORY:
            if (level->countdown == 0) {
                g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
                wldcore_menu_push_sound_novel_history_level();
                return;
            }
            level->countdown--;
            return;
        case WLDCORE_SOUND_NOVEL_PHASE_END:
            if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
                return;
            }
            g_menu_blocked_buttons = 0;
            g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            wldcore_gfx_prepare_play_time_display();
            fade = &g_wldcore_screen_fade_state;
            g_wldcore_screen_fade_state.priorities[0] = 0xF;
            g_wldcore_screen_fade_state.flags[1] = 0;
            g_wldcore_window_record_count -= 3;
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 5;
            g_wldcore_menu_stack_depth--;
            fade->flags[0] = (fade->flags[0] & ~WLDCORE_SCREEN_FADE_BOX_VISIBLE) | WLDCORE_SCREEN_FADE_OUT;
            main_sound_stop_weather_sfx_music();
            suzuki_sound_remove_resource(g_wldcore_sound_novel_sound_resource);
            wldcore_reset_selected_saved_record(g_wldcore_saved_record_index);
            g_wldcore_sound_novel_countdown_frames = 0;
            wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TREASURE_DETAIL, level->message, 0);
            return;
        }
    }

    state_flags = &g_wldcore_active_saved_record.state_flags;
    if (*state_flags & WLDCORE_OPCODE_STATE_END) {
        return;
    }

    if (wldcore_script_run_until_yield() != 0) {
        if (g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1)) {
            if (g_input_current_buttons & PSX_PAD_L1) {
                if (g_wldcore_active_saved_record.brightness < 0xFF) {
                    g_wldcore_active_saved_record.brightness++;
                }
            }
            if (g_input_current_buttons & PSX_PAD_R1) {
                brightness = &g_wldcore_active_saved_record.brightness;
                if (*brightness != 0) {
                    (*brightness)--;
                }
            }
            shade = g_wldcore_active_saved_record.brightness;
            g_wldcore_screen_fade_state.boxes[1].r = shade;
            g_wldcore_screen_fade_state.boxes[1].g = shade;
            g_wldcore_screen_fade_state.boxes[1].b = shade;
            level->fade_timer = 0x3C;
        }

        if (level->fade_timer != 0) {
            g_wldcore_window_render_records[level->render_index].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[level->overlay_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            width = 0x100 - g_wldcore_active_saved_record.brightness;
            g_wldcore_window_render_records[level->render_index].bounds.w = width / 4;
            level->fade_timer--;
        } else {
            g_wldcore_window_render_records[level->render_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[level->overlay_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
        }

        if (world_menu_get_event_state_flag() != 0) {
            buttons = g_wldcore_input_new_button_presses;
            if (buttons & PSX_PAD_CIRCLE) {
                return;
            }
            g_wldcore_window_records[level->left_window].palette = g_wldcore_window_records[level->right_window].palette
                = 0;
            if (buttons & PSX_PAD_SELECT) {
                wldcore_menu_push_message_level(0x1036, 1);
                return;
            }
            if ((buttons & PSX_PAD_START)
                && (g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE)
                && !(g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_WHITE_OVERLAY)) {
                g_wldcore_window_records[level->left_window].sequence
                    = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_START_BUTTON_PRESS;
                g_wldcore_window_records[level->left_window].frame_index
                    = g_wldcore_window_records[level->left_window].anim_counter = 0;
                level->fade_timer = 0;
                g_wldcore_window_render_records[level->render_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
                g_wldcore_window_records[level->overlay_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
                wldcore_menu_push_sound_novel_quit_level();
                return;
            }
            if ((g_input_current_buttons & PSX_PAD_SQUARE)
                && (g_wldcore_active_saved_record.state_flags & WLDCORE_OPCODE_STATE_PARENT_WINDOWS_VISIBLE)
                && g_wldcore_active_saved_record.text_history_3 != -1) {
                g_wldcore_window_records[level->right_window].sequence
                    = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_SQUARE_BUTTON_PRESS;
                window = level->right_window;
                g_wldcore_window_records[window].anim_counter = 0;
                g_wldcore_window_records[window].frame_index = 0;
                level->fade_timer = 0;
                g_wldcore_window_render_records[level->render_index].flags |= WORLD_RENDER_RECORD_HIDDEN;
                g_wldcore_window_records[level->overlay_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
                g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
                level->phase = WLDCORE_SOUND_NOVEL_PHASE_OPEN_HISTORY;
                level->countdown = 8;
            }
        } else {
            g_wldcore_window_records[level->left_window].palette = g_wldcore_window_records[level->right_window].palette
                = 0xA;
        }
    } else {
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            world_thread_request_stop(WLDCORE_THREAD_SLOT_MESSAGE);
        }
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x20);
        main_sound_stop_weather_sfx_music();
        level->phase = WLDCORE_SOUND_NOVEL_PHASE_END;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        if (!(*state_flags & WLDCORE_OPCODE_STATE_WHITE_OVERLAY)) {
            wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        }
    }
}

/* Pushes the type-0x16 menu level: two window records (sequences 2 and 4)
 * and one render record showing text 0xb83a in a 0x3c x 0x30 image at
 * (-0x1e, -0x18), with the windows offset inside it. Blocks every button
 * (-1), clears the fade box color, sets fade flag 8 and clears the new
 * level's 32-bit values 0x10..0x1c.
 *
 * The level is written through the save_confirm member of the stack-record
 * union; union field access keeps the target's per-field `$at` bases. The
 * fade state is written through a pointer, which keeps the
 * target's register base for the offset-0 32-bit flags. The unused array
 * preserves the target's 0x68-byte frame. */
static void wldcore_menu_push_sound_novel_quit_level(void) {
    RECT bounds;
    u8 unused[0x28];
    point32_t* base;
    s32 index;
    s32 depth;
    wldcore_sortbox_state_t* fade;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].save_confirm.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].save_confirm.second_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_LIST_SIDE_LABEL;
    g_wldcore_window_records[index].priority = 8;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].save_confirm.render_index = index;
    bounds.x = 0x80;
    bounds.w = 0x3C;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record_variant(index, bounds, 0, WLDCORE_TEXT_ID_QUIT_CONTINUE, g_wldcore_window_image_buffer);
    base = &g_wldcore_window_render_records[index].base;
    g_wldcore_window_render_records[index].priority = 8;
    depth = g_wldcore_menu_stack_depth;
    base->x = -(bounds.w / 2);
    base->y = -0x18;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.cursor_window].position.x
        = g_wldcore_window_render_records[index].base.x + 6;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.cursor_window].position.y
        = g_wldcore_window_render_records[index].base.y + 0xE;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.second_window].position.x
        = g_wldcore_window_render_records[index].base.x + 4;
    g_menu_blocked_buttons = -1;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth].save_confirm.second_window].position.y
        = g_wldcore_window_render_records[index].base.y - 2;
    g_wldcore_menu_stack_depth = depth + 1;
    fade = &g_wldcore_screen_fade_state;
    fade->boxes[0].r = 0;
    fade->boxes[0].g = 0;
    fade->boxes[0].b = 0;
    fade->flags[0] |= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
    g_wldcore_menu_stack_records_next[depth].save_confirm.choice = 0;
    g_wldcore_menu_stack_records_next[depth].save_confirm._unknown_18 = 0;
    g_wldcore_menu_stack_records_next[depth].save_confirm.fade_in = 0;
    g_wldcore_menu_stack_records_next[depth].save_confirm.fade_out = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL_QUIT;
}

/* Restore the save confirmation palettes and cursor animation. */
void wldcore_window_clear_slot_state(wldcore_menu_stack_record_t* level) {
    s32 first = level->save_confirm.cursor_window;
    s32 second = level->save_confirm.second_window;
    s32 index;
    /* The original frame reserves eight unused bytes. */
    s32 scratch[2];

    g_wldcore_window_render_records[level->save_confirm.render_index].palette = 0;
    g_wldcore_window_records[second].palette = 0;
    g_wldcore_window_records[first].palette = 0;
    g_wldcore_window_records[level->save_confirm.cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    index = level->save_confirm.cursor_window;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}

/* Restore the save confirmation windows and restart the cursor animation. */
void wldcore_window_reset_slot_state(wldcore_menu_save_confirm_level_t* level) {
    s32 index;

    g_wldcore_window_records[level->cursor_window].palette = 10;
    g_wldcore_window_records[level->second_window].palette = 6;
    g_wldcore_window_render_records[level->render_index].palette = 2;
    g_wldcore_window_records[level->cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_STILL;
    index = level->cursor_window;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
}

/* Step handler for the save confirmation level.
 *
 * Drives the screen dim: fade_in raises the overlay color in steps of 8 and
 * fade_out lowers it again, popping the level when it reaches 0. While no fade
 * runs it takes input: button 0x100 opens the help text for the highlighted
 * row through g_wldcore_sound_novel_quit_help_text_ids, up/down toggle the two rows and move the cursor
 * window, button 0x20 on row 0 starts the save (system flag 4, audio command
 * 2/0x20, main_sound_stop_weather_sfx_music), and button 0x20 on row 1 or
 * button 0x40 begins the fade out. Once the save finishes (WORLD thread 14
 * idle) it shifts the operand history g_wldcore_sound_novel_text_history_0..66, captures the play time
 * and hands off to wldcore_menu_push_screen_transition_level with screen 0x1b.
 *
 * Every color store rereads the 32-bit fade value through a local: a direct
 * `u8 = level->fade_in` assignment narrows the reload to `lbu`, while the
 * target keeps `lw`. The target stores the -1 history marker with a signed
 * `addiu`, so it is written through an s16 view of the u16 slot. The cursor
 * update needs the window pointer, the base_y local and a separate offset
 * local: that keeps base_y as the first `addu` operand and stops fold from
 * reassociating the +0xe. */
void wldcore_menu_step_sound_novel_quit_level(wldcore_menu_save_confirm_level_t* level) {
    u32 flags;
    u32 buttons;
    s32 value;
    s32 fade;
    s32 base;
    s32 offset;
    world_animated_render_record_t* record;
    s32 window;
    u16 first;

    if (level->fade_in < 0x60) {
        level->fade_in += 8;
        fade = level->fade_in;
        g_wldcore_screen_fade_state.boxes[0].r = fade;
        fade = level->fade_in;
        g_wldcore_screen_fade_state.boxes[0].g = fade;
        fade = level->fade_in;
        g_wldcore_screen_fade_state.boxes[0].b = fade;
    }

    if (level->fade_out != 0) {
        level->fade_out -= 8;
        fade = level->fade_out;
        g_wldcore_screen_fade_state.boxes[0].r = fade;
        fade = level->fade_out;
        g_wldcore_screen_fade_state.boxes[0].g = fade;
        fade = level->fade_out;
        g_wldcore_screen_fade_state.boxes[0].b = fade;
        if (level->fade_out <= 0) {
            g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
            g_wldcore_screen_fade_state.flags[0] ^= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count -= 3;
            g_wldcore_menu_stack_depth--;
            wldcore_menu_dispatch_resume_handler();
        }
        return;
    }

    if (g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    flags = g_main_system_flags;
    if (flags & MAIN_SYSTEM_FLAG_FADING) {
        return;
    }

    if (flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) {
        if (level->fade_in < 0xFF) {
            level->fade_in += 8;
            if (level->fade_in >= 0x100) {
                level->fade_in = 0xFF;
            }
            fade = level->fade_in;
            g_wldcore_screen_fade_state.boxes[0].r = fade;
            fade = level->fade_in;
            g_wldcore_screen_fade_state.boxes[0].g = fade;
            fade = level->fade_in;
            g_wldcore_screen_fade_state.boxes[0].b = fade;
            return;
        }
        if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        g_wldcore_screen_brightness_rgb.r = 0;
        g_wldcore_screen_brightness_rgb.g = 0;
        g_wldcore_screen_brightness_rgb.b = 0;
        g_wldcore_screen_fade_state.flags[0] |= WLDCORE_SCREEN_FADE_OUT;
        first = g_wldcore_sound_novel_text_history_0;
        g_wldcore_sound_novel_text_history_0 = -1;
        g_menu_blocked_buttons = 0;
        g_wldcore_sound_novel_countdown_frames = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_sound_novel_text_history_3 = g_wldcore_sound_novel_text_history_2;
        g_wldcore_sound_novel_text_history_2 = g_wldcore_sound_novel_text_history_1;
        g_wldcore_sound_novel_text_history_1 = first;
        wldcore_gfx_prepare_play_time_display();
        g_wldcore_screen_fade_state.priorities[0] = 0xF;
        g_wldcore_screen_fade_state.flags[1] = 0;
        g_wldcore_window_record_count -= 3;
        g_wldcore_window_render_record_count -= 2;
        g_wldcore_window_render_object_count -= 5;
        g_wldcore_menu_stack_depth -= 2;
        suzuki_sound_remove_resource(g_wldcore_sound_novel_sound_resource);
        wldcore_menu_push_screen_transition_level(WLDCORE_MENU_LEVEL_TREASURE_DETAIL,
            g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].list_window.selected_entry, 0);
        return;
    }

    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        level->fade_out = level->fade_in;
        return;
    }
    if (buttons & PSX_PAD_SELECT) {
        wldcore_window_reset_slot_state(level);
        wldcore_menu_push_text_window_level(g_wldcore_sound_novel_quit_help_text_ids[level->choice] | 0x1000, 1);
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0 || wldcore_input_update_repeat(PSX_PAD_DOWN) != 0) {
        value = level->choice ^ 1;
        level->choice = value;
        record = &g_wldcore_window_records[level->cursor_window];
        base = g_wldcore_window_render_records[level->render_index].base.y;
        offset = value * 0x10 + 0xE;
        record->position.y = base + offset;
        wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) {
        g_wldcore_window_records[level->cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        window = level->cursor_window;
        g_wldcore_window_records[window].anim_counter = 0;
        g_wldcore_window_records[window].frame_index = 0;
        if (level->choice == 0) {
            g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count -= 3;
            if (thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) != 0) {
                world_thread_request_stop(WLDCORE_THREAD_SLOT_MESSAGE);
            }
            wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x20);
            main_sound_stop_weather_sfx_music();
            wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            return;
        }
        level->fade_out = level->fade_in;
    }
}

/* Restore the HUD position and display the current play time.
 * Cache the flags and time before publishing the display fields. */
static void wldcore_gfx_prepare_play_time_display(void) {
    u32 flags;
    s32 hours;
    s32 minutes;
    s32 seconds;

    g_wldcore_context_value_display_position = g_wldcore_saved_context_value_display_position;
    g_wldcore_context_value_display_mode = WLDCORE_VALUE_DISPLAY_PLAY_TIME;
    g_wldcore_hud_ot_priority = 1;
    flags = g_main_system_flags;
    hours = g_main_system_play_time_hours;
    minutes = g_main_system_play_time_minutes;
    seconds = g_main_system_play_time_seconds;
    g_main_system_flags = flags | MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_wldcore_displayed_numeric_value = hours;
    g_wldcore_displayed_numeric_value_secondary = minutes;
    g_wldcore_displayed_numeric_value_tertiary = seconds;
}

/* Pushes the save-slot selection level (menu type 0x17).
 *
 * Suspends WORLD thread 14, appends the two header windows (kinds 0x6c and
 * 0x6d, the second hidden) and the 0xf8 x 0xb0 panel render record built from
 * g_wldcore_window_image_buffer, centers the newest history entry's name text
 * (0x8800 + g_wldcore_active_saved_record.text_history_3) in it, and copies the four operand-history entries
 * (newest first) into the level at +0x0c..+0x18. It also tints the picture
 * render record to half brightness, clears the screen dim and sets the menu
 * result to -1. The first header window is hidden when the second history
 * entry is empty.
 *
 * The level is written through the sound_novel_history member of the
 * stack-record union (as in wldcore_menu_push_sound_novel_quit_level).
 *
 * Several spellings are load-bearing. `windows` holds the record base in a
 * saved register: the window positions are derived from it and the second
 * window's anim_counter store goes through it, while the other field stores use
 * the absolute `$at` form. The render position and color pointers reproduce
 * the target's shared render-record base. The history is read through a
 * pointer to its newest entry: the indexed reads count as in-struct
 * references, so the scheduler keeps each load after the preceding level
 * store. `top` and `depth` are separate locals for the target's register
 * assignment, and `unused` preserves the 0x50-byte frame. */
static void wldcore_menu_push_sound_novel_history_level(void) {
    RECT bounds;
    u8 unused[8];
    world_animated_render_record_t* windows;
    wldcore_window_xy_t* positions;
    point32_t* position;
    CVECTOR* colors;
    s16* newest;
    s32 index;
    s32 depth;
    s32 top;

    thread_suspend(WLDCORE_THREAD_SLOT_MESSAGE);

    windows = g_wldcore_window_records;
    positions = (wldcore_window_xy_t*)&windows->position.x;
    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_history.upper_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_HISTORY_UPPER;
    g_wldcore_window_records[index].priority = 9;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    positions[index].x = 0;
    positions[index].y = -0x6A;

    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_history.lower_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_SOUND_NOVEL_HISTORY_LOWER;
    g_wldcore_window_records[index].priority = 9;
    windows[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    g_wldcore_window_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    positions[index].x = 0;
    positions[index].y = 0x5E;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_history.render_index = index;
    bounds.w = 0xF8;
    bounds.h = 0xB0;
    bounds.x = 0;
    bounds.y = 0;
    wldcore_window_build_render_record_image(index, bounds, 1, g_wldcore_window_image_buffer);
    position = &g_wldcore_window_render_records[index].base;
    newest = &g_wldcore_active_saved_record.text_history_3;
    g_wldcore_window_render_records[index].priority = g_wldcore_menu_ordering_table_offset;
    position->x = -0x80;
    position->y = -0x64;
    g_wldcore_window_render_records[index].clut.x = 0x3C0;
    g_wldcore_window_render_records[index].clut.y = 0x1F2;
    wldcore_window_draw_centered_text(index, *newest + TEXT_ID_WORLD_MESSAGE_BASE);

    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                 .window_pair_render.first_window]
        .palette = 0xA;
    /* Direct record.color access changes the column base and instruction scheduling. */
    colors = &g_wldcore_window_render_records->color;
    ((CVECTOR*)((u8*)colors
         + g_wldcore_active_saved_record.picture_render_index * sizeof(g_wldcore_window_render_records[0])))
        ->r = 0x80;
    ((CVECTOR*)((u8*)colors
         + g_wldcore_active_saved_record.picture_render_index * sizeof(g_wldcore_window_render_records[0])))
        ->g = 0x80;
    ((CVECTOR*)((u8*)colors
         + g_wldcore_active_saved_record.picture_render_index * sizeof(g_wldcore_window_render_records[0])))
        ->b = 0x80;
    g_wldcore_screen_fade_state.boxes[0].r = 0;
    g_wldcore_screen_fade_state.boxes[0].g = 0;
    g_wldcore_screen_fade_state.boxes[0].b = 0;
    g_wldcore_screen_fade_state.flags[0] |= WLDCORE_SCREEN_FADE_BOX_VISIBLE;

    top = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.scroll = 0;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.newest_text_id = *newest;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[0] = newest[-1];
    g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[1] = newest[-2];
    g_menu_blocked_buttons = -1;
    g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[2] = newest[-3];
    if (g_wldcore_menu_stack_records_next[top].sound_novel_history.text_ids[0] == -1) {
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[top].sound_novel_history.upper_window].flags
            |= WORLD_RENDER_RECORD_HIDDEN;
    }

    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].sound_novel_history.phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_DIM;
    g_wldcore_menu_stack_records_next[depth].sound_novel_history.step = 0;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL_HISTORY;
    g_wldcore_menu_stack_depth = depth + 1;
}

/* Handler of the history-text level (type 0x17).
 *
 * Phase 1 steps the picture render record (g_wldcore_active_saved_record.picture_render_index) and g_wldcore_hud_color
 * from 0x80 down to 0x60 while the screen fade box brightens, in five steps. Phase 2 runs the reverse ramp and then, if
 * g_wldcore_active_saved_record.text_id names a text, clears the image buffer for the level's render record (width *
 * height / 8 32-bit values), renders text 0x8800 + g_wldcore_active_saved_record.text_id into it, uploads it and clears
 * script variables 0x5c-0x5e; it then pops the level and resumes WORLD thread 14. Phase 0 scrolls the text history with
 * up/down, hiding the arrow window at either end, and starts phase 2 once button bit 0x80 is released.
 *
 * Load-bearing spellings: the nonzero phases are a switch nested under
 * `phase != 0`, matching the target's test order; each fade case holds
 * &g_wldcore_active_saved_record.picture_render_index in its own pointer local, and `shade` is shared between the
 * cases (a global pseudo, as in the target's $a2); the text box position is
 * read into `x`/`y` so the s32 point is loaded with `lw`; `unused` preserves
 * the 0x58-byte frame. */
void wldcore_menu_step_sound_novel_history_level(wldcore_menu_sound_novel_history_level_t* level) {
    glyph_blit_t box;
    point32_t point;
    u8 unused[0x20];
    u32* image;
    s16* index1;
    s16* index2;
    s32 shade;
    s32 i;
    s32 text_id;
    s32 step;
    s32 depth;
    s32 render_index;
    s32 x;
    s32 y;

    if (level->phase != WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE) {
        switch (level->phase) {
        case WLDCORE_SOUND_NOVEL_HISTORY_PHASE_DIM:
            shade = 0x80 - level->step * 8;
            index1 = &g_wldcore_active_saved_record.picture_render_index;
            g_wldcore_window_render_records[*index1].color.r = shade;
            g_wldcore_window_render_records[*index1].color.g = shade;
            g_wldcore_window_render_records[*index1].color.b = shade;
            g_wldcore_hud_color.r = shade;
            g_wldcore_hud_color.g = shade;
            g_wldcore_hud_color.b = shade;
            shade = level->step * 8;
            g_wldcore_screen_fade_state.boxes[0].r = shade;
            g_wldcore_screen_fade_state.boxes[0].g = shade;
            g_wldcore_screen_fade_state.boxes[0].b = shade;
            step = level->step + 1;
            level->step = step;
            if (step >= 5) {
                level->step = 0;
                level->phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE;
            }
            break;
        case WLDCORE_SOUND_NOVEL_HISTORY_PHASE_RESTORE:
            index2 = &g_wldcore_active_saved_record.picture_render_index;
            shade = level->step * 0x10 + 0x40;
            g_wldcore_window_render_records[*index2].color.r = shade;
            g_wldcore_window_render_records[*index2].color.g = shade;
            g_wldcore_window_render_records[*index2].color.b = shade;
            g_wldcore_hud_color.r = shade;
            g_wldcore_hud_color.g = shade;
            g_wldcore_hud_color.b = shade;
            shade = 0x20 - level->step * 8;
            g_wldcore_screen_fade_state.boxes[0].r = shade;
            g_wldcore_screen_fade_state.boxes[0].g = shade;
            g_wldcore_screen_fade_state.boxes[0].b = shade;
            step = level->step + 1;
            level->step = step;
            if (step < 5) {
                break;
            }
            text_id = g_wldcore_active_saved_record.text_id;
            if (text_id != -1) {
                text_id += TEXT_ID_WORLD_MESSAGE_BASE;
                i = 0;
                image = g_wldcore_window_image_buffer;
                point.x = 8;
                point.y = 8;
                for (;;) {
                    render_index = level->render_index;
                    if (i >= g_wldcore_window_render_records[render_index].bounds.w
                            * g_wldcore_window_render_records[render_index].bounds.h / 8) {
                        break;
                    }
                    i++;
                    *image = 0;
                    image++;
                }
                x = point.x;
                y = point.y;
                box.rect.x = x;
                box.rect.y = y;
                box.stride = g_wldcore_window_render_records[level->render_index].bounds.w;
                menu_display_text_entry_past_choice_marks(text_id, g_wldcore_window_image_buffer, &box);
                wldcore_window_load_image_record_to_vram(level->render_index, g_wldcore_window_image_buffer);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
            }
            level->step = 0;
            level->phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_BROWSE;
            g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
            g_wldcore_screen_fade_state.flags[0] ^= WLDCORE_SCREEN_FADE_BOX_VISIBLE;
            depth = g_wldcore_menu_stack_depth;
            g_wldcore_window_record_count -= 2;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count -= 3;
            g_wldcore_menu_stack_depth = depth - 1;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].window_pair_render.first_window]
                .palette = 0;
            thread_resume(WLDCORE_THREAD_SLOT_MESSAGE);
            break;
        }
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) != 0) {
        if (level->scroll < 3 && level->text_ids[level->scroll] != -1) {
            wldcore_window_draw_indexed_text(level, 4);
            level->scroll = level->scroll + 1;
            if (level->scroll == 3 || level->text_ids[level->scroll] == -1) {
                g_wldcore_window_records[level->upper_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
            } else {
                g_wldcore_window_records[level->upper_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
            }
            g_wldcore_window_records[level->lower_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        }
    }
    if (wldcore_input_update_repeat(PSX_PAD_DOWN) != 0 && level->scroll != 0) {
        wldcore_window_draw_indexed_text(level, 2);
        level->scroll = level->scroll - 1;
        if (level->scroll == 0) {
            g_wldcore_window_records[level->lower_window].flags |= WORLD_RENDER_RECORD_HIDDEN;
        } else {
            g_wldcore_window_records[level->lower_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
        }
        g_wldcore_window_records[level->upper_window].flags &= ~WORLD_RENDER_RECORD_HIDDEN;
    }
    if (!(g_input_current_buttons & PSX_PAD_SQUARE)) {
        level->phase = WLDCORE_SOUND_NOVEL_HISTORY_PHASE_RESTORE;
    }
}

/* Skip -1, otherwise draw the history text in text section 17 (0x8800). */
static void wldcore_window_draw_indexed_text(wldcore_menu_sound_novel_history_level_t* level, s32 index) {
    /* Named text-array access reverses the final address-add operands. */
    s32 text_id = ((s32*)level)[index + level->scroll];

    if (text_id != -1) {
        wldcore_window_draw_centered_text(level->render_index, text_id + TEXT_ID_WORLD_MESSAGE_BASE);
    }
}

/* Open the Brave Story list of unexplored lands found so far (their script flags set), up to eight
 * rows. */
void wldcore_list_open_unexplored_lands(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s32 count = 0;
    s32 i; /* reused as the visible-row count, which the target keeps in $s0 */
    s32 record_index;

    for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) != 0) {
            g_wldcore_list_entry_values[count] = i + TEXT_ID_UNEXPLORED_LAND_NAME_BASE;
            g_wldcore_list_row_flags[count] = 0;
            count++;
        }
    }
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x60;
    origin.x = 0x80;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_UNEXPLORED_LANDS, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_UNEXPLORED_LANDS;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* Resume handler of the unexplored lands list: block Circle, Cross and Select for the
 * levels underneath and show prompt 0xB848 again. */
void wldcore_list_unexplored_lands_restore_prompt(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
}

/* The caller hands in its menu level; the target reads no argument. */
void wldcore_list_unexplored_lands_block_all_buttons(wldcore_menu_panel_level_t* level) {
    g_menu_blocked_buttons = -1;
}

/* Twin of wldcore_list_handle_rumors_input for cursor slot 7: closes the level inline, opens
 * help message 0x1058 on 0x100, and hands a confirmed entry to 0x8007e360.
 * Cancel (0x40) is accepted even while a file is loading.
 *
 * The target passes the level to 0x8007e178, which reads no argument. */
void wldcore_list_handle_unexplored_lands_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_UNEXPLORED_LANDS);
            wldcore_menu_push_unexplored_land_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_UNEXPLORED_LANDS);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if (g_wldcore_input_new_button_presses & PSX_PAD_CROSS) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_list_unexplored_lands_block_all_buttons(level);
        wldcore_menu_push_message_level(0x1058, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes the unexplored-land detail text window as menu level 0x19. Records
 * the land on the new level, loads that land's picture data set, appends a
 * render record for the picture and places it at (-116, -34), decodes the
 * land's discovery date out of the packed nine-bit array at
 * g_main_land_discovery_date_bits (date.month month 1-12, date.day day 1-31), and
 * sets up the shared scrollable text window at g_wldcore_scrollable_text_window for text
 * 0x8800 + land with the month name (0xb84f + month) and the day as its two
 * substitutions. Then hides the parent list-window level's four window records
 * and its frame render record with flag 0x10 and starts WORLD thread 14 on
 * text 0xb849 with substitution value 0xd800 + land.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The render record's position is written through a pointer to its base_x: the
 * target reaches base_x/base_y at 0 and 4 off one register holding
 * &g_wldcore_window_render_records[index].base_x, while the flags update above
 * it still uses the $at absolute form. Spelling both coordinates as ordinary
 * indexed field stores recomputes the $at base twice and costs four
 * instructions. The cast is over adjacent s32 base_x/base_y; the header would
 * express it better as a point32_t member.
 *
 * `unused` reproduces eight bytes of locals below the date bytes that the
 * target never touches. */
static void wldcore_menu_push_unexplored_land_detail_level(s32 value) {
    s32 index;
    point32_t* base;
    s32 coordinate_mode = 1;
    u8 unused[8];
    calendar_date_t date;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.value = value;
    wldcore_proposition_load_picture_data_set(g_wldcore_unexplored_land_picture_sets[value]);
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.render_index = index;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    base = &g_wldcore_window_render_records[index].base;
    base->x = -0x74;
    base->y = -0x22;
    wldcore_calendar_read_date(g_main_land_discovery_date_bits, value, &date);
    g_wldcore_scrollable_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x60;
        break;
    case 1:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        break;
    }
    g_wldcore_scrollable_text_window.base.x = -122;
    g_wldcore_scrollable_text_window.base.y = -40;
    g_wldcore_scrollable_text_window.text_width = 0xdc;
    g_wldcore_scrollable_text_window.rows_per_page = 8;
    g_wldcore_scrollable_text_window.priority = 0xa;
    g_wldcore_scrollable_text_window.image_coordinate_mode = coordinate_mode;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = date.month + WLDCORE_TEXT_ID_MONTH_NAME_BASE;
    g_wldcore_scrollable_text_window.text_substitutions[1] = date.day;
    wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.main_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.side_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.frame_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.upper_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.lower_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_UNEXPLORED_LAND_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.phase = WLDCORE_DETAIL_PHASE_LOAD;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_UNEXPLORED_LAND_DETAIL;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

/* Handler of the unexplored-land detail level (type 0x19).
 *
 * Phase 1 waits for the render record the 32-bit value at 0x08 names to leave its 0x100 state; no push sets
 * that 32-bit value.
 * Phase 2, once file loading finishes, uploads the land picture's TIM pixels
 * and CLUT, binds the level's render record to the VRAM image at (-116, -34)
 * and makes it the scrollable text window's extra render record (position and
 * image rectangle). Phase 3 waits for that render record. Phase 0 waits for
 * the text window to finish, restores the parent list-window level's windows
 * and frame render record, pops the level and reopens the unexplored-land
 * list.
 *
 * The TIM widths are read unsigned (`lhu`), as Psy-Q's GsIMAGE declares
 * pw/ph. The text window's base and image rectangle are each copied as one
 * aggregate: the target loads both 32-bit values before either store. */
void wldcore_menu_step_unexplored_land_detail_level(wldcore_menu_variable_detail_level_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t* base;
    s32* index;
    s32 render;
    s32 depth;

    switch (level->phase) {
    case WLDCORE_DETAIL_PHASE_LOAD:
        if (!(g_wldcore_window_render_records[level->saved_record_index].flags & WORLD_RENDER_RECORD_POP_IN)) {
            level->phase = WLDCORE_DETAIL_PHASE_BUILD;
        }
        break;
    case WLDCORE_DETAIL_PHASE_BUILD:
        if (g_main_file_still_loading != 0) {
            break;
        }
        GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);
        render = level->render_index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        clut_position.x = image.cx;
        clut_position.y = image.cy;
        wldcore_window_init_vram_render_record(render, bounds, clut_position, WORLD_RENDER_RECORD_8BIT_CLUT);
        g_wldcore_window_render_records[render].priority = 0xA;
        base = &g_wldcore_window_render_records[render].base;
        base->x = -0x74;
        base->y = -0x22;
        g_wldcore_window_render_records[render].flags
            = (g_wldcore_window_render_records[render].flags & ~WORLD_RENDER_RECORD_HIDDEN)
            | WORLD_RENDER_RECORD_POP_IN;
        g_wldcore_scrollable_text_window.extra_render_index = render;
        g_wldcore_scrollable_text_window.extra_base = *base;
        g_wldcore_scrollable_text_window.extra_bounds = g_wldcore_window_render_records[render].bounds;
        level->phase = level->phase + 1;
        break;
    case WLDCORE_DETAIL_PHASE_POP_IN:
        if (!(g_wldcore_window_render_records[level->render_index].flags & WORLD_RENDER_RECORD_POP_IN)) {
            level->phase = WLDCORE_DETAIL_PHASE_SHOWN;
        }
        break;
    default:
        index = &g_wldcore_scrollable_text_window.render_record_index;
        if (g_wldcore_window_render_records[*index].flags & WORLD_RENDER_RECORD_POP_IN) {
            break;
        }
        if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) != 0) {
            break;
        }
        depth = g_wldcore_menu_stack_depth;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.main_window].flags
            ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.side_window].flags
            ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
            ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
            |= WORLD_RENDER_RECORD_POP_IN;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.upper_window].flags
            ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_window_render_record_count--;
        g_wldcore_window_render_object_count--;
        g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.lower_window].flags
            ^= WORLD_RENDER_RECORD_HIDDEN;
        g_wldcore_menu_stack_depth = depth - 1;
        wldcore_list_open_unexplored_lands();
        break;
    }
}

/* Opens a panel list of the set script flags 0x321-0x34f (entry values
 * 0xe000 + flag index), shows up to eight rows at (0x88, 0x50) with width
 * 0x5a, and appends its 0x5c-byte record tagged 0x1a. Twin of 0x8007dfcc. */
void wldcore_list_open_treasures(void) {
    wldcore_window_panel_layout_t layout;
    point32_t origin;
    s32 count = 0;
    s32 i; /* reused as the visible-row count, which the target keeps in $s0 */
    s32 record_index;

    for (i = 0; i < EVENT_SCRIPT_VAR_TREASURE_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
            g_wldcore_list_entry_values[count] = i + TEXT_ID_TREASURE_NAME_BASE;
            g_wldcore_list_row_flags[count] = 0;
            count++;
        }
    }
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x5A;
    origin.x = 0x88;
    layout.visible_rows = i;
    origin.y = 0x50;
    wldcore_list_clamp_cursor_state(WLDCORE_CURSOR_SLOT_TREASURES, count, i);
    wldcore_window_start_panel_render_thread(WLDCORE_THREAD_SLOT_WINDOW_PANEL,
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_wldcore_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_wldcore_menu_stack_types[record_index + 1] = WLDCORE_MENU_LEVEL_TREASURES;
    g_wldcore_menu_stack_depth = record_index + 1;
}

/* Resume handler of the treasures list: block Circle, Cross and Select for the
 * levels underneath and show prompt 0xB848 again. */
void wldcore_list_treasures_restore_prompt(void) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
}

/* Withhold every button from the WORLD menu frame while a level opened from the treasures list
 * runs. */
void wldcore_list_treasures_block_all_buttons(void) {
    g_menu_blocked_buttons = -1;
}

/* Treasure-list level input step (cursor slot 8); twin of wldcore_list_handle_unexplored_lands_input.
 * Cancel (0x40) and confirm (0x20) wait for file loading, start thread 12's
 * closing animation and record the choice in result; 0x100 on a non-empty
 * list opens help message 0x101f. Once thread 12 finishes, the level closes
 * and a confirmed choice passes the selected entry's value to 0x8007efa0.
 *
 * The target passes the level to 0x8007e178, which reads no argument. */
void wldcore_list_handle_treasures_input(wldcore_menu_panel_level_t* level) {
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED)
        && thread_is_running(WLDCORE_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_wldcore_menu_ordering_table_offset = 1;
        g_menu_blocked_buttons = 0;
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_menu_stack_depth--;
        if (level->result != 0) {
            wldcore_list_store_cursor_state(WLDCORE_CURSOR_SLOT_TREASURES);
            wldcore_menu_push_treasure_detail_level(
                g_wldcore_list_entry_values[g_wldcore_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        wldcore_list_clear_cursor_state(WLDCORE_CURSOR_SLOT_TREASURES);
        wldcore_menu_dispatch_resume_handler();
        return;
    }
    if ((g_wldcore_input_new_button_presses & PSX_PAD_CROSS) && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        level->result = 0;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_SELECT) && level->entry_count != 0) {
        wldcore_list_unexplored_lands_block_all_buttons(level);
        wldcore_menu_push_message_level(0x101F, 1);
    } else if ((g_wldcore_input_new_button_presses & PSX_PAD_CIRCLE) && level->entry_count != 0
        && g_main_file_still_loading == 0) {
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        level->result = 1;
        thread_set_parameters(WLDCORE_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes the treasure detail text window as menu level 0x1b; the twin of
 * wldcore_menu_push_unexplored_land_detail_level (unexplored lands, level 0x19) and
 * wldcore_menu_push_story_event_text_level (level 0x1e).
 *
 * Records the treasure on the new level, loads its picture data set from
 * g_wldcore_treasure_picture_sets, appends a render record for the picture and hides it with flag
 * 0x10, decodes the acquisition date out of the packed nine-bit array at
 * g_main_treasure_acquisition_date_bits (date.month month, date.day day) and sets
 * up the shared scrollable text window at g_wldcore_scrollable_text_window for text 0x8800 + value
 * with the month name (0xb84f + month) and the day as its two substitutions.
 * It then hides the parent list-window level's four window records and its
 * frame render record and starts WORLD thread 14 on text 0xb849 with
 * substitution value 0xe000 + value.
 *
 * Unlike wldcore_menu_push_unexplored_land_detail_level this one does not reposition the picture render
 * record, and it clears the new level's timer as well as setting its
 * phase to 1.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * `unused` reproduces eight bytes of locals below the date bytes that the
 * target never touches. */
void wldcore_menu_push_treasure_detail_level(s32 value) {
    s32 index;
    s32 coordinate_mode = 1;
    u8 unused[8];
    calendar_date_t date;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.value = value;
    wldcore_proposition_load_picture_data_set(g_wldcore_treasure_picture_sets[value]);
    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.render_index = index;
    g_wldcore_window_render_records[index].flags |= WORLD_RENDER_RECORD_HIDDEN;
    wldcore_calendar_read_date(g_main_treasure_acquisition_date_bits, value, &date);
    g_wldcore_scrollable_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x60;
        break;
    case 1:
        g_wldcore_scrollable_text_window.image_x = 0;
        g_wldcore_scrollable_text_window.image_y = 0x70;
        break;
    }
    g_wldcore_scrollable_text_window.base.x = -122;
    g_wldcore_scrollable_text_window.base.y = -40;
    g_wldcore_scrollable_text_window.text_width = 0xdc;
    g_wldcore_scrollable_text_window.rows_per_page = 8;
    g_wldcore_scrollable_text_window.priority = 0xa;
    g_wldcore_scrollable_text_window.image_coordinate_mode = coordinate_mode;
    g_wldcore_scrollable_text_window.extra_render_index = -1;
    g_wldcore_scrollable_text_window.text_substitutions[0] = date.month + WLDCORE_TEXT_ID_MONTH_NAME_BASE;
    g_wldcore_scrollable_text_window.text_substitutions[1] = date.day;
    wldcore_text_init_scrollable_window(&g_wldcore_scrollable_text_window);
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.main_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.side_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1]
                                        .list_window.frame_render]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.upper_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth - 1].list_window.lower_window]
        .flags |= WORLD_RENDER_RECORD_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_TREASURE_NAME_BASE;
    thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.phase = WLDCORE_DETAIL_PHASE_LOAD;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].variable_detail.timer = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_TREASURE_DETAIL;
    g_wldcore_menu_stack_depth = g_wldcore_menu_stack_depth + 1;
}

/* Handler of the treasure detail level (the treasure's picture and text): the
 * phases mirror wldcore_menu_step_unexplored_land_detail_level.
 *
 * Phase 1 waits for the render record the 32-bit value at 0x08 names to leave its 0x100 state; no push sets
 * that 32-bit value.
 * Phase 2, once file loading finishes, uploads the preview TIM's CLUT, binds
 * the level's render record to the VRAM image with its right edge at x -0x3e
 * and centered on y 0x10, starts the progressive upload
 * (wldcore_gfx_step_dissolve_image_upload) and
 * publishes the record index, position and image rectangle in
 * g_wldcore_scrollable_text_window (extra_render_index, extra_base, extra_bounds). Phase
 * 3 advances that upload one step per frame. Phase 0 tears the level down once the fade started through system flag 4
 * and WORLD thread 14 finish (then opens the treasure's sound novel through
 * wldcore_menu_push_sound_novel_resume_level or wldcore_menu_push_sound_novel_level), restores the parent
 * list-window level and reopens the treasure list when the text window finishes, and on Triangle selects the sound
 * novel named by g_wldcore_treasure_sound_novel_ids[value].
 *
 * The record index is staged in `saved` before the depth store: the target
 * reloads the full 32-bit value for the 16-bit store instead of narrowing the load.
 * The button path's byte uses its own local; sharing `slot` changes the
 * register allocation. */
void wldcore_menu_step_treasure_detail_level(wldcore_menu_variable_detail_level_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t* base;
    s32 render;
    s32 slot;
    s32 depth;
    s32 saved;
    s32 entry;

    switch (level->phase) {
    case WLDCORE_DETAIL_PHASE_LOAD:
        if (!(g_wldcore_window_render_records[level->saved_record_index].flags & WORLD_RENDER_RECORD_POP_IN)) {
            level->phase = WLDCORE_DETAIL_PHASE_BUILD;
        }
        break;
    case WLDCORE_DETAIL_PHASE_BUILD:
        if (g_main_file_still_loading != 0) {
            break;
        }
        GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
        LoadClut(image.clut, image.cx, image.cy);
        render = level->render_index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        clut_position.x = image.cx;
        clut_position.y = image.cy;
        wldcore_window_init_vram_render_record(render, bounds, clut_position, WORLD_RENDER_RECORD_8BIT_CLUT);
        g_wldcore_window_render_records[render].priority = 0xA;
        g_wldcore_window_render_records[render].flags &= ~(WORLD_RENDER_RECORD_HIDDEN | WORLD_RENDER_RECORD_POP_IN);
        base = &g_wldcore_window_render_records[render].base;
        base->x = -0x3E - image.pw;
        base->y = 0x10 - (image.ph >> 1);
        wldcore_gfx_step_dissolve_image_upload(&image, 0);
        g_wldcore_scrollable_text_window.extra_render_index = render;
        g_wldcore_scrollable_text_window.extra_base = *base;
        g_wldcore_scrollable_text_window.extra_bounds = g_wldcore_window_render_records[render].bounds;
        level->phase = level->phase + 1;
        break;
    case WLDCORE_DETAIL_PHASE_POP_IN:
        level->timer = level->timer + 1;
        GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
        if (wldcore_gfx_step_dissolve_image_upload(&image, level->timer) == 0) {
            level->phase = WLDCORE_DETAIL_PHASE_SHOWN;
        }
        break;
    default:
        if ((g_main_system_flags & (MAIN_SYSTEM_FLAG_INPUT_BLOCKED | MAIN_SYSTEM_FLAG_FADING))
                == MAIN_SYSTEM_FLAG_INPUT_BLOCKED
            && thread_is_running(WLDCORE_THREAD_SLOT_MESSAGE) == 0) {
            slot = level->saved_record_index;
            g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
            g_wldcore_window_record_count -= 4;
            g_wldcore_window_render_record_count -= 2;
            g_wldcore_window_render_object_count -= 6;
            saved = level->saved_record_index;
            g_wldcore_menu_stack_depth--;
            g_wldcore_saved_record_index = saved;
            if (slot < 5 && g_main_saved_records[slot].section != 0) {
                wldcore_menu_push_sound_novel_resume_level(level->value);
                break;
            }
            wldcore_menu_push_sound_novel_level(level->value);
            break;
        }
        if (wldcore_text_is_window_finished(&g_wldcore_scrollable_text_window) == 0) {
            depth = g_wldcore_menu_stack_depth;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.main_window].flags
                ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.side_window].flags
                ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
                ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.frame_render].flags
                |= WORLD_RENDER_RECORD_POP_IN;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.upper_window].flags
                ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_window_render_record_count--;
            g_wldcore_window_render_object_count--;
            g_wldcore_window_records[g_wldcore_menu_stack_records_next[depth - 2].list_window.lower_window].flags
                ^= WORLD_RENDER_RECORD_HIDDEN;
            g_wldcore_menu_stack_depth = depth - 1;
            wldcore_list_open_treasures();
            break;
        }
        if (g_wldcore_input_new_button_presses & PSX_PAD_TRIANGLE) {
            entry = g_wldcore_treasure_sound_novel_ids[level->value];
            if (entry != 0) {
                level->saved_record_index = entry - 1;
                if (entry - 1 >= 5 || g_main_saved_records[entry - 1].section == 0) {
                    wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
                }
                g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
                thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0, -1, 0);
                wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
            }
        }
        break;
    }
}

#define BLOCK_BUFFER ((u8*)0x801EE000)

/*
 * Streams an image into VRAM through the 0x801EE000 staging buffer in 1 KiB
 * bands, copying 32 more bytes of each active band per step in the order
 * given by the g_wldcore_dissolve_band_order offset table.
 *
 * Step 0 clears the buffer to 0xFF, uploads it and resets the per-band
 * progress; it returns the address its clearing cursor ends on, one
 * byte below the progress table, and no caller reads that result. Returns 0
 * once step reaches 50, otherwise 1.
 */
static s32 wldcore_gfx_step_dissolve_image_upload(GsIMAGE* image, s32 step) {
    u8* pixels = (u8*)image->pixel;
    u16* order = g_wldcore_dissolve_band_order;
    u8* buffer = BLOCK_BUFFER;
    s32 i;
    s32 j;
    s32 size;
    s32 base;
    s32 index;
    s32 start;
    s32 rows;
    u32 height;
    s32 count;
    s32 first;
    u8* state;

    if (step == 0) {
        for (i = 0; i < image->pw * (image->ph << 1); i++) {
            buffer[i] = 0xff;
        }
        LoadTPage((u32*)buffer, 0, 0, image->px, image->py, image->pw * 4, image->ph);
        state = &g_wldcore_dissolve_band_progress[15];
        for (i = 15; i >= 0; i--) {
            *state-- = 0;
        }
        return;
    }
    if (step >= 50) {
        return 0;
    }
    step--;
    size = image->pw * image->ph * 2;
    height = image->ph;
    rows = height >> 3;
    if (height & 7) {
        start = 15 - rows;
    } else {
        start = 16 - rows;
    }
    for (i = start; i < 16; i++) {
        if (i <= step && (count = g_wldcore_dissolve_band_progress[i]) < 32U) {
            base = (15 - i) << 10;
            /* Computed before the size check: the target copies it into j. */
            first = count * 32;
            if (base >= size) {
                continue;
            }
            for (j = first; j < first + 32; j++) {
                index = base + order[j];
                if (index < size) {
                    buffer[index] = pixels[index];
                }
            }
            g_wldcore_dissolve_band_progress[i]++;
        }
        LoadTPage((u32*)buffer, 0, 0, image->px, image->py, image->pw * 4, image->ph);
    }
    return 1;
}

/* Pushes menu level type 0x14 (driven by wldcore_menu_step_sound_novel_resume_level) with the given mode:
 * appends the cursor window, a VRAM render record for the TIM at g_wldcore_picture_buffer
 * (uploaded progressively through wldcore_gfx_step_dissolve_image_upload, starting at step 0), and
 * two text render records (0xb84c with substitution value 0xe000 + mode, and
 * 0xb84b). The picture record is centered against the first text record from
 * the TIM's width and half height, and the cursor window is placed at the
 * second text record's origin + (0x18, 0x3c).
 *
 * The target reserves 0x10 frame bytes the function never uses. The
 * two offset temporaries keep GCC from folding base_x - (width - 0x3e) into
 * (base_x + 0x3e) - width, and each render-record base pointer is its own
 * local so the pointer shares the register of the constant it dies into. */
static void wldcore_menu_push_sound_novel_resume_level(s32 mode) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut;
    s32 unused[4];
    s32 index;
    s32 depth;
    s32 offset_x;
    s32 offset_y;
    point32_t* base1;
    point32_t* base2;
    point32_t* base3;
    point32_t* base4;

    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.mode = mode;
    index = window_append_animated_record_variant(
        g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.cursor_window = index;
    g_wldcore_window_records[index].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_IDLE;
    g_wldcore_window_records[index].priority = 0xA;
    g_wldcore_window_records[index].anim_counter = 0;
    g_wldcore_window_records[index].frame_index = 0;
    GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.picture_render = index;
    bounds.x = image.px;
    bounds.y = image.py;
    bounds.w = image.pw;
    bounds.h = image.ph;
    clut.x = image.cx;
    clut.y = image.cy;
    wldcore_window_init_vram_render_record(index, bounds, clut, WORLD_RENDER_RECORD_8BIT_CLUT);
    g_wldcore_window_render_records[index].priority = 0xA;
    wldcore_gfx_step_dissolve_image_upload(&image, 0);

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.message_render = index;
    bounds.x = 0;
    bounds.y = 0x70;
    bounds.w = 0x7C;
    bounds.h = 0x80;
    g_text_substitution_values[0] = mode + TEXT_ID_TREASURE_NAME_BASE;
    window_init_text_record_variant(index, bounds, 1, WLDCORE_TEXT_ID_SUBSTITUTED_TEXT, g_wldcore_window_image_buffer);
    g_wldcore_window_render_records[index].priority = 0xA;
    base1 = &g_wldcore_window_render_records[index].base;
    base1->x = -0x70;
    base1->y = -0x38;
    DrawSync(0);
    depth = g_wldcore_menu_stack_depth;
    base2 = &g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].sound_novel_resume.picture_render]
                 .base;
    offset_x = image.pw - 0x3E;
    base2->x = g_wldcore_window_render_records[index].base.x - offset_x;
    base3 = &g_wldcore_window_render_records[g_wldcore_menu_stack_records_next[depth].sound_novel_resume.picture_render]
                 .base;
    offset_y = (image.ph >> 1) - 0x4C;
    base3->y = g_wldcore_window_render_records[index].base.y - offset_y;

    index = window_append_record_variant(g_wldcore_window_render_object_queue, &g_wldcore_window_render_object_count);
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.option_render = index;
    bounds.x = 0x80;
    bounds.y = 0x70;
    bounds.w = 0x70;
    bounds.h = 0x70;
    window_init_text_record_variant(
        index, bounds, 1, WLDCORE_TEXT_ID_SOUND_NOVEL_RESUME_CHOICES, g_wldcore_window_image_buffer + 0x400);
    g_wldcore_window_render_records[index].priority = 0xA;
    base4 = &g_wldcore_window_render_records[index].base;
    base4->x = 8;
    base4->y = -0x38;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                 .sound_novel_resume.cursor_window]
        .position.x = g_wldcore_window_render_records[index].base.x + 0x18;
    g_wldcore_window_records[g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth]
                                 .sound_novel_resume.cursor_window]
        .position.y = g_wldcore_window_render_records[index].base.y + 0x3C;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.dissolve_step = 1;
    g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].sound_novel_resume.cursor_row = 0;
    g_wldcore_menu_stack_types[g_wldcore_menu_stack_depth + 1] = WLDCORE_MENU_LEVEL_SOUND_NOVEL_RESUME;
    g_wldcore_menu_stack_depth++;
}

/* Input step for the type-0x14 sound-novel resume level.
 *
 * While dissolve_step is set the level is feeding the progressive texture upload at
 * 0x8007f998, one step per frame. Cross tears the level down through thread
 * 14; Circle fades the screen out and defers the teardown to the
 * system-flag-4 path; up and down toggle cursor_row, which moves the owned
 * window record down by 16.
 *
 * wldcore_reset_selected_saved_record ignores its argument; retail still
 * passes g_wldcore_saved_record_index. */
void wldcore_menu_step_sound_novel_resume_level(wldcore_menu_sound_novel_resume_level_t* level) {
    GsIMAGE image;
    s32 buttons;

    if (g_wldcore_window_render_records[level->message_render].flags & WORLD_RENDER_RECORD_POP_IN) {
        return;
    }
    if (level->dissolve_step != 0) {
        GsGetTimInfo(g_wldcore_picture_buffer + 1, &image);
        if (wldcore_gfx_step_dissolve_image_upload(&image, level->dissolve_step) == 0) {
            level->dissolve_step = 0;
            return;
        }
        level->dissolve_step++;
        return;
    }
    if ((g_main_system_flags & MAIN_SYSTEM_FLAG_INPUT_BLOCKED) && !(g_main_system_flags & MAIN_SYSTEM_FLAG_FADING)) {
        g_main_system_flags ^= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        g_wldcore_window_record_count--;
        g_wldcore_window_render_record_count -= 3;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth--;
        if (level->cursor_row != 0) {
            wldcore_reset_selected_saved_record(g_wldcore_saved_record_index);
        }
        wldcore_menu_push_sound_novel_level(level->mode);
        return;
    }
    buttons = g_wldcore_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        g_wldcore_window_record_count--;
        g_wldcore_window_render_record_count -= 3;
        g_wldcore_window_render_object_count -= 4;
        g_wldcore_menu_stack_depth--;
        thread_start(WLDCORE_THREAD_SLOT_MESSAGE, world_text_message_box_thread);
        thread_set_parameters(WLDCORE_THREAD_SLOT_MESSAGE, 0x19, WLDCORE_TEXT_ID_BRAVE_STORY_INTRO, 0);
        wldcore_sound_play_effect(MAIN_SFX_CANCEL);
        wldcore_menu_push_treasure_detail_level(level->mode);
        return;
    }
    if (buttons & PSX_PAD_CIRCLE) {
        g_wldcore_window_records[level->cursor_window].sequence = WLDCORE_ANIM_SEQUENCE_CURSOR_PRESS;
        g_wldcore_window_records[level->cursor_window].frame_index
            = g_wldcore_window_records[level->cursor_window].anim_counter = 0;
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x10);
        g_main_system_flags |= MAIN_SYSTEM_FLAG_INPUT_BLOCKED;
        wldcore_sound_play_effect(MAIN_SFX_CONFIRM);
        return;
    }
    if (wldcore_input_update_repeat(PSX_PAD_UP) == 0 && wldcore_input_update_repeat(PSX_PAD_DOWN) == 0) {
        return;
    }
    level->cursor_row ^= 1;
    g_wldcore_window_records[level->cursor_window].position.y
        = g_wldcore_window_render_records[level->option_render].base.y + 0x3c + level->cursor_row * 0x10;
    wldcore_sound_play_effect(MAIN_SFX_CURSOR_MOVE);
}

/* Pushes menu level type 0x23 with the given mode. Mode 2 only starts a
 * screen fade; any other mode copies VRAM (0x100, 0) 0x80x0x100 to
 * (0x200, 0x100), clears (0x100, 0) 0x100x0xf0, sets up the indicator sprite
 * and picks two random values for the level. */
void wldcore_menu_push_event_transition_level(s32 mode) {
    RECT rect;
    s32 depth;

    wldcore_file_seek_to_battle_bin();
    if (mode == WLDCORE_EVENT_TRANSITION_MODE_FADE) {
        g_main_system_flags |= MAIN_SYSTEM_FLAG_FREEZE_ANIMATION;
        wldcore_fade_start_screen(WLDCORE_SCREEN_FADE_OUT, 0x20);
    } else {
        rect.x = 0x100;
        rect.y = 0;
        rect.w = 0x80;
        rect.h = 0x100;
        g_main_system_flags |= MAIN_SYSTEM_FLAG_DRAW_MAP | MAIN_SYSTEM_FLAG_HIDE_MAP_DOTS
            | MAIN_SYSTEM_FLAG_HIDE_MAP_ROUTES | MAIN_SYSTEM_FLAG_NO_BACKGROUND_IMAGE
            | MAIN_SYSTEM_FLAG_HIDE_CONTEXT_VALUE | MAIN_SYSTEM_FLAG_HIDE_CALENDAR_DATE
            | MAIN_SYSTEM_FLAG_HIDE_LOCATION_MARKER;
        g_wldcore_map_projection_state.marker.render.flags |= WORLD_RENDER_RECORD_HIDDEN;
        MoveImage(&rect, 0x200, 0x100);
        DrawSync(0);
        rect.x = 0x100;
        rect.y = 0;
        rect.w = 0x100;
        rect.h = 0xF0;
        ClearImage(&rect, 0, 0, 0);
        g_wldcore_menu_indicator_sprite.attribute = 0x42000000;
        g_wldcore_menu_indicator_sprite.x = 0x20;
        g_wldcore_menu_indicator_sprite.y = 0x20;
        g_wldcore_menu_indicator_sprite.tpage = GetTPage(2, 0, 0x100, 0);
        g_wldcore_menu_indicator_sprite.w = 0xFF;
        g_wldcore_menu_indicator_sprite.mx = 0xA0;
        g_wldcore_menu_indicator_sprite.my = 0x98;
        g_wldcore_menu_indicator_sprite.scaley = ONE;
        g_wldcore_menu_indicator_sprite.scalex = ONE;
        g_wldcore_menu_indicator_sprite.cy = 0;
        g_wldcore_menu_indicator_sprite.cx = 0;
        g_wldcore_menu_indicator_sprite.v = 0;
        g_wldcore_menu_indicator_sprite.u = 0;
        g_wldcore_menu_indicator_sprite.h = 0xF0;
        g_wldcore_menu_indicator_sprite.rotate = 0;
        g_wldcore_menu_indicator_sprite.r = 0x80;
        g_wldcore_menu_indicator_sprite.g = 0x80;
        g_wldcore_menu_indicator_sprite.b = 0x80;
        wldcore_sound_enqueue_audio_command(WLDCORE_AUDIO_COMMAND_FADE_OUT, 0x10);
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.frame = 0;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.anim_step = 0;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.drift_direction
            = (rand() * 4) >> 15;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.spin_direction
            = (rand() * 2) >> 15;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.phase
            = WLDCORE_EVENT_TRANSITION_PHASE_SPIN_IN;
        g_wldcore_menu_stack_records_next[g_wldcore_menu_stack_depth].event_transition.fade_phase
            = WLDCORE_EVENT_TRANSITION_FADE_NONE;
    }
    depth = g_wldcore_menu_stack_depth;
    g_wldcore_menu_stack_records_next[depth].event_transition.mode = mode;
    g_wldcore_menu_stack_types[depth + 1] = WLDCORE_MENU_LEVEL_EVENT_TRANSITION;
    g_wldcore_menu_stack_depth = depth + 1;
}
