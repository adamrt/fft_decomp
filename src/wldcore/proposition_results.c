/* Proposition participant messages, success and rewards, discovery announcements,
 * active propositions and cancel confirmation, sound novel levels, unexplored lands and treasures. */

#include "fft/brave_story.h"
#include "fft/calendar.h"
#include "fft/file.h"
#include "fft/formation.h"
#include "fft/gfx.h"
#include "fft/input.h"
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
#include "fft/save.h"
#include "fft/screen.h"
#include "fft/script_variable.h"
#include "fft/sound.h"
#include "fft/sound_novel.h"
#include "fft/suzuki.h"
#include "fft/system.h"
#include "fft/text.h"
#include "fft/thread.h"
#include "fft/unit_party.h"
#include "fft/window.h"
#include "fft/world_map.h"
#include "psx/libc.h"
#include "psx/libgpu.h"
#include "psx/libgs.h"
#include "psx/libgte.h"
#include "psx/pad.h"
#include "psx/types.h"

extern u8 g_treasure_sound_novel_ids[];
extern u32 g_proposition_detail_image_buffer[];

/* The collection counts that earn a milestone message (proposition_get_milestone_rank): eight each,
 * 1/4/8/12/16/20/24/31 treasures and 1/3/6/8/10/12/14/16 unexplored lands. */
extern u8 g_proposition_treasure_milestones[];
extern u8 g_proposition_unexplored_land_milestones[];
extern u8 g_sound_novel_quit_help_text_ids[];
extern point32_t g_hud_saved_value_position; /* saved copy of g_hud_value_position */

/* Offsets of the 1 KiB bands of a picture, in the order its dissolving upload fills them. */
extern u16* g_picture_dissolve_band_order;

/* Scalar history views retained for the Quit shuffle: direct member accesses
 * change its original loads and store scheduling. */
extern s16 g_sound_novel_history_0;
extern u16 g_sound_novel_history_2;

static u8 g_picture_dissolve_band_progress[16] = { 0 };

static void hud_show_play_time(void);
static s32 picture_step_dissolve_upload(GsIMAGE* image, s32 step);
static void sound_novel_push_history_level(void);
static void sound_novel_push_quit_level(void);
static void sound_novel_push_restart_level(s32 mode);
static void brave_story_push_unexplored_land_level(s32 value);
static void proposition_show_next_milestone_message(proposition_discovery_level_t* discovery);
static void proposition_push_recall_level(s32 proposition);
static void sound_novel_draw_history_entry(sound_novel_history_level_t* level, s32 index);

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
        audio_queue_add(AUDIO_COMMAND_FADE_OUT, 4);
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
 * the audio queue is idle. The 32-bit busy fields
 * are read as g_audio_queue members, not scalars, which keeps the timer store
 * ahead of their loads as in the target. */
