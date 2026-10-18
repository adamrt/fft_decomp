/* Proposition reports, participant rewards, discoveries and active-proposition recall menus. */

#include "fft/gfx.h"
#include "fft/job.h"
#include "fft/menu.h"
#include "fft/menu_input.h"
#include "fft/menu_stack.h"
#include "fft/menu_stack_records.h"
#include "fft/message_box.h"
#include "fft/music.h"
#include "fft/party.h"
#include "fft/portrait.h"
#include "fft/proposition.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_party.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u32 g_proposition_detail_image_buffer[];

/* Collection counts that earn a milestone message. */
static u8 g_proposition_treasure_milestones[8] = { 1, 4, 8, 12, 16, 20, 24, 31 };
static u8 g_proposition_unexplored_land_milestones[8] = { 1, 3, 6, 8, 10, 12, 14, 16 };

static void proposition_show_next_milestone_message(proposition_discovery_level_t* discovery);
static void proposition_push_recall_level(s32 proposition);

/*
 * Run the proposition speaker-selection and message sequence.
 *
 * With speaker == -1 the speaker is drawn at random from the selected
 * proposition's participants other than the excluded one; a proposition with a
 * single participant has nobody else to speak and returns 1 at once. Returns 1
 * once the message has been acknowledged and the phase reset, 0 while running.
 * The phase switch's six-entry jump table is this unit's .rodata.
 */
s32 proposition_step_participant_message(proposition_report_level_t* state, s32 message, s32 speaker) {
    switch (state->phase) {
    case 0:
        if (speaker == -1) {
            s32 candidates[5];
            s32 count;
            s32 i;

            if (g_proposition_active[g_proposition_report.proposition_index].participant_count == 1)
                return 1;
            i = 0;
            count = 0;
            if (g_proposition_active[g_proposition_report.proposition_index].participant_count != 0)
                do {
                    s32 participant
                        = g_proposition_active[g_proposition_report.proposition_index].participant_indices[i];
                    if (participant != g_proposition_report.reporter_party_index)
                        candidates[count++] = participant;
                    i++;
                } while (i < g_proposition_active[g_proposition_report.proposition_index].participant_count);
            g_proposition_report.speaker_party_index = candidates[(rand() * count) >> 15];
        } else {
            g_proposition_report.speaker_party_index = speaker;
        }
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        state->phase++;
        break;
    case 1:
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE))
            return 0;
        state->phase++;
        break;
    case 2:
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
        g_text_substitution_values[0] = g_proposition_report.speaker_party_index + TEXT_ID_UNIT_NAME_BASE;
        script_set_variable(
            EVENT_SCRIPT_VAR_DIALOG_PORTRAIT, portrait_get_party_unit_code(g_proposition_report.speaker_party_index));
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19,
            proposition_adjust_message(message, g_proposition_report.speaker_party_index) + TEXT_ID_WORLD_MESSAGE_BASE,
            0);
        state->phase++;
        break;
    case 3:
        if (!(g_world_map_input_new_button_presses & PSX_PAD_CIRCLE))
            return 0;
        if (!screen_is_dialog_waiting())
            return 0;
        state->phase++;
        break;
    case 4:
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
        state->phase++;
        break;
    case 5:
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE))
            return 0;
        state->phase = 0;
        return 1;
    }

    return 0;
}

/* Proposition job category of a job: 0..19 for Squire..Mime, 22 for the
 * special jobs below Squire and 21 for anything above Mime (monsters). */
static s32 proposition_get_job_category(s32 job_id) {
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
 * A recalled proposition (PROPOSITION_PROGRESS_RECALLED) clears the JP
 * and gil awards and reports a bad failure. Otherwise each participant's score
 * accumulates four proposition data tables (records 3..6) keyed on the
 * proposition's fields, the participant's job category and its bravery, faith
 * and level tiers; a total of 99 or more is a success, 59 or more a failure,
 * anything less a bad failure. A named preferred job then gives a 61 in 100
 * roll to promote the result to success. The target builds the row walk pointers
 * from the proposition_index address, which needs the arrays and that 32-bit value to be
 * members of one object (g_proposition_report). */
void proposition_determine_success(void) {
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

    participant_count = g_proposition_active[g_proposition_report.proposition_index].participant_count;
    job_table_a = proposition_get_data(PROPOSITION_DATA_CATEGORY_JOB_SCORES);
    job_table_b = proposition_get_data(PROPOSITION_DATA_JOB_LIST_JOB_SCORES);
    tier_table_a = proposition_get_data(PROPOSITION_DATA_BRAVERY_LEVEL_SCORES);
    total = 0;
    tier_table_b = proposition_get_data(PROPOSITION_DATA_FAITH_SCORES);

    if (g_proposition_active[g_proposition_report.proposition_index].flags & PROPOSITION_PROGRESS_RECALLED) {
        for (i = 0; i < participant_count; i++) {
            g_proposition_report.participant_gil[i] = 0;
            g_proposition_report.participant_jp[i] = 0;
        }
        g_proposition_report.recalled = 1;
        g_proposition_report.result = PROPOSITION_RESULT_BAD_FAILURE;
        g_proposition_report.reward_type = PROPOSITION_REWARD_NONE;
        return;
    }

    g_proposition_report.recalled = 0;
    for (i = 0; i < participant_count; i++) {
        unit = party_get_roster_unit(
            g_proposition_active[g_proposition_report.proposition_index].participant_indices[i]);

        g_proposition_report.participant_scores[i] = job_table_a[(g_proposition_selected_row.category_index - 1) * 22
            + proposition_get_job_category(unit->job_id)];
        g_proposition_report.participant_scores[i] += job_table_b[(g_proposition_selected_row.job_list_index - 1) * 22
            + proposition_get_job_category(unit->job_id)];

        value = unit->bravery;
        if (value != 0) {
            tier = (value - 1) / 20;
        } else {
            tier = 0;
        }
        g_proposition_report.participant_scores[i]
            += tier_table_a[(g_proposition_selected_row.job_list_index - 1) * 5 + tier];

        value = unit->faith;
        if (value != 0) {
            tier = (value - 1) / 20;
        } else {
            tier = 0;
        }
        g_proposition_report.participant_scores[i]
            += tier_table_b[(g_proposition_selected_row.job_list_index - 1) * 5 + tier];

        value = unit->level;
        if (value != 0) {
            tier = (value - 1) / 10;
        } else {
            tier = 0;
        }
        score = g_proposition_report.participant_scores[i]
            + tier_table_a[(g_proposition_selected_row.job_list_index - 1) * 10 + tier];
        total += score;
        g_proposition_report.participant_scores[i] = score;
    }

    g_proposition_report.result = PROPOSITION_RESULT_SUCCESS;
    if (total < 0x63) {
        g_proposition_report.result = PROPOSITION_RESULT_FAILURE;
    }
    if (total < 0x3B) {
        g_proposition_report.result = PROPOSITION_RESULT_BAD_FAILURE;
    }
    if (g_proposition_report.result == PROPOSITION_RESULT_SUCCESS) {
        return;
    }
    if (g_proposition_selected_row.preferred_job == 0) {
        return;
    }

    /* Retail bug: the unit is fetched once, with the scoring loop's leftover
     * index (one past the last participant), so every pass tests that same
     * stale slot instead of each participant's job. */
    unit = party_get_roster_unit(g_proposition_active[g_proposition_report.proposition_index].participant_indices[i]);
    for (i = 0; i < participant_count; i++) {
        if (g_proposition_selected_row.preferred_job - 1 == proposition_get_job_category(unit->job_id)) {
            if (((rand() * 100) >> 15) < 0x3D) {
                g_proposition_report.result = PROPOSITION_RESULT_SUCCESS;
                return;
            }
        }
    }
}

/* JP and gil of each participant: the proposition's JP and gil
 * amounts, on a failure JP / 10 and gil / 5, on a bad failure both / 20; each
 * then gains the participant's score. No awards are calculated
 * when the proposition was recalled. */
void proposition_calculate_jp_and_gil(void) {
    const u16* jp_rewards;
    const u16* gil_rewards;
    s32 count;
    s32 i;
    s32 value;

    if (g_proposition_report.recalled == 0) {
        jp_rewards = proposition_get_data(PROPOSITION_DATA_JP_AMOUNTS);
        gil_rewards = proposition_get_data(PROPOSITION_DATA_GIL_AMOUNTS);
        count = g_proposition_active[g_proposition_report.proposition_index].participant_count;
        value = 0;

        for (i = 0; i < count; i++) {
            value = jp_rewards[g_proposition_selected_row.jp_reward - 1];
            if (g_proposition_report.result == PROPOSITION_RESULT_FAILURE) {
                value = value / 10;
            }
            if (g_proposition_report.result == PROPOSITION_RESULT_BAD_FAILURE) {
                value = value / 20;
            }
            g_proposition_report.participant_jp[i] = value;
            g_proposition_report.participant_jp[i] += g_proposition_report.participant_scores[i];
        }

        for (i = 0; i < count; i++) {
            value = gil_rewards[g_proposition_selected_row.gil_reward - 1];
            if (g_proposition_report.result == PROPOSITION_RESULT_FAILURE) {
                value = value / 5;
            }
            if (g_proposition_report.result == PROPOSITION_RESULT_BAD_FAILURE) {
                value = value / 20;
            }
            g_proposition_report.participant_gil[i] = value;
            g_proposition_report.participant_gil[i] += g_proposition_report.participant_scores[i];
        }
    }
}

/* Rolls the reward of a successful proposition.
 *
 * A treasure or unexplored-land discovery (the row's discovery_kind) picks one
 * not yet found at random (among the first 31 treasures or the 16 lands),
 * falling back to any when all are found; otherwise random_bonus == 1 rolls a
 * three-tier random reward (60/30/10%). The chosen pair from the discovery or
 * random reward table then adds its JP bonus to every participant's award and
 * sets the reward's gil value. The target places
 * the random-reward branch last, hence the goto. */
void proposition_roll_bonus_reward(void) {
    s32 candidates[32];
    u16* bonus_table;
    u16* value_table;
    /* Discovery-row indexing adds two instructions; retain its byte index. */
    u8* location_pairs;
    proposition_reward_pair_t* random_pairs;
    s32 count;
    s32 i;
    s32 index;
    s32 participant_count;
    u16 discovery_kind;
    s32 bonus;

    bonus_table = proposition_get_data(PROPOSITION_DATA_JP_AMOUNTS);
    value_table = proposition_get_data(PROPOSITION_DATA_GIL_AMOUNTS);
    location_pairs = proposition_get_data(PROPOSITION_DATA_DISCOVERY_REWARDS);
    random_pairs = proposition_get_data(PROPOSITION_DATA_RANDOM_REWARDS);
    g_proposition_report.reward_type = PROPOSITION_REWARD_NONE;
    g_proposition_report.reward_value = 0;
    if (g_proposition_report.result != PROPOSITION_RESULT_SUCCESS) {
        return;
    }

    discovery_kind = g_proposition_selected_row.discovery_kind;
    if (discovery_kind == PROPOSITION_REWARD_TREASURE) {
        g_proposition_report.reward_type = PROPOSITION_REWARD_TREASURE;
        count = 0;
        for (i = 0; i < 31; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) == 0) {
                candidates[count] = i;
                count++;
            }
        }
    } else if (discovery_kind == PROPOSITION_REWARD_UNEXPLORED_LAND) {
        count = 0;
        g_proposition_report.reward_type = discovery_kind;
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
        g_proposition_report.reward_index = candidates[index];
    } else if (g_proposition_report.reward_type == PROPOSITION_REWARD_TREASURE) {
        g_proposition_report.reward_index = (rand() * 31) >> 15;
    } else {
        g_proposition_report.reward_index = (rand() * 16) >> 15;
    }

    participant_count = g_proposition_active[g_proposition_report.proposition_index].participant_count;
    index = (g_proposition_selected_row.category_index - 1) * 2;
    for (i = 0; i < participant_count; i++) {
        bonus = bonus_table[location_pairs[index + 1] - 1];
        g_proposition_report.participant_jp[i] += bonus;
    }

    g_proposition_report.reward_value = value_table[location_pairs[index] - 1];
    return;