void proposition_step_discovery_level(menu_stack_record_t* discovery) {
    if (discovery->discovery.phase == PROPOSITION_DISCOVERY_PHASE_CLOSE) {
        if (g_audio_queue.count == 0 && g_audio_queue.current_command == AUDIO_COMMAND_NONE) {
            g_menu_stack_depth--;
            proposition_open_returned_list();
        }
    } else if (discovery->discovery.phase == PROPOSITION_DISCOVERY_PHASE_JINGLE) {
        discovery->discovery.timer++;
        if (g_audio_queue.count == 0 && g_audio_queue.current_command == AUDIO_COMMAND_NONE
            && discovery->discovery.timer >= 180) {
            discovery->discovery.phase = PROPOSITION_DISCOVERY_PHASE_CLOSE;
            audio_queue_add(AUDIO_COMMAND_FADE_OUT, 0x10);
            audio_queue_add(AUDIO_COMMAND_PLAY_MUSIC, AUDIO_MUSIC(2, MUSIC_BAR));
            audio_queue_add(AUDIO_COMMAND_FADE_IN, 0x10);
        }
    } else if (discovery->discovery.phase == PROPOSITION_DISCOVERY_PHASE_LAST_MESSAGE) {
        if (screen_is_dialog_waiting() != 0 && g_audio_queue.current_command == AUDIO_COMMAND_NONE
            && g_audio_queue.count == 0) {
            audio_queue_add(AUDIO_COMMAND_PLAY_MUSIC, AUDIO_MUSIC(2, MUSIC_MILESTONE));
            audio_queue_add(AUDIO_COMMAND_FADE_IN, 0x10);
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
    proposition_build_detail_panel(&g_menu_stack_records[depth], proposition, 0);
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
void proposition_build_detail_panel(menu_stack_record_t* level, s32 proposition, s32 coordinate_mode) {
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
    level->proposition_detail_panel.render_index = index;
    text_measure_pixels(
        &text_width, &text_rows, text_find_entry(*panel_text + (TEXT_ID_PROPOSITION_JOB_CONTENTS_BASE - 1)));
    rows = text_rows + 2;
    level->proposition_detail_panel.panel_rows = rows;
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

/*
 * Pushes the sound-novel menu level (MENU_STACK_LEVEL_SOUND_NOVEL).
 * `argument` is kept in the level and names the treasure detail reopened
 * when the novel ends; the novel read is the one in
 * g_sound_novel_selected.
 *
 * Hides the context value display, clears VRAM and the scratch buffer, and
 * appends the level's display objects, all hidden: the two button prompts,
 * the VRAM strip record and the overlay. It then appends the novel's sound
 * effect data, restores any saved reading state into the first render record
 * (sound_novel_restore_reading), blocks Cross, Square and Select
 * for the menu and starts the screen fade.
 */
static void sound_novel_push_level(s32 argument) {
    RECT bounds;
    point32_t origin;
    display_object_animated_t* window1;
    display_object_animated_t* window2;
    display_object_animated_t* window4;
    s32 bar_render;
    s32 index;
    point32_t* pos1;
    point32_t* pos2;
    point32_t* pos3;
    point32_t* pos4;
    point32_t* render_pos;

    /* Held from entry: with every saved register taken, reload rematerializes
     * the constant beside its one use, which is the target's la placement. */
    render_pos = &g_window_gs_image_records[0].base;
    g_hud_saved_value_position = g_hud_value_position;
    g_system_flags |= SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    world_map_clear_vram_and_scratch(1);
    g_menu_stack_records[g_menu_stack_depth].sound_novel.treasure_id = argument;

    bar_render = window_append_record(g_window_render_object_queue, &g_window_render_object_count);

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel.quit_prompt_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_SOUND_NOVEL_START_BUTTON;
    window1 = &g_window_records[index];
    window1->priority = 0xA;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    g_window_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
    pos1 = &g_window_records[index].position;
    pos1->x = -0x7A;
    pos1->y = 0x60;
    g_window_records[index].palette = 0xA;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel.history_prompt_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_SOUND_NOVEL_SQUARE_BUTTON;
    g_window_records[index].priority = 0xA;
    g_window_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
    window2 = &g_window_records[index];
    window2->anim_counter = 0;
    g_window_records[index].frame_index = 0;
    pos2 = &g_window_records[index].position;
    pos2->x = 0x42;
    pos2->y = 0x60;
    g_window_records[index].palette = 0xA;

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel.brightness_image = index;
    bounds.x = 0x1E4;
    bounds.y = 0x1E8;
    bounds.w = 0x20;
    bounds.h = 8;
    origin.x = 0x20;
    origin.y = 0x1E8;
    window_init_vram_render_record(index, bounds, origin, 0);
    pos3 = (point32_t*)((u8*)render_pos + index * sizeof(window_gs_image_record_t));
    pos3->x = 0x34;
    pos3->y = 0x55;
    g_window_gs_image_records[index].priority = 8;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_UNTINTED;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel.brightness_overlay_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_SOUND_NOVEL_OVERLAY;
    g_window_records[index].priority = 8;
    g_window_records[index].flags |= DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_UNTINTED;
    window4 = &g_window_records[index];
    window4->anim_counter = 0;
    g_window_records[index].frame_index = 0;
    pos4 = &g_window_records[index].position;
    pos4->x = 0x54;
    pos4->y = 0x58;
    g_window_records[index].palette = 0;

    suzuki_sound_append_resource(g_sound_novel_sound_resource);
    sound_novel_restore_reading(bar_render);
    g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
    g_menu_stack_ot_offset = 9;
    g_world_map_fade.priorities[0] = 8;
    g_world_map_fade.flags[1] |= WORLD_MAP_FADE_BOX_VISIBLE;
    g_menu_stack_records[g_menu_stack_depth].sound_novel._unknown_20 = 1;
    g_menu_stack_records[g_menu_stack_depth].sound_novel.countdown = 9999;
    g_menu_stack_records[g_menu_stack_depth].sound_novel.fade_timer = 0;
    g_menu_stack_records[g_menu_stack_depth].sound_novel.phase = SOUND_NOVEL_PHASE_RUN;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_SOUND_NOVEL;
    g_menu_stack_depth++;
    world_map_fade_start(0, 0x10);
}

/* Resume handler of the sound novel level: reload the saved background set, show the picture again
 * (and the side windows when the script left them visible) and resume the message thread. */
void sound_novel_resume_level(menu_stack_record_t* level) {
    s16 set = g_sound_novel_reading.background_set;

    if (set != -1) {
        picture_load_background_set(set);
        file_wait_for_pending();
    }
    g_window_gs_image_records[g_sound_novel_reading.picture_render_index].flags &= ~DISPLAY_OBJECT_BLANK;
    if (g_sound_novel_reading.state_flags & SOUND_NOVEL_STATE_PARENT_WINDOWS_VISIBLE) {
        g_window_records[level->sound_novel.quit_prompt_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        g_window_records[level->sound_novel.history_prompt_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
    }
    thread_resume(WORLD_MAP_THREAD_SLOT_MESSAGE);
}

/* The g_menu_stack_suspend_handlers entry of MENU_STACK_LEVEL_SOUND_NOVEL: nothing to suspend. */
void sound_novel_suspend_level(menu_stack_record_t* unused_level) {
}

/*
 * Per-frame step of the sound novel (MENU_STACK_LEVEL_SOUND_NOVEL).
 *
 * It runs the novel's script until it yields (sound_novel_run_until_yield).
 * L1 and R1 raise and lower the screen brightness, showing the brightness bar
 * for 60 frames. While the message box waits for a button: Select opens help
 * message 0x1036, Start (with the side windows up) opens the Quit/Continue
 * menu and holding Square opens the text history after 8 frames. When the
 * script ends the message box is stopped and the music and screen fade out;
 * then (END) the level's records are released and a screen transition reopens
 * the treasure detail the novel was started from.
 *
 * The unused array reproduces the target's 0x40-byte frame.
 */
void sound_novel_step_level(menu_stack_record_t* level) {
    s32 unused[2];
    u16* state_flags;
    world_map_fade_t* fade;
    s16* brightness;
    u32 buttons;
    s32 width;
    s32 window;
    u16 shade;

    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && !(g_system_flags & SYSTEM_FLAG_FADING)) {
        switch (level->sound_novel.phase) {
        case SOUND_NOVEL_PHASE_OPEN_HISTORY:
            if (level->sound_novel.countdown == 0) {
                g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
                sound_novel_push_history_level();
                return;
            }
            level->sound_novel.countdown--;
            return;
        case SOUND_NOVEL_PHASE_END:
            if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
                return;
            }
            g_menu_blocked_buttons = 0;
            g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
            hud_show_play_time();
            fade = &g_world_map_fade;
            g_world_map_fade.priorities[0] = 0xF;
            g_world_map_fade.flags[1] = 0;
            g_window_record_count -= 3;
            g_window_render_record_count -= 2;
            g_window_render_object_count -= 5;
            g_menu_stack_depth--;
            fade->flags[0] = (fade->flags[0] & ~WORLD_MAP_FADE_BOX_VISIBLE) | WORLD_MAP_FADE_OUT;
            sound_stop_all_sfx();
            suzuki_sound_remove_resource(g_sound_novel_sound_resource);
            sound_novel_reset_selected_reading(g_sound_novel_selected);
            g_sound_novel_countdown_frames = 0;
            screen_transition_push_level(MENU_STACK_LEVEL_BRAVE_STORY_TREASURE, level->sound_novel.treasure_id, 0);
            return;
        }
    }

    state_flags = &g_sound_novel_reading.state_flags;
    if (*state_flags & SOUND_NOVEL_STATE_END) {
        return;
    }

    if (sound_novel_run_until_yield() != 0) {
        if (g_input_current_buttons & (PSX_PAD_L1 | PSX_PAD_R1)) {
            if (g_input_current_buttons & PSX_PAD_L1) {
                if (g_sound_novel_reading.brightness < 0xFF) {
                    g_sound_novel_reading.brightness++;
                }
            }
            if (g_input_current_buttons & PSX_PAD_R1) {
                brightness = &g_sound_novel_reading.brightness;
                if (*brightness != 0) {
                    (*brightness)--;
                }
            }
            shade = g_sound_novel_reading.brightness;
            g_world_map_fade.boxes[1].r = shade;
            g_world_map_fade.boxes[1].g = shade;
            g_world_map_fade.boxes[1].b = shade;
            level->sound_novel.fade_timer = 0x3C;
        }

        if (level->sound_novel.fade_timer != 0) {
            g_window_gs_image_records[level->sound_novel.brightness_image].flags &= ~DISPLAY_OBJECT_HIDDEN;
            g_window_records[level->sound_novel.brightness_overlay_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
            width = 0x100 - g_sound_novel_reading.brightness;
            g_window_gs_image_records[level->sound_novel.brightness_image].bounds.w = width / 4;
            level->sound_novel.fade_timer--;
        } else {
            g_window_gs_image_records[level->sound_novel.brightness_image].flags |= DISPLAY_OBJECT_HIDDEN;
            g_window_records[level->sound_novel.brightness_overlay_window].flags |= DISPLAY_OBJECT_HIDDEN;
        }

        if (screen_is_dialog_waiting() != 0) {
            buttons = g_world_map_input_new_button_presses;
            if (buttons & PSX_PAD_CIRCLE) {
                return;
            }
            g_window_records[level->sound_novel.quit_prompt_window].palette
                = g_window_records[level->sound_novel.history_prompt_window].palette = 0;
            if (buttons & PSX_PAD_SELECT) {
                menu_stack_push_message_level(0x1036, 1);
                return;
            }
            if ((buttons & PSX_PAD_START)
                && (g_sound_novel_reading.state_flags & SOUND_NOVEL_STATE_PARENT_WINDOWS_VISIBLE)
                && !(g_sound_novel_reading.state_flags & SOUND_NOVEL_STATE_WHITE_OVERLAY)) {
                g_window_records[level->sound_novel.quit_prompt_window].sequence
                    = WINDOW_ANIM_SOUND_NOVEL_START_BUTTON_PRESS;
                g_window_records[level->sound_novel.quit_prompt_window].frame_index
                    = g_window_records[level->sound_novel.quit_prompt_window].anim_counter = 0;
                level->sound_novel.fade_timer = 0;
                g_window_gs_image_records[level->sound_novel.brightness_image].flags |= DISPLAY_OBJECT_HIDDEN;
                g_window_records[level->sound_novel.brightness_overlay_window].flags |= DISPLAY_OBJECT_HIDDEN;
                sound_novel_push_quit_level();
                return;
            }
            if ((g_input_current_buttons & PSX_PAD_SQUARE)
                && (g_sound_novel_reading.state_flags & SOUND_NOVEL_STATE_PARENT_WINDOWS_VISIBLE)
                && g_sound_novel_reading.text_history[3] != -1) {
                g_window_records[level->sound_novel.history_prompt_window].sequence
                    = WINDOW_ANIM_SOUND_NOVEL_SQUARE_BUTTON_PRESS;
                window = level->sound_novel.history_prompt_window;
                g_window_records[window].anim_counter = 0;
                g_window_records[window].frame_index = 0;
                level->sound_novel.fade_timer = 0;
                g_window_gs_image_records[level->sound_novel.brightness_image].flags |= DISPLAY_OBJECT_HIDDEN;
                g_window_records[level->sound_novel.brightness_overlay_window].flags |= DISPLAY_OBJECT_HIDDEN;
                g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
                level->sound_novel.phase = SOUND_NOVEL_PHASE_OPEN_HISTORY;
                level->sound_novel.countdown = 8;
            }
        } else {
            g_window_records[level->sound_novel.quit_prompt_window].palette
                = g_window_records[level->sound_novel.history_prompt_window].palette = 0xA;
        }
    } else {
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
            screen_thread_request_stop(WORLD_MAP_THREAD_SLOT_MESSAGE);
        }
        audio_queue_add(AUDIO_COMMAND_FADE_OUT, 0x20);
        sound_stop_all_sfx();
        level->sound_novel.phase = SOUND_NOVEL_PHASE_END;
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        if (!(*state_flags & SOUND_NOVEL_STATE_WHITE_OVERLAY)) {
            world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
        }
    }
}

/* Pushes the sound novel's Quit/Continue menu (MENU_STACK_LEVEL_SOUND_NOVEL_QUIT):
 * the cursor and side label windows and a 0x3c x 0x30 TEXT_ID_QUIT_CONTINUE
 * record at (-0x1e, -0x18), with the windows placed on it. Blocks every
 * button, shows the fade box black (the step dims the screen with it) and
 * starts on Quit with no fade.
 *
 * The level is written through the sound_novel_quit member of the stack-record
 * union; union field access keeps the target's per-field `$at` bases. The
 * fade state is written through a pointer, which keeps the
 * target's register base for the offset-0 32-bit flags. The unused array
 * preserves the target's 0x68-byte frame. */
static void sound_novel_push_quit_level(void) {
    RECT bounds;
    u8 unused[0x28];
    point32_t* base;
    s32 index;
    s32 depth;
    world_map_fade_t* fade;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_quit.cursor_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_quit.second_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_LIST_SIDE_LABEL;
    g_window_records[index].priority = 8;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_quit.render_index = index;
    bounds.x = 0x80;
    bounds.w = 0x3C;
    bounds.h = 0x30;
    bounds.y = 0;
    window_init_text_record(index, bounds, 0, TEXT_ID_QUIT_CONTINUE, g_window_image_buffer);
    base = &g_window_gs_image_records[index].base;
    g_window_gs_image_records[index].priority = 8;
    depth = g_menu_stack_depth;
    base->x = -(bounds.w / 2);
    base->y = -0x18;
    g_window_records[g_menu_stack_records[depth].sound_novel_quit.cursor_window].position.x
        = g_window_gs_image_records[index].base.x + 6;
    g_window_records[g_menu_stack_records[depth].sound_novel_quit.cursor_window].position.y
        = g_window_gs_image_records[index].base.y + 0xE;
    g_window_records[g_menu_stack_records[depth].sound_novel_quit.second_window].position.x
        = g_window_gs_image_records[index].base.x + 4;
    g_menu_blocked_buttons = -1;
    g_window_records[g_menu_stack_records[depth].sound_novel_quit.second_window].position.y
        = g_window_gs_image_records[index].base.y - 2;
    g_menu_stack_depth = depth + 1;
    fade = &g_world_map_fade;
    fade->boxes[0].r = 0;
    fade->boxes[0].g = 0;
    fade->boxes[0].b = 0;
    fade->flags[0] |= WORLD_MAP_FADE_BOX_VISIBLE;
    g_menu_stack_records[depth].sound_novel_quit.choice = 0;
    g_menu_stack_records[depth].sound_novel_quit._unknown_18 = 0;
    g_menu_stack_records[depth].sound_novel_quit.fade_in = 0;
    g_menu_stack_records[depth].sound_novel_quit.fade_out = 0;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_SOUND_NOVEL_QUIT;
}

/* Resume handler of the Quit/Continue menu: default palettes and the idle
 * cursor again. */
void sound_novel_resume_quit_level(menu_stack_record_t* level) {
    s32 first = level->sound_novel_quit.cursor_window;
    s32 second = level->sound_novel_quit.second_window;
    s32 index;
    /* The original frame reserves eight unused bytes. */
    s32 scratch[2];

    g_window_gs_image_records[level->sound_novel_quit.render_index].palette = 0;
    g_window_records[second].palette = 0;
    g_window_records[first].palette = 0;
    g_window_records[level->sound_novel_quit.cursor_window].sequence = WINDOW_ANIM_CURSOR_IDLE;
    index = level->sound_novel_quit.cursor_window;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
}

/* Gray out the Quit/Continue menu (palettes 10, 6 and 2) and still its cursor
 * while a help message opens over it. */
void sound_novel_suspend_quit_level(menu_stack_record_t* level) {
    s32 index;

    g_window_records[level->sound_novel_quit.cursor_window].palette = 10;
    g_window_records[level->sound_novel_quit.second_window].palette = 6;
    g_window_gs_image_records[level->sound_novel_quit.render_index].palette = 2;
    g_window_records[level->sound_novel_quit.cursor_window].sequence = WINDOW_ANIM_CURSOR_STILL;
    index = level->sound_novel_quit.cursor_window;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
}

/* Step of the sound novel's Quit/Continue menu.
 *
 * The screen dims behind it: fade_in raises the black box by 8 a frame to
 * 0x60, and fade_out (set by Cross or Continue) lowers it again and pops the
 * level at 0. While no fade runs: Select opens the highlighted row's help
 * (g_sound_novel_quit_help_text_ids), Up and Down toggle the rows,
 * and Circle on Quit stops the novel: input blocks, the menu's records go, the
 * message box stops and the music fades. The box then darkens to black; once
 * the message box has gone, the text history shifts down a slot (the cleared
 * first slot becomes -1), the play time display comes back and a screen
 * transition reopens the treasure detail.
 *
 * Every color store rereads the 32-bit fade value through a local: a direct
 * byte assignment directly from the fade field narrows the reload to `lbu`, while the
 * target keeps `lw`. The target stores the -1 history marker with a signed
 * `addiu`, so the first history slot is declared s16. The cursor
 * update needs the window pointer, the base_y local and a separate offset
 * local: that keeps base_y as the first `addu` operand and stops fold from
 * reassociating the +0xe. */
void sound_novel_step_quit_level(menu_stack_record_t* level) {
    u32 flags;
    u32 buttons;
    s32 value;
    s32 fade;
    s32 base;
    s32 offset;
    display_object_animated_t* record;
    s32 window;
    u16 first;

    if (level->sound_novel_quit.fade_in < 0x60) {
        level->sound_novel_quit.fade_in += 8;
        fade = level->sound_novel_quit.fade_in;
        g_world_map_fade.boxes[0].r = fade;
        fade = level->sound_novel_quit.fade_in;
        g_world_map_fade.boxes[0].g = fade;
        fade = level->sound_novel_quit.fade_in;
        g_world_map_fade.boxes[0].b = fade;
    }

    if (level->sound_novel_quit.fade_out != 0) {
        level->sound_novel_quit.fade_out -= 8;
        fade = level->sound_novel_quit.fade_out;
        g_world_map_fade.boxes[0].r = fade;
        fade = level->sound_novel_quit.fade_out;
        g_world_map_fade.boxes[0].g = fade;
        fade = level->sound_novel_quit.fade_out;
        g_world_map_fade.boxes[0].b = fade;
        if (level->sound_novel_quit.fade_out <= 0) {
            g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
            g_world_map_fade.flags[0] ^= WORLD_MAP_FADE_BOX_VISIBLE;
            g_window_record_count -= 2;
            g_window_render_record_count--;
            g_window_render_object_count -= 3;
            g_menu_stack_depth--;
            menu_stack_dispatch_resume();
        }
        return;
    }

    if (g_window_gs_image_records[level->sound_novel_quit.render_index].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    flags = g_system_flags;
    if (flags & SYSTEM_FLAG_FADING) {
        return;
    }

    if (flags & SYSTEM_FLAG_INPUT_BLOCKED) {
        if (level->sound_novel_quit.fade_in < 0xFF) {
            level->sound_novel_quit.fade_in += 8;
            if (level->sound_novel_quit.fade_in >= 0x100) {
                level->sound_novel_quit.fade_in = 0xFF;
            }
            fade = level->sound_novel_quit.fade_in;
            g_world_map_fade.boxes[0].r = fade;
            fade = level->sound_novel_quit.fade_in;
            g_world_map_fade.boxes[0].g = fade;
            fade = level->sound_novel_quit.fade_in;
            g_world_map_fade.boxes[0].b = fade;
            return;
        }
        if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
            return;
        }
        g_world_map_brightness_rgb.r = 0;
        g_world_map_brightness_rgb.g = 0;
        g_world_map_brightness_rgb.b = 0;
        g_world_map_fade.flags[0] |= WORLD_MAP_FADE_OUT;
        first = g_sound_novel_history_0;
        g_sound_novel_history_0 = -1;
        g_menu_blocked_buttons = 0;
        g_sound_novel_countdown_frames = 0;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_sound_novel_reading.text_history[3] = g_sound_novel_history_2;
        g_sound_novel_history_2 = g_sound_novel_history_1;
        g_sound_novel_history_1 = first;
        hud_show_play_time();
        g_world_map_fade.priorities[0] = 0xF;
        g_world_map_fade.flags[1] = 0;
        g_window_record_count -= 3;
        g_window_render_record_count -= 2;
        g_window_render_object_count -= 5;
        g_menu_stack_depth -= 2;
        suzuki_sound_remove_resource(g_sound_novel_sound_resource);
        screen_transition_push_level(
            MENU_STACK_LEVEL_BRAVE_STORY_TREASURE, g_menu_stack_records[g_menu_stack_depth].sound_novel.treasure_id, 0);
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        level->sound_novel_quit.fade_out = level->sound_novel_quit.fade_in;
        return;
    }

    if (buttons & PSX_PAD_SELECT) {
        sound_novel_suspend_quit_level(level);
        menu_stack_push_text_window_level(g_sound_novel_quit_help_text_ids[level->sound_novel_quit.choice] | 0x1000, 1);
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0 || world_map_update_input_repeat(PSX_PAD_DOWN) != 0) {
        value = level->sound_novel_quit.choice ^ 1;
        level->sound_novel_quit.choice = value;
        record = &g_window_records[level->sound_novel_quit.cursor_window];
        base = g_window_gs_image_records[level->sound_novel_quit.render_index].base.y;
        offset = value * 0x10 + 0xE;
        record->position.y = base + offset;
        sound_play_effect(SOUND_SFX_CURSOR_MOVE);
    }

    if (g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) {
        g_window_records[level->sound_novel_quit.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
        window = level->sound_novel_quit.cursor_window;
        g_window_records[window].anim_counter = 0;
        g_window_records[window].frame_index = 0;
        if (level->sound_novel_quit.choice == 0) {
            g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
            g_window_record_count -= 2;
            g_window_render_record_count--;
            g_window_render_object_count -= 3;
            if (thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) != 0) {
                screen_thread_request_stop(WORLD_MAP_THREAD_SLOT_MESSAGE);
            }
            audio_queue_add(AUDIO_COMMAND_FADE_OUT, 0x20);
            sound_stop_all_sfx();
            sound_play_effect(SOUND_SFX_CONFIRM);
            return;
        }
        level->sound_novel_quit.fade_out = level->sound_novel_quit.fade_in;
    }
}

/* Restore the HUD position and display the current play time.
 * Cache the flags and time before publishing the display fields. */
static void hud_show_play_time(void) {
    u32 flags;
    s32 hours;
    s32 minutes;
    s32 seconds;

    g_hud_value_position = g_hud_saved_value_position;
    g_hud_value_mode = HUD_VALUE_PLAY_TIME;
    g_hud_ot_priority = 1;
    flags = g_system_flags;
    hours = g_save_play_time_hours;
    minutes = g_save_play_time_minutes;
    seconds = g_save_play_time_seconds;
    g_system_flags = flags | SYSTEM_FLAG_HIDE_CONTEXT_VALUE;
    g_hud_value = hours;
    g_hud_value_secondary = minutes;
    g_hud_value_tertiary = seconds;
}

/* Pushes the sound novel's text history (MENU_STACK_LEVEL_SOUND_NOVEL_HISTORY).
 *
 * Suspends the message box, appends the upper and lower scroll indicators
 * (the lower one hidden) and the 0xf8 x 0xb0 text panel, draws the newest
 * history entry (TEXT_ID_WORLD_MESSAGE_BASE + text_history[3]) centered in it,
 * and copies the four history entries (newest first) into the level. It also
 * grays the novel's Start button window, sets the picture to full brightness
 * and shows the black fade box for the dim. The upper indicator is hidden
 * when there is no older entry.
 *
 * The level is written through the sound_novel_history member of the
 * stack-record union (as in sound_novel_push_quit_level).
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
static void sound_novel_push_history_level(void) {
    RECT bounds;
    u8 unused[8];
    display_object_animated_t* windows;
    window_xy_t* positions;
    point32_t* position;
    CVECTOR* colors;
    s16* newest;
    s32 index;
    s32 depth;
    s32 top;

    thread_suspend(WORLD_MAP_THREAD_SLOT_MESSAGE);

    windows = g_window_records;
    positions = (window_xy_t*)&windows->position;
    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_history.upper_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_SOUND_NOVEL_HISTORY_UPPER;
    g_window_records[index].priority = 9;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    positions[index].x = 0;
    positions[index].y = -0x6A;

    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_history.lower_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_SOUND_NOVEL_HISTORY_LOWER;
    g_window_records[index].priority = 9;
    windows[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    g_window_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
    positions[index].x = 0;
    positions[index].y = 0x5E;

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_history.render_index = index;
    bounds.w = 0xF8;
    bounds.h = 0xB0;
    bounds.x = 0;
    bounds.y = 0;
    window_build_render_record_image(index, bounds, 1, g_window_image_buffer);
    position = &g_window_gs_image_records[index].base;
    newest = &g_sound_novel_reading.text_history[3];
    g_window_gs_image_records[index].priority = g_menu_stack_ot_offset;
    position->x = -0x80;
    position->y = -0x64;
    g_window_gs_image_records[index].clut.x = 0x3C0;
    g_window_gs_image_records[index].clut.y = 0x1F2;
    window_draw_centered_text(index, *newest + TEXT_ID_WORLD_MESSAGE_BASE);

    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].sound_novel.quit_prompt_window].palette = 0xA;
    /* Direct record.color access changes the column base and instruction scheduling. */
    colors = &g_window_gs_image_records->color;
    ((CVECTOR*)((u8*)colors + g_sound_novel_reading.picture_render_index * sizeof(g_window_gs_image_records[0])))->r
        = 0x80;
    ((CVECTOR*)((u8*)colors + g_sound_novel_reading.picture_render_index * sizeof(g_window_gs_image_records[0])))->g
        = 0x80;
    ((CVECTOR*)((u8*)colors + g_sound_novel_reading.picture_render_index * sizeof(g_window_gs_image_records[0])))->b
        = 0x80;
    g_world_map_fade.boxes[0].r = 0;
    g_world_map_fade.boxes[0].g = 0;
    g_world_map_fade.boxes[0].b = 0;
    g_world_map_fade.flags[0] |= WORLD_MAP_FADE_BOX_VISIBLE;

    top = g_menu_stack_depth;
    g_menu_stack_records[top].sound_novel_history.scroll = 0;
    g_menu_stack_records[top].sound_novel_history.newest_text_id = *newest;
    g_menu_stack_records[top].sound_novel_history.text_ids[0] = newest[-1];
    g_menu_stack_records[top].sound_novel_history.text_ids[1] = newest[-2];
    g_menu_blocked_buttons = -1;
    g_menu_stack_records[top].sound_novel_history.text_ids[2] = newest[-3];
    if (g_menu_stack_records[top].sound_novel_history.text_ids[0] == -1) {
        g_window_records[g_menu_stack_records[top].sound_novel_history.upper_window].flags |= DISPLAY_OBJECT_HIDDEN;
    }

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].sound_novel_history.phase = SOUND_NOVEL_HISTORY_PHASE_DIM;
    g_menu_stack_records[depth].sound_novel_history.step = 0;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_SOUND_NOVEL_HISTORY;
    g_menu_stack_depth = depth + 1;
}

/* Step of the sound novel's text history.
 *
 * DIM darkens the picture (g_sound_novel_reading.picture_render_index)
 * and g_hud_color from 0x80 to 0x60 in five steps while the fade box
 * brightens. BROWSE scrolls the history with Up and Down, hiding each
 * indicator at its end, until Square is released. RESTORE runs the reverse
 * ramp and then, if g_sound_novel_reading.text_id names a text,
 * clears the panel image (width * height / 8 32-bit values), redraws that
 * text into it past its choice marks, uploads it and clears the pending
 * sound, weather and music variables; it then pops the level and resumes the
 * message box.
 *
 * Load-bearing spellings: the nonzero phases are a switch nested under
 * `phase != 0`, matching the target's test order; each fade case holds
 * &g_sound_novel_reading.picture_render_index in its own pointer local, and `shade` is shared between the
 * cases (a global pseudo, as in the target's $a2); the text box position is
 * read into `x`/`y` so the s32 point is loaded with `lw`; `unused` preserves
 * the 0x58-byte frame. */
void sound_novel_step_history_level(menu_stack_record_t* level) {
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

    if (level->sound_novel_history.phase != SOUND_NOVEL_HISTORY_PHASE_BROWSE) {
        switch (level->sound_novel_history.phase) {
        case SOUND_NOVEL_HISTORY_PHASE_DIM:
            shade = 0x80 - level->sound_novel_history.step * 8;
            index1 = &g_sound_novel_reading.picture_render_index;
            g_window_gs_image_records[*index1].color.r = shade;
            g_window_gs_image_records[*index1].color.g = shade;
            g_window_gs_image_records[*index1].color.b = shade;
            g_hud_color.r = shade;
            g_hud_color.g = shade;
            g_hud_color.b = shade;
            shade = level->sound_novel_history.step * 8;
            g_world_map_fade.boxes[0].r = shade;
            g_world_map_fade.boxes[0].g = shade;
            g_world_map_fade.boxes[0].b = shade;
            step = level->sound_novel_history.step + 1;
            level->sound_novel_history.step = step;
            if (step >= 5) {
                level->sound_novel_history.step = 0;
                level->sound_novel_history.phase = SOUND_NOVEL_HISTORY_PHASE_BROWSE;
            }
            break;
        case SOUND_NOVEL_HISTORY_PHASE_RESTORE:
            index2 = &g_sound_novel_reading.picture_render_index;
            shade = level->sound_novel_history.step * 0x10 + 0x40;
            g_window_gs_image_records[*index2].color.r = shade;
            g_window_gs_image_records[*index2].color.g = shade;
            g_window_gs_image_records[*index2].color.b = shade;
            g_hud_color.r = shade;
            g_hud_color.g = shade;
            g_hud_color.b = shade;
            shade = 0x20 - level->sound_novel_history.step * 8;
            g_world_map_fade.boxes[0].r = shade;
            g_world_map_fade.boxes[0].g = shade;
            g_world_map_fade.boxes[0].b = shade;
            step = level->sound_novel_history.step + 1;
            level->sound_novel_history.step = step;
            if (step < 5) {
                break;
            }
            text_id = g_sound_novel_reading.text_id;
            if (text_id != -1) {
                text_id += TEXT_ID_WORLD_MESSAGE_BASE;
                i = 0;
                image = g_window_image_buffer;
                point.x = 8;
                point.y = 8;
                for (;;) {
                    render_index = level->sound_novel_history.render_index;
                    if (i >= g_window_gs_image_records[render_index].bounds.w
                            * g_window_gs_image_records[render_index].bounds.h / 8) {
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
                box.stride = g_window_gs_image_records[level->sound_novel_history.render_index].bounds.w;
                menu_display_text_entry_past_choice_marks(text_id, g_window_image_buffer, &box);
                window_load_render_record_image(level->sound_novel_history.render_index, g_window_image_buffer);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_SOUND_EFFECT, 0);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_WEATHER_SOUND, 0);
                script_set_variable(EVENT_SCRIPT_VAR_PENDING_MUSIC_TRACK, 0);
            }
            level->sound_novel_history.step = 0;
            level->sound_novel_history.phase = SOUND_NOVEL_HISTORY_PHASE_BROWSE;
            g_menu_blocked_buttons = PSX_PAD_CROSS | PSX_PAD_SQUARE | PSX_PAD_SELECT;
            g_world_map_fade.flags[0] ^= WORLD_MAP_FADE_BOX_VISIBLE;
            depth = g_menu_stack_depth;
            g_window_record_count -= 2;
            g_window_render_record_count--;
            g_window_render_object_count -= 3;
            g_menu_stack_depth = depth - 1;
            g_window_records[g_menu_stack_records[depth - 2].sound_novel.quit_prompt_window].palette = 0;
            thread_resume(WORLD_MAP_THREAD_SLOT_MESSAGE);
            break;
        }
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) != 0) {
        if (level->sound_novel_history.scroll < 3
            && level->sound_novel_history.text_ids[level->sound_novel_history.scroll] != -1) {
            sound_novel_draw_history_entry(&level->sound_novel_history, 4);
            level->sound_novel_history.scroll = level->sound_novel_history.scroll + 1;
            if (level->sound_novel_history.scroll == 3
                || level->sound_novel_history.text_ids[level->sound_novel_history.scroll] == -1) {
                g_window_records[level->sound_novel_history.upper_window].flags |= DISPLAY_OBJECT_HIDDEN;
            } else {
                g_window_records[level->sound_novel_history.upper_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
            }
            g_window_records[level->sound_novel_history.lower_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        }
    }

    if (world_map_update_input_repeat(PSX_PAD_DOWN) != 0 && level->sound_novel_history.scroll != 0) {
        sound_novel_draw_history_entry(&level->sound_novel_history, 2);
        level->sound_novel_history.scroll = level->sound_novel_history.scroll - 1;
        if (level->sound_novel_history.scroll == 0) {
            g_window_records[level->sound_novel_history.lower_window].flags |= DISPLAY_OBJECT_HIDDEN;
        } else {
            g_window_records[level->sound_novel_history.lower_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
        }
        g_window_records[level->sound_novel_history.upper_window].flags &= ~DISPLAY_OBJECT_HIDDEN;
    }

    if (!(g_input_current_buttons & PSX_PAD_SQUARE)) {
        level->sound_novel_history.phase = SOUND_NOVEL_HISTORY_PHASE_RESTORE;
    }
}

/* Draw the history entry at 32-bit slot `index + scroll` of the level (counted
 * from the level's start) centered in the panel; an empty entry (-1) draws
 * nothing. */
static void sound_novel_draw_history_entry(sound_novel_history_level_t* level, s32 index) {
    /* Named text-array access reverses the final address-add operands. */
    s32 text_id = ((s32*)level)[index + level->scroll];

    if (text_id != -1) {
        window_draw_centered_text(level->render_index, text_id + TEXT_ID_WORLD_MESSAGE_BASE);
    }
}

/* Open the Brave Story list of unexplored lands found so far (their script flags set), up to eight
 * rows. */
void brave_story_open_unexplored_land_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 count = 0;
    s32 i; /* reused as the visible-row count, which the target keeps in $s0 */
    s32 record_index;

    for (i = 0; i < EVENT_SCRIPT_VAR_UNEXPLORED_LAND_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_UNEXPLORED_LAND_FIRST) != 0) {
            g_menu_stack_entry_values[count] = i + TEXT_ID_UNEXPLORED_LAND_NAME_BASE;
            g_menu_stack_row_flags[count] = 0;
            count++;
        }
    }
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x60;
    origin.x = 0x80;
    layout.visible_rows = i;
    origin.y = 0x50;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_UNEXPLORED_LANDS, count, i);
    window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LANDS;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the unexplored lands list: block Circle, Cross and Select for the
 * levels underneath and show TEXT_ID_BRAVE_STORY_INTRO again. */
void brave_story_restore_unexplored_land_list_prompt(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
}

/* Withholds every button from the WORLD menu frame while the next menu level
 * runs. The caller hands in its menu level; the target reads no argument. */
void brave_story_block_unexplored_land_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the unexplored lands list; twin of rumor_handle_list_input
 * that closes the level inline, opens help message 0x1058 on Select, and hands
 * a confirmed entry to brave_story_push_unexplored_land_level. Cross
 * is accepted even while a file is loading.
 *
 * The target passes the level to brave_story_block_unexplored_land_list_buttons,
 * which reads no argument. */
void brave_story_handle_unexplored_land_list_input(menu_stack_record_t* level) {
    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_stack_ot_offset = 1;
        g_menu_blocked_buttons = 0;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_UNEXPLORED_LANDS);
            brave_story_push_unexplored_land_level(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_UNEXPLORED_LANDS);
        menu_stack_dispatch_resume();
        return;
    }
    if (g_world_map_input_new_button_presses & PSX_PAD_CROSS) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0
        && g_file_cd_state.state == 0) {
        brave_story_block_unexplored_land_list_buttons(level);
        menu_stack_push_message_level(0x1058, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LAND, the picture and text of
 * one unexplored land. Records the land on the new level, loads its picture
 * data set, appends a hidden render record for the picture at (-116, -34),
 * reads the land's discovery date from g_brave_story_land_discovery_date_bits, and
 * sets up g_text_window on the land's text
 * (TEXT_ID_WORLD_MESSAGE_BASE + land) with the month name
 * (TEXT_ID_MONTH_NAME_BASE + month) and the day substituted. Then
 * hides the Brave Story menu's four windows and frame and says
 * TEXT_ID_BRAVE_STORY_ENTRY_INFO with the land's name.
 *
 * The coordinate mode is a constant local, yet retail still switched on it to
 * choose the text image origin: (0, 0x60) for mode 0, (0, 0x70) for mode 1.
 * The compiler folds the switch in its first CSE pass, after the stores were
 * split around it, which is why image_y keeps its absolute address while the
 * later stores go through the text_id base register. That only happens when
 * the live case is the last one.
 *
 * The render record's position is written through a pointer to its base: the
 * target reaches base.x/base.y at 0 and 4 off one register holding
 * &g_window_gs_image_records[index].base, while the flags update above
 * it still uses the $at absolute form. Spelling both coordinates as ordinary
 * indexed field stores recomputes the $at base twice and costs four
 * instructions.
 *
 * `unused` reproduces eight bytes of locals below the date bytes that the
 * target never touches. */
static void brave_story_push_unexplored_land_level(s32 value) {
    s32 index;
    point32_t* base;
    s32 coordinate_mode = 1;
    u8 unused[8];
    calendar_date_t date;

    g_menu_stack_records[g_menu_stack_depth].detail.value = value;
    picture_load_set(g_unexplored_land_picture_sets[value]);
    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].detail.render_index = index;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
    base = &g_window_gs_image_records[index].base;
    base->x = -0x74;
    base->y = -0x22;
    calendar_read_date(g_brave_story_land_discovery_date_bits, value, &date);
    g_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_text_window.image_x = 0;
        g_text_window.image_y = 0x60;
        break;
    case 1:
        g_text_window.image_x = 0;
        g_text_window.image_y = 0x70;
        break;
    }

    g_text_window.base.x = -122;
    g_text_window.base.y = -40;
    g_text_window.text_width = 0xdc;
    g_text_window.rows_per_page = 8;
    g_text_window.priority = 0xa;
    g_text_window.image_coordinate_mode = coordinate_mode;
    g_text_window.extra_render_index = -1;
    g_text_window.text_substitutions[0] = date.month + TEXT_ID_MONTH_NAME_BASE;
    g_text_window.text_substitutions[1] = date.day;
    text_window_open(&g_text_window);
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.cursor_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.label_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.rows_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.land_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.treasure_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_UNEXPLORED_LAND_NAME_BASE;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_menu_stack_records[g_menu_stack_depth].detail.phase = BRAVE_STORY_DETAIL_PHASE_LOAD;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_BRAVE_STORY_UNEXPLORED_LAND;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Step of the unexplored-land detail level.
 *
 * LOAD waits until render record saved_record_index is not popping in; no
 * push sets that field (retail). BUILD, once file loading finishes, uploads
 * the land picture's TIM pixels and CLUT, binds the level's render record to
 * the VRAM image at (-116, -34) and makes it the scrollable text window's
 * extra render record (position and image rectangle). POP_IN waits for that
 * record's pop-in. SHOWN waits for the text window to finish, shows the
 * Brave Story menu's windows and frame again, pops the level and reopens the
 * unexplored-land list.
 *
 * The TIM widths are read unsigned (`lhu`), as Psy-Q's GsIMAGE declares
 * pw/ph. The text window's base and image rectangle are each copied as one
 * aggregate: the target loads both 32-bit values before either store. */
void brave_story_step_unexplored_land_level(menu_stack_record_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t* base;
    s32* index;
    s32 render;
    s32 depth;

    switch (level->detail.phase) {
    case BRAVE_STORY_DETAIL_PHASE_LOAD:
        if (!(g_window_gs_image_records[level->detail.saved_record_index].flags & DISPLAY_OBJECT_POP_IN)) {
            level->detail.phase = BRAVE_STORY_DETAIL_PHASE_BUILD;
        }
        break;
    case BRAVE_STORY_DETAIL_PHASE_BUILD:
        if (g_file_cd_state.state != 0) {
            break;
        }
        GsGetTimInfo(g_picture_buffer + 1, &image);
        LoadTPage(image.pixel, 0, 0, image.px, image.py, image.pw * 4, image.ph);
        LoadClut(image.clut, image.cx, image.cy);
        render = level->detail.render_index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        clut_position.x = image.cx;
        clut_position.y = image.cy;
        window_init_vram_render_record(render, bounds, clut_position, DISPLAY_OBJECT_8BIT_CLUT);
        g_window_gs_image_records[render].priority = 0xA;
        base = &g_window_gs_image_records[render].base;
        base->x = -0x74;
        base->y = -0x22;
        g_window_gs_image_records[render].flags
            = (g_window_gs_image_records[render].flags & ~DISPLAY_OBJECT_HIDDEN) | DISPLAY_OBJECT_POP_IN;
        g_text_window.extra_render_index = render;
        g_text_window.extra_base = *base;
        g_text_window.extra_bounds = g_window_gs_image_records[render].bounds;
        level->detail.phase = level->detail.phase + 1;
        break;
    case BRAVE_STORY_DETAIL_PHASE_POP_IN:
        if (!(g_window_gs_image_records[level->detail.render_index].flags & DISPLAY_OBJECT_POP_IN)) {
            level->detail.phase = BRAVE_STORY_DETAIL_PHASE_SHOWN;
        }
        break;
    default:
        index = &g_text_window.render_record_index;
        if (g_window_gs_image_records[*index].flags & DISPLAY_OBJECT_POP_IN) {
            break;
        }
        if (text_window_step(&g_text_window) != 0) {
            break;
        }
        depth = g_menu_stack_depth;
        g_window_records[g_menu_stack_records[depth - 2].brave_story.cursor_window].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_window_records[g_menu_stack_records[depth - 2].brave_story.label_window].flags ^= DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
            ^= DISPLAY_OBJECT_HIDDEN;
        g_window_gs_image_records[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
            |= DISPLAY_OBJECT_POP_IN;
        g_window_records[g_menu_stack_records[depth - 2].brave_story.land_completion_star].flags
            ^= DISPLAY_OBJECT_HIDDEN;
        g_window_render_record_count--;
        g_window_render_object_count--;
        g_window_records[g_menu_stack_records[depth - 2].brave_story.treasure_completion_star].flags
            ^= DISPLAY_OBJECT_HIDDEN;
        g_menu_stack_depth = depth - 1;
        brave_story_open_unexplored_land_list();
        break;
    }
}

/* Opens the Brave Story's treasure list: every treasure found
 * (EVENT_SCRIPT_VAR_TREASURE_FIRST + treasure set), by name
 * (TEXT_ID_TREASURE_NAME_BASE + treasure), up to eight rows at (0x88, 0x50),
 * 0x5a wide, and pushes MENU_STACK_LEVEL_BRAVE_STORY_TREASURES. Twin of
 * brave_story_open_unexplored_land_list. */
void brave_story_open_treasure_list(void) {
    window_panel_layout_t layout;
    point32_t origin;
    s32 count = 0;
    s32 i; /* reused as the visible-row count, which the target keeps in $s0 */
    s32 record_index;

    for (i = 0; i < EVENT_SCRIPT_VAR_TREASURE_COUNT; i++) {
        if (script_get_variable(i + EVENT_SCRIPT_VAR_TREASURE_FIRST) != 0) {
            g_menu_stack_entry_values[count] = i + TEXT_ID_TREASURE_NAME_BASE;
            g_menu_stack_row_flags[count] = 0;
            count++;
        }
    }
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
    g_menu_stack_records[g_menu_stack_depth].panel.entry_count = count;
    if (count < 8) {
        i = count;
    } else {
        i = 8;
    }
    layout.text_width = 0x5A;
    origin.x = 0x88;
    layout.visible_rows = i;
    origin.y = 0x50;
    menu_stack_clamp_cursor(MENU_STACK_CURSOR_SLOT_TREASURES, count, i);
    window_start_panel_thread(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL,
        g_menu_stack_records[g_menu_stack_depth].panel.entry_count, &layout, &origin);
    record_index = g_menu_stack_depth;
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    g_menu_stack_types[record_index] = MENU_STACK_LEVEL_BRAVE_STORY_TREASURES;
    g_menu_stack_depth = record_index + 1;
}

/* Resume handler of the treasures list: block Circle, Cross and Select for the
 * levels underneath and show TEXT_ID_BRAVE_STORY_INTRO again. */
void brave_story_restore_treasure_list_prompt(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = PSX_PAD_CIRCLE | PSX_PAD_CROSS | PSX_PAD_SELECT;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
}

/* Withhold every button from the WORLD menu frame while a level opened from the treasures list
 * runs. */
void brave_story_block_treasure_list_buttons(menu_stack_record_t* unused_level) {
    g_menu_blocked_buttons = -1;
}

/* Input step of the treasure list; twin of brave_story_handle_unexplored_land_list_input.
 * Cross and Circle wait for file loading, close the panel and record the
 * choice in result; Select on a non-empty list opens help message 0x101f.
 * Once the panel has closed the level pops, and a confirmed choice passes the
 * selected entry's value to brave_story_push_treasure_level.
 *
 * The target passes the level to brave_story_block_unexplored_land_list_buttons,
 * which reads no argument. */
void brave_story_handle_treasure_list_input(menu_stack_record_t* level) {
    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && thread_is_running(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL) == 0) {
        g_menu_stack_ot_offset = 1;
        g_menu_blocked_buttons = 0;
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_menu_stack_depth--;
        if (level->panel.result != 0) {
            menu_stack_store_cursor(MENU_STACK_CURSOR_SLOT_TREASURES);
            brave_story_push_treasure_level(
                g_menu_stack_entry_values[g_window_panel_render_state.selected_index] & TEXT_ID_ENTRY_MASK);
            return;
        }
        menu_stack_clear_cursor(MENU_STACK_CURSOR_SLOT_TREASURES);
        menu_stack_dispatch_resume();
        return;
    }
    if ((g_world_map_input_new_button_presses & PSX_PAD_CROSS) && g_file_cd_state.state == 0) {
        sound_play_effect(SOUND_SFX_CANCEL);
        level->panel.result = 0;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_SELECT) && level->panel.entry_count != 0) {
        brave_story_block_unexplored_land_list_buttons(level);
        menu_stack_push_message_level(0x101F, 1);
    } else if ((g_world_map_input_new_button_presses & PSX_PAD_CIRCLE) && level->panel.entry_count != 0
        && g_file_cd_state.state == 0) {
        sound_play_effect(SOUND_SFX_CONFIRM);
        level->panel.result = 1;
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_WINDOW_PANEL, 0, 0, 1);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
    }
}