random_reward:
    if (g_proposition_selected_row.random_bonus != 1) {
        return;
    }

    g_proposition_report.reward_type = PROPOSITION_REWARD_RANDOM_BONUS;
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

    index += (g_proposition_selected_row.category_index - 1) * 3;
    g_proposition_report.reward_index = index;
    participant_count = g_proposition_active[g_proposition_report.proposition_index].participant_count;
    for (i = 0; i < participant_count; i++) {
        bonus = bonus_table[random_pairs[index].jp_index - 1];
        g_proposition_report.participant_jp[i] += bonus;
    }

    g_proposition_report.reward_value = value_table[random_pairs[index].gil_index - 1];
}

/* Scale each participant's JP and gil by the percentages the two EXTRA_DAY
 * tables give for the days spent beyond the job's minimum (at most 9 counted).
 * A recalled job, or one back early, is left alone. */
void proposition_apply_extra_days(void) {
    s32 record;
    const u16* reward_scale;
    const u16* bonus_scale;
    s32 day;
    s32 count;
    s32 percent;
    s32 i;

    if (g_proposition_report.recalled != 0) {
        return;
    }
    reward_scale = proposition_get_data(PROPOSITION_DATA_EXTRA_DAY_JP_SCALES);
    bonus_scale = proposition_get_data(PROPOSITION_DATA_EXTRA_DAY_GIL_SCALES);
    record = g_proposition_report.proposition_index;
    day = g_proposition_active[record].elapsed_days - g_proposition_selected_row.min_days;
    count = g_proposition_active[record].participant_count;
    if (day < 0) {
        return;
    }
    if (day >= 10) {
        day = 9;
    }
    percent = reward_scale[day];
    for (i = 0; i < count; i++) {
        g_proposition_report.participant_jp[i] = g_proposition_report.participant_jp[i] * percent / 100;
    }
    percent = bonus_scale[day];
    for (i = 0; i < count; i++) {
        g_proposition_report.participant_gil[i] = g_proposition_report.participant_gil[i] * percent / 100;
    }
}