/* Pushes MENU_STACK_LEVEL_BRAVE_STORY_TREASURE, the picture and text of one
 * treasure; the twin of brave_story_push_unexplored_land_level and
 * brave_story_push_event_level.
 *
 * Records the treasure on the new level, loads its picture data set from
 * g_treasure_picture_sets, appends a hidden render record for the
 * picture, reads the acquisition date from
 * g_brave_story_treasure_acquisition_date_bits and sets up
 * g_text_window on its text (TEXT_ID_WORLD_MESSAGE_BASE +
 * value) with the month name and the day substituted. It then hides the
 * Brave Story menu's four windows and frame and says
 * TEXT_ID_BRAVE_STORY_ENTRY_INFO with the treasure's name.
 *
 * Unlike brave_story_push_unexplored_land_level this one does not
 * place the picture render record (the step does), and it clears the new
 * level's timer as well as starting it in LOAD.
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
void brave_story_push_treasure_level(s32 value) {
    s32 index;
    s32 coordinate_mode = 1;
    u8 unused[8];
    calendar_date_t date;

    g_menu_stack_records[g_menu_stack_depth].detail.value = value;
    picture_load_set(g_treasure_picture_sets[value]);
    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].detail.render_index = index;
    g_window_gs_image_records[index].flags |= DISPLAY_OBJECT_HIDDEN;
    calendar_read_date(g_brave_story_treasure_acquisition_date_bits, value, &date);
    g_text_window.text_id = value + TEXT_ID_WORLD_MESSAGE_BASE;
    switch (coordinate_mode) {
    case 0:
        g_text_window.image_x = 0;
        g_text_window.image_y = 0x60;
        break;
    case 1:
        g_text_window.image_x = 0;
        g_text_window.image_y = 0x70;
        break;
    }

    g_text_window.base.x = -122;
    g_text_window.base.y = -40;
    g_text_window.text_width = 0xdc;
    g_text_window.rows_per_page = 8;
    g_text_window.priority = 0xa;
    g_text_window.image_coordinate_mode = coordinate_mode;
    g_text_window.extra_render_index = -1;
    g_text_window.text_substitutions[0] = date.month + TEXT_ID_MONTH_NAME_BASE;
    g_text_window.text_substitutions[1] = date.day;
    text_window_open(&g_text_window);
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.cursor_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.label_window].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_gs_image_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.rows_image].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.land_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_window_records[g_menu_stack_records[g_menu_stack_depth - 1].brave_story.treasure_completion_star].flags
        |= DISPLAY_OBJECT_HIDDEN;
    g_text_substitution_values[0] = value + TEXT_ID_TREASURE_NAME_BASE;
    thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_ENTRY_INFO, 0);
    g_menu_stack_records[g_menu_stack_depth].detail.phase = BRAVE_STORY_DETAIL_PHASE_LOAD;
    g_menu_stack_records[g_menu_stack_depth].detail.timer = 0;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_BRAVE_STORY_TREASURE;
    g_menu_stack_depth = g_menu_stack_depth + 1;
}

/* Handler of the treasure detail level (the treasure's picture and text): the
 * phases mirror brave_story_step_unexplored_land_level.
 *
 * LOAD waits until render record saved_record_index is not popping in; no
 * push sets that field (retail). BUILD, once file loading finishes, uploads
 * the picture TIM's CLUT, binds the level's render record to the VRAM image
 * with its right edge at x -0x3e and centered on y 0x10, starts the dissolve
 * upload (picture_step_dissolve_upload) and publishes the record in
 * g_text_window (extra_render_index, extra_base,
 * extra_bounds). POP_IN advances that upload one step per frame. SHOWN:
 * Triangle on a treasure with a sound novel (g_treasure_sound_novel_ids)
 * picks it, blocks input and closes the message box, fading the screen out
 * unless a saved reading exists; once that is done the level is released
 * and the novel opens (sound_novel_push_restart_level for a saved
 * reading, else sound_novel_push_level). When the text window
 * finishes instead, the Brave Story menu comes back and the treasure list
 * reopens.
 *
 * The record index is staged in `saved` before the depth store: the target
 * reloads the full 32-bit value for the 16-bit store instead of narrowing the load.
 * The button path's byte uses its own local; sharing `slot` changes the
 * register allocation. */
void brave_story_step_treasure_level(menu_stack_record_t* level) {
    GsIMAGE image;
    RECT bounds;
    point32_t clut_position;
    point32_t* base;
    s32 render;
    s32 slot;
    s32 depth;
    s32 saved;
    s32 entry;

    switch (level->detail.phase) {
    case BRAVE_STORY_DETAIL_PHASE_LOAD:
        if (!(g_window_gs_image_records[level->detail.saved_record_index].flags & DISPLAY_OBJECT_POP_IN)) {
            level->detail.phase = BRAVE_STORY_DETAIL_PHASE_BUILD;
        }
        break;
    case BRAVE_STORY_DETAIL_PHASE_BUILD:
        if (g_file_cd_state.state != 0) {
            break;
        }
        GsGetTimInfo(g_picture_buffer + 1, &image);
        LoadClut(image.clut, image.cx, image.cy);
        render = level->detail.render_index;
        bounds.x = image.px;
        bounds.y = image.py;
        bounds.w = image.pw;
        bounds.h = image.ph;
        clut_position.x = image.cx;
        clut_position.y = image.cy;
        window_init_vram_render_record(render, bounds, clut_position, DISPLAY_OBJECT_8BIT_CLUT);
        g_window_gs_image_records[render].priority = 0xA;
        g_window_gs_image_records[render].flags &= ~(DISPLAY_OBJECT_HIDDEN | DISPLAY_OBJECT_POP_IN);
        base = &g_window_gs_image_records[render].base;
        base->x = -0x3E - image.pw;
        base->y = 0x10 - (image.ph >> 1);
        picture_step_dissolve_upload(&image, 0);
        g_text_window.extra_render_index = render;
        g_text_window.extra_base = *base;
        g_text_window.extra_bounds = g_window_gs_image_records[render].bounds;
        level->detail.phase = level->detail.phase + 1;
        break;
    case BRAVE_STORY_DETAIL_PHASE_POP_IN:
        level->detail.timer = level->detail.timer + 1;
        GsGetTimInfo(g_picture_buffer + 1, &image);
        if (picture_step_dissolve_upload(&image, level->detail.timer) == 0) {
            level->detail.phase = BRAVE_STORY_DETAIL_PHASE_SHOWN;
        }
        break;
    default:
        if ((g_system_flags & (SYSTEM_FLAG_INPUT_BLOCKED | SYSTEM_FLAG_FADING)) == SYSTEM_FLAG_INPUT_BLOCKED
            && thread_is_running(WORLD_MAP_THREAD_SLOT_MESSAGE) == 0) {
            slot = level->detail.saved_record_index;
            g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
            g_window_record_count -= 4;
            g_window_render_record_count -= 2;
            g_window_render_object_count -= 6;
            saved = level->detail.saved_record_index;
            g_menu_stack_depth--;
            g_sound_novel_selected = saved;
            if (slot < 5 && g_sound_novel_saved_readings[slot].section != 0) {
                sound_novel_push_restart_level(level->detail.value);
                break;
            }
            sound_novel_push_level(level->detail.value);
            break;
        }
        if (text_window_step(&g_text_window) == 0) {
            depth = g_menu_stack_depth;
            g_window_records[g_menu_stack_records[depth - 2].brave_story.cursor_window].flags ^= DISPLAY_OBJECT_HIDDEN;
            g_window_records[g_menu_stack_records[depth - 2].brave_story.label_window].flags ^= DISPLAY_OBJECT_HIDDEN;
            g_window_gs_image_records[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
                ^= DISPLAY_OBJECT_HIDDEN;
            g_window_gs_image_records[g_menu_stack_records[depth - 2].brave_story.rows_image].flags
                |= DISPLAY_OBJECT_POP_IN;
            g_window_records[g_menu_stack_records[depth - 2].brave_story.land_completion_star].flags
                ^= DISPLAY_OBJECT_HIDDEN;
            g_window_render_record_count--;
            g_window_render_object_count--;
            g_window_records[g_menu_stack_records[depth - 2].brave_story.treasure_completion_star].flags
                ^= DISPLAY_OBJECT_HIDDEN;
            g_menu_stack_depth = depth - 1;
            brave_story_open_treasure_list();
            break;
        }
        if (g_world_map_input_new_button_presses & PSX_PAD_TRIANGLE) {
            entry = g_treasure_sound_novel_ids[level->detail.value];
            if (entry != 0) {
                level->detail.saved_record_index = entry - 1;
                if (entry - 1 >= 5 || g_sound_novel_saved_readings[entry - 1].section == 0) {
                    world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
                }
                g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
                thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0, -1, 0);
                sound_play_effect(SOUND_SFX_CONFIRM);
            }
        }
        break;
    }
}