/* Return 1 + the party index of the participant the job report praises, or 0
 * for none. A participant whose job falls in the proposition's preferred job
 * category wins, preferring one other than reporter_party_index. Otherwise,
 * among the other participants whose job is in the proposition's nine-job
 * list, the one whose job comes earliest wins, but only when one or two
 * match; three matches name nobody.
 *
 * The target keeps &g_proposition_report.proposition_index in s5 and
 * reaches reporter_party_index, 12 bytes below, off the same base, which is
 * why it saves one more callee-saved register than a per-field access would.
 *
 * `base` stays an explicit local so the job-list offset is computed once in
 * the preheader and the inner add uses two registers. The slot test is
 * written `slots[0] > slots[1]`; the reversed spelling swaps the slots[] load
 * pair and costs instructions.
 */
s32 proposition_find_preferred_job_unit(void) {
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

    participant_count = g_proposition_active[g_proposition_report.proposition_index].participant_count;
    jobs = proposition_get_data(PROPOSITION_DATA_JOB_LISTS);
    count = 0;

    if (g_proposition_selected_row.preferred_job != 0) {
        for (i = 0; i < participant_count; i++) {
            party_index = g_proposition_active[g_proposition_report.proposition_index].participant_indices[i];
            unit = party_get_roster_unit(party_index);
            if (g_proposition_selected_row.preferred_job - 1 == proposition_get_job_category(unit->job_id)) {
                candidates[count] = party_index;
                count++;
            }
        }
    }

    if (count != 0) {
        for (i = 0; i < count; i++) {
            if (candidates[i] != g_proposition_report.reporter_party_index) {
                return candidates[i] + 1;
            }
        }
        return candidates[0] + 1;
    }

    base = (g_proposition_selected_row.job_list_index - 1) * 9;
    for (i = 0; i < participant_count; i++) {
        party_index = g_proposition_active[g_proposition_report.proposition_index].participant_indices[i];
        if (party_index != g_proposition_report.reporter_party_index) {
            unit = party_get_roster_unit(party_index);
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
void proposition_push_discovery_level(void) {
    s32 count = proposition_get_milestone_rank();

    if (g_proposition_report.reward_type == PROPOSITION_REWARD_TREASURE) {
        g_menu_stack_records[g_menu_stack_depth].discovery.current = TEXT_ID_TREASURE_FOUND_FIRST;
    } else {
        g_menu_stack_records[g_menu_stack_depth].discovery.current = TEXT_ID_LAND_FOUND_FIRST;
    }
    if (count == 1) {
        g_menu_stack_records[g_menu_stack_depth].discovery.remaining = 4;
    } else {
        g_menu_stack_records[g_menu_stack_depth].discovery.remaining = 1;
        g_menu_stack_records[g_menu_stack_depth].discovery.current += 2 + count;
    }
    if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
    }
    proposition_show_next_milestone_message(&g_menu_stack_records[g_menu_stack_depth].discovery);
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_PROPOSITION_DISCOVERY;
    g_menu_stack_depth++;
}

/* Show the discovery announcement's next message and count it off; the last one
 * also fades the music out and moves the announcement to
 * PROPOSITION_DISCOVERY_PHASE_LAST_MESSAGE with the timer cleared. */
static void proposition_show_next_milestone_message(proposition_discovery_level_t* discovery) {
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, discovery->current, 0);
    discovery->current++;
    discovery->remaining--;
    if (discovery->remaining == 0) {
        music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 4);
        discovery->phase = PROPOSITION_DISCOVERY_PHASE_LAST_MESSAGE;
        discovery->timer = 0;
    } else {
        discovery->phase = PROPOSITION_DISCOVERY_PHASE_MESSAGES;
    }
}

/* Return the one-based treasure or unexplored-land collection milestone
 * reached by a successful proposition, or zero when the count is not a milestone. */
s32 proposition_get_milestone_rank(void) {
    s32 count;
    s32 i;
    s32 index;

    if (g_proposition_report.result != PROPOSITION_RESULT_SUCCESS) {
        return 0;
    }

    if (g_proposition_report.reward_type != PROPOSITION_REWARD_TREASURE
        && g_proposition_report.reward_type != PROPOSITION_REWARD_UNEXPLORED_LAND) {
        return 0;
    }

    if (g_proposition_report.reward_type == PROPOSITION_REWARD_TREASURE) {
        count = 0;
        for (i = 0; i < 31; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
                count++;
            }
        }
        for (i = 0; i < 8; i++) {
            if (g_proposition_treasure_milestones[i] == count) {
                break;
            }
        }
        if (i == 8) {
            return 0;
        }
        index = i;
    }

    if (g_proposition_report.reward_type == PROPOSITION_REWARD_UNEXPLORED_LAND) {
        count = 0;
        for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
            if (script_get_variable(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) != 0) {
                count++;
            }
        }
        for (i = 0; i < 8; i++) {
            if (g_proposition_unexplored_land_milestones[i] == count) {
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

/* Step of a discovery announcement: Circle shows the next message; after the
 * last one (LAST_MESSAGE) it waits for the message box and plays the milestone
 * music, JINGLE waits at least 180 frames and switches back to the bar music,
 * and CLOSE pops the level into proposition_open_returned_list once
 * the music queue is idle. The 32-bit busy fields
 * are read as g_music_queue members, not scalars, which keeps the timer store
 * ahead of their loads as in the target. */
void proposition_step_discovery_level(menu_stack_record_t* discovery) {
    if (discovery->discovery.phase == PROPOSITION_DISCOVERY_PHASE_CLOSE) {
        if (g_music_queue.count == 0 && g_music_queue.current_command == MUSIC_QUEUE_COMMAND_NONE) {
            g_menu_stack_depth--;
            proposition_open_returned_list();
        }
    } else if (discovery->discovery.phase == PROPOSITION_DISCOVERY_PHASE_JINGLE) {
        discovery->discovery.timer++;
        if (g_music_queue.count == 0 && g_music_queue.current_command == MUSIC_QUEUE_COMMAND_NONE
            && discovery->discovery.timer >= 180) {
            discovery->discovery.phase = PROPOSITION_DISCOVERY_PHASE_CLOSE;
            music_queue_add(MUSIC_QUEUE_COMMAND_FADE_OUT, 0x10);
            music_queue_add(MUSIC_QUEUE_COMMAND_PLAY_TRACK, MUSIC_QUEUE_TRACK(2, MUSIC_BAR));
            music_queue_add(MUSIC_QUEUE_COMMAND_FADE_IN, 0x10);
        }
    } else if (discovery->discovery.phase == PROPOSITION_DISCOVERY_PHASE_LAST_MESSAGE) {
        if (screen_is_dialog_waiting() != 0 && g_music_queue.current_command == MUSIC_QUEUE_COMMAND_NONE
            && g_music_queue.count == 0) {
            music_queue_add(MUSIC_QUEUE_COMMAND_PLAY_TRACK, MUSIC_QUEUE_TRACK(2, MUSIC_MILESTONE));
            music_queue_add(MUSIC_QUEUE_COMMAND_FADE_IN, 0x10);
            discovery->discovery.phase = PROPOSITION_DISCOVERY_PHASE_JINGLE;
        }
    } else if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        proposition_show_next_milestone_message(&discovery->discovery);
    }
}

/* Open the bar's in-progress list: every accepted job that has not returned, recalled ones marked,
 * under 'These jobs're in progress.'; up to six rows. */
void proposition_open_active_list(void) {
    point32_t origin;
    window_panel_layout_t layout;
    point32_t unused_point;
    s32 count;
    s32 i;
    s32 loaded_count;
    s32 previous;
    s32 total;
    s32 entry_value_base;
    s16* row_flags;
    s32 active_row_flag;

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_JOBS_IN_PROGRESS, 0);
    count = 0;
    loaded_count = g_proposition_count;
    if ((i = 0, loaded_count) > 0) {
        entry_value_base = TEXT_ID_PROPOSITION_NAME_BASE;
        row_flags = g_menu_stack_row_flags;
        active_row_flag = 8;
        total = loaded_count;
        do {
            if ((g_proposition_active[i].flags & PROPOSITION_PROGRESS_RETURNED) == 0) {
                g_menu_stack_entry_values[count] = g_proposition_active[i].proposition_id + entry_value_base;
                if ((g_proposition_active[i].flags & PROPOSITION_PROGRESS_RECALLED) != 0) {
                    row_flags[count] = active_row_flag;
                } else {
                    row_flags[count] = 0;
                }
                g_menu_stack_records[g_menu_stack_depth].panel.entries[count] = i;
                count++;
            }
            i++;
        } while (i < total);
    }

    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
    if (count < 6) {
        i = count;
    } else {
        i = 6;
    }

    layout.text_width = 0x96;
    origin.x = 0x4C;
    layout.visible_rows = i;
    origin.y = 0x50;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_ACTIVE_PROPOSITIONS, g_proposition_count, i);
    window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    previous = g_menu_stack_depth - 1;
    g_menu_stack_ot_offset = 9;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    if ((g_window_gs_image_records[g_menu_stack_records[previous].bar.rows_image].flags & DISPLAY_OBJECT_HIDDEN) != 0) {
        g_window_records[g_menu_stack_records[previous].bar.cursor_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_records[g_menu_stack_records[previous].bar.label_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[g_menu_stack_records[previous].bar.rows_image].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[g_menu_stack_records[previous].bar.rows_image].flags |= DISPLAY_OBJECT_POP_IN;
    }

    g_menu_stack_records[g_menu_stack_depth].panel.result = 0;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_PROPOSITION_ACTIVE_LIST;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Resume handler of the active propositions list: block Circle, Cross and
 * Select for the levels underneath and show TEXT_ID_JOBS_IN_PROGRESS
 * again. */
void proposition_restore_active_list_prompt(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_JOBS_IN_PROGRESS, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void proposition_block_active_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the bar's in-progress list: Cross and Circle close the panel
 * and Select opens help message 0x1062; Circle on a job already recalled says
 * TEXT_ID_RETURN_TOMORROW instead. Once the panel has closed, a
 * chosen job goes to proposition_push_recall_level.
 *
 * The target keeps each `level->panel.result` store ahead of the `li $a0,5`
 * argument of the following cursor-state call, which lands in the jal delay
 * slot; without the empty barriers GCC schedules the li first.
 */
void proposition_handle_active_list_input(menu_stack_record_t* level) {
    u32 buttons;
    s32 unused[2]; /* 8-byte frame slot the target reserves */

    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_stack_ot_offset = 1;
        g_menu_blocked_buttons = 0;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->panel.result != 0) {
            proposition_push_recall_level(level->panel.entries[g_window_panel_render_state.selected_index]);
        } else {
            menu_stack_dispatch_resume();
        }
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        /* Scheduling barrier: keeps the store ahead of the cursor argument. */
        __asm__("");
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_ACTIVE_PROPOSITIONS);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
    } else if (buttons & PSX_PAD_SELECT) {
        proposition_block_active_list_buttons(level);
        menu_stack_push_message_level(0x1062, 1);
    } else if (buttons & PSX_PAD_CIRCLE) {
        if (g_menu_stack_row_flags[g_window_panel_render_state.selected_index] != 0) {
            sound_play_effect(SOUND_SFX_INVALID);
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_RETURN_TOMORROW, 0);
        } else {
            sound_play_effect(SOUND_SFX_CONFIRM);
            level->panel.result = 1;
            /* Scheduling barrier: keeps the store ahead of the cursor argument. */
            __asm__("");
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_ACTIVE_PROPOSITIONS);
            g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
            thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        }
    }
}