/* Free RAM above WLDCORE's data that the dissolve upload stages its image in. */
#define DISSOLVE_STAGING_BUFFER ((u8*)0x801EE000)

/*
 * Streams an image into VRAM through the 0x801EE000 staging buffer in 1 KiB
 * bands, copying 32 more bytes of each active band per step in the order
 * given by the g_picture_dissolve_band_order offset table.
 *
 * Step 0 clears the buffer to 0xFF, uploads it and resets the per-band
 * progress. Its result is unspecified, and initialization callers ignore it.
 * Later steps return 0 once step reaches 50, otherwise 1.
 */
static s32 picture_step_dissolve_upload(GsIMAGE* image, s32 step) {
    u8* pixels = (u8*)image->pixel;
    u16* order = g_picture_dissolve_band_order;
    u8* buffer = DISSOLVE_STAGING_BUFFER;
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
        state = &g_picture_dissolve_band_progress[15];
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
        if (i <= step && (count = g_picture_dissolve_band_progress[i]) < 32U) {
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
            g_picture_dissolve_band_progress[i]++;
        }
        LoadTPage((u32*)buffer, 0, 0, image->px, image->py, image->pw * 4, image->ph);
    }

    return 1;
}

/* Pushes MENU_STACK_LEVEL_SOUND_NOVEL_RESTART (stepped by
 * sound_novel_step_restart_level) for treasure `mode`, whose sound
 * novel has a saved reading: appends the cursor window, a VRAM render record
 * for the TIM at g_picture_buffer (uploaded progressively through
 * picture_step_dissolve_upload, starting at step 0), and two text
 * records: the treasure's name (TEXT_ID_SUBSTITUTED_TEXT) and the
 * resume choices (TEXT_ID_SOUND_NOVEL_RESUME_CHOICES). The picture
 * record is placed against the first text record from the TIM's width and
 * half height, and the cursor window at the second text record's origin +
 * (0x18, 0x3c).
 *
 * The target reserves 0x10 frame bytes the function never uses. The
 * two offset temporaries keep GCC from folding base_x - (width - 0x3e) into
 * (base_x + 0x3e) - width, and each render-record base pointer is its own
 * local so the pointer shares the register of the constant it dies into. */
static void sound_novel_push_restart_level(s32 mode) {
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

    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.mode = mode;
    index = window_append_animated_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.cursor_window = index;
    g_window_records[index].sequence = WINDOW_ANIM_CURSOR_IDLE;
    g_window_records[index].priority = 0xA;
    g_window_records[index].anim_counter = 0;
    g_window_records[index].frame_index = 0;
    GsGetTimInfo(g_picture_buffer + 1, &image);

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.picture_render = index;
    bounds.x = image.px;
    bounds.y = image.py;
    bounds.w = image.pw;
    bounds.h = image.ph;
    clut.x = image.cx;
    clut.y = image.cy;
    window_init_vram_render_record(index, bounds, clut, DISPLAY_OBJECT_8BIT_CLUT);
    g_window_gs_image_records[index].priority = 0xA;
    picture_step_dissolve_upload(&image, 0);

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.message_render = index;
    bounds.x = 0;
    bounds.y = 0x70;
    bounds.w = 0x7C;
    bounds.h = 0x80;
    g_text_substitution_values[0] = mode + TEXT_ID_TREASURE_NAME_BASE;
    window_init_text_record(index, bounds, 1, TEXT_ID_SUBSTITUTED_TEXT, g_window_image_buffer);
    g_window_gs_image_records[index].priority = 0xA;
    base1 = &g_window_gs_image_records[index].base;
    base1->x = -0x70;
    base1->y = -0x38;
    DrawSync(0);
    depth = g_menu_stack_depth;
    base2 = &g_window_gs_image_records[g_menu_stack_records[depth].sound_novel_resume.picture_render].base;
    offset_x = image.pw - 0x3E;
    base2->x = g_window_gs_image_records[index].base.x - offset_x;
    base3 = &g_window_gs_image_records[g_menu_stack_records[depth].sound_novel_resume.picture_render].base;
    offset_y = (image.ph >> 1) - 0x4C;
    base3->y = g_window_gs_image_records[index].base.y - offset_y;

    index = window_append_record(g_window_render_object_queue, &g_window_render_object_count);
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.option_render = index;
    bounds.x = 0x80;
    bounds.y = 0x70;
    bounds.w = 0x70;
    bounds.h = 0x70;
    window_init_text_record(index, bounds, 1, TEXT_ID_SOUND_NOVEL_RESUME_CHOICES, g_window_image_buffer + 0x400);
    g_window_gs_image_records[index].priority = 0xA;
    base4 = &g_window_gs_image_records[index].base;
    base4->x = 8;
    base4->y = -0x38;
    g_window_records[g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.cursor_window].position.x
        = g_window_gs_image_records[index].base.x + 0x18;
    g_window_records[g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.cursor_window].position.y
        = g_window_gs_image_records[index].base.y + 0x3C;
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.dissolve_step = 1;
    g_menu_stack_records[g_menu_stack_depth].sound_novel_resume.cursor_row = 0;
    g_menu_stack_types[g_menu_stack_depth] = MENU_STACK_LEVEL_SOUND_NOVEL_RESTART;
    g_menu_stack_depth++;
}