/* Ask whether to call back the party sent on active proposition `proposition`: hide the
 * list level's windows, show the call-back prompt, an OK/No choice with its cursor and
 * frame and the proposition's detail panel, then push the confirm level. */
static void proposition_push_recall_level(s32 proposition) {
    RECT bounds;
    u8 unused[0x30];
    s32 index;
    s32 depth;
    point32_t* base;

    proposition_unpack_row(&g_proposition_selected_row, g_proposition_active[proposition].proposition_id);
    g_menu_stack_records[g_menu_stack_depth].proposition_cancel_confirm.proposition = proposition;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.label_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].bar.rows_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CALL_BACK_PROMPT, 0);

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_cancel_confirm.cursor_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_cancel_confirm.frame_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CHOICE_PANEL_HEADER;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].proposition_cancel_confirm.choice_render = index;
    bounds.x = 0x80;
    bounds.w = 0x28;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record(index, bounds, 0, TEXT_ID_OK_NO, g_window_image_buffer);
    base = &g_window_gs_image_records[index].base;
    depth = g_menu_stack_depth;
    g_window_gs_image_records[index].priority = 8;
    base->x = 0x48;
    base->y = 0x20;
    g_window_records[g_menu_stack_records[depth].proposition_cancel_confirm.cursor_window].position.x
        = g_window_gs_image_records[index].base.x + 6;
    g_window_records[g_menu_stack_records[depth].proposition_cancel_confirm.cursor_window].position.y
        = g_window_gs_image_records[index].base.y + 0xE;
    g_window_records[g_menu_stack_records[depth].proposition_cancel_confirm.frame_window].position.x
        = g_window_gs_image_records[index].base.x + 3;
    g_window_records[g_menu_stack_records[depth].proposition_cancel_confirm.frame_window].position.y
        = g_window_gs_image_records[index].base.y - 2;
    proposition_build_detail_panel(&g_menu_stack_records[depth].proposition_detail_panel, proposition, 0);
    g_menu_stack_records[g_menu_stack_depth].proposition_cancel_confirm.choice = 0;
    g_menu_stack_records[g_menu_stack_depth].proposition_cancel_confirm.confirmed = 0;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_PROPOSITION_RECALL;
    g_menu_stack_depth++;
}