/* Input step of the sound novel resume level.
 *
 * While dissolve_step is set the level feeds the dissolve upload
 * (picture_step_dissolve_upload), one step per frame. Cross
 * releases the level, restarts the message box and returns to the treasure
 * detail; Circle fades the screen out, and once the fade is over the level is
 * released and the novel opens, its saved reading discarded when the second
 * row was chosen. Up and down toggle cursor_row, moving the cursor by 16.
 *
 * sound_novel_reset_selected_reading ignores its argument; retail still
 * passes g_sound_novel_selected. */
void sound_novel_step_restart_level(menu_stack_record_t* level) {
    GsIMAGE image;
    s32 buttons;

    if (g_window_gs_image_records[level->sound_novel_resume.message_render].flags & DISPLAY_OBJECT_POP_IN) {
        return;
    }

    if (level->sound_novel_resume.dissolve_step != 0) {
        GsGetTimInfo(g_picture_buffer + 1, &image);
        if (picture_step_dissolve_upload(&image, level->sound_novel_resume.dissolve_step) == 0) {
            level->sound_novel_resume.dissolve_step = 0;
            return;
        }
        level->sound_novel_resume.dissolve_step++;
        return;
    }

    if ((g_system_flags & SYSTEM_FLAG_INPUT_BLOCKED) && !(g_system_flags & SYSTEM_FLAG_FADING)) {
        g_system_flags ^= SYSTEM_FLAG_INPUT_BLOCKED;
        g_window_record_count--;
        g_window_render_record_count -= 3;
        g_window_render_object_count -= 4;
        g_menu_stack_depth--;
        if (level->sound_novel_resume.cursor_row != 0) {
            sound_novel_reset_selected_reading(g_sound_novel_selected);
        }
        sound_novel_push_level(level->sound_novel_resume.mode);
        return;
    }

    buttons = g_world_map_input_new_button_presses;
    if (buttons & PSX_PAD_CROSS) {
        g_window_record_count--;
        g_window_render_record_count -= 3;
        g_window_render_object_count -= 4;
        g_menu_stack_depth--;
        thread_start(WORLD_MAP_THREAD_SLOT_MESSAGE, message_box_thread);
        thread_set_parameters(WORLD_MAP_THREAD_SLOT_MESSAGE, 0x19, TEXT_ID_BRAVE_STORY_INTRO, 0);
        sound_play_effect(SOUND_SFX_CANCEL);
        brave_story_push_treasure_level(level->sound_novel_resume.mode);
        return;
    }

    if (buttons & PSX_PAD_CIRCLE) {
        g_window_records[level->sound_novel_resume.cursor_window].sequence = WINDOW_ANIM_CURSOR_PRESS;
        g_window_records[level->sound_novel_resume.cursor_window].frame_index
            = g_window_records[level->sound_novel_resume.cursor_window].anim_counter = 0;
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x10);
        g_system_flags |= SYSTEM_FLAG_INPUT_BLOCKED;
        sound_play_effect(SOUND_SFX_CONFIRM);
        return;
    }

    if (world_map_update_input_repeat(PSX_PAD_UP) == 0 && world_map_update_input_repeat(PSX_PAD_DOWN) == 0) {
        return;
    }

    level->sound_novel_resume.cursor_row ^= 1;
    g_window_records[level->sound_novel_resume.cursor_window].position.y
        = g_window_gs_image_records[level->sound_novel_resume.option_render].base.y + 0x3c
        + level->sound_novel_resume.cursor_row * 0x10;
    sound_play_effect(SOUND_SFX_CURSOR_MOVE);
}