/* Build the image of the proposition detail panel for g_proposition_active[proposition]:
 * the elapsed/assigned day counts and the location go into text substitution
 * 32-bit values 0-2, and one 16-pixel row is drawn per participant.
 *
 * Both menu variants share the detail render index and row count.
 *
 * The panel's row count, the job contents text's measured rows plus two, is
 * kept in the level (proposition_detail_panel.panel_rows).
 *
 * The target stores base_x/base_y through one pointer to the pair, and reads
 * every progress field through the array form g_proposition_active[proposition], which
 * is what produces its four-instruction ASPSX $at indexed loads.
 *
 * `panel_text` keeps the text-id source's address in a saved register across
 * both reads, as the target does. Assigning bounds.x before
 * position.y is what puts the two by-value halves in the target's registers. */
void proposition_build_detail_panel(proposition_detail_panel_t* panel, s32 proposition, s32 coordinate_mode) {
    RECT bounds;
    point32_t point;
    s16 text_width;
    s16 text_rows;
    s32 index;
    s32 rows;
    s32 i;
    u16* panel_text;
    point32_t* base;

    panel_text = &g_proposition_selected_row.name_text;
    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    panel->render_index = index;
    text_measure_pixels(
        &text_width, &text_rows, text_find_entry(*panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1)));
    rows = text_rows + 2;
    panel->panel_rows = rows;
    g_text_substitution_values[0] = g_proposition_active[proposition].elapsed_days;
    g_text_substitution_values[1] = g_proposition_active[proposition].assigned_days;
    g_text_substitution_values[2] = g_proposition_active[proposition].location | TEXT_ID_LOCATION_NAME_BASE;
    bounds.x = 0;
    bounds.y = 0x60;
    bounds.w = 0xF4;
    bounds.h = (g_proposition_active[proposition].participant_count + rows) * 0x10 + 0x10;
    window_build_render_record_image(index, bounds, coordinate_mode, g_proposition_detail_image_buffer);
    point.x = 8;
    point.y = text_rows * 0x10 + 8;
    window_display_text_entry(index, TEXT_ID_JOB_CONDITION_LABELS, point, g_proposition_detail_image_buffer);
    point.x = 8;
    point.y = 8;
    window_display_text_entry(index, TEXT_ID_JOB_CONTENTS_LABEL, point, g_proposition_detail_image_buffer);
    point.x = 0x44;
    point.y = 8;
    window_display_text_entry(
        index, *panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1), point, g_proposition_detail_image_buffer);
    for (i = 0; i < g_proposition_active[proposition].participant_count; i++) {
        s32 row = i + 2;

        point.x = 0x3C;
        point.y = (text_rows + row) * 0x10 + 8;
        window_display_text_entry(index,
            g_proposition_active[proposition].participant_indices[i] | TEXT_ID_UNIT_NAME_BASE, point,
            g_proposition_detail_image_buffer);
    }
    window_load_render_record_image(index, g_proposition_detail_image_buffer);
    g_window_gs_image_records[index].priority = 8;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_POP_IN;
    base = &g_window_gs_image_records[index].base;
    base->x = -0x7A;
    base->y = -0x28;
}