/* Pushes the event transition level (MENU_STACK_LEVEL_EVENT_TRANSITION)
 * after starting the seek to BATTLE.BIN. A FADE transition freezes the map
 * and fades the screen out. The spinning ones hide everything on the map but
 * the tiles, save VRAM (0x100, 0) 128x256 to (0x200, 0x100) and clear it for
 * the screen capture, set up the transition sprite over that area, fade the
 * music out and pick a random drift and spin direction. */
void event_transition_push_level(s32 mode) {
    RECT rect;
    s32 depth;

    event_transition_seek_battle_bin();
    if (mode == EVENT_TRANSITION_MODE_FADE) {
        g_system_flags |= SYSTEM_FLAG_FREEZE_ANIMATION;
        world_map_fade_start(WORLD_MAP_FADE_OUT, 0x20);
    } else {
        rect.x = 0x100;
        rect.y = 0;
        rect.w = 0x80;
        rect.h = 0x100;
        g_system_flags |= SYSTEM_FLAG_DRAW_MAP | SYSTEM_FLAG_HIDE_MAP_DOTS | SYSTEM_FLAG_HIDE_MAP_ROUTES
            | SYSTEM_FLAG_NO_BACKGROUND_IMAGE | SYSTEM_FLAG_HIDE_CONTEXT_VALUE | SYSTEM_FLAG_HIDE_CALENDAR_DATE
            | SYSTEM_FLAG_HIDE_LOCATION_LABEL;
        g_world_map_marker_state.marker.render.flags |= DISPLAY_OBJECT_HIDDEN;
        MoveImage(&rect, 0x200, 0x100);
        DrawSync(0);
        rect.x = 0x100;
        rect.y = 0;
        rect.w = 0x100;
        rect.h = 0xF0;
        ClearImage(&rect, 0, 0, 0);
        g_event_transition_sprite.attribute = 0x42000000;
        g_event_transition_sprite.x = 0x20;
        g_event_transition_sprite.y = 0x20;
        g_event_transition_sprite.tpage = GetTPage(2, 0, 0x100, 0);
        g_event_transition_sprite.w = 0xFF;
        g_event_transition_sprite.mx = 0xA0;
        g_event_transition_sprite.my = 0x98;
        g_event_transition_sprite.scaley = ONE;
        g_event_transition_sprite.scalex = ONE;
        g_event_transition_sprite.cy = 0;
        g_event_transition_sprite.cx = 0;
        g_event_transition_sprite.v = 0;
        g_event_transition_sprite.u = 0;
        g_event_transition_sprite.h = 0xF0;
        g_event_transition_sprite.rotate = 0;
        g_event_transition_sprite.r = 0x80;
        g_event_transition_sprite.g = 0x80;
        g_event_transition_sprite.b = 0x80;
        audio_queue_add(AUDIO_COMMAND_FADE_OUT, 0x10);
        g_menu_stack_records[g_menu_stack_depth].event_transition.frame = 0;
        g_menu_stack_records[g_menu_stack_depth].event_transition.anim_step = 0;
        g_menu_stack_records[g_menu_stack_depth].event_transition.drift_direction = (rand() * 4) >> 15;
        g_menu_stack_records[g_menu_stack_depth].event_transition.spin_direction = (rand() * 2) >> 15;
        g_menu_stack_records[g_menu_stack_depth].event_transition.phase = EVENT_TRANSITION_PHASE_SPIN_IN;
        g_menu_stack_records[g_menu_stack_depth].event_transition.fade_phase = EVENT_TRANSITION_FADE_NONE;
    }

    depth = g_menu_stack_depth;
    g_menu_stack_records[depth].event_transition.mode = mode;
    g_menu_stack_types[depth] = MENU_STACK_LEVEL_EVENT_TRANSITION;
    g_menu_stack_depth = depth + 1;
}