/* Input step for the call-back question on an active proposition: up/down
 * toggle the highlighted row and move the cursor window. Circle on the first
 * row recalls the party (PROPOSITION_PROGRESS_RECALLED) and says "Ok,
 * I'll have them return by tomorrow."; Circle on the second row, or Cross,
 * closes the level and reopens the propositions list. */
void proposition_handle_recall_input(menu_stack_record_t* level) {
    s32 choice;

    if (g_window_gs_image_records[level->proposition_cancel_confirm.choice_render].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (level->proposition_cancel_confirm.confirmed != 0) {
        if (g_world_map_input_new_button_presses & (PSX_PAD_CIRCLE | PSX_PAD_CROSS)) {
            sound_play_effect(SOUND_SFX_CONFIRM);
            goto close;
        }
        return;
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        /* Shared tail: the target keeps this copy and the other close paths
         * jump into it; duplicated copies cross-jump into the last one. */
    close:
        g_window_record_count -= 2;
        g_window_render_record_count -= 2;
        g_window_render_object_count -= 4;
        g_menu_stack_depth--;
        proposition_open_active_list();
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0 || world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        choice = level->proposition_cancel_confirm.choice ^ 1;
        level->proposition_cancel_confirm.choice = choice;
        g_window_records[level->proposition_cancel_confirm.cursor_window].position.y
            = g_window_gs_image_records[level->proposition_cancel_confirm.choice_render].base.y
            - -(choice * 0x10 + 0xE);
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }

    if (!(g_world_map_input_new_button_presses & PSX_PAD_CIRCLE)) {
        return;
    }

    if (level->proposition_cancel_confirm.confirmed != 0) {
        return;
    }

    sound_play_effect(SOUND_SFX_CONFIRM);
    if (level->proposition_cancel_confirm.choice == 1) {
        goto close;
    }

    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_CALL_BACK_ACCEPTED, 0);
    level->proposition_cancel_confirm.confirmed = 1;
    g_proposition_active[level->proposition_cancel_confirm.proposition].flags |= PROPOSITION_PROGRESS_RECALLED;
    g_window_records[level->proposition_cancel_confirm.cursor_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[level->proposition_cancel_confirm.frame_window].flags |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[level->proposition_cancel_confirm.choice_render].flags |= DISPLAY_OBJECT_HIDDEN;
}
